#include "GUI_QR.h"
#include "GUI_Utils.h"
#include "SED1530_LCD.h"
#include "qrcodesed.h"
#include "carousel_manager.h"
#include <Arduino.h>
#include <string.h>

static void _show(const char* qr_text,
                  const char* l1, const char* l2,
                  const char* bottom) {
    clearCanvas();

    QRcode_SED1530 qr(lcd);
    qr.init();
    qr.create(qr_text);

    int max_chars = qr.getOffsetX() / CHAR_W;
    lcd->setTextSize(1);
    lcd->setTextColor(GLCD_COLOR_SET);
    if (l1) { lcd->setCursor(0, 4);  lcd->write((const uint8_t*)l1, min((int)strlen(l1), max_chars)); }
    if (l2) { lcd->setCursor(0, 14); lcd->write((const uint8_t*)l2, min((int)strlen(l2), max_chars)); }
    if (bottom) { lcd->setCursor(0, 40); lcd->print(bottom); }

    lcd->updateWholeScreen();
    delay(10);
    lcd->updateWholeScreen();
}

void GUI_QR::clear() {
    clearCanvas();
    lcd->updateWholeScreen();
    CarouselManager::instance().release();
}

void GUI_QR::show_wifi(const char* ssid, const char* password) {
    CarouselManager::instance().override();

    char buf[128];
    if (password && password[0] != '\0')
        snprintf(buf, sizeof(buf), "WIFI:S:%s;T:WPA;P:%s;;", ssid, password);
    else
        snprintf(buf, sizeof(buf), "WIFI:S:%s;T:nopass;;", ssid);

    _show(buf, "Connect", "to AP", ssid);
}

void GUI_QR::show_url(const char* url) {
    CarouselManager::instance().override();

    const char* ip = url;
    if (strncmp(url, "http://", 7) == 0)
        ip = url + 7;
    else if (strncmp(url, "https://", 8) == 0)
        ip = url + 8;

    _show(url, "Open", "portal", ip);
}
