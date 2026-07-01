#include "Weather.h"
#include "GUI_Utils.h"
#include "WeatherAnim.h"
#include <math.h>

namespace Weather {

static const unsigned long FETCH_MS    = 86400000UL;  // 24 h
static const unsigned long SCREEN_MS   = 8000UL;
static const int           NUM_SCREENS = 2;

// ── screen 1 layout ───────────────────────────────────────────────────────────

static const int ANIM_X         = 0;
static const int ANIM_Y         = 0;
static const int S1_TEXT_X      = 43;    // right panel start (pixels after 40-px anim)
static const int DESC_SCROLL_MS = 80;

// ── screen 2 layout ───────────────────────────────────────────────────────────

static const int COMP_CX   = 19;         // compass centre x
static const int COMP_CY   = 23;         // compass centre y
static const int COMP_R    = 16;         // compass radius (bounding box x=3..35, y=7..39)
static const int S2_TEXT_X = 38;         // right panel start (just clear of compass)

// ── translatable strings ──────────────────────────────────────────────────────
// Select a language by defining WEATHER_LANG_EN or WEATHER_LANG_NL below.
// Label length constraints (text size 1, 6 px/char, panel 62 px wide):
//   feelsLike : 1–4 chars   (prepended to the temperature value)
//   humidity  : 3–4 chars   (formatted as "<label>: <value>%")
//   cloud     : 3–4 chars   (formatted as "<label>: <value>%")
//   rain      : 4 chars max (formatted as "<label>:<value>mm")
//   snow      : 4 chars max (formatted as "<label>:<value>cm")

#define WEATHER_LANG_EN
// #define WEATHER_LANG_NL

struct WeatherStrings {
    const char* loading;
    const char* noData;
    const char* noDataSub;
    const char* feelsLike;   // prefix before feels-like temperature
    const char* humidity;
    const char* cloud;
    const char* rain;        // total liquid precipitation
    const char* snow;
};

static const WeatherStrings STR_EN = {
    .loading="Loading weather...",
    .noData="No weather",
    .noDataSub="data yet",
    .feelsLike="~",          // "~" is universally understood as "approximately"
    .humidity="Hum",
    .cloud="Cld",
    .rain="Rain",
    .snow="Snow",
};

static const WeatherStrings STR_NL = {
    .loading="Weer laden...",
    .noData="Geen weer",
    .noDataSub="data beschikbaar",
    .feelsLike="~",
    .humidity="Voch",       // Vochtigheid
    .cloud="Bwk",        // Bewolking  — 3 chars keeps "Bwk: 85%" within panel
    .rain="Nrs",        // Neerslag   — 3 chars keeps "Nrs:2.4mm" within panel
    .snow="Snee",       // Sneeuw     — 4 chars keeps "Snee:0.0cm" within panel
};

#ifdef WEATHER_LANG_NL
static const WeatherStrings& STR = STR_NL;
#else
static const WeatherStrings& STR = STR_EN;
#endif

// ── internal state ────────────────────────────────────────────────────────────

static GFXcanvas1  animCanvas(WeatherAnim::W, WeatherAnim::H);
static WeatherAnim weatherAnim;

weather_t  w   = {};
location_t loc = {};

static unsigned long _lastFetch  = 0;
static unsigned long _lastSwitch = 0;
static int           _screen     = 0;
static int           _rotsDone   = 0;

// ── helpers ───────────────────────────────────────────────────────────────────

int severity() {
    if (!w.status) return 0;
    if (w.weather_code >= 95) return 3;
    if (w.weather_code >= 51) return 2;
    if (w.weather_code >= 45) return 1;
    return 0;
}

static const char* windCardinal(int deg) {
    static const char* dirs[] = { "N","NE","E","SE","S","SW","W","NW" };
    return dirs[((deg + 22) % 360) / 45];
}

// Classic two-tone compass needle pointing where the wind blows TO
// (meteorological convention: wind_direction is where it comes FROM,
// so the arrow tip is at wind_direction + 180°).
//
// Needle geometry — diamond shape split at the pivot:
//   Front half (solid filled triangle) → shows the wind destination clearly
//   Rear half  (outline triangle only) → classic counterweight look
//   Pivot      (hollow ring, 2 px radius)
//
// Bezel: outer circle + 2-px inward tick marks at S/E/W and a 3-px tick at N
// so the compass can be read without a label.
//
// Animation: ±3° sinusoidal wobble on a 4-second period simulates a real
// compass needle settling on its bearing.
static void drawCompassArrow(int cx, int cy, int r, int degFrom, unsigned long ph) {
    // ── bezel ─────────────────────────────────────────────────────────────────
    lcd->drawCircle(cx, cy, r, GLCD_COLOR_SET);
    // Cardinal tick marks — N is longer so the compass orientation is legible
    lcd->drawLine(cx,     cy - r,     cx,     cy - r + 3, GLCD_COLOR_SET);  // N
    lcd->drawLine(cx,     cy + r,     cx,     cy + r - 2, GLCD_COLOR_SET);  // S
    lcd->drawLine(cx - r, cy,         cx - r + 2, cy,     GLCD_COLOR_SET);  // W
    lcd->drawLine(cx + r, cy,         cx + r - 2, cy,     GLCD_COLOR_SET);  // E

    // ── needle ────────────────────────────────────────────────────────────────
    float wobble = 3.0f * sinf((float)(ph % 4000u) / 636.6f);  // 636.6 ≈ 4000/(2π)
    float rad    = (degFrom + 180.0f + wobble) * (float)M_PI / 180.0f;
    float sdx    =  sinf(rad);
    float sdy    = -cosf(rad);   // screen y grows downward → negate cos

    static const int HW = 4;    // half-width of needle at the pivot

    int tipFx = cx + (int)roundf((r - 3) * sdx);   // front tip (near bezel)
    int tipFy = cy + (int)roundf((r - 3) * sdy);
    int tipRx = cx - (int)roundf((r / 3) * sdx);   // rear  tip (shorter)
    int tipRy = cy - (int)roundf((r / 3) * sdy);
    int lftX  = cx + (int)roundf(HW * (-sdy));      // left  wing at pivot
    int lftY  = cy + (int)roundf(HW *   sdx);
    int rgtX  = cx + (int)roundf(HW *   sdy);       // right wing at pivot
    int rgtY  = cy - (int)roundf(HW *   sdx);

    // Front half: solid — indicates the wind direction at a glance
    lcd->fillTriangle(tipFx, tipFy, lftX, lftY, rgtX, rgtY, GLCD_COLOR_SET);
    // Rear half: outline only — classic counterweight, visible against the bezel
    lcd->drawLine(tipRx, tipRy, lftX, lftY, GLCD_COLOR_SET);
    lcd->drawLine(tipRx, tipRy, rgtX, rgtY, GLCD_COLOR_SET);

    // ── pivot bearing ─────────────────────────────────────────────────────────
    lcd->fillCircle(cx, cy, 2, GLCD_COLOR_CLEAR);  // erase needle centre
    lcd->drawCircle(cx, cy, 2, GLCD_COLOR_SET);    // bearing ring
}

// ── screens ───────────────────────────────────────────────────────────────────

static void drawScreen1(unsigned long phaseMs) {
    clearCanvas();
    weatherAnim.draw(animCanvas, w.weather_code, w.is_day, phaseMs);
    lcd->drawBitmap(ANIM_X, ANIM_Y,
                       animCanvas.getBuffer(),
                       WeatherAnim::W, WeatherAnim::H, GLCD_COLOR_SET);
    lcd->setTextSize(1);
    lcd->setTextColor(GLCD_COLOR_SET);
    lcd->setTextWrap(false);

    char buf[12];
    // Temperature — value only; the unit makes it self-explanatory
    snprintf(buf, sizeof(buf), "%.1f\xB0""C", w.temperature);
    lcd->setCursor(S1_TEXT_X, 4); lcd->print(buf);

    // Feels-like — translated prefix + value
    snprintf(buf, sizeof(buf), "%s%.1f\xB0""C", STR.feelsLike, w.apparent_temperature);
    lcd->setCursor(S1_TEXT_X, 16); lcd->print(buf);

    // Humidity — translated label
    snprintf(buf, sizeof(buf), "%s: %d%%", STR.humidity, w.humidity);
    lcd->setCursor(S1_TEXT_X, 28); lcd->print(buf);

    // Scrolling weather description across the full bottom strip
    const char* desc   = weatherDescription(w.weather_code);
    int         textW  = (int)strlen(desc) * 6;
    int         totalW = LCD_W + textW;
    int         period = totalW * DESC_SCROLL_MS;
    int         x = LCD_W - (int)((phaseMs % (unsigned long)period) * totalW / period);
    lcd->setCursor(x, 40);
    lcd->print(desc);
    lcd->display();
}

// Screen 2: animated wind compass (left) + wind speed, direction, cloud,
// precipitation, and snow (right).
// precipitation = total liquid (rain + showers); snowfall in cm.
static void drawScreen2(unsigned long phaseMs) {
    clearCanvas();
    lcd->setTextSize(1);
    lcd->setTextColor(GLCD_COLOR_SET);
    lcd->setTextWrap(false);

    drawCompassArrow(COMP_CX, COMP_CY, COMP_R, w.wind_direction, phaseMs);

    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fkm/h", w.wind_speed);
    lcd->setCursor(S2_TEXT_X, 2); lcd->print(buf);

    lcd->setCursor(S2_TEXT_X, 12);
    lcd->print(windCardinal(w.wind_direction));

    snprintf(buf, sizeof(buf), "%s: %d%%", STR.cloud, w.cloud_cover);
    lcd->setCursor(S2_TEXT_X, 21); lcd->print(buf);

    snprintf(buf, sizeof(buf), "%s:%.1fmm", STR.rain, w.precipitation);
    lcd->setCursor(S2_TEXT_X, 30); lcd->print(buf);

    snprintf(buf, sizeof(buf), "%s:%.1fcm", STR.snow, w.snowfall);
    lcd->setCursor(S2_TEXT_X, 39); lcd->print(buf);

    lcd->display();
}

// ── public API ────────────────────────────────────────────────────────────────

void fetch(unsigned long now) {
    (void)now;
}

void init(unsigned long now) {
    _screen     = 0;
    _lastSwitch = now;
    _rotsDone   = 0;
    Serial.println("[weather] init");
}

bool tick(unsigned long now) {
    if (!w.status) { showSplash(STR.loading); return true; }

    if (now - _lastSwitch >= SCREEN_MS) {
        _screen++;
        if (_screen >= NUM_SCREENS) { _screen = 0; _rotsDone++; }
        _lastSwitch = now;
    }

    if (_screen == 0) drawScreen1(now);
    else              drawScreen2(now);
    delay(50);
    return (_rotsDone >= 1);
}

} // namespace Weather
