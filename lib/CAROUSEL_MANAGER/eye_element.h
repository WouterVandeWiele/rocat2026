#pragma once
#include "gui_element.h"
#include "Eye.h"

class EyeElement : public GuiElement {
public:
    const char* name() const override { return "Eye"; }
    void init(unsigned long now) override { Eye::init(now); }
    bool tick(unsigned long now) override { return Eye::tick(now); }
};
