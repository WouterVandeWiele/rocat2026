#pragma once
#include <Arduino.h>
#include "RssFeed.h"

namespace News {
    extern RssFeed feed;
    bool hasItems();
    void init(unsigned long now);
    bool tick(unsigned long now);
}
