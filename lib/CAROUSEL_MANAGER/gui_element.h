#pragma once
#include <cstdint>

class GuiElement {
public:
    virtual ~GuiElement() = default;

    virtual const char* name() const = 0;

    virtual bool ready() const { return true; }
    virtual bool fetching() const { return false; }

    virtual void fetch(unsigned long now) { (void)now; }

    virtual void init(unsigned long now) = 0;

    // Drive one frame. Returns true when the element has completed one
    // full cycle (e.g. all weather screens shown once). The carousel
    // uses this as a hint but primarily relies on the configured duration.
    virtual bool tick(unsigned long now) = 0;

    virtual void onButton(uint8_t button) { (void)button; }
    virtual void onGesture(uint8_t gesture) { (void)gesture; }
};
