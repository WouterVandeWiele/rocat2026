#pragma once

#include "driver_base.h"
#include <SparkFunSX1509.h>
#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSPIFFS.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/CoreAudio/AudioPWM/PWMAudioOutput.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AudioDriverPWM : public DriverBase {
public:
    AudioDriverPWM(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig);

    void begin() override;
    void play(const char* path);
    void stop();

private:
    static const AudioInfo AUDIO_INFO;

    SX1509&           _sx;
    const uint8_t     _pin_aud_en;
    const uint8_t     _pin_aud_sig;
    AudioSourceSPIFFS _source;
    PWMAudioOutput    _out;
    MP3DecoderHelix   _decoder;
    AudioPlayer       _player;
    TaskHandle_t      _task = nullptr;

    void _enable(bool on);
    static void _copyTask(void* param);
};
