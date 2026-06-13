#include "GUI_Utils.h"
#include <string.h>

extern SED1530_LCD display;

void clearCanvas() { memset(display.getBuffer(), 0, LCD_BUF_BYTES); }

void showSplash(const char* line1, const char* line2) {
    clearCanvas();
    display.setTextSize(1);
    display.setTextColor(GLCD_COLOR_SET);
    display.setCursor(0, line2 ? 16 : 20);
    display.print(line1);
    if (line2) { display.setCursor(0, 28); display.print(line2); }
    display.display();
}
