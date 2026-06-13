#include "power_manager_driver.h"

PowerManagerDriver::PowerManagerDriver(SX1509& sx, uint8_t pin_keep_awake)
    : DriverBase()
    , _sx(sx)
    , _pin_keep_awake(pin_keep_awake) {}

void PowerManagerDriver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.pinMode(_pin_keep_awake, OUTPUT);
    _sx.digitalWrite(_pin_keep_awake, HIGH);
}

void PowerManagerDriver::shut_down() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_keep_awake, LOW);
}
