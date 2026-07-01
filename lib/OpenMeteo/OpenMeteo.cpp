#include "OpenMeteo.h"
#include <WiFiClient.h>

const char* weatherDescription(int code) {
    switch (code) {
        case  0: return "Clear sky";
        case  1: return "Mainly clear";
        case  2: return "Partly cloudy";
        case  3: return "Overcast";
        case 45: return "Fog";
        case 48: return "Rime fog";
        case 51: return "Light drizzle";
        case 53: return "Moderate drizzle";
        case 55: return "Dense drizzle";
        case 56: return "Light freezing drizzle";
        case 57: return "Heavy freezing drizzle";
        case 61: return "Slight rain";
        case 63: return "Moderate rain";
        case 65: return "Heavy rain";
        case 66: return "Light freezing rain";
        case 67: return "Heavy freezing rain";
        case 71: return "Slight snowfall";
        case 73: return "Moderate snowfall";
        case 75: return "Heavy snowfall";
        case 77: return "Snow grains";
        case 80: return "Slight showers";
        case 81: return "Moderate showers";
        case 82: return "Violent showers";
        case 85: return "Slight snow showers";
        case 86: return "Heavy snow showers";
        case 95: return "Thunderstorm";
        case 96: return "Thunderstorm w/ slight hail";
        case 99: return "Thunderstorm w/ heavy hail";
        default: return "Unknown";
    }
}

static const char* BASE_URL =
    "http://api.open-meteo.com/v1/forecast"
    "?current=temperature_2m,relative_humidity_2m,apparent_temperature,"
    "is_day,wind_speed_10m,wind_direction_10m,weather_code,"
    "cloud_cover,rain,showers,snowfall,precipitation";

static weather_t _fetch(WiFiClient& client, float latitude, float longitude) {
    weather_t result = {};

    String url = String(BASE_URL)
                 + "&latitude="  + String(latitude,  4)
                 + "&longitude=" + String(longitude, 4);

    HTTPClient http;
    http.begin(client, url);
    int code = http.GET();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[OpenMeteo] HTTP error %d\n", code);
        http.end();
        return result;
    }

    JsonDocument doc;
    String body = http.getString();
    http.end();
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
        Serial.printf("[OpenMeteo] JSON error: %s\n", err.c_str());
        return result;
    }

    JsonObjectConst cur = doc["current"];
    strncpy(result.time, cur["time"] | "", sizeof(result.time) - 1);
    result.temperature          = cur["temperature_2m"]       | 0.0f;
    result.humidity             = cur["relative_humidity_2m"] | 0;
    result.apparent_temperature = cur["apparent_temperature"] | 0.0f;
    result.is_day               = (cur["is_day"]              | 0) != 0;
    result.wind_speed           = cur["wind_speed_10m"]       | 0.0f;
    result.wind_direction       = cur["wind_direction_10m"]   | 0;
    result.weather_code         = cur["weather_code"]         | 0;
    result.cloud_cover          = cur["cloud_cover"]          | 0;
    result.rain                 = cur["rain"]                 | 0.0f;
    result.showers              = cur["showers"]              | 0.0f;
    result.snowfall             = cur["snowfall"]             | 0.0f;
    result.precipitation        = cur["precipitation"]        | 0.0f;
    result.status               = true;

    return result;
}

weather_t OpenMeteo::getWeather(float latitude, float longitude) {
    WiFiClient client;
    return _fetch(client, latitude, longitude);
}

weather_t OpenMeteo::getWeather(float latitude, float longitude, WiFiClient& client) {
    return _fetch(client, latitude, longitude);
}
