#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct weather_t {
    bool    status;
    char    time[20];
    float   temperature;
    int     humidity;
    float   apparent_temperature;
    bool    is_day;
    float   wind_speed;
    int     wind_direction;
    int     weather_code;
    int     cloud_cover;
    float   rain;
    float   showers;
    float   snowfall;
    float   precipitation;
};

const char* weatherDescription(int code);

class OpenMeteo {
public:
    weather_t getWeather(float latitude, float longitude);
};
