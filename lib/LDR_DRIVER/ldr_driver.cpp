#include "ldr_driver.h"
#include <Arduino.h>

// Voltage divider: LDR on top (Vcc side), fixed resistor on the bottom (GND side),
// ADC sampling the midpoint node.
//   Vout = Vcc * R_FIXED / (R_ldr + R_FIXED)
//   R_ldr = R_FIXED * (Vcc / Vout - 1) = R_FIXED * (ADC_MAX / adc - 1)
// (assuming the ADC reference equals Vcc, so the Vcc terms cancel out)
static constexpr float   R_FIXED = 10000.0f;
static constexpr int     ADC_MAX = 4095;     // ESP32 ADC: 12-bit

LdrDriver::LdrDriver(uint8_t pin_adc)
    : DriverBase(), _pin_adc(pin_adc) {}

void LdrDriver::begin() {
    pinMode(_pin_adc, INPUT);
}

float LdrDriver::read() {
    int adc = analogRead(_pin_adc);
    if (adc < 1) adc = 1;  // avoid division by zero / negative resistance
    return R_FIXED * ((float)ADC_MAX / adc - 1.0f);
}
