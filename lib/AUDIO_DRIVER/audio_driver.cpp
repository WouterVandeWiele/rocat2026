#include "audio_driver.h"
#include <mutex>
#include <cstring>
#include "rocat_constants.h"


    // AudioSourceSPIFFS _source("/", ".mp3");
    // AnalogAudioStream _out;
    // MP3DecoderHelix   _decoder;
    // AudioPlayer       _player(_source, _out, _decoder);

// const AudioInfo AudioDriver::AUDIO_INFO{44100, 2, 16};

AudioDriver::AudioDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig)
    : DriverBase()
    , _sx(sx)
    , _pin_aud_en(pin_aud_en)
    , _pin_aud_sig(pin_aud_sig)
    , _source("/", ".mp3")
    , _player(_source, _out, _decoder)
     {}

void AudioDriver::begin() {
    // {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.pinMode(_pin_aud_en, OUTPUT);
        // _sx.digitalWrite(_pin_aud_en, HIGH);
    // }
    // auto cfg = _out.defaultConfig(TX_MODE);
    auto cfg = _out.defaultConfig();
    // cfg.copyFrom(AUDIO_INFO);
    // cfg.port_no = 26;
    // On an I2S DMA underrun (e.g. a 2-5ms SPIFFS read/decode stall in
    // _player.copy()), auto_clear fills the gap with raw 0x00, which the
    // DAC reads as 0V -- a half-scale step down from the mid-scale (0x80)
    // idle level that shows up as a sharp "digital level" spike on the
    // analog output. Repeat the last sample instead.
    // cfg.auto_clear = false;
    _out.begin(cfg);

    // Prime the DMA buffer with real silence (PCM 0 -> DAC mid-scale 0x80)
    // before the amp is enabled. begin() internally zero-fills the DMA
    // buffer with raw 0x00, which the DAC reads as 0V (minimum) rather than
    // silence -- without this, GPIO25 jumps 0V -> mid-scale once playback
    // starts, producing an audible pop.
    //
    // NOTE: AnalogDriverESP32::outputStereo() converts the buffer IN PLACE
    // (dst and src alias the same memory), turning PCM 0 into 0x8000. On the
    // next call that 0x8000 gets re-converted into 0x0000 (DAC 0V), then back
    // to 0x8000, etc. -- alternating "silence"/"0V" writes that show up as a
    // train of ~3ms digital-looking pulses on GPIO25. Re-zero the buffer
    // before every write to keep it true silence.
    // {
    //     int16_t silence[256] = {0};
    //     size_t total = (size_t)cfg.buffer_size * cfg.buffer_count;
    //     size_t written = 0;
    //     while (written < total) {
    //         memset(silence, 0, sizeof(silence));
    //         written += _out.write((const uint8_t*)silence, sizeof(silence)) / sizeof(int16_t);
    //     }
    // }

    _player.setAutoNext(false);
    // _player.setVolume(1.0f);
    _player.begin();

    xTaskCreatePinnedToCore(
        _copyTask,
            "audio_copy",
            4096,
            this,
            configMAX_PRIORITIES - 1,
            &_task,
            0
        );
}

void AudioDriver::_enable(bool on) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_aud_en, on ? HIGH : LOW);
}

void AudioDriver::play(const char* path) {
    _enable(true);
    _player.setPath(path);
    // pinMode(pin_gio_sda, OUTPUT);
}

void AudioDriver::_copyTask(void* param) {
    auto* self = static_cast<AudioDriver*>(param);
    // self->_enable(true);
    for (;;) {
        self->_player.copy();
        // _player.copy();
        vTaskDelay(1);
    }
}

void AudioDriver::tick() {
    _player.copy();
    // pinMode(pin_gio_sda, OUTPUT);
}

void AudioDriver::stop() {
    _player.setActive(false);
    _enable(false);
    // pinMode(pin_gio_sda, OUTPUT);
}
