#pragma once

#include "driver_base.h"

#include <SparkFunLIS3DH.h>
#include <SparkFunSX1509.h>

class BatteryDriver : public DriverBase {
public:
    // address: 0x00 — current implementation is ADC-only, no I2C fuel gauge
    explicit BatteryDriver(LIS3DH& lis, SX1509& sx);

    void begin() override;

    // Returns estimated battery percentage (0–100)
    float get_voltage();

    // Returns raw 10-bit ADC value from LIS3DH ADC1 (0–1023)
    uint16_t get_raw_adc();

    // Returns true if charger is active (via SX1509 pin_chg_ind)
    bool is_charging();

private:
    LIS3DH& _lis;
    SX1509& _sx;
};
