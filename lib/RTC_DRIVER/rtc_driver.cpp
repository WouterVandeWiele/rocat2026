#include "rtc_driver.h"

static constexpr uint8_t PCF8523_ADDR     = 0x68;
static constexpr uint8_t REG_CONTROL_1    = 0x00; // bit 1 = AIE (alarm interrupt enable)
static constexpr uint8_t REG_CONTROL_2    = 0x01; // bit 3 = AF (alarm flag, clear to de-assert INT1)
static constexpr uint8_t REG_TMR_CLKOUT   = 0x0F; // bits 2:0 = COF (111 = CLKOUT disabled)
static constexpr uint8_t REG_MINUTE_ALARM = 0x0A; // bit 7 = AEN_M (0 = enabled)
static constexpr uint8_t REG_HOUR_ALARM   = 0x0B; // bit 7 = AEN_H
static constexpr uint8_t REG_DAY_ALARM    = 0x0C; // bit 7 = AEN_D
static constexpr uint8_t REG_WDAY_ALARM   = 0x0D; // bit 7 = AEN_W

static uint8_t to_bcd(uint8_t v)   { return ((v / 10) << 4) | (v % 10); }
static uint8_t from_bcd(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

RtcDriver::RtcDriver(TwoWire& wire)
    : DriverBase(), _wire(wire) {}

void RtcDriver::begin() {
    std::lock_guard<std::mutex> lck(i2c_operations);

    if (! _rtc.begin(&_wire)) {
    Serial.println("Couldn't find RTC");
    }

    if (_rtc.lostPower()) {
        Serial.println("RTC is NOT initialized, let's set the time!");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 
    }

    // _rtc.start();
    // Disable CLKOUT (COF=111) so the pin is high-Z and INT1 can be used cleanly
    // _write_reg(REG_TMR_CLKOUT, _read_reg(REG_TMR_CLKOUT) | 0x07);
    _rtc.writeSqwPinMode(PCF8523_OFF);
}

DateTime RtcDriver::get_time() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return _rtc.now();
}

void RtcDriver::set_time(const DateTime& dt) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    // _rtc.stop();
    _rtc.adjust(dt);
    // _rtc.start();
}

void RtcDriver::set_alarm(uint8_t day, uint8_t hour, uint8_t minute) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _write_reg(REG_MINUTE_ALARM, to_bcd(minute));   // AEN=0: active
    _write_reg(REG_HOUR_ALARM,   to_bcd(hour));
    _write_reg(REG_DAY_ALARM,    to_bcd(day));
    _write_reg(REG_WDAY_ALARM,   0x80);             // AEN=1: weekday alarm unused
    _write_reg(REG_CONTROL_1, _read_reg(REG_CONTROL_1) | (1 << 1)); // set AIE
}

AlarmSettings RtcDriver::get_alarm() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    return {
        from_bcd(_read_reg(REG_DAY_ALARM)    & 0x3F),
        from_bcd(_read_reg(REG_HOUR_ALARM)   & 0x3F),
        from_bcd(_read_reg(REG_MINUTE_ALARM) & 0x7F),
    };
}

void RtcDriver::clear_alarm_flag() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _write_reg(REG_CONTROL_2, _read_reg(REG_CONTROL_2) & ~(1 << 3)); // clear AF
}

void RtcDriver::disable_alarm() {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _write_reg(REG_CONTROL_1, _read_reg(REG_CONTROL_1) & ~(1 << 1)); // clear AIE
}

void RtcDriver::_write_reg(uint8_t reg, uint8_t val) {
    _wire.beginTransmission(PCF8523_ADDR);
    _wire.write(reg);
    _wire.write(val);
    _wire.endTransmission();
}

uint8_t RtcDriver::_read_reg(uint8_t reg) {
    _wire.beginTransmission(PCF8523_ADDR);
    _wire.write(reg);
    _wire.endTransmission();
    _wire.requestFrom(PCF8523_ADDR, (uint8_t)1);
    return _wire.read();
}
