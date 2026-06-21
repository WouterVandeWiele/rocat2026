#include "GUI_QR.h"
#include "GUI_Utils.h"
#include "SED1530_LCD.h"
#include "qrcodesed.h"
#include <Arduino.h>
#include <string.h>

extern SED1530_LCD display;
static QRcode_SED1530 _qr(&display);

static void _show(const char* qr_text,
                  const char* l1, const char* l2,
                  const char* bottom) {
    clearCanvas();

    _qr.init();
    _qr.create(qr_text);

    int max_chars = _qr.getOffsetX() / CHAR_W;
    display.setTextSize(1);
    display.setTextColor(GLCD_COLOR_SET);
    if (l1) { display.setCursor(0, 4);  display.write((const uint8_t*)l1, min((int)strlen(l1), max_chars)); }
    if (l2) { display.setCursor(0, 14); display.write((const uint8_t*)l2, min((int)strlen(l2), max_chars)); }
    if (bottom) { display.setCursor(0, 40); display.print(bottom); }

    display.updateWholeScreen();
    delay(10);
    display.updateWholeScreen();
}

void GUI_QR::clear() {
    clearCanvas();
    display.updateWholeScreen();
}

void GUI_QR::show_wifi(const char* ssid, const char* password) {
    char buf[128];
    if (password && password[0] != '\0')
        snprintf(buf, sizeof(buf), "WIFI:S:%s;T:WPA;P:%s;;", ssid, password);
    else
        snprintf(buf, sizeof(buf), "WIFI:S:%s;T:nopass;;", ssid);

    _show(buf, "Connect", "to AP", ssid);
}

void GUI_QR::show_url(const char* url) {
    const char* ip = url;
    if (strncmp(url, "http://", 7) == 0)
        ip = url + 7;
    else if (strncmp(url, "https://", 8) == 0)
        ip = url + 8;

    _show(url, "Open", "portal", ip);
}
