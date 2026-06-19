#include "adc_keys.h"
#include <Arduino.h>

AdcKeys::AdcKeys(uint8_t pin_ac1, uint8_t pin_bd1, uint8_t pin_c12,
                 uint8_t pin_ac2, uint8_t pin_bd2)
    : DriverBase()
{
    const uint8_t pins[] = {pin_ac1, pin_bd1, pin_c12, pin_ac2, pin_bd2};
    const byte pairs[][2] = {
        {GEM_KEY_DOWN,        GEM_KEY_UP},
        {GEM_KEY_LEFT,        GEM_KEY_RIGHT},
        {GEM_KEY_OK,          GEM_KEY_CANCEL},
        {GEM_KEY_FUNC_1_DOWN, GEM_KEY_FUNC_2_UP},
        {GEM_KEY_FUNC_3_LEFT, GEM_KEY_FUNC_4_RIGHT},
    };
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        _channels[i].pin      = pins[i];
        _channels[i].button_a = pairs[i][0];
        _channels[i].button_b = pairs[i][1];
        _channels[i].buffer   = {Level::e_key_NONE, Level::e_key_NONE, Level::e_key_NONE};
        _channels[i].state    = {GEM_KEY_NONE, false};
    }
}

void AdcKeys::begin() {
    analogReadResolution(12);
    _queue = xQueueCreate(QUEUE_SIZE, sizeof(ButtonPress));
}

void AdcKeys::tick() {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        compare(_channels[i]);
        if (_channels[i].state.last_event) {
            xQueueSend(_queue, &_channels[i].state, 0);
        }
    }
}

void AdcKeys::send_empty(QueueHandle_t q) {
    ButtonPress empty = {GEM_KEY_NONE, true};
    xQueueSend(q, &empty, 0);
}

void AdcKeys::debug() {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
        Serial.printf(">adc_%d:%lu\n", _channels[i].pin, analogReadMilliVolts(_channels[i].pin));
}

void AdcKeys::compare(Channel& ch) {
    uint32_t voltage = analogReadMilliVolts(ch.pin);

    ch.state.button     = GEM_KEY_NONE;
    ch.state.last_event = false;

    Level sample;
    if (voltage < KEY_TH_1)
        sample = Level::e_key_NONE;
    else if (voltage < KEY_TH_2)
        sample = Level::e_key_LOW;
    else
        sample = Level::e_key_HIGH;

    ch.buffer.emplace_front(sample);
    ch.buffer.pop_back();

    uint8_t count_low  = 0;
    uint8_t count_high = 0;
    Level   first      = ch.buffer.front();
    Level   last       = Level::e_key_NONE;

    for (auto it = ch.buffer.begin(); it != ch.buffer.end(); ++it) {
        if (*it == Level::e_key_LOW)  count_low++;
        if (*it == Level::e_key_HIGH) count_high++;
        last = *it;
    }

    if (count_low == 0 && count_high == 0) {
        ch.pressed = false;
        return;
    }

    if (ch.pressed)
        return;

    bool press_active = (first != Level::e_key_NONE);

    if ((count_low == 2 && press_active) || (count_high == 2 && press_active)) {
        ch.state.button = (ch.buffer.front() == Level::e_key_LOW) ? ch.button_a : ch.button_b;
        ch.state.last_event = true;
        ch.pressed = true;
    }
}
