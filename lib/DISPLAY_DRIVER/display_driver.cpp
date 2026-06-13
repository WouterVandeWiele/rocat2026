#include "display_driver.h"

DisplayDriver::DisplayDriver(TwoWire& wire, SX1509& sx, SED1530_LCD& lcd)
    : DriverBase(wire, 0x00), _sx(sx), _lcd(lcd) {}

bool DisplayDriver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    // TODO: configure SX1509 pins for display power and backlight
    return true;
}

void DisplayDriver::set_power(bool on) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(pin_dis_pwr, on ? HIGH : LOW);
}

void DisplayDriver::set_backlight(uint8_t level) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.analogWrite(pin_dis_bl, level);
}

void DisplayDriver::flush() {
    // Parallel-bus write — no I2C traffic, no lock needed
    _lcd.display();
}
