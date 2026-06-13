#include "accelerometer_driver.h"
#include <Arduino.h>

#include "rocat_constants.h"

// INT2 generator registers (not defined in SparkFun library header)
static constexpr uint8_t LIS3DH_INT2_CFG      = 0x34;
static constexpr uint8_t LIS3DH_INT2_SRC      = 0x35;
static constexpr uint8_t LIS3DH_INT2_THS      = 0x36;
static constexpr uint8_t LIS3DH_INT2_DURATION = 0x37;

volatile bool motionDetected = false;

void IRAM_ATTR onMotionInterrupt() {
    motionDetected = true; 
    // Serial.println("acc int");
}


AccelerometerDriver::AccelerometerDriver(LIS3DH& lis)
    : DriverBase(), _lis(lis) {}

void AccelerometerDriver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    status_t status = _lis.begin();
    if (status != IMU_SUCCESS) {
        Serial.printf("[accel] begin failed (status=%d) — check I2C wiring and address\n", (int)status);
    } else {
        Serial.println("[accel] begin OK");
    }
    // setup_motion_interrupt();
    // setup_freefall_interrupt();
    pinMode(pin_acc_int1, INPUT);
    attachInterrupt(digitalPinToInterrupt(pin_acc_int1), onMotionInterrupt, FALLING);    
}

float AccelerometerDriver::get_accel_x() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return _lis.readFloatAccelX();
}

float AccelerometerDriver::get_accel_y() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return _lis.readFloatAccelY();
}

float AccelerometerDriver::get_accel_z() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return _lis.readFloatAccelZ();
}

float AccelerometerDriver::get_temp() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    // With settings.tempEnabled = 1, the internal temperature sensor is routed
    // to ADC3 as a relative reading centered at mid-scale (512), ~1 degree C per unit.
    int16_t sample = 490 - (int16_t)_lis.read10bitADC3();

    // Moving average over the last TEMP_WINDOW samples
    _temp_sum -= _temp_samples[_temp_index];
    _temp_samples[_temp_index] = sample;
    _temp_sum += sample;
    _temp_index = (_temp_index + 1) % TEMP_WINDOW;
    if (_temp_count < TEMP_WINDOW) _temp_count++;

    return (float)_temp_sum / _temp_count;
}

void AccelerometerDriver::setup_motion_interrupt(uint16_t threshold_mg, uint8_t duration_ms) {
    std::lock_guard<std::mutex> lck(i2c_operations);

    // 1. Only monitor X and Y axis to prevent 1g gravity from permanently locking the Z-axis interrupt.
    // Use 0x2A instead of 0x0A ONLY if you have enabled the High-Pass Filter in CTRL_REG2.
    _lis.writeRegister(LIS3DH_INT1_CFG,      0x0A);               // AOI=0, 6D=0, YHIE|XHIE

    // 2. Threshold: 1 LSB = 16 mg (at ±2g FS). Add 8 for proper rounding.
    uint8_t ths_val = (threshold_mg + 8) / 16;
    if (ths_val > 0x7F) ths_val = 0x7F; // Cap at max 7-bit value
    _lis.writeRegister(LIS3DH_INT1_THS,      ths_val);  

    // 3. Duration: 1 LSB = 10 ms (assumes ODR is strictly set to 100 Hz). Round properly.
    uint8_t dur_val = (duration_ms + 5) / 10;
    if (dur_val > 0x7F) dur_val = 0x7F; // Cap at max 7-bit value
    _lis.writeRegister(LIS3DH_INT1_DURATION, dur_val);  

    // 4. Latch the interrupt until INT1_SRC is read
    _lis.writeRegister(LIS3DH_CTRL_REG5,     0x08);               // LIR_INT1 enabled (LIR_INT2 disabled)

    // 5. Route ONLY the INT1 generator to the physical INT1 pin
    _lis.writeRegister(LIS3DH_CTRL_REG3,     0x40);               // I1_IA1 routed to INT1 pin
}


// void AccelerometerDriver::setup_motion_interrupt(uint16_t threshold_mg, uint8_t duration_ms) {
//     std::lock_guard<std::mutex> lck(i2c_operations);
//     // INT1 generator: OR-high on X/Y/Z → fires when any axis exceeds threshold
//     _lis.writeRegister(LIS3DH_INT1_CFG,      0x2A);               // AOI=0, 6D=0, ZHIE|YHIE|XHIE
//     _lis.writeRegister(LIS3DH_INT1_THS,      threshold_mg / 16);  // 1 LSB = 16 mg at ±2g
//     _lis.writeRegister(LIS3DH_INT1_DURATION, duration_ms  / 10);  // 1 LSB = 10 ms at 100 Hz
//     _lis.writeRegister(LIS3DH_CTRL_REG5,     0x0A);               // LIR_INT1 | LIR_INT2: latch both until read
//     _lis.writeRegister(LIS3DH_CTRL_REG3,     0x60);               // I1_IA1 | I1_IA2: route both generators to INT1 pin
// }

void AccelerometerDriver::setup_freefall_interrupt(uint16_t threshold_mg, uint8_t duration_ms) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    // INT2 generator: AND-low on X/Y/Z → fires when all axes are below threshold (0g)
    _lis.writeRegister(LIS3DH_INT2_CFG,      0x95);               // AOI=1, 6D=0, ZLIE|YLIE|XLIE
    _lis.writeRegister(LIS3DH_INT2_THS,      threshold_mg / 16);
    _lis.writeRegister(LIS3DH_INT2_DURATION, duration_ms  / 10);
    _lis.writeRegister(LIS3DH_CTRL_REG5,     0x0A);               // LIR_INT1 | LIR_INT2: latch both until read
    _lis.writeRegister(LIS3DH_CTRL_REG3,     0x60);               // I1_IA1 | I1_IA2: route both generators to INT1 pin
}

void AccelerometerDriver::clear_interrupt() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    uint8_t dummy;
    _lis.readRegister(&dummy, LIS3DH_INT1_SRC);  // reading clears LIR_INT1 latch
    // _lis.readRegister(&dummy, LIS3DH_INT2_SRC);  // reading clears LIR_INT2 latch
}

AccelEvent AccelerometerDriver::read_interrupt_source() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    uint8_t src1, src2;
    _lis.readRegister(&src1, LIS3DH_INT1_SRC);  // reading clears the latch
    _lis.readRegister(&src2, LIS3DH_INT2_SRC);
    bool motion   = src1 & 0x40;  // IA bit: at least one threshold crossed
    bool freefall = src2 & 0x40;
    return static_cast<AccelEvent>((uint8_t)motion | ((uint8_t)freefall << 1));
}
