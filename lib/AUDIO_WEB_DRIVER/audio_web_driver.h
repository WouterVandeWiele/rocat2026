#pragma once

#include "driver_base.h"
#include "nvs_store.h"
#include <SparkFunSX1509.h>

#include <AudioTools.h>
#include <AudioTools/Communication/HTTP/URLStream.h>
#include <AudioTools/Disk/AudioSourceURL.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>

class AudioWebDriver : public DriverBase {
public:
    static constexpr int MAX_STATIONS = NvsStore::MAX_STATIONS;

    AudioWebDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig);

    void begin() override;

    void setStations(const Station* stations, int count);
    int  stationCount() const { return _station_count; }

    void play(int station_index = 0);
    void stop();
    void next();
    void previous();

    int         current_index() { return _source.index(); }
    const char* current_name();
    void        setVolume(float vol);
    float       volume();

private:
    SX1509&          _sx;
    const uint8_t    _pin_aud_en;
    const uint8_t    _pin_aud_sig;

    Station          _stations[MAX_STATIONS];
    const char*      _urls[MAX_STATIONS + 1];
    int              _station_count = 0;

    URLStream        _urlStream;
    AudioSourceURL   _source;
    AnalogAudioStream _out;
    MP3DecoderHelix  _decoder;
    AudioPlayer      _player;
    TaskHandle_t     _task = nullptr;

    void _enable(bool on);
    static void _copyTask(void* param);

    SemaphoreHandle_t _guard = nullptr;
};
