#include "audio_web_driver.h"
#include <mutex>
#include "rocat_constants.h"

AudioWebDriver::AudioWebDriver(SX1509& sx, uint8_t pin_aud_en, uint8_t pin_aud_sig)
    : DriverBase()
    , _sx(sx)
    , _pin_aud_en(pin_aud_en)
    , _pin_aud_sig(pin_aud_sig)
    , _urls{}
    , _urlStream()
    , _source(_urlStream, _urls, "audio/mp3")
    , _player(_source, _out, _decoder)
{}

void AudioWebDriver::setStations(const Station* stations, int count) {
    if (count > MAX_STATIONS) count = MAX_STATIONS;

    if (_guard) xSemaphoreTake(_guard, portMAX_DELAY);

    bool wasPlaying = _player.isActive();
    if (wasPlaying) {
        _player.setActive(false);
        _enable(false);
    }

    _station_count = count;
    for (int i = 0; i < count; i++) {
        _stations[i] = stations[i];
        _urls[i] = _stations[i].url;
    }
    _urls[count] = nullptr;

    if (_guard) xSemaphoreGive(_guard);
    Serial.printf("[web_audio] loaded %d stations\n", count);
}

void AudioWebDriver::begin() {
    {
        std::lock_guard<std::mutex> lck(i2c_operations);
        _sx.pinMode(_pin_aud_en, OUTPUT);
    }

    _guard = xSemaphoreCreateMutex();

    auto cfg = _out.defaultConfig();
    cfg.buffer_count = 8;    // more DMA buffers absorbs WiFi interrupt latency
    cfg.buffer_size  = 512;  // larger buffer per slot
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
    if (station_index < 0 || station_index >= _station_count) return;
    xSemaphoreTake(_guard, portMAX_DELAY);
    _enable(true);
    _player.setIndex(station_index);
    _player.setActive(true);
    xSemaphoreGive(_guard);
    Serial.printf("[web_audio] playing: %s\n", current_name());
}

void AudioWebDriver::stop() {
    xSemaphoreTake(_guard, portMAX_DELAY);
    _player.setActive(false);
    _enable(false);
    xSemaphoreGive(_guard);
    Serial.println("[web_audio] stopped");
}

void AudioWebDriver::next() {
    xSemaphoreTake(_guard, portMAX_DELAY);
    _player.next();
    xSemaphoreGive(_guard);
    Serial.printf("[web_audio] next: %s\n", current_name());
}

void AudioWebDriver::previous() {
    xSemaphoreTake(_guard, portMAX_DELAY);
    _player.previous();
    xSemaphoreGive(_guard);
    Serial.printf("[web_audio] previous: %s\n", current_name());
}

void AudioWebDriver::setVolume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    _player.setVolume(vol);
}

float AudioWebDriver::volume() {
    return _player.volume();
}

const char* AudioWebDriver::current_name() {
    int idx = _source.index();
    if (idx >= 0 && idx < _station_count)
        return _stations[idx].name;
    return "Unknown";
}

void AudioWebDriver::_copyTask(void* param) {
    auto* self = static_cast<AudioWebDriver*>(param);
    for (;;) {
        if (xSemaphoreTake(self->_guard, pdMS_TO_TICKS(5)) == pdTRUE) {
            self->_player.copy();
            xSemaphoreGive(self->_guard);
        }
        vTaskDelay(1);
    }
}
