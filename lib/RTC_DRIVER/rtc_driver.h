#pragma once

#include "driver_base.h"
#include <RTClib.h>

struct AlarmSettings {
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
};

class RtcDriver : public DriverBase {
public:
    explicit RtcDriver(TwoWire& wire);

    void begin() override;

    DateTime get_time();
    void set_time(const DateTime& dt);

    // Sets day/hour/minute alarm and enables INT1 interrupt output
    void set_alarm(uint8_t day, uint8_t hour, uint8_t minute);
    AlarmSettings get_alarm();
    void clear_alarm_flag();
    void disable_alarm();

private:
    TwoWire&    _wire;
    RTC_PCF8523 _rtc;

    void    _write_reg(uint8_t reg, uint8_t val);
    uint8_t _read_reg(uint8_t reg);
};
