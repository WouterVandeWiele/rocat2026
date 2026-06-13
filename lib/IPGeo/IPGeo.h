#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct location_t {
    bool  status;
    float lat;
    float lon;
    char  city[36];
    char  region[36];
    char  country[36];
    char  timezone[36];
    int   offsetSeconds;
};

class IPGeo {
public:
    // Auto-detect location from public IP via ip-api.com
    location_t getLocation();

    // Skip the API call and use explicit coordinates
    location_t getLocation(float lat, float lon);
};
