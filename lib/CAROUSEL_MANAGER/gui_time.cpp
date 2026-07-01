#include "gui_time.h"
#include "GUI_Utils.h"
#include "data_broker.h"

namespace GuiTime {

static constexpr int GW = 10;
static constexpr int GH = 14;
static constexpr int GAP = 1;

// 10px wide glyphs stored as uint16_t per row (top 10 bits used)
// 0-9 digits + colon at index 10
static const uint16_t GLYPHS[11][GH] = {
    { // 0
        0b0111111000,0b1111111100,0b1100001100,0b1100001100,
        0b1100001100,0b1100001100,0b1100001100,0b1100001100,
        0b1100001100,0b1100001100,0b1100001100,0b1100001100,
        0b1111111100,0b0111111000,
    },
    { // 1
        0b0001100000,0b0011100000,0b0111100000,0b0001100000,
        0b0001100000,0b0001100000,0b0001100000,0b0001100000,
        0b0001100000,0b0001100000,0b0001100000,0b0001100000,
        0b0111111000,0b0111111000,
    },
    { // 2
        0b0111111000,0b1111111100,0b1100001100,0b0000001100,
        0b0000011000,0b0000110000,0b0001100000,0b0011000000,
        0b0110000000,0b1100000000,0b1100000000,0b1100000000,
        0b1111111100,0b1111111100,
    },
    { // 3
        0b0111111000,0b1111111100,0b0000001100,0b0000001100,
        0b0000011000,0b0011110000,0b0011110000,0b0000011000,
        0b0000001100,0b0000001100,0b0000001100,0b0000001100,
        0b1111111100,0b0111111000,
    },
    { // 4
        0b0000110000,0b0001110000,0b0011110000,0b0110110000,
        0b1100110000,0b1100110000,0b1100110000,0b1111111100,
        0b1111111100,0b0000110000,0b0000110000,0b0000110000,
        0b0000110000,0b0000110000,
    },
    { // 5
        0b1111111100,0b1111111100,0b1100000000,0b1100000000,
        0b1111111000,0b1111111100,0b0000001100,0b0000001100,
        0b0000001100,0b0000001100,0b0000001100,0b1100001100,
        0b1111111100,0b0111111000,
    },
    { // 6
        0b0111111000,0b1111111100,0b1100000000,0b1100000000,
        0b1111111000,0b1111111100,0b1100001100,0b1100001100,
        0b1100001100,0b1100001100,0b1100001100,0b1100001100,
        0b1111111100,0b0111111000,
    },
    { // 7
        0b1111111100,0b1111111100,0b0000001100,0b0000011000,
        0b0000110000,0b0000110000,0b0001100000,0b0001100000,
        0b0011000000,0b0011000000,0b0011000000,0b0011000000,
        0b0011000000,0b0011000000,
    },
    { // 8
        0b0111111000,0b1111111100,0b1100001100,0b1100001100,
        0b1100001100,0b0111111000,0b0111111000,0b1100001100,
        0b1100001100,0b1100001100,0b1100001100,0b1100001100,
        0b1111111100,0b0111111000,
    },
    { // 9
        0b0111111000,0b1111111100,0b1100001100,0b1100001100,
        0b1100001100,0b1100001100,0b1111111100,0b0111111100,
        0b0000001100,0b0000001100,0b0000001100,0b0000001100,
        0b1111111100,0b0111111000,
    },
    { // colon
        0b0000000000,0b0000000000,0b0000000000,0b0011000000,
        0b0011000000,0b0000000000,0b0000000000,0b0000000000,
        0b0011000000,0b0011000000,0b0000000000,0b0000000000,
        0b0000000000,0b0000000000,
    },
};

// HH:MM:SS = 8 glyphs + 7 gaps = 87px, centered
static constexpr int TIME_W = 8 * GW + 7 * GAP;
static constexpr int TIME_X = (LCD_W - TIME_W) / 2;
static constexpr int TIME_Y = 2;
static constexpr int DATE_Y = TIME_Y + GH + 4;

static void drawGlyph(int ox, int oy, int idx) {
    for (int row = 0; row < GH; row++) {
        uint16_t bits = GLYPHS[idx][row];
        for (int col = 0; col < GW; col++) {
            if (bits & (1 << (GW - 1 - col))) {
                lcd->drawPixel(ox + col, oy + row, GLCD_COLOR_SET);
            }
        }
    }
}

static void draw(const RocatTime& t) {
    clearCanvas();

    const int glyphs[] = {
        t.hour / 10, t.hour % 10, 10,
        t.minute / 10, t.minute % 10, 10,
        t.second / 10, t.second % 10
    };

    int x = TIME_X;
    for (int i = 0; i < 8; i++) {
        drawGlyph(x, TIME_Y, glyphs[i]);
        x += GW + GAP;
    }

    lcd->setTextSize(1);
    lcd->setTextColor(GLCD_COLOR_SET);
    char dateBuf[11];
    snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%04d", t.day, t.month, t.year);
    lcd->setCursor((LCD_W - 10 * CHAR_W) / 2, DATE_Y);
    lcd->print(dateBuf);

    lcd->renderScreen();
}

static uint8_t _lastSecond = 0xFF;

void init(unsigned long) {
    _lastSecond = 0xFF;
    auto t = DataBroker::instance().read(
        [](const Blackboard& b) { return b.time; });
    draw(t);
    Serial.println("[time] init");
}

bool tick(unsigned long) {
    auto t = DataBroker::instance().read(
        [](const Blackboard& b) { return b.time; });

    if (t.second == _lastSecond) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return false;
    }
    _lastSecond = t.second;
    draw(t);
    return false;
}

} // namespace GuiTime
