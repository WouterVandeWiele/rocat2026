#pragma once

#include <ESPAsyncWebServer.h>

class NvsStore;
class TimeManager;

class WebDashboard {
public:
    WebDashboard(NvsStore& store, TimeManager& time);

    void begin();
    void update();

private:
    NvsStore&     _store;
    TimeManager&  _time;

    AsyncWebServer* _server = nullptr;
    AsyncWebSocket* _ws     = nullptr;

    bool          _started    = false;
    unsigned long _lastUpdate = 0;

    void _onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len);
    void _handleCommand(const char* data, size_t len);
    void _broadcastSensors(AsyncWebSocketClient* client = nullptr);
    void _broadcastStationList(AsyncWebSocketClient* client = nullptr);
};
