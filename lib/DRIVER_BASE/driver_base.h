#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <mutex>

class DriverBase {
public:
    virtual void begin() = 0;

protected:
    inline static std::mutex i2c_operations;
};
