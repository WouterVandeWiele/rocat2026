#pragma once

#include "driver_base.h"

enum class PetGesture { NONE, DOWN, UP };

class TouchDriver : public DriverBase {
public:
    // pin_a / pin_b: the two ESP32 touch-capable GPIO pins wired to the
    // interleaved pad strip (physical order: A, B, A, B).
    // threshold: touchRead() value at-or-below which a pad is considered touched.
    // Observed baseline ~32 (untouched), ~20 (near), so 26 sits in the middle.
    TouchDriver(uint8_t pin_a, uint8_t pin_b, uint16_t threshold = 26);

    void begin() override;

    // Call every loop iteration.  Returns PetGesture::DOWN or UP on full
    // release after a valid swipe (≥3 pads covered), otherwise NONE.
    PetGesture tick();
    void       tickB();

private:
    const uint8_t  _pin_a;
    const uint8_t  _pin_b;
    const uint16_t _threshold;

    // Minimum time a pin must be dominant before the change is recorded —
    // prevents electrical noise from generating false transitions.
    static constexpr unsigned long DEBOUNCE_MS    = 50;
    // If the whole gesture takes longer than this, it is discarded.
    static constexpr unsigned long GESTURE_MAX_MS = 5000;
    // 3 entries in the dominance sequence = 3 pads covered (A→B→A or B→A→B).
    static constexpr int           MIN_TRANSITIONS = 3;
    static constexpr int           MAX_SEQ         = 8;

    // 0 = none, 1 = A, 2 = B
    uint8_t       _seq[MAX_SEQ]     = {};
    int           _seq_len          = 0;
    uint8_t       _last_dominant    = 0;
    unsigned long _last_change_ms   = 0;
    unsigned long _gesture_start_ms = 0;
    bool          _prev_a           = false;
    bool          _prev_b           = false;

    PetGesture _evaluate() const;
    void       _reset();
};
