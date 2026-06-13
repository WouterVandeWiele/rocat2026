#include "audio_driver_pwm.h"
#include <mutex>

const AudioInfo AudioDriverPWM::AUDIO_INFO{44100, 1, 16};

AudioDriverPWM::AudioDriverPWM(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig)
    : DriverBase()
    , _sx(sx)
    , _pin_aud_en(pin_aud_en)
    , _pin_aud_sig(pin_aud_sig)
    , _source("/", ".mp3")
    , _player(_source, _out, _decoder) {}

void AudioDriverPWM::begin() {
    
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.pinMode(_pin_aud_en, OUTPUT);
    // _sx.digitalWrite(_pin_aud_en, HIGH);
    
    auto cfg = _out.defaultConfig(TX_MODE);
    cfg.copyFrom(AUDIO_INFO);
    cfg.start_pin = _pin_aud_sig;
    cfg.resolution = 8;          // 8-11 bits, drives default freq if pwm_frequency==0
    // cfg.pwm_frequency = 300000;  // explicit carrier frequency in Hz
    // Larger ring buffer so the PWM ISR has enough headroom to ride out
    // SPIFFS read / MP3 decode jitter without underflowing (which caused
    // the slow, stuttering playback).
    // cfg.buffer_size = 2048;
    // cfg.buffers = 16;
    // AudioLogger::instance().begin(Serial, AudioLogger::Info);
    _out.begin(cfg);
    _player.setAutoNext(false);
    _player.setVolume(0.75f);
    // Pull bigger chunks from SPIFFS per tick() so the decoder keeps the
    // PWM buffer topped up.
    // _player.setBufferSize(2048);
    _player.begin();

    cfg.logConfig();

    // Feed the decoder/PWM ring buffer from its own high-priority task so
    // it can't be starved by other work (I2C, display, etc.) in loop().
    xTaskCreatePinnedToCore(_copyTask, "audio_copy", 4096, this,
                            configMAX_PRIORITIES - 1, &_task, 1);
}

void AudioDriverPWM::_enable(bool on) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_aud_en, on ? HIGH : LOW);
}

void AudioDriverPWM::play(const char* path) {
    _enable(true);
    _player.setPath(path);
}

void AudioDriverPWM::_copyTask(void* param) {
    auto* self = static_cast<AudioDriverPWM*>(param);
    for (;;) {
        self->_player.copy();
        vTaskDelay(1);
    }
}

void AudioDriverPWM::stop() {
    _player.setActive(false);
    _enable(false);
}
