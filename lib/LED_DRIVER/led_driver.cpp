#include "led_driver.h"
#include <mutex>
#include "rocat_constants.h"


LedDriver::LedDriver(SX1509& sx, uint8_t pwr_pin)
    : DriverBase(), _sx(sx), _pwr_pin(pwr_pin),
      _strip(count_ws2812, pin_gio_ws2812, NEO_GRB + NEO_KHZ800) {}

void LedDriver::begin() {
    {
        std::lock_guard<std::mutex> lck(i2c_operations);
        _sx.pinMode(_pwr_pin, OUTPUT);
        _sx.digitalWrite(_pwr_pin, LOW);
    }

    // Pin is already OUTPUT LOW from ESP-IDF calls in setup().
    // begin() will call pinMode() which may glitch, but LEDs have
    // no power so it doesn't matter.
    _strip.begin();
    _strip.setBrightness(100);
}

void LedDriver::power(uint8_t on) {
    if (!on) {
        _strip.clear();
        _strip.show();
    }
    {
        std::lock_guard<std::mutex> lck(i2c_operations);
        _sx.digitalWrite(_pwr_pin, on);
    }
    if (on) {
        delay(1);
        _strip.clear();
        _strip.show();
    }
}

// void LedDriver::power_off() {
//     std::lock_guard<std::mutex> lck(i2c_operations);
//     _sx.digitalWrite(_pwr_pin, LOW);
// }

void LedDriver::show() {
    _strip.show();
}

void LedDriver::set_random() {
    for (uint8_t i = 0; i < count_ws2812; i++)
        _strip.setPixelColor(i, _strip.Color(random(256), random(256), random(256)));
}

void LedDriver::set_all(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t c = _strip.Color(r, g, b);
    for (uint8_t i = 0; i < count_ws2812; i++)
        _strip.setPixelColor(i, c);
}

void LedDriver::set(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= count_ws2812) return;
    _strip.setPixelColor(index, _strip.Color(r, g, b));
}

void LedDriver::set_brightness(uint8_t brightness) {
    _strip.setBrightness(brightness);
}

void LedDriver::led_clear() {
    _strip.clear();
    _strip.show();
}

void LedDriver::led_breath() {
    uint32_t current_millis = millis();
    if (current_millis - _breath_prev_millis < 50) return;
    _breath_prev_millis = current_millis;

    _breath_hue++;

    _breath_val += _breath_val_inc;
    if (_breath_val <= 0.0f || _breath_val >= 255.0f)
        _breath_val_inc = -_breath_val_inc;

    for (uint8_t i = 0; i < count_ws2812; i++) {
        uint8_t hue = _breath_hue + (uint8_t)((uint16_t)i * 256 / count_ws2812);
        _strip.setPixelColor(i, _strip.gamma32(_strip.ColorHSV((uint16_t)hue * 256, 255, (uint8_t)_breath_val)));
    }
    _strip.show();
}
