#include "wifi_driver.h"
#include <Arduino.h>

WifiDriver::WifiDriver(const char* ap_name, const char* ap_password)
    : DriverBase(), _ap_name(ap_name), _ap_password(ap_password) {}

void WifiDriver::begin() {
    _wm.setConfigPortalBlocking(false);
    _wm.setCleanConnect(true);
    _wm.setShowInfoUpdate(false);
    std::vector<const char*> menu = {"wifi", "info", "exit"};
    _wm.setMenu(menu);
    _wm.autoConnect(_ap_name, _ap_password);

    if (is_connected()) {
        _wm.stopWebPortal();
        Serial.printf("[wifi] connected to %s — IP: %s\n",
                      ssid().c_str(), local_ip().c_str());
    } else {
        Serial.printf("[wifi] config portal started on AP: %s\n", _ap_name);
    }
}

bool WifiDriver::process() {
    if (!_portal_stopped && is_connected()) {
        _wm.stopWebPortal();
        _portal_stopped = true;
        Serial.printf("[wifi] connected to %s — IP: %s\n",
                      ssid().c_str(), local_ip().c_str());
    }
    return _wm.process();
}

void WifiDriver::start_portal() {
    _wm.startConfigPortal(_ap_name, _ap_password);
}

void WifiDriver::stop_portal() {
    _wm.stopConfigPortal();
}

void WifiDriver::reset() {
    _wm.resetSettings();
    Serial.println("[wifi] credentials erased, rebooting...");
    delay(500);
    ESP.restart();
}

bool WifiDriver::is_connected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiDriver::local_ip() const {
    return WiFi.localIP().toString();
}

String WifiDriver::ssid() const {
    return WiFi.SSID();
}
