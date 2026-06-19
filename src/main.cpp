#include <Arduino.h>
#include "driver/gpio.h"
#include "rocat_constants.h"

#include "GUI_Utils.h"
#include "WeatherAnim.h"
#include "Weather.h"

#include <Wire.h>           // Include the I2C library (required)
#include <SPIFFS.h>
#include <SparkFunSX1509.h> //Click here for the library: http://librarymanager/All#SparkFun_SX1509
#include <SparkFunLIS3DH.h>

#include "motor_driver.h"
#include "led_driver.h"
#include "power_manager_driver.h"
#include "rtc_driver.h"
#include "accelerometer_driver.h"
#include "battery_driver.h"
#include "audio_driver.h"   // uses I2S DAC — enables both DAC1(GPIO25) and DAC2(GPIO26=SDA), breaking I2C
// #include "audio_driver_pwm.h"
#include "touch_driver.h"
#include "ldr_driver.h"
#include "adc_keys.h"
#include "wifi_driver.h"

#include "debug_cli.h"

SX1509 io;
LIS3DH lis(I2C_MODE, lis3dh_address);

SED1530_LCD display(io, pin_dis_ao, pin_dis_rw, pin_dis_en, pin_dis_bl, pin_dis_pwr, pin_display);
MotorDriver motor(io, pin_m1_dir, pin_m1_spd, pin_m2_dir, pin_m2_spd, pin_m_stby, pin_m_drv_en, pin_m_fault);
LedDriver leds(io, pin_rgb_pwr);
PowerManagerDriver wakeup(io, pin_mcu_keep_awake);
RtcDriver rtcDriver(Wire);
AccelerometerDriver accelerometer(lis);
BatteryDriver battery(lis, io);

AudioDriver audio(io, pin_aud_en, pin_aud_sig);
// AudioDriverPWM audio(io, pin_aud_en, pin_aud_sig);
TouchDriver touch(pin_touch_pet_sens_1, pin_touch_pet_sens_2);
LdrDriver ldr(pin_gpio_ldr);
AdcKeys adcKeys(pin_gpio_js_a_c1, pin_gpio_js_b_d1, pin_gpio_js_c1_2,
                pin_gpio_js_a_c2, pin_gpio_js_b_d2);
WifiDriver wifi("ROCAT");

DebugCLI debugCLI(wakeup, rtcDriver, accelerometer, battery, wifi);


extern bool motionDetected;
// ── animation gallery ─────────────────────────────────────────────────────────

struct AnimEntry {
    int         code;
    bool        isDay;
    const char* label;    // max 16 chars
};

static const AnimEntry GALLERY[] = {
    {  0, true,  "0 Clear/Day"     },
    {  0, false, "0 Clear/Night"   },
    {  1, true,  "1 Mainly clr/D"  },
    {  1, false, "1 Mainly clr/N"  },
    {  2, true,  "2 Part.cloudy/D" },
    {  2, false, "2 Part.cloudy/N" },
    {  3, true,  "3 Overcast"      },
    { 45, true,  "45 Fog"          },
    { 48, true,  "48 Dep.fog"      },
    { 51, true,  "51 Lt drizzle"   },
    { 53, true,  "53 Drizzle"      },
    { 55, true,  "55 Hvy drizzle"  },
    { 56, true,  "56 Frz drizzle"  },
    { 57, true,  "57 Hvy frz drz"  },
    { 61, true,  "61 Lt rain"      },
    { 63, true,  "63 Rain"         },
    { 65, true,  "65 Hvy rain"     },
    { 66, true,  "66 Frz rain"     },
    { 67, true,  "67 Hvy frz rain" },
    { 71, true,  "71 Lt snow"      },
    { 73, true,  "73 Snow"         },
    { 75, true,  "75 Hvy snow"     },
    { 77, true,  "77 Snow grains"  },
    { 80, true,  "80 Lt showers"   },
    { 81, true,  "81 Showers"      },
    { 82, true,  "82 Hvy showers"  },
    { 85, true,  "85 Snow shower"  },
    { 86, true,  "86 Hvy sn/show"  },
    { 95, true,  "95 Thunderstorm" },
    { 96, true,  "96 T-storm hail" },
    { 99, true,  "99 T-storm hail" },
};
static const int GALLERY_COUNT = (int)(sizeof(GALLERY) / sizeof(GALLERY[0]));

// ── dummy weather data for phase 2 ───────────────────────────────────────────

static weather_t makeDummyWeather(int code = 63, bool isDay = true) {
    weather_t w           = {};
    w.status              = true;
    w.temperature         = 17.3f;
    w.apparent_temperature = 15.1f;
    w.humidity            = 62;
    w.wind_speed          = 14.2f;
    w.wind_direction      = 225;
    w.weather_code        = code;
    w.cloud_cover         = 85;
    w.rain                = 2.4f;
    w.showers             = 0.0f;
    w.snowfall            = 0.0f;
    w.precipitation       = 2.4f;
    w.is_day              = isDay;
    return w;
}

// ── state ─────────────────────────────────────────────────────────────────────

static GFXcanvas1  animCanvas(WeatherAnim::W, WeatherAnim::H);
static WeatherAnim anim;

enum Phase { e_GALLERY, e_CAROUSEL };
static Phase         _phase    = e_GALLERY;
static int           _galIdx   = 0;
static unsigned long _galStart = 0;

static void enterGallery(int idx) {
    _galIdx   = idx;
    _galStart = millis();
    Serial.printf("[test-weather] gallery %s\n", GALLERY[idx].label);
}

static void enterCarousel() {
    _phase         = e_CAROUSEL;
    Weather::w     = makeDummyWeather();
    Weather::init(millis());
    Serial.println("[test-weather] carousel render");
}

void setup() {
  // The RTC/analog pad-mux state for GPIO25 (DAC1) persists across a soft
  // reset (esp_restart/watchdog) but not a power-on reset, so depending on
  // reset history this pin can start up either already in RTC/analog mode
  // or in its default floating digital state. gpio_reset_pin() forces it
  // back to default digital/IOMUX-GPIO mode regardless of reset history, so
  // the pinMode/digitalWrite below reliably take effect on every boot.
//   gpio_reset_pin((gpio_num_t)pin_aud_sig);

  // Give the audio DAC pin (GPIO25) a defined, low-impedance state before
  // any I2C init runs. Until audio.begin() reconfigures it as the DAC pad,
  // it would otherwise float and capacitively pick up the adjacent I2C
  // bus (SDA=26/SCL=27) traffic from io.begin()/accelerometer/battery/etc.
  // as data-like pulses.
//   pinMode(pin_aud_sig, OUTPUT);
//   digitalWrite(pin_aud_sig, LOW);

  Serial.begin(115200);

  Serial.println("                                                                                                                ");
  Serial.println("                                                                                                              ");
  Serial.println("RRRRRRRRRRRRRRRRR        OOOOOOOOO             CCCCCCCCCCCCC               AAA         TTTTTTTTTTTTTTTTTTTTTTT");
  Serial.println("R::::::::::::::::R     OO:::::::::OO        CCC::::::::::::C              A:::A        T:::::::::::::::::::::T");
  Serial.println("R::::::RRRRRR:::::R  OO:::::::::::::OO    CC:::::::::::::::C             A:::::A       T:::::::::::::::::::::T");
  Serial.println("RR:::::R     R:::::RO:::::::OOO:::::::O  C:::::CCCCCCCC::::C            A:::::::A      T:::::TT:::::::TT:::::T");
  Serial.println("  R::::R     R:::::RO::::::O   O::::::O C:::::C       CCCCCC           A:::::::::A     TTTTTT  T:::::T  TTTTTT");
  Serial.println("  R::::R     R:::::RO:::::O     O:::::OC:::::C                        A:::::A:::::A            T:::::T        ");
  Serial.println("  R::::RRRRRR:::::R O:::::O     O:::::OC:::::C                       A:::::A A:::::A           T:::::T        ");
  Serial.println("  R:::::::::::::RR  O:::::O     O:::::OC:::::C                      A:::::A   A:::::A          T:::::T        ");
  Serial.println("  R::::RRRRRR:::::R O:::::O     O:::::OC:::::C                     A:::::A     A:::::A         T:::::T        ");
  Serial.println("  R::::R     R:::::RO:::::O     O:::::OC:::::C                    A:::::AAAAAAAAA:::::A        T:::::T        ");
  Serial.println("  R::::R     R:::::RO:::::O     O:::::OC:::::C                   A:::::::::::::::::::::A       T:::::T        ");
  Serial.println("  R::::R     R:::::RO::::::O   O::::::O C:::::C       CCCCCC    A:::::AAAAAAAAAAAAA:::::A      T:::::T        ");
  Serial.println("RR:::::R     R:::::RO:::::::OOO:::::::O  C:::::CCCCCCCC::::C   A:::::A             A:::::A   TT:::::::TT      ");
  Serial.println("R::::::R     R:::::R OO:::::::::::::OO    CC:::::::::::::::C  A:::::A               A:::::A  T:::::::::T      ");
  Serial.println("R::::::R     R:::::R   OO:::::::::OO        CCC::::::::::::C A:::::A                 A:::::A T:::::::::T      ");
  Serial.println("RRRRRRRR     RRRRRRR     OOOOOOOOO             CCCCCCCCCCCCCAAAAAAA                   AAAAAAATTTTTTTTTTT      ");
  Serial.println("                                                                                                              ");
  Serial.println("                                                                                                              ");

  Serial.printf("Compiled on %s at %s\n", __DATE__, __TIME__);

  
  Serial.println("Mounting SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted");
  }

  Serial.println("Setting up IO expander...");
  Wire.begin(pin_gio_sda, pin_gio_scl, 400000UL);
  if (io.begin(sx1509_address) == false)
  {
    Serial.printf("Failed to communicate. Check wiring and address(%x) of SX1509.\n", sx1509_address);
    while (1)
    {
      // If we fail to communicate, loop forever.
      delay(1000);
    }
  }

  Serial.println("Setting up esp wakeup manager...");
  wakeup.begin();

  Serial.println("Setting up LCD...");
  display.lcd_init();
  // display.setTextWrap(false);
//   enterGallery(0);

  Serial.println("Setting up motor driver...");
  motor.begin();
  motor.enable(true);
  motor.standby(true);
  motor.set_motor1(-20);
  motor.set_motor2(200);

  Serial.println("Setting up ws2812...");
  leds.setup();
  leds.power_on();
  leds.begin();
  
  Serial.println("Setting up RTC driver...");
  rtcDriver.begin();


  Serial.println("Setting up accelerometer...");

  lis.settings.adcEnabled = 1;
  //Note:  By also setting tempEnabled = 1, temperature data is available
  //on ADC3.  Temperature *differences* can be read at a rate of
  //1 degree C per unit of ADC3
  lis.settings.tempEnabled = 1;
  lis.settings.accelSampleRate = 50;  //Hz.  Can be: 0,1,10,25,50,100,200,400,1600,5000 Hz
  lis.settings.accelRange = 2;      //Max G force readable.  Can be: 2, 4, 8, 16
  lis.settings.xAccelEnabled = 1;
  lis.settings.yAccelEnabled = 1;
  lis.settings.zAccelEnabled = 1;
  
  //Call .begin() to configure the IMU
  accelerometer.begin();
  Serial.printf("Accelerometer interrupt %d\n", accelerometer.read_interrupt_source());
  accelerometer.clear_interrupt();

  Serial.println("Setting up battery driver...");
  battery.begin();

  Serial.println("Setting up audio driver...");

  File root = SPIFFS.open("/");
  File f = root.openNextFile();
  while (f) {
      Serial.printf("  %s  %u bytes\n", f.name(), f.size());
      f = root.openNextFile();
  }

  audio.begin();
  // audio.play("/3 Doors Down - Kryptonite.mp3");

  Serial.println("Setting up touch pins...");
  touch.begin();
  Serial.println("Setting up LDR driver...");
  ldr.begin();

  Serial.println("Setting up ADC keys...");
  adcKeys.begin();

  Serial.println("Setting up WiFi...");
  wifi.begin();

  Serial.println("Setting up debug CLI...");
  debugCLI.begin();
}

static const char* gem_key_name(byte key) {
    switch (key) {
        case GEM_KEY_UP:     return "UP";
        case GEM_KEY_DOWN:   return "DOWN";
        case GEM_KEY_LEFT:   return "LEFT";
        case GEM_KEY_RIGHT:  return "RIGHT";
        case GEM_KEY_OK:     return "OK";
        case GEM_KEY_CANCEL: return "CANCEL";
        case GEM_KEY_FUNC_1_DOWN: return "FUNC_1_DOWN";
        case GEM_KEY_FUNC_2_UP: return "FUNC_2_UP";
        case GEM_KEY_FUNC_3_LEFT: return "FUNC_3_LEFT";
        case GEM_KEY_FUNC_4_RIGHT: return "FUNC_4_RIGHT";
        default:             return "NONE";
    }
}

void loop() {
  // audio.tick();
    if (motionDetected) {
        // Reset de vlag direct om volgende interrupts op te vangen
        motionDetected = false;

        Serial.println("![ALERT] Beweging gedetecteerd via Rising Edge!");

        // --- HIER ROEP JOUW LIS3DH FUNCTIE AAN ---
        // Onthoud: Omdat de LIS3DH interrupt op 'Latch' staat (LIR_INT1), 
        // moet je hier NU het INT1_SRC register uitlezen via I2C, 
        // anders blijft de pin HIGH en krijg je nooit meer een nieuwe interrupt!
        //
        // accelDriver.clear_motion_interrupt_latch(); 
        // ----------------------------------------
    }
    // unsigned long now = millis();

    // if (_phase == e_GALLERY) {
    //     // Advance after 3 s
    //     if (now - _galStart >= 3000UL) {
    //         int next = _galIdx + 1;
    //         if (next >= GALLERY_COUNT) {
    //             enterCarousel();
    //             return;
    //         }
    //         enterGallery(next);
    //     }

    //     const AnimEntry& e = GALLERY[_galIdx];
    //     clearCanvas();
    //     animCanvas.fillScreen(0);
    //     anim.draw(animCanvas, e.code, e.isDay, now);

    //     // Centre the 40×40 animation, leaving bottom 8 px for label
    //     int ax = (LCD_W - WeatherAnim::W) / 2;
    //     display.drawBitmap(ax, 0,
    //                        animCanvas.getBuffer(),
    //                        WeatherAnim::W, WeatherAnim::H,
    //                        GLCD_COLOR_SET);

    //     // Label in bottom strip
    //     display.setTextSize(1);
    //     display.setTextColor(GLCD_COLOR_SET);
    //     int labelW = (int)strlen(e.label) * 6;
    //     display.setCursor((LCD_W - labelW) / 2, LCD_H - 8);
    //     display.print(e.label);

    //     display.display();
    //     delay(33);

    // } else {
    //     // Phase 2: full carousel weather render with dummy data
    //     bool done = Weather::tick(now);
    //     if (done) {
    //         // Reset rotation so it loops
    //         Weather::w = makeDummyWeather();
    //         Weather::init(millis());
    //     }
    // }


    wifi.process();

    adcKeys.tick();
    ButtonPress bp;
    while (xQueueReceive(adcKeys.queue(), &bp, 0) == pdTRUE) {
        Serial.printf("[adc_keys] %s\n", gem_key_name(bp.button));
    }
    // adcKeys.debug();

    switch (touch.tick()) {
        case PetGesture::DOWN: Serial.println("[touch] pet down"); break;
        case PetGesture::UP:   Serial.println("[touch] pet up");   break;
        default: break;
    }
    // touch.tickB();

    
    leds.led_breath();
    static unsigned long _last_print = 0;
    if (millis() - _last_print >= 500) {
        _last_print = millis();

        float ldrVal   = ldr.read();
        float batt  = battery.get_voltage();
        float ax       = accelerometer.get_accel_x();
        float ay       = accelerometer.get_accel_y();
        float az       = accelerometer.get_accel_z();
        float temp     = accelerometer.get_temp();

        // Serial.printf(">ldr_ohm:%.1f\n", ldrVal);
        // Serial.printf(">batt:%.2f\n",       batt);
        // Serial.printf(">accel_x:%.3f\n", ax);
        // Serial.printf(">accel_y:%.3f\n", ay);
        // Serial.printf(">accel_z:%.3f\n", az);
        // Serial.printf(">temp:%.3f\n",    temp);

        char line[20];
        clearCanvas();
        display.setTextSize(1);
        display.setTextColor(GLCD_COLOR_SET);

        // Two columns of 3 values, each value shown as a label line
        // followed by its reading on the next line.
        const int colX[2] = { 0, 50 };

        display.setCursor(colX[0], 0);  display.print("LDR");
        snprintf(line, sizeof(line), "%.0f", ldrVal);
        display.setCursor(colX[0], 8);  display.print(line);

        display.setCursor(colX[0], 16); display.print("Batt");
        snprintf(line, sizeof(line), "%.2f", batt);
        display.setCursor(colX[0], 24); display.print(line);

        display.setCursor(colX[0], 32); display.print("AccX");
        snprintf(line, sizeof(line), "%.2f", ax);
        display.setCursor(colX[0], 40); display.print(line);

        display.setCursor(colX[1], 0);  display.print("AccY");
        snprintf(line, sizeof(line), "%.2f", ay);
        display.setCursor(colX[1], 8);  display.print(line);

        display.setCursor(colX[1], 16); display.print("AccZ");
        snprintf(line, sizeof(line), "%.2f", az);
        display.setCursor(colX[1], 24); display.print(line);

        display.setCursor(colX[1], 32); display.print("Temp");
        snprintf(line, sizeof(line), "%.2f", temp);
        display.setCursor(colX[1], 40); display.print(line);

        display.display();
    }
}
