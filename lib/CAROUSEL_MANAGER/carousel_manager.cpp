#include "carousel_manager.h"
#include "GUI_Utils.h"
#include "adc_keys.h"
#include "touch_driver.h"
#include <Arduino.h>
#include <WiFi.h>

static constexpr uint8_t KEY_PREV = GEM_KEY_FUNC_3_LEFT;
static constexpr uint8_t KEY_NEXT = GEM_KEY_FUNC_4_RIGHT;

CarouselManager& CarouselManager::instance() {
    static CarouselManager mgr;
    return mgr;
}

void CarouselManager::setDisplay(SED1530_LCD* display) {
    lcd = display;
}

void CarouselManager::setInput(AdcKeys* keys, TouchDriver* touch) {
    _keys  = keys;
    _touch = touch;
}

void CarouselManager::add(GuiElement* element, unsigned long durationMs) {
    if (_count >= MAX_ELEMENTS) return;
    _entries[_count].element        = element;
    _entries[_count].durationMs     = durationMs;
    _entries[_count].baseDurationMs = durationMs;
    _count++;
}

void CarouselManager::begin(unsigned long now, uint32_t stackSize, uint8_t priority) {
    if (_count == 0) return;
    _current = 0;
    _switchTime = now;
    for (int i = 0; i < _count; i++) {
        if (_entries[i].element->ready()) {
            _current = i;
            _entries[i].element->init(now);
            break;
        }
    }
    xTaskCreate(_taskEntry, "carousel", stackSize, this, priority, nullptr);
}

void CarouselManager::override() {
    _overrideStart = millis();
    _overridden.store(true);
    Serial.println("[carousel] overridden");
}

void CarouselManager::release() {
    _overridden.store(false);
    unsigned long paused = millis() - _overrideStart;
    _switchTime += paused;
    _entries[_current].element->init(millis());
    Serial.printf("[carousel] released, resuming %s\n",
                  _entries[_current].element->name());
}

void CarouselManager::_taskEntry(void* arg) {
    static_cast<CarouselManager*>(arg)->_run();
}

void CarouselManager::_run() {
    Serial.printf("[carousel] task started, element %s\n",
                  _entries[_current].element->name());
    for (;;) {
        if (_overridden.load()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        pollInput();

        unsigned long now = millis();
        int prev = _current;
        _entries[_current].element->tick(now);

        if (_current != prev) continue;

        if (now - _switchTime >= _entries[_current].durationMs) {
            next(millis());
        }
    }
}

void CarouselManager::pollInput() {
    if (_keys) {
        _keys->tick();
        ButtonPress bp;
        while (xQueueReceive(_keys->queue(), &bp, 0) == pdTRUE) {
            handleButton(bp.button);
        }
    }

    if (_touch) {
        PetGesture g = _touch->tick();
        if (g != PetGesture::NONE) {
            handleGesture(static_cast<uint8_t>(g));
        }
    }
}

void CarouselManager::handleButton(uint8_t button) {
    if (button == KEY_PREV) { prev(millis()); return; }
    if (button == KEY_NEXT) { next(millis()); return; }
    _entries[_current].element->onButton(button);
}

void CarouselManager::handleGesture(uint8_t gesture) {
    _entries[_current].element->onGesture(gesture);
}

void CarouselManager::next(unsigned long now) {
    advance(1, now);
}

void CarouselManager::prev(unsigned long now) {
    advance(-1, now);
}

GuiElement* CarouselManager::current() const {
    if (_count == 0) return nullptr;
    return _entries[_current].element;
}

void CarouselManager::advance(int dir, unsigned long now) {
    for (int i = 0; i < _count; i++) {
        int candidate = ((_current + dir * (i + 1)) % _count + _count) % _count;
        auto* el = _entries[candidate].element;

        if (!el->ready() && WiFi.isConnected()) {
            el->fetch(now);

            if (el->fetching()) {
                _current    = candidate;
                _switchTime = now;
                _entries[candidate].durationMs = LOADING_DURATION_MS;
                Serial.printf("[carousel] -> %s (loading)\n", el->name());
                return;
            }
            Serial.printf("[carousel] skip %s (not ready)\n", el->name());
            continue;
        }

        if (el->ready()) {
            _entries[candidate].durationMs = _entries[candidate].baseDurationMs;
            _current    = candidate;
            _switchTime = now;
            el->init(now);
            Serial.printf("[carousel] -> %s (%d/%d)\n",
                          el->name(), _current + 1, _count);
            return;
        }

        Serial.printf("[carousel] skip %s (not ready)\n", el->name());
    }
}
