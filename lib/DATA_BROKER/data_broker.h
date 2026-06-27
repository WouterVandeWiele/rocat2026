#pragma once

#include <cstdint>
#include <mutex>
#include "time_manager.h"

enum class Topic : uint8_t {
    ACCEL, BATTERY, LDR, RTC_TIME,
    MOTOR, LED, AUDIO, WIFI,
    TOUCH, BUTTON,
    _COUNT
};

enum class AudioCmd : uint8_t { NONE, PLAY, STOP, NEXT, PREV, RELOAD, VOLUME };

struct Blackboard {
    // Accelerometer
    float accel_x    = 0.0f;
    float accel_y    = 0.0f;
    float accel_z    = 0.0f;
    float accel_temp = 0.0f;

    // Battery
    float    battery_voltage = 0.0f;
    uint16_t battery_raw_adc = 0;
    bool     battery_charging = false;

    // LDR
    float ldr_ohms = 0.0f;

    // Time
    RocatTime time = {};
    unsigned long ntp_last_sync_ago = 0;
    bool          ntp_synced        = false;

    // Motors
    int16_t motor1_speed  = 0;
    int16_t motor2_speed  = 0;
    bool    motor_enabled = false;
    bool    motor_standby = true;

    // LEDs
    uint8_t led_r          = 0;
    uint8_t led_g          = 0;
    uint8_t led_b          = 0;
    uint8_t led_brightness = 128;

    // Audio
    AudioCmd    audio_cmd           = AudioCmd::NONE;
    int         audio_station_index = 0;
    const char* audio_station_name  = "";
    bool        audio_playing       = false;
    float       audio_volume        = 1.0f;

    // WiFi
    bool wifi_connected = false;
    char wifi_ip[16]    = {};
    char wifi_ssid[33]  = {};

    // Input
    uint8_t last_gesture = 0;
    uint8_t last_button  = 0;
};

class DataBroker {
public:
    static DataBroker& instance();

    Blackboard snapshot();

    template<typename Fn>
    void update(Topic topic, Fn&& fn) {
        std::lock_guard<std::mutex> lk(_mtx);
        fn(_board);
        _dirty |= (1u << static_cast<uint8_t>(topic));
    }

    template<typename Fn>
    auto read(Fn&& fn) -> decltype(fn(std::declval<const Blackboard&>())) {
        std::lock_guard<std::mutex> lk(_mtx);
        return fn(static_cast<const Blackboard&>(_board));
    }

    bool consume(Topic topic) {
        std::lock_guard<std::mutex> lk(_mtx);
        uint32_t mask = 1u << static_cast<uint8_t>(topic);
        bool was = _dirty & mask;
        _dirty &= ~mask;
        return was;
    }

    bool isDirty(Topic topic) {
        std::lock_guard<std::mutex> lk(_mtx);
        return _dirty & (1u << static_cast<uint8_t>(topic));
    }

private:
    DataBroker() = default;
    std::mutex _mtx;
    Blackboard _board;
    uint32_t   _dirty = 0;
};
