#include "web_dashboard.h"
#include "data_broker.h"
#include "nvs_store.h"
#include "time_manager.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

static constexpr unsigned long UPDATE_INTERVAL_MS = 1000;

WebDashboard::WebDashboard(NvsStore& store, TimeManager& time)
    : _store(store), _time(time) {}

void WebDashboard::begin() {
    if (_started) return;

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
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[dashboard] client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
            _handleCommand((const char*)data, len);
        }
    }
}

void WebDashboard::_handleCommand(const char* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    auto& db = DataBroker::instance();

    if (strcmp(cmd, "motor") == 0) {
        int id    = doc["id"] | 0;
        int value = doc["value"] | 0;
        db.update(Topic::MOTOR, [id, value](Blackboard& b) {
            if (id == 1) b.motor1_speed = value;
            else         b.motor2_speed = value;
        });
    }
    else if (strcmp(cmd, "motor_enable") == 0) {
        bool on = doc["value"] | false;
        db.update(Topic::MOTOR, [on](Blackboard& b) {
            b.motor_enabled = on;
            b.motor_standby = !on;
        });
    }
    else if (strcmp(cmd, "led") == 0) {
        uint8_t r = doc["r"] | 0;
        uint8_t g = doc["g"] | 0;
        uint8_t bv = doc["b"] | 0;
        db.update(Topic::LED, [r, g, bv](Blackboard& b) {
            b.led_r = r; b.led_g = g; b.led_b = bv;
        });
    }
    else if (strcmp(cmd, "led_brightness") == 0) {
        uint8_t val = doc["value"] | 0;
        db.update(Topic::LED, [val](Blackboard& b) {
            b.led_brightness = val;
        });
    }
    else if (strcmp(cmd, "radio_play") == 0) {
        int station = doc["station"] | 0;
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
        float vol = doc["value"] | 1.0f;
        db.update(Topic::AUDIO, [vol](Blackboard& b) {
            b.audio_cmd = AudioCmd::VOLUME;
            b.audio_volume = vol;
        });
    }
    else if (strcmp(cmd, "station_add") == 0) {
        const char* name = doc["name"] | "";
        const char* url  = doc["url"] | "";
        if (name[0] && url[0]) {
            _store.addStation(name, url);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "station_remove") == 0) {
        int index = doc["index"] | -1;
        if (index >= 0) {
            _store.removeStation(index);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "station_update") == 0) {
        int index        = doc["index"] | -1;
        const char* name = doc["name"] | "";
        const char* url  = doc["url"] | "";
        if (index >= 0 && name[0] && url[0]) {
            _store.updateStation(index, name, url);
            db.update(Topic::AUDIO, [](Blackboard& b) {
                b.audio_cmd = AudioCmd::RELOAD;
            });
            _broadcastStationList();
        }
    }
    else if (strcmp(cmd, "set_time") == 0) {
        RocatTime t;
        t.year   = doc["year"]   | 2026;
        t.month  = doc["month"]  | 1;
        t.day    = doc["day"]    | 1;
        t.hour   = doc["hour"]   | 0;
        t.minute = doc["minute"] | 0;
        t.second = doc["second"] | 0;
        _time.setTime(t);
    }
    else if (strcmp(cmd, "set_ntp") == 0) {
        const char* server = doc["server"] | "";
        if (server[0]) {
            _time.setNtpServer(server);
        }
    }
    else if (strcmp(cmd, "ntp_sync") == 0) {
        _time.requestSync();
    }
    else if (strcmp(cmd, "set_tz") == 0) {
        const char* tz = doc["tz"] | "";
        if (tz[0]) {
            _time.setTimezone(tz);
        }
    }
}

void WebDashboard::update() {
    if (!_started) return;

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

    JsonDocument doc;
    doc["type"]     = "sensors";
    doc["battery"]  = snap.battery_voltage;
    doc["charging"] = snap.battery_charging;
    doc["temp"]     = snap.accel_temp;
    doc["accelX"]   = snap.accel_x;
    doc["accelY"]   = snap.accel_y;
    doc["accelZ"]   = snap.accel_z;
    doc["ldr"]      = snap.ldr_ohms;

    char timeBuf[20];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             snap.time.year, snap.time.month, snap.time.day,
             snap.time.hour, snap.time.minute, snap.time.second);
    doc["time"]        = timeBuf;
    doc["ntpSynced"]   = snap.ntp_synced;
    doc["ntpSyncAgo"]  = snap.ntp_last_sync_ago / 1000;

    doc["radioStation"] = snap.audio_station_index;
    doc["radioName"]    = snap.audio_station_name;
    doc["radioPlaying"] = snap.audio_playing;
    doc["radioVolume"]  = snap.audio_volume;

    String json;
    serializeJson(doc, json);
    if (client) client->text(json);
    else _ws->textAll(json);
}

void WebDashboard::_broadcastStationList(AsyncWebSocketClient* client) {
    Station stations[NvsStore::MAX_STATIONS];
    int count = _store.loadStations(stations, NvsStore::MAX_STATIONS);

    JsonDocument doc;
    doc["type"] = "stations";
    doc["defaultCount"] = _store.defaultCount();
    JsonArray arr = doc["stations"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject st = arr.add<JsonObject>();
        st["name"] = stations[i].name;
        st["url"]  = stations[i].url;
    }

    String json;
    serializeJson(doc, json);
    if (client) client->text(json);
    else _ws->textAll(json);
}
