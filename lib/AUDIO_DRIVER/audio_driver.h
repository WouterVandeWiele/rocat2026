#pragma once

#include "driver_base.h"
#include <SparkFunSX1509.h>


#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSPIFFS.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>


class AudioDriver : public DriverBase {
public:
    AudioDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig);

    void begin() override;
    void tick();
    void play(const char* path);
    void stop();
    void _enable(bool on);

private:
    static const AudioInfo AUDIO_INFO;

    SX1509&           _sx;
    const uint8_t     _pin_aud_en;
    const uint8_t     _pin_aud_sig;
    AudioSourceSPIFFS _source;
    AnalogAudioStream _out;
    MP3DecoderHelix   _decoder;
    AudioPlayer       _player;
    TaskHandle_t      _task = nullptr;

    static void _copyTask(void* param);
};
