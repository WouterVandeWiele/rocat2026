#pragma once

#include "../DRIVER_BASE/driver_base.h"
#include <WiFiManager.h>

class WifiDriver : public DriverBase {
public:
    WifiDriver(const char* ap_name = "ROCAT", const char* ap_password = nullptr);

    void begin() override;

    // Non-blocking: call in loop() while the config portal is active.
    bool process();

    // Start the captive portal manually (e.g. from a menu action).
    void start_portal();

    // Stop the captive portal.
    void stop_portal();

    // Erase stored credentials and restart the portal.
    void reset();

    bool is_connected() const;

    String local_ip() const;
    String ssid() const;

private:
    WiFiManager _wm;
    const char* _ap_name;
    const char* _ap_password;
    bool        _portal_stopped = false;
};
