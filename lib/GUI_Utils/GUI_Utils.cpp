#include "GUI_Utils.h"
#include <string.h>
#include <Arduino.h>

SED1530_LCD* lcd = nullptr;

void clearCanvas() { memset(lcd->getBuffer(), 0, LCD_BUF_BYTES); }

void showSplash(const char* line1, const char* line2) {
    clearCanvas();
    lcd->setTextSize(1);
    lcd->setTextColor(GLCD_COLOR_SET);
    lcd->setCursor(0, line2 ? 16 : 20);
    lcd->print(line1);
    if (line2) { lcd->setCursor(0, 28); lcd->print(line2); }
    lcd->display();
}
