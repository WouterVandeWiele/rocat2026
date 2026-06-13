#pragma once

#include "driver_base.h"
#include <SparkFunSX1509.h>

class PowerManagerDriver : public DriverBase {
public:
    PowerManagerDriver(SX1509& sx, uint8_t pin_keep_awake);

    void begin() override;

    void shut_down();

private:
    SX1509& _sx;
    const uint8_t _pin_keep_awake;
};
