#include "wifi_driver.h"
#include "GUI_QR.h"
#include <Arduino.h>

WifiDriver::WifiDriver(const char* ap_prefix, const char* ap_password)
    : DriverBase(), _ap_prefix(ap_prefix), _ap_password(ap_password), _ap_name{} {}

void WifiDriver::_ensureWm() {
    if (!_wm) {
        _wm = new WiFiManager();
    }
}

void WifiDriver::begin() {
    uint32_t id = (uint32_t)(ESP.getEfuseMac() >> 24);
    snprintf(_ap_name, sizeof(_ap_name), "%s_%06X", _ap_prefix, id);

    _ensureWm();
    _wm->setConfigPortalBlocking(false);
    _wm->setCleanConnect(true);
    _wm->setShowInfoUpdate(false);
    std::vector<const char*> menu = {"wifi", "info", "exit"};
    _wm->setMenu(menu);
    _wm->autoConnect(_ap_name, _ap_password);

    if (is_connected()) {
        _wm->stopWebPortal();
        Serial.printf("[wifi] connected to %s — IP: %s\n",
                      ssid().c_str(), local_ip().c_str());
    } else {
        Serial.printf("[wifi] config portal started on AP: %s\n", _ap_name);
        GUI_QR::show_wifi(_ap_name, _ap_password);
        _showing_wifi_qr = true;
    }
}

bool WifiDriver::process() {
    if (!_wm) return is_connected();

    if (!_portal_stopped && is_connected()) {
        _wm->stopWebPortal();
        _portal_stopped = true;
        _showing_wifi_qr = false;
        GUI_QR::clear();
        Serial.printf("[wifi] connected to %s — IP: %s\n",
                      ssid().c_str(), local_ip().c_str());

        delete _wm;
        _wm = nullptr;
        Serial.println("[wifi] WiFiManager released (~3-5 KB heap freed)");
        return true;
    }

    if (_showing_wifi_qr && WiFi.softAPgetStationNum() > 0) {
        _showing_wifi_qr = false;
        GUI_QR::show_url("http://192.168.4.1");
        Serial.println("[wifi] client connected to AP, showing portal QR");
    }

    return _wm->process();
}

void WifiDriver::start_portal() {
    _ensureWm();
    _portal_stopped = false;
    _wm->startConfigPortal(_ap_name, _ap_password);
}

void WifiDriver::stop_portal() {
    if (_wm) _wm->stopConfigPortal();
}

void WifiDriver::reset() {
    _ensureWm();
    _wm->resetSettings();
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
