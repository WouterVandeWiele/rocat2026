#include "web_dashboard.h"
#include "data_broker.h"
#include "nvs_store.h"
#include "time_manager.h"
#include "web_fetcher.h"
#include "News.h"

#include <SPIFFS.h>
#include <WiFi.h>

static constexpr unsigned long UPDATE_INTERVAL_MS = 1000;

WebDashboard::WebDashboard(NvsStore& store, TimeManager& time)
    : _store(store), _time(time) {}

void WebDashboard::begin() {
    _beginRequested = true;
    _tryStart();
}

void WebDashboard::_tryStart() {
    if (_started) return;
    if (!_beginRequested) return;
    if (WiFi.status() != WL_CONNECTED) return;

    _server = new AsyncWebServer(80);
    _ws     = new AsyncWebSocket("/ws");

    _ws->onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                        AwsEventType t, void* a, uint8_t* d, size_t l) {
        _onWsEvent(s, c, t, a, d, l);
    });
    _server->addHandler(_ws);

    _server->serveStatic("/", SPIFFS, "/www/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache, no-store, must-revalidate");

    _server->begin();
    _started = true;

    Serial.printf("[dashboard] started on http://%s/\n",
                  WiFi.localIP().toString().c_str());
}

void WebDashboard::_onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[dashboard] client #%u connected\n", client->id());
        _broadcastSensors(client);
        _broadcastStationList(client);
        _broadcastRssFeedList(client);
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[dashboard] client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
            _handleCommand((const char*)data, len);
        }
    }
}

void WebDashboard::_sendJson(AsyncWebSocketClient* client) {
    size_t len = serializeJson(_doc, _jsonBuf, sizeof(_jsonBuf));
    if (client) client->text(_jsonBuf, len);
    else _ws->textAll(_jsonBuf, len);
}

void WebDashboard::_handleCommand(const char* data, size_t len) {
    _doc.clear();
    if (deserializeJson(_doc, data, len)) return;

    const char* cmd = _doc["cmd"];
    if (!cmd) return;

    auto& db = DataBroker::instance();

    if (strcmp(cmd, "motor") == 0) {
        int id    = _doc["id"] | 0;
        int value = _doc["value"] | 0;
        db.update(Topic::MOTOR, [id, value](Blackboard& b) {
            if (id == 1) b.motor1_speed = value;
            else         b.motor2_speed = value;
        });
    }
    else if (strcmp(cmd, "motor_enable") == 0) {
        bool on = _doc["value"] | false;
        db.update(Topic::MOTOR, [on](Blackboard& b) {
            b.motor_enabled = on;
            b.motor_standby = !on;
        });
    }
    else if (strcmp(cmd, "led") == 0) {
        uint8_t r = _doc["r"] | 0;
        uint8_t g = _doc["g"] | 0;
        uint8_t bv = _doc["b"] | 0;
        db.update(Topic::LED, [r, g, bv](Blackboard& b) {
            b.led_r = r; b.led_g = g; b.led_b = bv;
        });
    }
    else if (strcmp(cmd, "led_brightness") == 0) {
        uint8_t val = _doc["value"] | 0;
        db.update(Topic::LED, [val](Blackboard& b) {
            b.led_brightness = val;
        });
    }
    else if (strcmp(cmd, "radio_play") == 0) {
        int station = _doc["station"] | 0;
        db.update(Topic::AUDIO, [station](Blackboard& b) {
            b.audio_cmd = AudioCmd::PLAY;
            b.audio_station_index = station;
        });
    }
    else if (strcmp(cmd, "radio_stop") == 0) {
        db.update(Topic::AUDIO, [](Blackboard& b) {
            b.audio_cmd = AudioCmd::STOP;
        });
    }
    else if (strcmp(cmd, "radio_next") == 0) {
        db.update(Topic::AUDIO, [](Blackboard& b) {
            b.audio_cmd = AudioCmd::NEXT;
        });
    }
    else if (strcmp(cmd, "radio_prev") == 0) {
        db.update(Topic::AUDIO, [](Blackboard& b) {
            b.audio_cmd = AudioCmd::PREV;
        });
    }
    else if (strcmp(cmd, "radio_volume") == 0) {
        float vol = _doc["value"] | 1.0f;
        db.update(Topic::AUDIO, [vol](Blackboard& b) {
            b.audio_cmd = AudioCmd::VOLUME;
            b.audio_volume = vol;
        });
    }
    else if (strcmp(cmd, "station_add") == 0) {
        const char* name = _doc["name"] | "";
        const char* url  = _doc["url"] | "";
        if (name[0] && url[0]) {
            _store.addStation(name, url);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "station_remove") == 0) {
        int index = _doc["index"] | -1;
        if (index >= 0) {
            _store.removeStation(index);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "station_update") == 0) {
        int index        = _doc["index"] | -1;
        const char* name = _doc["name"] | "";
        const char* url  = _doc["url"] | "";
        if (index >= 0 && name[0] && url[0]) {
            _store.updateStation(index, name, url);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "rss_add") == 0) {
        const char* url = _doc["url"] | "";
        if (url[0]) {
            _store.addRssFeed(url);
            _reloadRssFeeds();
            _broadcastRssFeedList();
        }
    }
    else if (strcmp(cmd, "rss_remove") == 0) {
        int index = _doc["index"] | -1;
        if (index >= 0) {
            _store.removeRssFeed(index);
            _reloadRssFeeds();
            _broadcastRssFeedList();
        }
    }
    else if (strcmp(cmd, "set_time") == 0) {
        RocatTime t;
        t.year   = _doc["year"]   | 2026;
        t.month  = _doc["month"]  | 1;
        t.day    = _doc["day"]    | 1;
        t.hour   = _doc["hour"]   | 0;
        t.minute = _doc["minute"] | 0;
        t.second = _doc["second"] | 0;
        _time.setTime(t);
    }
    else if (strcmp(cmd, "set_ntp") == 0) {
        const char* server = _doc["server"] | "";
        if (server[0]) {
            _time.setNtpServer(server);
        }
    }
    else if (strcmp(cmd, "ntp_sync") == 0) {
        _time.requestSync();
    }
    else if (strcmp(cmd, "set_tz") == 0) {
        const char* tz = _doc["tz"] | "";
        if (tz[0]) {
            _time.setTimezone(tz);
        }
    }
}

void WebDashboard::update() {
    if (!_started) {
        _tryStart();
        if (!_started) return;
    }

    _ws->cleanupClients();

    unsigned long now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL_MS) return;
    _lastUpdate = now;

    if (_ws->count() > 0) {
        _broadcastSensors();
    }
}

void WebDashboard::_broadcastSensors(AsyncWebSocketClient* client) {
    Blackboard snap = DataBroker::instance().snapshot();

    _doc.clear();
    _doc["type"]     = "sensors";
    _doc["battery"]  = snap.battery_voltage;
    _doc["charging"] = snap.battery_charging;
    _doc["temp"]     = snap.accel_temp;
    _doc["accelX"]   = snap.accel_x;
    _doc["accelY"]   = snap.accel_y;
    _doc["accelZ"]   = snap.accel_z;
    _doc["ldr"]      = snap.ldr_ohms;

    char timeBuf[20];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             snap.time.year, snap.time.month, snap.time.day,
             snap.time.hour, snap.time.minute, snap.time.second);
    _doc["time"]        = timeBuf;
    _doc["ntpSynced"]   = snap.ntp_synced;
    _doc["ntpSyncAgo"]  = snap.ntp_last_sync_ago / 1000;

    _doc["radioStation"] = snap.audio_station_index;
    _doc["radioName"]    = snap.audio_station_name;
    _doc["radioPlaying"] = snap.audio_playing;
    _doc["radioVolume"]  = snap.audio_volume;

    _sendJson(client);
}

void WebDashboard::_broadcastStationList(AsyncWebSocketClient* client) {
    Station stations[NvsStore::MAX_STATIONS];
    int count = _store.loadStations(stations, NvsStore::MAX_STATIONS);

    _doc.clear();
    _doc["type"] = "stations";
    _doc["defaultCount"] = _store.defaultCount();
    JsonArray arr = _doc["stations"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject st = arr.add<JsonObject>();
        st["name"] = stations[i].name;
        st["url"]  = stations[i].url;
    }

    _sendJson(client);
}

void WebDashboard::_broadcastRssFeedList(AsyncWebSocketClient* client) {
    RssFeedEntry feeds[NvsStore::MAX_RSS_FEEDS];
    int count = _store.loadRssFeeds(feeds, NvsStore::MAX_RSS_FEEDS);

    _doc.clear();
    _doc["type"] = "rssFeeds";
    JsonArray arr = _doc["feeds"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        arr.add(feeds[i].url);
    }

    _sendJson(client);
}

void WebDashboard::_reloadRssFeeds() {
    RssFeedEntry feeds[NvsStore::MAX_RSS_FEEDS];
    int count = _store.loadRssFeeds(feeds, NvsStore::MAX_RSS_FEEDS);

    News::feed = RssFeed();
    for (int i = 0; i < count; i++) {
        News::feed.addFeed(feeds[i].url);
    }
    WebFetcher::instance().request(FetchJob::NEWS);
    Serial.printf("[dashboard] reloaded %d RSS feeds, fetch requested\n", count);
}
