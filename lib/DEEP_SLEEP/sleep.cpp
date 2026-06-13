#include <Arduino.h>
#include "constants.h"
// #include "driver/adc.h"   
#include "driver/rtc_io.h"

#include "esp32/ulp.h"
#include "soc/rtc_cntl_reg.h"

#include "esp_sleep.h"
#include <esp_bt.h>             
#include <esp_wifi.h>

// https://neurotechhub.wustl.edu/using-the-esp32-s3-ulp-coprocessor-with-the-arduino-ide/

// https://forum.arduino.cc/t/esp32-ulp-analog-reading-what-are-the-arguments-for-i-adc-c-macro/1195753/3
// This is just a test ULP C macro code. It reads 31 analog values and store them in
// RTC_SLOW_MEM [101 .. 132]. It also stores the number of samples taken into 
// RTC_SLOW_MEM [100]
//
// Connect GPIO 25, which will serve as output of analog voltage with a wire to 
// GPIO 34, where the voltage will be mesaured.

// ULP adc program
// const ulp_insn_t program [] = {
//     I_MOVI  (R1, 0),            // R1 = RTC_SLOW_MEM;                   // R1 is pointer to RTC_SLOW_MEM array
//     I_MOVI  (R2, 0),            // R2 = 0;                              // R2 is sample counter

//     M_LABEL (1),                // do {

//         I_MOVI  (R0, 200),          // R0 = n * 1000 / 5, where n is the number of seconds to delay, 200 = 1 s
//         M_LABEL (2),                // do {
//             I_DELAY (40000),            // delay (5);                           // since ULP runs at 8 MHz, 40000 cycles correspond to 5 ms (max possible delay is 65535 cycles or 8.19 ms)
//             I_SUBI  (R0, R0, 1),        // R0 --;
//         M_BGE (2, 1),               // } while (R0 >= 1); ... jump to label 2 if R0 > 0

//         I_ADC   (R3, 0, 6),         // R3 ≈ adc1_get_raw (ADC1_CHANNEL_6);  // GPIO 34
//         I_ADDI  (R2, R2, 1),        // R2 ++
//         I_ST    (R3, R2, 100),      // RTC_SLOW_MEM [R2 + 100] = R3;        // store measurement result to RTC_SLOW_MEM [R2 + 100]
//         I_ST    (R2, R1, 100),      // RTC_SLOW_MEM [R1 + 100] = R2;        // store sample count to RTC_SLOW_MEM [100]

//     I_MOVR  (R0, R2),           // while (R2 < 32);
//     M_BL    (1, 32),            // ... jump to label 1 if R0 < 32

//     I_END   ()
// };



//Method to print the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
        default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}

void start_ulp_program(void) {

    // RTC GPIO channel mapping: GPIO36=ch0, GPIO2=ch12, GPIO12=ch15
    // M_BL(2,1) wakes on LOW; swap to M_BGE(2,1) for active-HIGH interrupt pins
    const ulp_insn_t program[] = {
      M_LABEL(1),

      // pin_gpio_js_c1_2: GPIO 36 = RTC channel 0
      I_RD_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 0, RTC_GPIO_IN_NEXT_S + 0),
      M_BL(2, 1),

      // pin_rtc_int1: GPIO 12 = RTC channel 15
      I_RD_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 15, RTC_GPIO_IN_NEXT_S + 15),
      M_BL(2, 1),

      // pin_acc_int1: GPIO 2 = RTC channel 12
      I_RD_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 12, RTC_GPIO_IN_NEXT_S + 12),
      M_BL(2, 1),

      M_BX(1),

      M_LABEL(2),
      I_WAKE(),
      I_HALT(),
    };
    size_t load_addr = 0;


    size_t size = sizeof(program) / sizeof(ulp_insn_t);

    esp_err_t err = ulp_process_macros_and_load(load_addr, program, &size);
    if (err != ESP_OK)
    {
        Serial.printf("Error loading ULP program: %d\n", err);
        return;
    }

    // Set ULP wakeup interval (in microseconds)
    // err = ulp_set_wakeup_period(0, 5000000); // 5 seconds
    // if (err != ESP_OK)
    // {
    //     Serial.printf("Error setting timer period: %d\n", err);
    //     return;
    // }

    err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK)
    {
        Serial.printf("Error enabling ULP as wakup source: %d\n", err);
        return;
    }

    // Start ULP from instruction 0
    err = ulp_run(load_addr);
    if (err != ESP_OK)
    {
        Serial.printf("Error starting ULP program: %d\n", err);
        return;
    }
}

void setup_deep_sleep()
{

    btStop();               // Power down BT for best power saving
    esp_wifi_stop();        // Power down the Wifi radios for best power saving

    start_ulp_program();
    esp_deep_sleep_start();
}

void recover_deep_sleep()
{
    print_wakeup_reason();

    // esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // if (wakeup_reason == ESP_SLEEP_WAKEUP_ULP)
    // {
    //     // Only print counter value if we woke up from deep sleep
    //     // Serial.printf("\nDeep sleep time: %d seconds\n", DEEP_SLEEP_TIME / 1000000);
    //     // Serial.printf("ULP timer period: %d milliseconds\n", ULP_TIMER_PERIOD / 1000);
    //     Serial.printf("Counter value:  %4i\n", RTC_SLOW_MEM[100] & 0xFFF);
    // }
}