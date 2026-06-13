#include "battery_driver.h"
#include <algorithm>

// LIS3DH ADC1 is 10-bit (0–1023), full-scale = LIS_VDD.
// A voltage divider sits between the battery and ADC1: DIVIDER_RATIO = R2/(R1+R2).
// Adjust DIVIDER_RATIO to match your hardware.
static constexpr float DIVIDER_RATIO = 0.32258064516129f;  // R2/(R1+R2)
static constexpr float ADC_BIT_STEP_VOLTAGE = 0.78125f;
static constexpr float ADC_MIN_VOLTAGE = 800.0f;

static constexpr float ADC_CAL_OFFSET = 316.0f;

BatteryDriver::BatteryDriver(LIS3DH& lis, SX1509& sx)
    : DriverBase(), _lis(lis), _sx(sx) {}

void BatteryDriver::begin() {
    // _lis.begin in currently executed in main setup
}

float BatteryDriver::get_voltage() {
    float adc_v = (get_raw_adc() * ADC_BIT_STEP_VOLTAGE) + ADC_MIN_VOLTAGE;
    return (adc_v / DIVIDER_RATIO) + ADC_CAL_OFFSET;
    // float pct   = (vbat - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f;
    // return (uint8_t)std::max(0.0f, std::min(100.0f, pct));
}

uint16_t BatteryDriver::get_raw_adc() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return _lis.read10bitADC1();
}

bool BatteryDriver::is_charging() {
    // TODO: read pin_chg_ind from SX1509 when SX1509 reference is available
    return false;
}
