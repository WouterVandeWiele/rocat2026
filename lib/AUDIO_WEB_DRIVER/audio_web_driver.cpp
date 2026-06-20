#include "audio_web_driver.h"
#include <mutex>
#include "rocat_constants.h"

const Station AudioWebDriver::STATIONS[STATION_COUNT] = {
    {"MNM Hits",       "http://icecast.vrtcdn.be/mnm_hits-mid.mp3"},
    {"MNM",            "http://icecast.vrtcdn.be/mnm-mid.mp3"},
    {"Radio 1",        "http://icecast.vrtcdn.be/radio1-mid.mp3"},
    {"Radio 2",        "http://icecast.vrtcdn.be/ra2ant-mid.mp3"},
    {"Studio Brussel", "http://icecast.vrtcdn.be/stubru-mid.mp3"},
};

const char* AudioWebDriver::STATION_URLS[STATION_COUNT] = {
    STATIONS[0].url,
    STATIONS[1].url,
    STATIONS[2].url,
    STATIONS[3].url,
    STATIONS[4].url,
};

AudioWebDriver::AudioWebDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig)
    : DriverBase()
    , _sx(sx)
    , _pin_aud_en(pin_aud_en)
    , _pin_aud_sig(pin_aud_sig)
    , _urlStream()
    , _source(_urlStream, STATION_URLS, "audio/mp3")
    , _player(_source, _out, _decoder)
{}

void AudioWebDriver::begin() {
    {
        std::lock_guard<std::mutex> lck(i2c_operations);
        _sx.pinMode(_pin_aud_en, OUTPUT);
    }

    auto cfg = _out.defaultConfig();
    _out.begin(cfg);

    _player.setAutoNext(false);
    _player.begin();

    xTaskCreatePinnedToCore(
        _copyTask,
        "audio_web",
        8192,
        this,
        configMAX_PRIORITIES - 1,
        &_task,
        0
    );
}

void AudioWebDriver::_enable(bool on) {
    std::lock_guard<std::mutex> lck(i2c_operations);
    _sx.digitalWrite(_pin_aud_en, on ? HIGH : LOW);
}

void AudioWebDriver::play(int station_index) {
    _enable(true);
    _player.setIndex(station_index);
    _player.setActive(true);
    Serial.printf("[web_audio] playing: %s\n", current_name());
}

void AudioWebDriver::stop() {
    _player.setActive(false);
    _enable(false);
    Serial.println("[web_audio] stopped");
}

void AudioWebDriver::next() {
    _enable(false);
    _player.next();
    _enable(true);
    Serial.printf("[web_audio] next: %s\n", current_name());
}

void AudioWebDriver::previous() {
    _enable(false);
    _player.previous();
    _enable(true);
    Serial.printf("[web_audio] previous: %s\n", current_name());
}

const char* AudioWebDriver::current_name() {
    int idx = _source.index();
    if (idx >= 0 && idx < STATION_COUNT)
        return STATIONS[idx].name;
    return "Unknown";
}

void AudioWebDriver::_copyTask(void* param) {
    auto* self = static_cast<AudioWebDriver*>(param);
    for (;;) {
        self->_player.copy();
        vTaskDelay(1);
    }
}
