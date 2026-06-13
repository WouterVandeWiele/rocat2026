#pragma once

#include "driver_base.h"
#include <SparkFunSX1509.h>

class MotorDriver : public DriverBase {
public:
    MotorDriver(SX1509& sx,
                uint8_t pin_m1_dir, uint8_t pin_m1_spd,
                uint8_t pin_m2_dir, uint8_t pin_m2_spd,
                uint8_t pin_stby, uint8_t pin_enable, uint8_t pin_fault);

    void begin() override;

    // Sets motor 1 speed; sign encodes direction (-255 to +255)
    void set_motor1(int16_t speed);

    // Sets motor 2 speed; sign encodes direction (-255 to +255)
    void set_motor2(int16_t speed);

    void standby(bool standby);
    void enable(bool enable);

private:
    SX1509& _sx;
    const uint8_t _pin_m1_dir;
    const uint8_t _pin_m1_spd;
    const uint8_t _pin_m2_dir;
    const uint8_t _pin_m2_spd;
    const uint8_t _pin_stby;
    const uint8_t _pin_enable;
    const uint8_t _pin_fault;

    void _set_motor(uint8_t pin_dir, uint8_t pin_spd, int16_t speed);
};
