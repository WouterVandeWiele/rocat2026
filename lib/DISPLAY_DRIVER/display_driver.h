#pragma once

#include "driver_base.h"
#include "SED1530_LCD.h"
#include <SparkFunSX1509.h>

class DisplayDriver : public DriverBase {
public:
    // sx: shared SX1509 instance (also used by MotorDriver, AudioDriver, LedDriver)
    // lcd: SED1530 instance — display data goes over parallel bus, not I2C
    DisplayDriver(TwoWire& wire, SX1509& sx, SED1530_LCD& lcd);

    void begin() override;

    // Controls display power via SX1509 pin_dis_pwr
    void set_power(bool on);

    // Controls backlight via SX1509 pin_dis_bl (0–255)
    void set_backlight(uint8_t level);

    // Flushes the GFX framebuffer to the physical LCD
    void flush();

private:
    SX1509&      _sx;
    SED1530_LCD& _lcd;
};
