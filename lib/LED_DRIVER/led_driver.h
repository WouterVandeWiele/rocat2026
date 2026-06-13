#pragma once

#include "driver_base.h"
#include <Adafruit_NeoPixel.h>
#include <SparkFunSX1509.h>

class LedDriver : public DriverBase {
public:
    LedDriver(SX1509& sx, uint8_t pwr_pin);
    void setup();
    void begin() override;
    void power_on();
    void power_off();

    void show();

    void set_random();
    void set_all(uint8_t r, uint8_t g, uint8_t b);
    void set(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    void set_brightness(uint8_t brightness);

    void led_clear();
    void led_breath();

private:
    SX1509& _sx;
    const uint8_t _pwr_pin;
    Adafruit_NeoPixel _strip;

    uint32_t _breath_prev_millis = 0;
    uint8_t  _breath_hue         = 0;
    float    _breath_val         = 128.0f;
    float    _breath_val_inc     = 0.5f;
};
