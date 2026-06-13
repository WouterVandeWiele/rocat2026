#include "motor_driver.h"

MotorDriver::MotorDriver(SX1509& sx,
                         uint8_t pin_m1_dir, uint8_t pin_m1_spd,
                         uint8_t pin_m2_dir, uint8_t pin_m2_spd,
                         uint8_t pin_stby, uint8_t pin_enable, uint8_t pin_fault)
    : DriverBase(), _sx(sx),
      _pin_m1_dir(pin_m1_dir), _pin_m1_spd(pin_m1_spd),
      _pin_m2_dir(pin_m2_dir), _pin_m2_spd(pin_m2_spd),
      _pin_stby(pin_stby), _pin_enable(pin_enable), _pin_fault(pin_fault) {}

void MotorDriver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.pinMode(_pin_m1_dir,  OUTPUT);
    _sx.pinMode(_pin_m2_dir,  OUTPUT);
    _sx.pinMode(_pin_stby,    OUTPUT);
    _sx.pinMode(_pin_enable,  OUTPUT);
    _sx.pinMode(_pin_m1_spd,  ANALOG_OUTPUT);
    _sx.pinMode(_pin_m2_spd,  ANALOG_OUTPUT);
    _sx.pinMode(_pin_fault,   INPUT);
}

void MotorDriver::set_motor1(int16_t speed) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _set_motor(_pin_m1_dir, _pin_m1_spd, speed);
}

void MotorDriver::set_motor2(int16_t speed) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _set_motor(_pin_m2_dir, _pin_m2_spd, speed);
}

void MotorDriver::standby(bool standby) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_stby, standby);
}

void MotorDriver::enable(bool enable) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_enable, enable);
}

void MotorDriver::_set_motor(uint8_t pin_dir, uint8_t pin_spd, int16_t speed) {
    _sx.digitalWrite(pin_dir, speed >= 0 ? HIGH : LOW);
    _sx.analogWrite(pin_spd, (uint8_t)abs(speed));
}
