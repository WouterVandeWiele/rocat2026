#pragma once

#include <Arduino.h>

#define RSS_MAX_TITLE      128
#define RSS_MAX_DESC       500
#define RSS_MAX_FEEDS       10
#define RSS_MAX_ITEMS       30

struct RssItem {
    char title[RSS_MAX_TITLE];
    char desc[RSS_MAX_DESC];
};

typedef void (*RssFetchCallback)(int feedIndex, int totalFeeds);

class WiFiClient;
class WiFiClientSecure;

class RssFeed {
public:
    void addFeed(const char* url);

    int fetchAll(RssFetchCallback onProgress = nullptr);
    int fetchAll(WiFiClient* plain, WiFiClientSecure* secure,
                 RssFetchCallback onProgress = nullptr);

    void addItem(const char* title, const char* desc = "");

    int            count()        const { return _count; }
    const RssItem& getItem(int i) const { return _items[i]; }

private:
    char        _urls[RSS_MAX_FEEDS][128] = {};
    int         _feedCount           = 0;
    RssItem     _items[RSS_MAX_ITEMS];
    int         _count               = 0;

    int _fetchOne(const char* url, int limit, int offset,
                  WiFiClient* plain, WiFiClientSecure* secure);
};
