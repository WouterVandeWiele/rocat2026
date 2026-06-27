#pragma once

#include "../DRIVER_BASE/driver_base.h"
#include <SparkFunTMP102.h>

class TMP102Driver : public DriverBase {
public:
    TMP102Driver(TwoWire& wire, uint8_t address = 0x48);

    void begin() override;

    float read_temp_c();
    float read_temp_f();

private:
    TwoWire& _wire;
    TMP102   _sensor;
    uint8_t  _addr;
    bool     _ready = false;
};
