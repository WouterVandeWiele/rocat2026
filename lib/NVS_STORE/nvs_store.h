#pragma once

#include <Preferences.h>

struct Station {
    char name[32];
    char url[128];
};

class NvsStore {
public:
    static constexpr int MAX_STATIONS = 20;

    void begin(const char* ns = "rocat");

    int  loadStations(Station* out, int maxCount);
    void saveStations(const Station* stations, int count);
    void addStation(const char* name, const char* url);
    void removeStation(int index);
    void updateStation(int index, const char* name, const char* url);
    int  stationCount();
    int  defaultCount();

    // Playback state
    void  savePlaybackState(int stationIndex, bool playing, float volume);
    int   lastStation();
    bool  lastPlaying();
    float lastVolume();

    // NTP server
    void saveNtpServer(const char* server);
    void loadNtpServer(char* buf, size_t len);

    // Timezone (POSIX TZ string)
    void saveTimezone(const char* tz);
    void loadTimezone(char* buf, size_t len);

private:
    Preferences _prefs;
    const char* _ns = "rocat";
};
