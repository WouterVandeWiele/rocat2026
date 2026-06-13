#include <Arduino.h>
#include "qrencode.h"
#include "qrcodesed.h"

// Implements functions for OLED displays SSD1306 and SSH1106

QRcode_SED1530::QRcode_SED1530(SED1530_LCD *display)
    : display(display)
{
}

void QRcode_SED1530::init() {
    this->screenwidth = 100;
    this->screenheight = 48;
    // this->screenwidth = 33;
    // this->screenheight = 33;
    display->lcd_init();
    // multiply = screenheight/WD;
    multiply = 1;
    offsetsX = (screenwidth-(WD*multiply))/2;
    offsetsY = (screenheight-(WD*multiply))/2;
}

void QRcode_SED1530::screenwhite() {
    display->clearScreen(GLCD_COLOR_CLEAR);
    display->fillRect(0, 0, screenwidth, screenheight, GLCD_COLOR_SET);
}

void QRcode_SED1530::screenupdate() {
     display->updateWholeScreen();
}

void QRcode_SED1530::drawPixel(int x, int y, int color) {
    uint16_t thecolor;
    if(color==1) {
        thecolor = GLCD_COLOR_SET;
    } else {
        thecolor = GLCD_COLOR_CLEAR;
    }
    display->fillRect(x, y, 1, 1, thecolor);
    // if (this->multiply>1) {
    //     display->fillRect(x, y, 2, 2, thecolor);
    // }
}