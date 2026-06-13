#include "News.h"
#include "GUI_Utils.h"

namespace News {

static const unsigned long FETCH_MS      = 3600000UL;
static const int CHAR_W                  = 6;
static const int CHAR_H                  = 8;
static const int TITLE_SCROLL_MS         = 80;
static const int TITLE_SCROLL_PX         = 1;
static const int DESC_SCROLL_MS          = 250;
static const int DESC_SCROLL_PX          = 1;
static const int ITEM_PAUSE_MS           = 1500;
static const int DESC_AREA_Y             = CHAR_H;
static const int DESC_AREA_H             = LCD_H - CHAR_H;
static const int CHARS_PER_LINE          = LCD_W / CHAR_W;
static const int MAX_DESC_LINES          = 24;

RssFeed feed;

static unsigned long _lastFetch  = 0;
static int           _itemIdx    = 0;
static uint32_t      _lastHash   = 0xFFFFFFFF;

static void pushIfChanged() {
    const uint8_t* buf = display.getBuffer();
    uint32_t h = 5381;
    for (int i = 0; i < LCD_BUF_BYTES; i++) h = ((h << 5) + h) ^ buf[i];
    if (h == _lastHash) return;
    display.updatePages(0, (LCD_H / 8) - 1);
    _lastHash = h;
}

static char wrapBuf[MAX_DESC_LINES][CHARS_PER_LINE + 1];
static int  wrapCount = 0;

static void wrapText(const char* text) {
    wrapCount = 0;
    const char* p = text;
    while (*p && wrapCount < MAX_DESC_LINES) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char* lineStart = p;
        const char* lastSpace = nullptr;
        int         col       = 0;
        while (*p && col < CHARS_PER_LINE) {
            if (*p == ' ') lastSpace = p;
            p++; col++;
        }
        int lineLen;
        if (!*p)          { lineLen = col; }
        else if (*p==' ') { lineLen = col; while (*p==' ') p++; }
        else if (lastSpace){ lineLen = (int)(lastSpace - lineStart); p = lastSpace + 1; }
        else               { lineLen = CHARS_PER_LINE; }
        if (lineLen > 0) {
            memcpy(wrapBuf[wrapCount], lineStart, lineLen);
            wrapBuf[wrapCount][lineLen] = '\0';
            wrapCount++;
        }
    }
}

static void drawTitleBar(const char* title, int x) {
    display.fillRect(0, 0, LCD_W, CHAR_H, GLCD_COLOR_SET);
    display.setTextSize(1);
    display.setTextColor(GLCD_COLOR_CLEAR);
    display.setCursor(x, 0);
    display.print(title);
}

static void drawDescLines(int offset) {
    display.setTextSize(1);
    display.setTextColor(GLCD_COLOR_SET);
    for (int i = 0; i < wrapCount; i++) {
        int y = DESC_AREA_Y + i * CHAR_H - offset;
        if (y >= LCD_H) break;
        display.setCursor(0, y);
        display.print(wrapBuf[i]);
    }
}

static void scrollItem(const RssItem& item) {
    bool hasDesc = item.desc[0] != '\0';
    int  titleW  = (int)strlen(item.title) * CHAR_W;
    if (hasDesc) wrapText(item.desc);

    for (int x = LCD_W; x > -titleW; x -= TITLE_SCROLL_PX) {
        clearCanvas();
        if (hasDesc) drawDescLines(0);
        drawTitleBar(item.title, x);
        pushIfChanged();
        delay(TITLE_SCROLL_MS);
    }
    if (!hasDesc) { delay(ITEM_PAUSE_MS); return; }

    int scrollRange = max(0, wrapCount * CHAR_H - DESC_AREA_H);
    for (int offset = 0; offset <= scrollRange; offset += DESC_SCROLL_PX) {
        clearCanvas();
        drawDescLines(offset);
        drawTitleBar(item.title, 0);
        pushIfChanged();
        delay(offset == 0 ? ITEM_PAUSE_MS : DESC_SCROLL_MS);
    }
    delay(ITEM_PAUSE_MS);
}

bool hasItems() { return feed.count() > 0; }

void init(unsigned long now) {
    if (feed.count() == 0 || now - _lastFetch >= FETCH_MS) {
        showSplash("Fetching news...");
        feed.fetchAll([](int i, int t){
            Serial.printf("[news] feed %d/%d\n", i+1, t);
        });
        _lastFetch = now;
        _itemIdx   = 0;
    }
    Serial.printf("[news] init — item %d/%d\n", _itemIdx, feed.count());
}

bool tick(unsigned long) {
    int count = feed.count();
    if (count == 0) return true;
    if (_itemIdx >= count) _itemIdx = 0;
    const RssItem& item = feed.getItem(_itemIdx);
    _itemIdx = (_itemIdx + 1) % count;
    if (item.title[0]) scrollItem(item);
    return true;
}

} // namespace News
