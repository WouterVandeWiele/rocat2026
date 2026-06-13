#pragma once

#include "driver_base.h"

class LdrDriver : public DriverBase {
public:
    explicit LdrDriver(uint8_t pin_adc);

    void begin() override;

    // Returns the photoresistor's resistance in ohms, derived from the
    // voltage divider (LDR on top, fixed 10k resistor on the bottom,
    // ADC sampling the midpoint).
    float read();

private:
    const uint8_t _pin_adc;
};
