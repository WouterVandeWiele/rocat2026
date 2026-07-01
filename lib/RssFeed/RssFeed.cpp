#include "RssFeed.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string.h>

// Static download buffer — stays in BSS, avoids heap fragmentation.
static char xmlBuf[32 * 1024];

// ---- text helpers --------------------------------------------------------

static void toAscii(char* dst, const char* src, int srcLen, int dstSize) {
    int  di    = 0;
    bool inTag = false;
    for (int si = 0; si < srcLen && di < dstSize - 1; si++) {
        unsigned char c = (unsigned char)src[si];
        if (c == '<')  { inTag = true;  continue; }
        if (c == '>')  { inTag = false; continue; }
        if (inTag)       continue;
        if (c == '&') {
            const char* e = src + si;
            if      (strncmp(e, "&amp;",  5) == 0) { dst[di++] = '&';  si += 4; }
            else if (strncmp(e, "&lt;",   4) == 0) { dst[di++] = '<';  si += 3; }
            else if (strncmp(e, "&gt;",   4) == 0) { dst[di++] = '>';  si += 3; }
            else if (strncmp(e, "&quot;", 6) == 0) { dst[di++] = '"';  si += 5; }
            else if (strncmp(e, "&apos;", 6) == 0) { dst[di++] = '\''; si += 5; }
            else if (strncmp(e, "&nbsp;", 6) == 0) { dst[di++] = ' ';  si += 5; }
            else dst[di++] = '&';
            continue;
        }
        if (c >= 0x20 && c < 0x80) dst[di++] = (char)c;
    }
    dst[di] = '\0';
}

static void extractField(char* dst, int dstSize,
                         const char* start, const char* end) {
    const char* text = start;
    int len = (int)(end - start);
    if (len >= 9 && strncmp(text, "<![CDATA[", 9) == 0) {
        text += 9;
        const char* ce = strstr(text, "]]>");
        len = ce ? (int)(ce - text) : 0;
    }
    toAscii(dst, text, len < dstSize - 1 ? len : dstSize - 1, dstSize);
}

// ---- HTTP download -------------------------------------------------------

static int download(const char* url,
                    WiFiClient* extPlain, WiFiClientSecure* extSecure) {
    bool isHttps = (strncmp(url, "https", 5) == 0);

    WiFiClient localPlain;
    WiFiClientSecure localSecure;

    WiFiClient* plain  = extPlain  ? extPlain  : &localPlain;
    WiFiClientSecure* secure = extSecure ? extSecure : &localSecure;
    if (isHttps && !extSecure) secure->setInsecure();

    if (isHttps) {
        Serial.printf("[RssFeed] HTTPS connect (heap: %u, block: %u)\n",
                      ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    }

    HTTPClient http;
    if (isHttps) http.begin(*secure, url);
    else         http.begin(*plain, url);
    http.setTimeout(15000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[RssFeed] HTTP %d for %s (heap: %u, block: %u)\n",
                      code, url, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        http.end();
        return 0;
    }

    Stream& stream  = http.getStream();
    int     total   = http.getSize();
    int     pos     = 0;
    int     limit   = (int)sizeof(xmlBuf) - 1;
    unsigned long deadline = millis() + 15000;

    while ((http.connected() || stream.available()) && pos < limit) {
        int avail = stream.available();
        if (avail > 0) {
            int got = stream.readBytes(xmlBuf + pos, min(avail, limit - pos));
            pos += got;
            if (total > 0 && pos >= total) break;
        } else {
            if (millis() > deadline) break;
            delay(1);
        }
    }
    xmlBuf[pos] = '\0';
    http.end();
    Serial.printf("[RssFeed] Downloaded %d bytes\n", pos);
    return pos;
}

// ---- public API ----------------------------------------------------------

void RssFeed::addFeed(const char* url) {
    if (_feedCount >= RSS_MAX_FEEDS) return;
    strncpy(_urls[_feedCount], url, sizeof(_urls[0]) - 1);
    _urls[_feedCount][sizeof(_urls[0]) - 1] = '\0';
    _feedCount++;
}

void RssFeed::addItem(const char* title, const char* desc) {
    if (_count >= RSS_MAX_ITEMS) return;
    RssItem& it = _items[_count++];
    memset(&it, 0, sizeof(it));
    strncpy(it.title, title, RSS_MAX_TITLE - 1);
    strncpy(it.desc,  desc,  RSS_MAX_DESC  - 1);
}

int RssFeed::fetchAll(RssFetchCallback onProgress) {
    return fetchAll(nullptr, nullptr, onProgress);
}

int RssFeed::fetchAll(WiFiClient* plain, WiFiClientSecure* secure,
                      RssFetchCallback onProgress) {
    memset(_items, 0, sizeof(_items));
    _count = 0;
    if (_feedCount == 0) return 0;
    int perFeed = RSS_MAX_ITEMS / _feedCount;
    for (int i = 0; i < _feedCount; i++) {
        if (onProgress) onProgress(i, _feedCount);
        _count += _fetchOne(_urls[i], perFeed, _count, plain, secure);
    }
    Serial.printf("[RssFeed] Total items: %d\n", _count);
    return _count;
}

// ---- parser (private) ----------------------------------------------------

int RssFeed::_fetchOne(const char* url, int limit, int offset,
                       WiFiClient* plain, WiFiClientSecure* secure) {
    if (download(url, plain, secure) == 0) return 0;

    const char* xml = xmlBuf;

    const char* feedEl = strstr(xml, "<feed");
    bool isAtom = feedEl && (feedEl[5] == ' ' || feedEl[5] == '>' ||
                  feedEl[5] == '\n' || feedEl[5] == '\r' || feedEl[5] == '\t');

    const char* openTag  = isAtom ? "<entry" : "<item";
    const char* closeTag = isAtom ? "</entry>" : "</item>";
    int         openLen  = isAtom ? 6 : 5;
    int         closeLen = isAtom ? 8 : 7;

    const char* p     = xml;
    int         added = 0;

    while (*p && added < limit) {
        const char* itemStart = strstr(p, openTag);
        if (!itemStart) break;
        char after = itemStart[openLen];
        if (after != '>' && after != ' ' && after != '\t' &&
            after != '\n' && after != '\r') {
            p = itemStart + 1;
            continue;
        }

        const char* itemEnd = strstr(itemStart, closeTag);
        if (!itemEnd) break;

        int slot = offset + added;

        const char* tTag     = strstr(itemStart, "<title");
        const char* tContent = tTag ? strchr(tTag + 6, '>') : nullptr;
        if (!tTag || !tContent || tContent > itemEnd) { p = itemEnd + closeLen; continue; }
        tContent++;
        const char* tClose = strstr(tContent, "</title>");
        if (!tClose || tClose > itemEnd) { p = itemEnd + closeLen; continue; }
        extractField(_items[slot].title, RSS_MAX_TITLE, tContent, tClose);
        if (!_items[slot].title[0]) { p = itemEnd + closeLen; continue; }

        _items[slot].desc[0] = '\0';
        const char* descTags[2]  = { isAtom ? "<summary"       : "<description",
                                     isAtom ? "<description"   : "<summary"     };
        const char* descClose[2] = { isAtom ? "</summary>"     : "</description>",
                                     isAtom ? "</description>" : "</summary>"   };
        for (int t = 0; t < 2; t++) {
            const char* dTag     = strstr(itemStart, descTags[t]);
            if (!dTag || dTag > itemEnd) continue;
            const char* dContent = strchr(dTag + strlen(descTags[t]), '>');
            if (!dContent || dContent > itemEnd) continue;
            if (*(dContent - 1) == '/') continue;
            dContent++;
            const char* dClose = strstr(dContent, descClose[t]);
            if (!dClose || dClose > itemEnd) continue;
            extractField(_items[slot].desc, RSS_MAX_DESC, dContent, dClose);
            break;
        }

        Serial.printf("[RssFeed] [%d] %s\n", slot, _items[slot].title);
        if (_items[slot].desc[0])
            Serial.printf("[RssFeed]      desc: %s\n", _items[slot].desc);

        added++;
        p = itemEnd + closeLen;
    }
    return added;
}
