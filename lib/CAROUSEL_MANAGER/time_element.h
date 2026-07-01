#pragma once
#include "gui_element.h"
#include "gui_time.h"

class TimeElement : public GuiElement {
public:
    const char* name() const override { return "Time"; }
    void init(unsigned long now) override { GuiTime::init(now); }
    bool tick(unsigned long now) override { return GuiTime::tick(now); }
};
