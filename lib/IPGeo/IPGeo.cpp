#include "IPGeo.h"

static const char* URL =
    "http://ip-api.com/json"
    "?fields=status,message,country,countryCode,region,regionName,city,zip,lat,lon,timezone,offset";

static const uint32_t MAX_RETRIES   = 5;
static const uint32_t RETRY_DELAY_MS = 2000;

static location_t _fetch(WiFiClient& client) {
    location_t result = {};

    for (uint32_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            Serial.printf("[IPGeo] Retry %d/%d\n", attempt + 1, MAX_RETRIES);
            delay(RETRY_DELAY_MS);
        }

        HTTPClient http;
        http.begin(client, URL);
        int code = http.GET();

        if (code != HTTP_CODE_OK) {
            Serial.printf("[IPGeo] HTTP error %d\n", code);
            http.end();
            continue;
        }

        JsonDocument doc;
        String body = http.getString();
        http.end();
        DeserializationError err = deserializeJson(doc, body);

        if (err) {
            Serial.printf("[IPGeo] JSON error: %s\n", err.c_str());
            continue;
        }

        if (strcmp(doc["status"] | "", "success") != 0) {
            Serial.printf("[IPGeo] API error: %s\n", doc["message"] | "unknown");
            continue;
        }

        result.lat           = doc["lat"]        | 0.0f;
        result.lon           = doc["lon"]        | 0.0f;
        result.offsetSeconds = doc["offset"]     | 0;
        strncpy(result.city,     doc["city"]       | "", sizeof(result.city)     - 1);
        strncpy(result.region,   doc["regionName"] | "", sizeof(result.region)   - 1);
        strncpy(result.country,  doc["country"]    | "", sizeof(result.country)  - 1);
        strncpy(result.timezone, doc["timezone"]   | "", sizeof(result.timezone) - 1);
        result.status = true;
        return result;
    }

    Serial.println("[IPGeo] All retries failed.");
    return result;
}

location_t IPGeo::getLocation() {
    WiFiClient client;
    return _fetch(client);
}

location_t IPGeo::getLocation(WiFiClient& client) {
    return _fetch(client);
}

location_t IPGeo::getLocation(float lat, float lon) {
    location_t result = {};
    result.lat    = lat;
    result.lon    = lon;
    result.status = true;
    return result;
}
