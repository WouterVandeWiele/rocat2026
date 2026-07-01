#include "web_fetcher.h"
#include "time_manager.h"
#include "News.h"

#include <WiFi.h>
#include <WiFiClient.h>

WebFetcher& WebFetcher::instance() {
    static WebFetcher inst;
    return inst;
}

void WebFetcher::begin(TimeManager* timeMgr, uint32_t stackSize, uint8_t priority) {
    _timeMgr = timeMgr;
    _queue = xQueueCreate(QUEUE_LEN, sizeof(FetchRequest));

    xTaskCreate(_taskEntry, "web_fetch", stackSize, this, priority, &_task);
    Serial.println("[web_fetcher] task started");
}

bool WebFetcher::request(FetchJob job, float lat, float lon) {
    if (!_queue) return false;
    FetchRequest req;
    req.job = job;
    req.lat = lat;
    req.lon = lon;
    return xQueueSend(_queue, &req, 0) == pdTRUE;
}

bool WebFetcher::requestWeatherIfStale(unsigned long staleMs) {
    if (_weather.fetching.load()) return true;
    if (_weather.valid.load() && millis() - _weather.lastFetchMs < staleMs) return true;
    return request(FetchJob::WEATHER);
}

bool WebFetcher::requestNewsIfStale(unsigned long staleMs) {
    if (_news.fetching.load()) return true;
    if (_news.valid.load() && millis() - _news.lastFetchMs < staleMs) return true;
    return request(FetchJob::NEWS);
}

void WebFetcher::_taskEntry(void* arg) {
    static_cast<WebFetcher*>(arg)->_run();
}

void WebFetcher::_run() {
    for (;;) {
        FetchRequest req;
        if (xQueueReceive(_queue, &req, pdMS_TO_TICKS(1000)) != pdTRUE)
            continue;

        if (!WiFi.isConnected()) {
            Serial.println("[web_fetcher] no WiFi, skipping job");
            continue;
        }

        Serial.printf("[web_fetcher] job %d (heap: %u, block: %u)\n",
                      (int)req.job, ESP.getFreeHeap(), ESP.getMaxAllocHeap());

        switch (req.job) {
            case FetchJob::WEATHER:  _doWeather(req); break;
            case FetchJob::NEWS:     _doNews();       break;
            case FetchJob::NTP_SYNC: _doNtpSync();    break;
            case FetchJob::GEO_ONLY: _doGeo();        break;
        }

        Serial.printf("[web_fetcher] done (heap: %u, block: %u)\n",
                      ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    }
}

void WebFetcher::_doWeather(const FetchRequest& req) {
    _weather.fetching.store(true);

    float lat = req.lat;
    float lon = req.lon;

    if (lat == 0 && lon == 0) {
        WiFiClient client;
        IPGeo geo;
        location_t loc = geo.getLocation(client);
        client.stop();
        if (!loc.status) {
            Serial.println("[web_fetcher] geo failed, skipping weather");
            _weather.fetching.store(false);
            return;
        }
        _weather.location = loc;
        lat = loc.lat;
        lon = loc.lon;
    }

    {
        WiFiClient client;
        OpenMeteo meteo;
        weather_t w = meteo.getWeather(lat, lon, client);
        client.stop();

        _weather.weather = w;
        _weather.valid.store(w.status);
        _weather.lastFetchMs = millis();

        if (w.status)
            Serial.printf("[web_fetcher] weather: %.1f°C code=%d\n",
                          w.temperature, w.weather_code);
    }

    _weather.fetching.store(false);
}

void WebFetcher::_doNews() {
    _news.fetching.store(true);

    {
        WiFiClient plain;
        News::feed.fetchAll(&plain, nullptr, [](int i, int t) {
            Serial.printf("[web_fetcher] feed %d/%d\n", i + 1, t);
        });
    }

    _news.valid.store(News::feed.count() > 0);
    _news.lastFetchMs = millis();
    _news.fetching.store(false);

    Serial.printf("[web_fetcher] news: %d items\n", News::feed.count());
}

void WebFetcher::_doNtpSync() {
    if (!_timeMgr) return;

    const char* tz  = _timeMgr->timezone();
    const char* srv = _timeMgr->ntpServer();
    if (!srv || !srv[0]) return;

    Serial.printf("[web_fetcher] NTP sync: tz=%s srv=%s\n", tz, srv);
    configTzTime(tz, srv);

    time_t t = 0;
    for (int i = 0; i < 100 && t < 1000000000L; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        t = time(nullptr);
    }

    if (t >= 1000000000L) {
        _timeMgr->notifyNtpSuccess();
        Serial.println("[web_fetcher] NTP sync OK");
    } else {
        Serial.println("[web_fetcher] NTP sync timed out");
    }
}

void WebFetcher::_doGeo() {
    _geo.fetching.store(true);

    {
        WiFiClient client;
        IPGeo geo;
        location_t loc = geo.getLocation(client);
        client.stop();

        _geo.location = loc;
        _geo.valid.store(loc.status);

        if (loc.status && _timeMgr) {
            _timeMgr->applyGeoTimezone(loc);
        }

        Serial.printf("[web_fetcher] geo: %s (%s)\n",
                      loc.status ? loc.city : "failed",
                      loc.status ? loc.timezone : "");
    }

    _geo.fetching.store(false);
}
