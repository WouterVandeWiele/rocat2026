#pragma once
#include "SED1530_LCD.h"

static constexpr int LCD_W         = 100;
static constexpr int LCD_H         = 48;
static constexpr int LCD_BUF_BYTES = ((LCD_W + 7) / 8) * LCD_H;
static constexpr int CHAR_W        = 6;
static constexpr int CHAR_H        = 8;

extern SED1530_LCD* lcd;

void clearCanvas();
void showSplash(const char* line1, const char* line2 = nullptr);
