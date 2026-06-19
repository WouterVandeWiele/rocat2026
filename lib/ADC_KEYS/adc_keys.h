#pragma once

#include "../DRIVER_BASE/driver_base.h"
#include <GEM_adafruit_gfx.h>
#include <list>

#define GEM_KEY_FUNC_1_DOWN 10
#define GEM_KEY_FUNC_2_UP 11
#define GEM_KEY_FUNC_3_LEFT 12
#define GEM_KEY_FUNC_4_RIGHT 13

struct ButtonPress {
    byte button;
    bool last_event;
};

class AdcKeys : public DriverBase {
public:
    static constexpr uint16_t KEY_TH_1       = 403;
    static constexpr uint16_t KEY_TH_2       = 1040;
    static constexpr uint8_t  QUEUE_SIZE     = 10;
    static constexpr size_t   FILTER_DEPTH   = 3;

    AdcKeys(uint8_t pin_ac1, uint8_t pin_bd1, uint8_t pin_c12,
            uint8_t pin_ac2, uint8_t pin_bd2);

    void begin() override;
    void tick();
    void debug();

    QueueHandle_t queue() const { return _queue; }

    static void send_empty(QueueHandle_t q);

private:
    enum class Level : uint8_t { e_key_NONE, e_key_LOW, e_key_HIGH };

    struct Channel {
        uint8_t              pin;
        byte                 button_a;
        byte                 button_b;
        std::list<Level>     buffer;
        ButtonPress          state;
        bool                 pressed = false;
    };

    static constexpr uint8_t NUM_CHANNELS = 5;
    Channel _channels[NUM_CHANNELS];

    QueueHandle_t _queue = nullptr;

    void compare(Channel& ch);
};
