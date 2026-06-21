#include <Arduino.h>
#include "qrencode.h"
#include "qrcodesed.h"

QRcode_SED1530::QRcode_SED1530(SED1530_LCD *display)
    : display(display) {}

void QRcode_SED1530::init() {
    this->screenwidth = 100;
    this->screenheight = 48;
    multiply = 1;
    offsetsX = screenwidth - WD - 5;
    offsetsY = 0;
}

void QRcode_SED1530::screenwhite() {
    display->fillRect(offsetsX, offsetsY, WD, WD, GLCD_COLOR_SET);
}

void QRcode_SED1530::screenupdate() {
}

void QRcode_SED1530::drawPixel(int x, int y, int color) {
    if (x < offsetsX || x >= offsetsX + WD || y < offsetsY || y >= offsetsY + WD)
        return;
    display->drawPixel(x, y, color ? GLCD_COLOR_SET : GLCD_COLOR_CLEAR);
}
