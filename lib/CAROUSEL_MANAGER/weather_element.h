#pragma once
#include "gui_element.h"
#include "Weather.h"
#include "web_fetcher.h"

class WeatherElement : public GuiElement {
public:
    const char* name() const override { return "Weather"; }

    bool ready() const override {
        return WebFetcher::instance().weather().valid.load();
    }

    bool fetching() const override {
        return WebFetcher::instance().weather().fetching.load();
    }

    void fetch(unsigned long now) override {
        WebFetcher::instance().requestWeatherIfStale();
    }

    void init(unsigned long now) override {
        auto& slot = WebFetcher::instance().weather();
        Weather::w   = slot.weather;
        Weather::loc = slot.location;
        Weather::init(now);
    }

    bool tick(unsigned long now) override { return Weather::tick(now); }
};
