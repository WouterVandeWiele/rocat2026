#pragma once
#include <Arduino.h>

namespace GuiTime {
    void init(unsigned long now);
    bool tick(unsigned long now);
}
