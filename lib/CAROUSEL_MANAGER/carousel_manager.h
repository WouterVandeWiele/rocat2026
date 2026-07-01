#pragma once
#include "gui_element.h"
#include <atomic>

class SED1530_LCD;
class AdcKeys;
class TouchDriver;
enum class PetGesture;

class CarouselManager {
public:
    static CarouselManager& instance();

    void setDisplay(SED1530_LCD* display);
    void setInput(AdcKeys* keys, TouchDriver* touch);
    void add(GuiElement* element, unsigned long durationMs);
    void begin(unsigned long now, uint32_t stackSize = 4096, uint8_t priority = 1);

    // Pause the carousel so the caller can use the display directly.
    // Call release() when done — the carousel re-inits the active element
    // and resumes from where it left off.
    void override();
    void release();
    bool isOverridden() const { return _overridden.load(); }

    void next(unsigned long now);
    void prev(unsigned long now);

    GuiElement*   current() const;
    int           currentIndex() const { return _current; }
    int           count() const { return _count; }

private:
    static constexpr int MAX_ELEMENTS = 8;

    static constexpr unsigned long LOADING_DURATION_MS = 2000;

    struct Entry {
        GuiElement*   element    = nullptr;
        unsigned long durationMs = 0;
        unsigned long baseDurationMs = 0;
    };

    Entry              _entries[MAX_ELEMENTS] = {};
    int                _count      = 0;
    int                _current    = 0;
    unsigned long      _switchTime = 0;
    std::atomic<bool>  _overridden{false};
    unsigned long      _overrideStart = 0;

    AdcKeys*      _keys  = nullptr;
    TouchDriver*  _touch = nullptr;

    static void _taskEntry(void* arg);
    void _run();
    void pollInput();
    void handleButton(uint8_t button);
    void handleGesture(uint8_t gesture);
    void advance(int dir, unsigned long now);
};
