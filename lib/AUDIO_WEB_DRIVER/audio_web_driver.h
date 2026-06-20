#pragma once

#include "driver_base.h"
#include <SparkFunSX1509.h>

#include <AudioTools.h>
#include <AudioTools/Communication/HTTP/URLStream.h>
#include <AudioTools/Disk/AudioSourceURL.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>

struct Station {
    const char* name;
    const char* url;
};

class AudioWebDriver : public DriverBase {
public:
    static constexpr int STATION_COUNT = 5;

    static const Station STATIONS[STATION_COUNT];
    static const char*   STATION_URLS[STATION_COUNT];

    AudioWebDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig);

    void begin() override;
    void play(int station_index = 0);
    void stop();
    void next();
    void previous();

    int         current_index() { return _source.index(); }
    const char* current_name();

private:
    SX1509&          _sx;
    const uint8_t    _pin_aud_en;
    const uint8_t    _pin_aud_sig;
    URLStream        _urlStream;
    AudioSourceURL   _source;
    AnalogAudioStream _out;
    MP3DecoderHelix  _decoder;
    AudioPlayer      _player;
    TaskHandle_t     _task = nullptr;

    void _enable(bool on);
    static void _copyTask(void* param);
};
