#include "time_manager.h"
#include "rtc_driver.h"
#include "wifi_driver.h"
#include "nvs_store.h"

#include "IPGeo.h"

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

static constexpr int NTP_TIMEOUT_MS = 10000;

struct IanaPosixEntry {
    const char* iana;
    const char* posix;
};

static const IanaPosixEntry IANA_TABLE[] = {
    {"Europe/Brussels",   "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam",  "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Paris",      "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Vienna",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Zurich",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Stockholm",  "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Oslo",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Warsaw",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Prague",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/London",     "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Dublin",     "GMT0IST,M3.5.0/1,M10.5.0"},
    {"Europe/Lisbon",     "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/Helsinki",   "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Athens",     "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Bucharest",  "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Istanbul",   "<+03>-3"},
    {"Europe/Moscow",     "MSK-3"},
    {"America/New_York",      "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago",       "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver",        "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles",   "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage",     "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Sao_Paulo",     "<-03>3"},
    {"America/Argentina/Buenos_Aires", "<-03>3"},
    {"America/Toronto",       "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Vancouver",     "PST8PDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo",        "JST-9"},
    {"Asia/Shanghai",     "CST-8"},
    {"Asia/Hong_Kong",    "HKT-8"},
    {"Asia/Singapore",    "<+08>-8"},
    {"Asia/Kolkata",      "IST-5:30"},
    {"Asia/Dubai",        "<+04>-4"},
    {"Asia/Seoul",        "KST-9"},
    {"Australia/Sydney",  "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Melbourne","AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Perth",   "AWST-8"},
    {"Pacific/Auckland",  "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"Pacific/Honolulu",  "HST10"},
    {"Africa/Cairo",      "EET-2"},
    {"Africa/Johannesburg","SAST-2"},
    {"Africa/Lagos",      "WAT-1"},
};

TimeManager::TimeManager(RtcDriver& rtc, WifiDriver& wifi, NvsStore& nvs)
    : _rtc(rtc), _wifi(wifi), _nvs(nvs) {}

void TimeManager::begin() {
    _nvs.loadNtpServer(_ntpServer, sizeof(_ntpServer));
    _nvs.loadTimezone(_timezone, sizeof(_timezone));

    if (_ntpServer[0]) {
        Serial.printf("[time] NTP server: %s\n", _ntpServer);
    } else {
        Serial.println("[time] no NTP server configured — NTP sync disabled");
    }
    Serial.printf("[time] timezone: %s\n", _timezone);
    setenv("TZ", _timezone, 1);
    tzset();

    Serial.println("[time] reading PCF8523 external RTC...");
    _syncExternalToInternal();
    _lastHourlySync = millis();

    RocatTime t = now();
    Serial.printf("[time] internal RTC seeded: %04d-%02d-%02d %02d:%02d:%02d\n",
                  t.year, t.month, t.day, t.hour, t.minute, t.second);
}

void TimeManager::update() {
    unsigned long now_ms = millis();

    if (_wifi.is_connected() && !_autoConfigured) {
        _autoConfigured = autoConfigureFromGeo();
    }

    bool ntpDue = !_ntpSynced || (now_ms - _lastNtpSync > DAILY_MS);
    if (_ntpServer[0] && _wifi.is_connected() && ntpDue) {
        Serial.println("[time] NTP sync due — attempting...");
        if (_syncNtpToExternal()) {
            _lastNtpSync    = now_ms;
            _ntpSynced      = true;
            _lastHourlySync = now_ms;
            Serial.println("[time] NTP sync complete — hourly timer reset");
        } else {
            Serial.println("[time] NTP sync failed — will retry when conditions are met");
        }
    }

    if (now_ms - _lastHourlySync > HOURLY_MS) {
        Serial.println("[time] hourly sync — reading PCF8523...");
        _syncExternalToInternal();
        _lastHourlySync = now_ms;

        RocatTime t = this->now();
        Serial.printf("[time] internal RTC updated: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.year, t.month, t.day, t.hour, t.minute, t.second);
    }
}

RocatTime TimeManager::now() {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    return {
        (uint16_t)(tm.tm_year + 1900),
        (uint8_t)(tm.tm_mon + 1),
        (uint8_t)tm.tm_mday,
        (uint8_t)tm.tm_hour,
        (uint8_t)tm.tm_min,
        (uint8_t)tm.tm_sec
    };
}

void TimeManager::setTime(const RocatTime& t) {
    Serial.printf("[time] manual set: %04d-%02d-%02d %02d:%02d:%02d\n",
                  t.year, t.month, t.day, t.hour, t.minute, t.second);

    struct tm tm = {};
    tm.tm_year = t.year - 1900;
    tm.tm_mon  = t.month - 1;
    tm.tm_mday = t.day;
    tm.tm_hour = t.hour;
    tm.tm_min  = t.minute;
    tm.tm_sec  = t.second;

    time_t epoch = mktime(&tm);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    Serial.println("[time] internal RTC set");

    _rtc.set_time(DateTime(t.year, t.month, t.day, t.hour, t.minute, t.second));
    Serial.println("[time] PCF8523 external RTC set");
}

void TimeManager::setNtpServer(const char* server) {
    strncpy(_ntpServer, server, sizeof(_ntpServer) - 1);
    _ntpServer[sizeof(_ntpServer) - 1] = '\0';
    _nvs.saveNtpServer(_ntpServer);
    _ntpSynced = false;
    Serial.printf("[time] NTP server changed to: %s — will sync on next update\n", _ntpServer);
}

const char* TimeManager::ntpServer() {
    return _ntpServer;
}

void TimeManager::requestSync() {
    _ntpSynced = false;
    _autoConfigured = false;
    Serial.println("[time] manual sync requested — will re-detect timezone and sync NTP");
}

void TimeManager::setTimezone(const char* tz) {
    strncpy(_timezone, tz, sizeof(_timezone) - 1);
    _timezone[sizeof(_timezone) - 1] = '\0';
    _nvs.saveTimezone(_timezone);
    setenv("TZ", _timezone, 1);
    tzset();
    Serial.printf("[time] timezone changed to: %s\n", _timezone);
}

const char* TimeManager::timezone() {
    return _timezone;
}

unsigned long TimeManager::timeSinceNtpSync() {
    if (!_ntpSynced) return 0;
    return millis() - _lastNtpSync;
}

bool TimeManager::hasNtpSynced() {
    return _ntpSynced;
}

void TimeManager::_syncExternalToInternal() {
    DateTime dt = _rtc.get_time();
    Serial.printf("[time] PCF8523 reads: %04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year(), dt.month(), dt.day(),
                  dt.hour(), dt.minute(), dt.second());

    struct tm tm = {};
    tm.tm_year = dt.year() - 1900;
    tm.tm_mon  = dt.month() - 1;
    tm.tm_mday = dt.day();
    tm.tm_hour = dt.hour();
    tm.tm_min  = dt.minute();
    tm.tm_sec  = dt.second();

    time_t epoch = mktime(&tm);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
    Serial.printf("[time] internal RTC set to epoch %ld\n", (long)epoch);
}

bool TimeManager::_syncNtpToExternal() {
    Serial.printf("[time] configTzTime(\"%s\", \"%s\")...\n", _timezone, _ntpServer);
    configTzTime(_timezone, _ntpServer);

    Serial.println("[time] waiting for NTP response...");
    unsigned long start = millis();
    time_t t = 0;
    while (t < 1000000000L && millis() - start < NTP_TIMEOUT_MS) {
        delay(100);
        t = time(nullptr);
    }

    unsigned long elapsed = millis() - start;

    if (t < 1000000000L) {
        Serial.printf("[time] NTP timed out after %lu ms\n", elapsed);
        return false;
    }

    Serial.printf("[time] NTP responded in %lu ms\n", elapsed);

    struct tm tm;
    localtime_r(&t, &tm);

    Serial.printf("[time] NTP time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);

    _rtc.set_time(DateTime(
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec
    ));
    Serial.println("[time] NTP → PCF8523 written");

    return true;
}

const char* TimeManager::_ianaToPosix(const char* iana) {
    for (const auto& entry : IANA_TABLE) {
        if (strcmp(iana, entry.iana) == 0) return entry.posix;
    }
    return nullptr;
}

bool TimeManager::autoConfigureFromGeo() {
    Serial.println("[time] auto-configuring from IP geolocation...");

    IPGeo geo;
    location_t loc = geo.getLocation();

    if (!loc.status) {
        Serial.println("[time] geolocation failed — keeping current settings");
        return false;
    }

    Serial.printf("[time] detected: %s, %s (%s)\n", loc.city, loc.country, loc.timezone);

    const char* posix = _ianaToPosix(loc.timezone);
    if (posix) {
        Serial.printf("[time] IANA '%s' → POSIX '%s'\n", loc.timezone, posix);
        setTimezone(posix);
    } else {
        int hours = loc.offsetSeconds / 3600;
        int mins  = abs(loc.offsetSeconds % 3600) / 60;
        char fallback[16];
        if (mins > 0)
            snprintf(fallback, sizeof(fallback), "<UTC%+d:%02d>%+d:%02d", -hours, mins, hours, mins);
        else
            snprintf(fallback, sizeof(fallback), "<UTC%+d>%+d", -hours, hours);
        Serial.printf("[time] IANA '%s' not in table — using offset: %s\n", loc.timezone, fallback);
        setTimezone(fallback);
    }

    return true;
}
