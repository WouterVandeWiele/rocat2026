#include "tmp102_driver.h"

TMP102Driver::TMP102Driver(TwoWire& wire, uint8_t address)
    : DriverBase(), _wire(wire), _addr(address) {}

void TMP102Driver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);

    if (!_sensor.begin(_addr, _wire)) {
        Serial.printf("[tmp102] not found at 0x%02X\n", _addr);
        return;
    }

    _ready = true;
    Serial.printf("[tmp102] found at 0x%02X\n", _addr);
    _sensor.sleep();
}

float TMP102Driver::read_temp_c() {
    if (!_ready) return 0.0f;

    std::lock_guard<std::mutex> lck(i2c_operations);

    _sensor.oneShot(1);
    while (_sensor.oneShot() == 0)
        ;

    float temp = _sensor.readTempC();
    _sensor.sleep();
    return temp;
}

float TMP102Driver::read_temp_f() {
    if (!_ready) return 0.0f;

    std::lock_guard<std::mutex> lck(i2c_operations);

    _sensor.oneShot(1);
    while (_sensor.oneShot() == 0)
        ;

    float temp = _sensor.readTempF();
    _sensor.sleep();
    return temp;
}
