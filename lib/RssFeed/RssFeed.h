#pragma once

#include <Arduino.h>

#define RSS_MAX_TITLE      128
#define RSS_MAX_DESC       500
#define RSS_MAX_FEEDS        4
#define RSS_ITEMS_PER_FEED  10

struct RssItem {
    char title[RSS_MAX_TITLE];
    char desc[RSS_MAX_DESC];
};

// Called before each feed is fetched so the application can show progress.
typedef void (*RssFetchCallback)(int feedIndex, int totalFeeds);

class RssFeed {
public:
    // Register a feed URL (call once per feed, up to RSS_MAX_FEEDS).
    void addFeed(const char* url);

    // Fetch all registered feeds. Clears previous items first.
    // Calls onProgress(i, total) before each request if provided.
    // Returns total items stored.
    int fetchAll(RssFetchCallback onProgress = nullptr);

    // Inject an item directly without a network fetch (for offline testing).
    void addItem(const char* title, const char* desc = "");

    int            count()        const { return _count; }
    const RssItem& getItem(int i) const { return _items[i]; }

private:
    const char* _urls[RSS_MAX_FEEDS] = {};
    int         _feedCount           = 0;
    RssItem     _items[RSS_MAX_FEEDS * RSS_ITEMS_PER_FEED];
    int         _count               = 0;

    int _fetchOne(const char* url, int limit, int offset);
};
