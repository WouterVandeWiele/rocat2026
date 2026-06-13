#include "WeatherAnim.h"

// ── shared primitives ──────────────────────────────────────────────────────

static const int8_t kRDx[8] = { 16, 11,  0, -11, -16, -11,  0,  11 };
static const int8_t kRDy[8] = {  0, 11, 16,  11,   0, -11, -16, -11 };

static int triWave(unsigned long ph, int amp, unsigned long period) {
    int t    = (int)(ph % period);
    int half = (int)(period >> 1);
    return (t < half) ? (-amp + t * (2 * amp) / half)
                      : ( amp - (t - half) * (2 * amp) / half);
}

// ── cloud ──────────────────────────────────────────────────────────────────
// The two large body circles (r=7) are fixed.
// peakMod animates the three smaller circles (r=5, r=6, r=6) so only the
// bumps and fill breathe while the cloud base stays solid.
static void cloud(GFXcanvas1& c, int cx, int cy, int peakMod = 0) {
    c.fillCircle(cx - 3,  cy + 6,  7,             1);  // left  body — fixed
    c.fillCircle(cx + 7,  cy + 6,  7,             1);  // right body — fixed
    c.fillCircle(cx + 2,  cy + 7,  6 + peakMod,   1);  // centre fill
    c.fillCircle(cx - 1,  cy + 1,  5 + peakMod,   1);  // left  peak
    c.fillCircle(cx + 7,  cy - 1,  6 + peakMod,   1);  // right peak
}

static void smallCloud(GFXcanvas1& c, int cx, int cy, int sizeAdd = 0) {
    c.fillCircle(cx,      cy,      4 + sizeAdd, 1);
    c.fillCircle(cx - 3,  cy + 3,  3 + sizeAdd, 1);
    c.fillCircle(cx + 3,  cy + 3,  3 + sizeAdd, 1);
    c.fillCircle(cx,      cy + 5,  2 + sizeAdd, 1);
}

// ── other primitives ───────────────────────────────────────────────────────

static void sun(GFXcanvas1& c, int cx, int cy, int bodyR, int rayLen) {
    c.fillCircle(cx, cy, bodyR, 1);
    for (int i = 0; i < 8; i++) {
        int x0 = cx + kRDx[i] * (bodyR + 2) / 16;
        int y0 = cy + kRDy[i] * (bodyR + 2) / 16;
        int x1 = cx + kRDx[i] * (bodyR + 2 + rayLen) / 16;
        int y1 = cy + kRDy[i] * (bodyR + 2 + rayLen) / 16;
        c.drawLine(x0, y0, x1, y1, 1);
    }
}

static void moon(GFXcanvas1& c, int cx, int cy) {
    c.fillCircle(cx,     cy,     9, 1);
    c.fillCircle(cx + 5, cy - 4, 8, 0);
}

static void drop(GFXcanvas1& c, int x, int y, int len) {
    if (x < 0 || x >= 40 || y < 0 || y + len > 40) return;
    c.drawLine(x, y, x, y + len - 1, 1);
}

// Falling snowflake: perpendicular '+' cross (4 arms, 90° symmetry).
static void flake(GFXcanvas1& c, int x, int y) {
    if (x < 2 || x > 37 || y < 2 || y > 36) return;
    c.drawLine(x - 2, y,     x + 2, y,     1);
    c.drawLine(x,     y - 2, x,     y + 2, 1);
}

// Ice crystal deposit: hexagonal 3-axis asterisk (6 arms, 60° symmetry).
static void iceCrystal(GFXcanvas1& c, int x, int y) {
    if (x < 3 || x > 36 || y < 3 || y > 36) return;
    c.drawLine(x - 3, y,     x + 3, y,     1);
    c.drawLine(x - 2, y - 2, x + 2, y + 2, 1);
    c.drawLine(x + 2, y - 2, x - 2, y + 2, 1);
}

static void pellet(GFXcanvas1& c, int x, int y) {
    if (x < 2 || x > 37 || y < 2 || y > 37) return;
    c.fillCircle(x, y, 2, 1);
}

template<typename DrawFn>
static void fallingCol(GFXcanvas1& c, int x, int yTop, int yBot,
                       int count, int periodMs, unsigned long ph, DrawFn drawFn) {
    int zone = yBot - yTop;
    if (zone <= 0) return;
    int off = (int)((ph % (unsigned long)periodMs) * zone / periodMs);
    for (int d = 0; d < count; d++) {
        int y = yTop + (off + d * zone / count) % zone;
        drawFn(c, x, y);
    }
}

// ── weather renderers ──────────────────────────────────────────────────────

static void rClear(GFXcanvas1& c, bool isDay, unsigned long ph) {
    if (isDay) {
        int t = (int)(ph % 2000u);
        int r = t < 1000 ? (3 + t * 3 / 1000) : (6 - (t - 1000) * 3 / 1000);
        sun(c, 20, 20, 9, r);
    } else {
        moon(c, 20, 21);
        bool tA = (ph / 500) % 2, tB = (ph / 700) % 2;
        c.drawPixel( 7,  8, tA);  c.drawPixel(33,  7, tB);
        c.drawPixel(31, 31, !tA); c.drawPixel( 5, 32, !tB);
    }
}

static void rMainlyClear(GFXcanvas1& c, bool isDay, unsigned long ph) {
    if (isDay) {
        int t = (int)(ph % 2000u);
        int r = t < 1000 ? (2 + t * 2 / 1000) : (4 - (t - 1000) * 2 / 1000);
        sun(c, 14, 14, 7, r);
    } else {
        moon(c, 14, 14);
    }
    int dx = triWave(ph, 2, 4100u);
    int sm = triWave(ph + 2100u, 1, 4300u);
    smallCloud(c, 27 + dx, 29, sm);
}

static void rPartlyCloudy(GFXcanvas1& c, bool isDay, unsigned long ph) {
    if (isDay) {
        int t = (int)(ph % 2000u);
        int r = t < 1000 ? (2 + t * 2 / 1000) : (4 - (t - 1000) * 2 / 1000);
        sun(c, 13, 13, 7, r);
    } else {
        moon(c, 13, 13);
    }
    int dx = triWave(ph,        2, 3800u);
    int pm = triWave(ph + 900u, 1, 2900u);
    cloud(c, 22 + dx, 24, pm);
}

static void rOvercast(GFXcanvas1& c, unsigned long ph) {
    int dx1 = triWave(ph,          3, 5200u);
    int dx2 = triWave(ph + 1700u,  2, 3700u);
    int pm1 = triWave(ph + 400u,   1, 2700u);
    int pm2 = triWave(ph + 3000u,  1, 2900u);
    cloud(c, 18 + dx1,  8, pm1);
    cloud(c, 22 + dx2, 22, pm2);
}

static void rFog(GFXcanvas1& c, unsigned long ph) {
    int shift = (int)((ph / 50u) % 14u);
    for (int row = 0; row < 5; row++) {
        int y  = 6 + row * 7;
        int rs = (shift + row * 4) % 14;
        for (int x = -rs; x < 40; x += 14)
            c.fillRect(x, y, 10, 2, 1);
    }
}

static void rDrizzle(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx = triWave(ph,        2, 4300u);
    int pm = triWave(ph + 700u, 1, 2400u);
    int cx = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols   = 1 + intensity, gap = 9 - intensity, x0 = cx - cols * gap / 2;
    int period = 1400 - intensity * 200;  // 1200 light · 1000 moderate · 800 heavy
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * gap;
        fallingCol(c, x, 25, 39, 2, period, ph,
                   [](GFXcanvas1& cv, int fx, int fy){ cv.drawPixel(fx, fy, 1); });
    }
}

static void rRain(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx    = triWave(ph,        2, 4600u);
    int pm    = triWave(ph + 600u, 1, 2000u);
    int cx    = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols   = 2 + intensity, gap = 7 - intensity, dropL = 2 + intensity;
    int x0     = cx - cols * gap / 2;
    int period = 900 - intensity * 150;  // 750 light · 600 moderate · 450 heavy
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * gap;
        fallingCol(c, x, 25, 39 - dropL, 2, period, ph,
                   [dropL](GFXcanvas1& cv, int fx, int fy){ drop(cv, fx, fy, dropL); });
    }
}

static void rFreezingRain(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx    = triWave(ph,        2, 4600u);
    int pm    = triWave(ph + 600u, 1, 2000u);
    int cx    = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols  = 2 + intensity, gap = 7 - intensity, dropL = 2 + intensity;
    int x0    = cx - cols * gap / 2;
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * gap;
        fallingCol(c, x, 25, 39 - dropL, 2, 450, ph,
                   [dropL](GFXcanvas1& cv, int fx, int fy){ drop(cv, fx, fy, dropL); });
    }
    int crystalCount = intensity + 1, spacing = (crystalCount == 1) ? 0 : 12;
    int cx0 = cx - (crystalCount - 1) * spacing / 2;
    for (int i = 0; i < crystalCount; i++)
        iceCrystal(c, cx0 + i * spacing, 35);
}

static void rSnow(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx  = triWave(ph,        2, 4900u);
    int pm  = triWave(ph + 800u, 1, 2200u);
    int cx  = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols = 1 + intensity, gap = 10 - intensity, x0 = cx - cols * gap / 2;
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * gap + (i % 2) * 2;
        fallingCol(c, x, 25, 38, 2, 1600, ph,
                   [](GFXcanvas1& cv, int fx, int fy){ flake(cv, fx, fy); });
    }
}

static void rShowers(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx  = triWave(ph,        2, 4200u);
    int pm  = triWave(ph + 500u, 1, 1900u);
    int cx  = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols = 2 + intensity, gap = 6 - intensity, x0 = cx - cols * gap / 2;
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * gap;
        fallingCol(c, x, 25, 37, 2, 380, ph,
                   [](GFXcanvas1& cv, int fx, int fy){
                       if (fy + 4 < 40) cv.drawLine(fx, fy, fx + 1, fy + 4, 1);
                   });
    }
}

static void rSnowShowers(GFXcanvas1& c, unsigned long ph, int intensity) {
    int dx  = triWave(ph,        2, 5100u);
    int pm  = triWave(ph + 700u, 1, 2100u);
    int cx  = 20 + dx;
    cloud(c, cx, 10, pm);
    int cols = 2 + intensity, x0 = cx - cols * 8 / 2;
    for (int i = 0; i < cols; i++) {
        int x = x0 + i * 8 + (i % 2) * 2;
        fallingCol(c, x, 25, 38, 2, 1300, ph,
                   [](GFXcanvas1& cv, int fx, int fy){ flake(cv, fx, fy); });
    }
}

static void rThunderstorm(GFXcanvas1& c, unsigned long ph, int hailLevel) {
    int dx = triWave(ph,        2, 3900u);
    int pm = triWave(ph + 500u, 1, 1600u);
    cloud(c, 20 + dx, 10, pm);

    // Original double-line animation:
    //   lit  → two parallel lines per segment (bold)
    //   dark → single lines (thin)
    int  t   = (int)(ph % 1400u);
    bool lit = (t < 140) || (t > 280 && t < 400);

    if (lit) {
        c.drawLine(21+dx, 22, 15+dx, 30, 1); c.drawLine(22+dx, 22, 16+dx, 30, 1);
        c.fillRect(14+dx, 30, 10, 2, 1);
        c.drawLine(22+dx, 30, 16+dx, 38, 1); c.drawLine(23+dx, 30, 17+dx, 38, 1);
    } else {
        c.drawLine(21+dx, 22, 15+dx, 30, 1);
        c.fillRect(14+dx, 30, 10, 1, 1);
        c.drawLine(22+dx, 30, 16+dx, 38, 1);
    }

    if (hailLevel > 0) {
        int cols = 1 + hailLevel, x0 = 6 + dx;
        for (int i = 0; i < cols; i++) {
            int x = x0 + i * 11;
            fallingCol(c, x, 25, 36, 1, 800, ph,
                       [](GFXcanvas1& cv, int fx, int fy){ pellet(cv, fx, fy); });
        }
    }
}

// ── public dispatcher ──────────────────────────────────────────────────────

void WeatherAnim::draw(GFXcanvas1& canvas, int weatherCode, bool isDay,
                       unsigned long phaseMs) const {
    canvas.fillScreen(0);
    switch (weatherCode) {
        case  0:           rClear(canvas, isDay, phaseMs);        break;
        case  1:           rMainlyClear(canvas, isDay, phaseMs);  break;
        case  2:           rPartlyCloudy(canvas, isDay, phaseMs); break;
        case  3:           rOvercast(canvas, phaseMs);             break;
        case 45: case 48:  rFog(canvas, phaseMs);                 break;
        case 51:           rDrizzle(canvas, phaseMs, 1);          break;
        case 53:           rDrizzle(canvas, phaseMs, 2);          break;
        case 55:           rDrizzle(canvas, phaseMs, 3);          break;
        case 56:           rFreezingRain(canvas, phaseMs, 1);     break;
        case 57:           rFreezingRain(canvas, phaseMs, 2);     break;
        case 61:           rRain(canvas, phaseMs, 1);             break;
        case 63:           rRain(canvas, phaseMs, 2);             break;
        case 65:           rRain(canvas, phaseMs, 3);             break;
        case 66:           rFreezingRain(canvas, phaseMs, 1);     break;
        case 67:           rFreezingRain(canvas, phaseMs, 2);     break;
        case 71:           rSnow(canvas, phaseMs, 1);             break;
        case 73:           rSnow(canvas, phaseMs, 2);             break;
        case 75:           rSnow(canvas, phaseMs, 3);             break;
        case 77:           rSnow(canvas, phaseMs, 1);             break;
        case 80:           rShowers(canvas, phaseMs, 1);          break;
        case 81:           rShowers(canvas, phaseMs, 2);          break;
        case 82:           rShowers(canvas, phaseMs, 3);          break;
        case 85:           rSnowShowers(canvas, phaseMs, 1);      break;
        case 86:           rSnowShowers(canvas, phaseMs, 2);      break;
        case 95:           rThunderstorm(canvas, phaseMs, 0);     break;
        case 96:           rThunderstorm(canvas, phaseMs, 1);     break;
        case 99:           rThunderstorm(canvas, phaseMs, 2);     break;
        default: break;  // unknown code — leave canvas blank
    }
}
