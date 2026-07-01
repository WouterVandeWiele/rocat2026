#pragma once
#include "gui_element.h"
#include "News.h"
#include "web_fetcher.h"

class NewsElement : public GuiElement {
public:
    const char* name() const override { return "News"; }

    bool ready() const override {
        return WebFetcher::instance().news().valid.load();
    }

    bool fetching() const override {
        return WebFetcher::instance().news().fetching.load();
    }

    void fetch(unsigned long now) override {
        WebFetcher::instance().requestNewsIfStale();
    }

    void init(unsigned long now) override { News::init(now); }
    bool tick(unsigned long now) override { return News::tick(now); }
};
