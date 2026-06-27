#pragma once

#include <cstdint>

class RtcDriver;
class WifiDriver;
class NvsStore;
class IPGeo;

struct RocatTime {
    uint16_t year   = 0;
    uint8_t  month  = 0;
    uint8_t  day    = 0;
    uint8_t  hour   = 0;
    uint8_t  minute = 0;
    uint8_t  second = 0;

    RocatTime() = default;
    RocatTime(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
        : year(y), month(mo), day(d), hour(h), minute(mi), second(s) {}
};

class TimeManager {
public:
    TimeManager(RtcDriver& rtc, WifiDriver& wifi, NvsStore& nvs);

    void begin();
    void update();

    RocatTime now();

    void setTime(const RocatTime& t);

    void setNtpServer(const char* server);
    const char* ntpServer();

    void setTimezone(const char* tz);
    const char* timezone();

    bool autoConfigureFromGeo();
    void requestSync();

    unsigned long timeSinceNtpSync();
    bool hasNtpSynced();

private:
    RtcDriver&  _rtc;
    WifiDriver& _wifi;
    NvsStore&   _nvs;

    char          _ntpServer[64] = {};
    char          _timezone[48]  = {};
    unsigned long _lastHourlySync = 0;
    unsigned long _lastNtpSync    = 0;
    bool          _ntpSynced      = false;

    static constexpr unsigned long HOURLY_MS = 3600000UL;
    static constexpr unsigned long DAILY_MS  = 86400000UL;

    bool _autoConfigured = false;

    void _syncExternalToInternal();
    bool _syncNtpToExternal();
    static const char* _ianaToPosix(const char* iana);
};
