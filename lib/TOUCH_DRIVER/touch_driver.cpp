#include "touch_driver.h"
#include <Arduino.h>

TouchDriver::TouchDriver(uint8_t pin_a, uint8_t pin_b, uint16_t threshold)
    : DriverBase(), _pin_a(pin_a), _pin_b(pin_b), _threshold(threshold) {}

void TouchDriver::begin() {
    touchRead(_pin_a);
    touchRead(_pin_b);
}

PetGesture TouchDriver::tick() {
    uint16_t ra = touchRead(_pin_a);
    uint16_t rb = touchRead(_pin_b);
    bool a = ra <= _threshold;
    bool b = rb <= _threshold;
    unsigned long now = millis();

    if (a != _prev_a) {
        Serial.printf("[touch] A %s (raw=%u)\n", a ? "pressed" : "released", ra);
        _prev_a = a;
    }
    if (b != _prev_b) {
        Serial.printf("[touch] B %s (raw=%u)\n", b ? "pressed" : "released", rb);
        _prev_b = b;
    }

    // Which pin is the finger sitting over right now?
    // When only one pin reads touched it is clearly dominant.
    // When both read touched (finger spanning a pad boundary), hold the last
    // dominant pin — switching to A by default here would create a false
    // A→B→A flicker that looks like a 3-pad sequence.
    uint8_t dominant = 0;
    if (a && b) dominant = (_last_dominant != 0) ? _last_dominant : 1;
    else if (a) dominant = 1;
    else if (b) dominant = 2;

    if (_seq_len == 0) {
        // Wait for first touch to start recording
        if (dominant != 0) {
            _gesture_start_ms = now;
            _last_change_ms   = now;
            _seq[_seq_len++]  = dominant;
            _last_dominant    = dominant;
        }
    } else {
        // Abort if the user has been touching for too long (hand resting, not swiping)
        if (now - _gesture_start_ms > GESTURE_MAX_MS) {
            _reset();
            return PetGesture::NONE;
        }

        // Record a dominance change only after the debounce window has elapsed,
        // filtering out noise-driven flicker at pad boundaries.
        if (dominant != 0 && dominant != _last_dominant &&
            now - _last_change_ms >= DEBOUNCE_MS &&
            _seq_len < MAX_SEQ) {
            _seq[_seq_len++] = dominant;
            _last_dominant   = dominant;
            _last_change_ms  = now;
        }

        // Finger fully lifted — evaluate the recorded sequence
        if (!a && !b) {
            PetGesture result = _evaluate();
            _reset();
            return result;
        }
    }

    return PetGesture::NONE;
}

void TouchDriver::tickB() {
    Serial.printf("[touch] A=%u  B=%u\n", touchRead(_pin_a), touchRead(_pin_b));
}

PetGesture TouchDriver::_evaluate() const {
    if (_seq_len < MIN_TRANSITIONS) return PetGesture::NONE;

    // Sequence must be strictly alternating — any repeated value means the
    // finger stalled or jumped, which is not a clean swipe.
    for (int i = 1; i < _seq_len; i++) {
        if (_seq[i] == _seq[i - 1]) return PetGesture::NONE;
    }

    // Direction is decided by whichever pin was dominant first:
    //   A first (pad 1 or top side) → swipe went top-to-bottom → DOWN
    //   B first (pad 2 or bottom side) → swipe went bottom-to-top → UP
    return (_seq[0] == 1) ? PetGesture::DOWN : PetGesture::UP;
}

void TouchDriver::_reset() {
    _seq_len       = 0;
    _last_dominant = 0;
}
