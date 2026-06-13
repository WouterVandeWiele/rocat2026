#pragma once

#include "../DRIVER_BASE/driver_base.h"
#include <SparkFunLIS3DH.h>

enum class AccelEvent : uint8_t { NONE = 0, MOTION = 1, FREEFALL = 2, BOTH = 3 };

class AccelerometerDriver : public DriverBase {
public:
    // address: lis3dh_address (0x19) from constants.h
    explicit AccelerometerDriver(LIS3DH& lis);

    void begin() override;

    float get_accel_x();
    float get_accel_y();
    float get_accel_z();
    float get_temp();

    // Routes both motion and free-fall interrupts to the INT1 pin.
    // threshold_mg: axis threshold in milli-g (1 LSB = 16 mg at ±2g range).
    // ff_threshold_mg: free-fall threshold — all axes must be below this.
    // duration_ms: minimum event duration at 100 Hz ODR (1 LSB = 10 ms).
    void setup_motion_interrupt(uint16_t threshold_mg = 96,  uint8_t duration_ms = 0);
    void setup_freefall_interrupt(uint16_t threshold_mg = 320, uint8_t duration_ms = 30);

    // Read and clear both interrupt sources. Call from ISR or polling loop.
    AccelEvent read_interrupt_source();

    // Clear both interrupt latches without returning the source.
    void clear_interrupt();

private:
    static constexpr int TEMP_WINDOW = 10;

    LIS3DH& _lis;

    int16_t _temp_samples[TEMP_WINDOW] = {};
    int     _temp_count = 0;
    int     _temp_index = 0;
    int32_t _temp_sum   = 0;
};
