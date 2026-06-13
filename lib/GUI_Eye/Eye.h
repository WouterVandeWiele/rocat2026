#pragma once
#include <Arduino.h>

namespace Eye {
    void        init(unsigned long now);
    bool        tick(unsigned long now);
    const char* moodName();   // current mood's display name
}
