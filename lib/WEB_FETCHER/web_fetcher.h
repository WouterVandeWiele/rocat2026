#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include "OpenMeteo.h"
#include "IPGeo.h"

enum class FetchJob : uint8_t {
    WEATHER,
    NEWS,
    NTP_SYNC,
    GEO_ONLY,
};

struct FetchRequest {
    FetchJob job;
    float    lat = 0;
    float    lon = 0;
};

struct WeatherSlot {
    weather_t         weather  = {};
    location_t        location = {};
    std::atomic<bool> valid{false};
    std::atomic<bool> fetching{false};
    unsigned long     lastFetchMs = 0;
};

struct NewsSlot {
    std::atomic<bool> valid{false};
    std::atomic<bool> fetching{false};
    unsigned long     lastFetchMs = 0;
};

struct GeoSlot {
    location_t        location = {};
    std::atomic<bool> valid{false};
    std::atomic<bool> fetching{false};
};

class TimeManager;

class WebFetcher {
public:
    static WebFetcher& instance();

    void begin(TimeManager* timeMgr, uint32_t stackSize = 6144,
               uint8_t priority = 1);

    bool request(FetchJob job, float lat = 0, float lon = 0);

    bool requestWeatherIfStale(unsigned long staleMs = 86400000UL);
    bool requestNewsIfStale(unsigned long staleMs = 3600000UL);

    const WeatherSlot& weather() const { return _weather; }
    const NewsSlot&    news()    const { return _news; }
    const GeoSlot&     geo()     const { return _geo; }

private:
    WebFetcher() = default;

    static constexpr int QUEUE_LEN = 4;

    QueueHandle_t _queue   = nullptr;
    TaskHandle_t  _task    = nullptr;
    TimeManager*  _timeMgr = nullptr;

    WeatherSlot _weather;
    NewsSlot    _news;
    GeoSlot     _geo;

    static void _taskEntry(void* arg);
    void _run();

    void _doWeather(const FetchRequest& req);
    void _doNews();
    void _doNtpSync();
    void _doGeo();
};
