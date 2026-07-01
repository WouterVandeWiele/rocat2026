#pragma once
#include <Arduino.h>
#include <OpenMeteo.h>
#include <IPGeo.h>

namespace Weather {
    extern weather_t  w;
    extern location_t loc;
    int  severity();
    void fetch(unsigned long now);
    void init(unsigned long now);
    bool tick(unsigned long now);
}
