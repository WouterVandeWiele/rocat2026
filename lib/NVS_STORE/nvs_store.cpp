#include "nvs_store.h"
#include <Arduino.h>

static const RssFeedEntry DEFAULT_RSS_FEEDS[] = {
    {"http://www.vrt.be/vrtnws/nl.rss.articles.xml"},
    // {"http://feeds.feedburner.com/tweaborssport"},
};
static constexpr int DEFAULT_RSS_COUNT = sizeof(DEFAULT_RSS_FEEDS) / sizeof(DEFAULT_RSS_FEEDS[0]);

static const Station DEFAULT_STATIONS[] = {
    {"MNM Hits",       "http://icecast.vrtcdn.be/mnm_hits-mid.mp3"},
    {"MNM",            "http://icecast.vrtcdn.be/mnm-mid.mp3"},
    {"Radio 1",        "http://icecast.vrtcdn.be/radio1-mid.mp3"},
    {"Radio 2",        "http://icecast.vrtcdn.be/ra2ant-mid.mp3"},
    {"Studio Brussel", "http://icecast.vrtcdn.be/stubru-mid.mp3"},
};
static constexpr int DEFAULT_COUNT = sizeof(DEFAULT_STATIONS) / sizeof(DEFAULT_STATIONS[0]);

void NvsStore::begin(const char* ns) {
    _ns = ns;

    _prefs.begin(_ns, true);
    bool initialized = _prefs.isKey("nvsInit");
    _prefs.end();

    int count = 0;
    if (initialized) {
        _prefs.begin(_ns, true);
        count = _prefs.getInt("st_count", 0);
        _prefs.end();
    }

    if (!initialized || count == 0) {
        Serial.println("[nvs] seeding default stations");
        _prefs.begin(_ns, false);
        _prefs.putBool("nvsInit", true);
        _prefs.putInt("st_def", DEFAULT_COUNT);
        _prefs.end();

        saveStations(DEFAULT_STATIONS, DEFAULT_COUNT);
    }

    int rssCount = 0;
    if (initialized) {
        _prefs.begin(_ns, true);
        rssCount = _prefs.getInt("rss_count", 0);
        _prefs.end();
    }
    if (!initialized || rssCount == 0) {
        Serial.println("[nvs] seeding default RSS feeds");
        saveRssFeeds(DEFAULT_RSS_FEEDS, DEFAULT_RSS_COUNT);
    }
}

int NvsStore::stationCount() {
    _prefs.begin(_ns, true);
    int count = _prefs.getInt("st_count", 0);
    _prefs.end();
    return count;
}

int NvsStore::loadStations(Station* out, int maxCount) {
    _prefs.begin(_ns, true);
    int count = _prefs.getInt("st_count", 0);
    if (count > maxCount) count = maxCount;

    for (int i = 0; i < count; i++) {
        char keyN[8], keyU[8];
        snprintf(keyN, sizeof(keyN), "st%d_n", i);
        snprintf(keyU, sizeof(keyU), "st%d_u", i);

        String name = _prefs.getString(keyN, "");
        String url  = _prefs.getString(keyU, "");
        strncpy(out[i].name, name.c_str(), sizeof(out[i].name) - 1);
        out[i].name[sizeof(out[i].name) - 1] = '\0';
        strncpy(out[i].url, url.c_str(), sizeof(out[i].url) - 1);
        out[i].url[sizeof(out[i].url) - 1] = '\0';
    }

    _prefs.end();
    return count;
}

void NvsStore::saveStations(const Station* stations, int count) {
    if (count > MAX_STATIONS) count = MAX_STATIONS;

    _prefs.begin(_ns, false);

    int oldCount = _prefs.getInt("st_count", 0);
    for (int i = count; i < oldCount; i++) {
        char keyN[8], keyU[8];
        snprintf(keyN, sizeof(keyN), "st%d_n", i);
        snprintf(keyU, sizeof(keyU), "st%d_u", i);
        _prefs.remove(keyN);
        _prefs.remove(keyU);
    }

    _prefs.putInt("st_count", count);
    for (int i = 0; i < count; i++) {
        char keyN[8], keyU[8];
        snprintf(keyN, sizeof(keyN), "st%d_n", i);
        snprintf(keyU, sizeof(keyU), "st%d_u", i);
        _prefs.putString(keyN, stations[i].name);
        _prefs.putString(keyU, stations[i].url);
    }

    _prefs.end();
}

void NvsStore::addStation(const char* name, const char* url) {
    Station stations[MAX_STATIONS];
    int count = loadStations(stations, MAX_STATIONS);
    if (count >= MAX_STATIONS) return;

    strncpy(stations[count].name, name, sizeof(stations[count].name) - 1);
    stations[count].name[sizeof(stations[count].name) - 1] = '\0';
    strncpy(stations[count].url, url, sizeof(stations[count].url) - 1);
    stations[count].url[sizeof(stations[count].url) - 1] = '\0';

    saveStations(stations, count + 1);
}

int NvsStore::defaultCount() {
    _prefs.begin(_ns, true);
    int count = _prefs.getInt("st_def", 0);
    _prefs.end();
    return count;
}

void NvsStore::removeStation(int index) {
    Station stations[MAX_STATIONS];
    int count = loadStations(stations, MAX_STATIONS);
    if (index < 0 || index >= count) return;
    if (index < defaultCount()) return;

    for (int i = index; i < count - 1; i++) {
        stations[i] = stations[i + 1];
    }

    saveStations(stations, count - 1);
}

void NvsStore::updateStation(int index, const char* name, const char* url) {
    Station stations[MAX_STATIONS];
    int count = loadStations(stations, MAX_STATIONS);
    if (index < 0 || index >= count) return;

    strncpy(stations[index].name, name, sizeof(stations[index].name) - 1);
    stations[index].name[sizeof(stations[index].name) - 1] = '\0';
    strncpy(stations[index].url, url, sizeof(stations[index].url) - 1);
    stations[index].url[sizeof(stations[index].url) - 1] = '\0';

    saveStations(stations, count);
}

void NvsStore::savePlaybackState(int stationIndex, bool playing, float volume) {
    _prefs.begin(_ns, false);
    _prefs.putInt("pb_st", stationIndex);
    _prefs.putBool("pb_play", playing);
    _prefs.putFloat("pb_vol", volume);
    _prefs.end();
}

int NvsStore::lastStation() {
    _prefs.begin(_ns, true);
    int val = _prefs.getInt("pb_st", 0);
    _prefs.end();
    return val;
}

bool NvsStore::lastPlaying() {
    _prefs.begin(_ns, true);
    bool val = _prefs.getBool("pb_play", false);
    _prefs.end();
    return val;
}

float NvsStore::lastVolume() {
    _prefs.begin(_ns, true);
    float val = _prefs.getFloat("pb_vol", 1.0f);
    _prefs.end();
    return val;
}

void NvsStore::saveNtpServer(const char* server) {
    _prefs.begin(_ns, false);
    _prefs.putString("ntp_srv", server);
    _prefs.end();
}

void NvsStore::loadNtpServer(char* buf, size_t len) {
    _prefs.begin(_ns, true);
    String val = _prefs.getString("ntp_srv", "pool.ntp.org");
    _prefs.end();
    strncpy(buf, val.c_str(), len - 1);
    buf[len - 1] = '\0';
}

void NvsStore::saveTimezone(const char* tz) {
    _prefs.begin(_ns, false);
    _prefs.putString("tz", tz);
    _prefs.end();
}

void NvsStore::loadTimezone(char* buf, size_t len) {
    _prefs.begin(_ns, true);
    String val = _prefs.getString("tz", "CET-1CEST,M3.5.0,M10.5.0/3");
    _prefs.end();
    strncpy(buf, val.c_str(), len - 1);
    buf[len - 1] = '\0';
}

// ── RSS feeds ────────────────────────────────────────────────────────────────

int NvsStore::rssFeedCount() {
    _prefs.begin(_ns, true);
    int count = _prefs.getInt("rss_count", 0);
    _prefs.end();
    return count;
}

int NvsStore::loadRssFeeds(RssFeedEntry* out, int maxCount) {
    _prefs.begin(_ns, true);
    int count = _prefs.getInt("rss_count", 0);
    if (count > maxCount) count = maxCount;

    for (int i = 0; i < count; i++) {
        char key[10];
        snprintf(key, sizeof(key), "rss%d_u", i);
        String url = _prefs.getString(key, "");
        strncpy(out[i].url, url.c_str(), sizeof(out[i].url) - 1);
        out[i].url[sizeof(out[i].url) - 1] = '\0';
    }

    _prefs.end();
    return count;
}

void NvsStore::saveRssFeeds(const RssFeedEntry* feeds, int count) {
    if (count > MAX_RSS_FEEDS) count = MAX_RSS_FEEDS;

    _prefs.begin(_ns, false);

    int oldCount = _prefs.getInt("rss_count", 0);
    for (int i = count; i < oldCount; i++) {
        char key[10];
        snprintf(key, sizeof(key), "rss%d_u", i);
        _prefs.remove(key);
    }

    _prefs.putInt("rss_count", count);
    for (int i = 0; i < count; i++) {
        char key[10];
        snprintf(key, sizeof(key), "rss%d_u", i);
        _prefs.putString(key, feeds[i].url);
    }

    _prefs.end();
}

void NvsStore::addRssFeed(const char* url) {
    RssFeedEntry feeds[MAX_RSS_FEEDS];
    int count = loadRssFeeds(feeds, MAX_RSS_FEEDS);
    if (count >= MAX_RSS_FEEDS) return;

    strncpy(feeds[count].url, url, sizeof(feeds[count].url) - 1);
    feeds[count].url[sizeof(feeds[count].url) - 1] = '\0';
    saveRssFeeds(feeds, count + 1);
}

void NvsStore::removeRssFeed(int index) {
    RssFeedEntry feeds[MAX_RSS_FEEDS];
    int count = loadRssFeeds(feeds, MAX_RSS_FEEDS);
    if (index < 0 || index >= count) return;

    for (int i = index; i < count - 1; i++) {
        feeds[i] = feeds[i + 1];
    }
    saveRssFeeds(feeds, count - 1);
}
