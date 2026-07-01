#include "Eye.h"
#include "GUI_Utils.h"
#include <math.h>

namespace Eye {

static const int MOODS_PER_CYCLE = 3;

// ── socket geometry ───────────────────────────────────────────────────────────

static const int CX    = 50;
static const int CY    = 22;
static const int RR_W  = 36;
static const int RR_H  = 18;
static const int RR_RC = 10;

// ── EyeParams ─────────────────────────────────────────────────────────────────

struct EyeParams {
    int upBase, upLeft, upRight;
    int loBase, loLeft, loRight;
    int pupilCX, pupilCY, pupilR;
    int sockTopLeft, sockTopRight;
    int sockBotLeft, sockBotRight;
};

// ── socket bound (rounded-rect / trapezoid) ───────────────────────────────────

static bool socketBound(int x, const EyeParams& p, int* yTop, int* yBot) {
    int lx    = CX - RR_W;
    int rx    = CX + RR_W;
    int flatL = lx + RR_RC;
    int flatR = rx - RR_RC;
    if (x < lx || x > rx) return false;

    float t      = (float)(x - lx) / (float)(2 * RR_W);
    int   topOff = (int)(p.sockTopLeft * (1.0f - t) + p.sockTopRight * t + 0.5f);
    int   botOff = (int)(p.sockBotLeft * (1.0f - t) + p.sockBotRight * t + 0.5f);

    if (x >= flatL && x <= flatR) {
        *yTop = CY - RR_H + topOff;
        *yBot = CY + RR_H - botOff;
    } else {
        int cx_c = (x < flatL) ? flatL : flatR;
        int dx   = x - cx_c;
        int r2   = RR_RC * RR_RC - dx * dx;
        if (r2 < 0) return false;
        int ry  = (int)sqrtf((float)r2);
        *yTop   = CY - RR_H + RR_RC - ry + topOff;
        *yBot   = CY + RR_H - RR_RC + ry - botOff;
    }
    if (*yTop < 0)         *yTop = 0;
    if (*yBot > LCD_H - 1) *yBot = LCD_H - 1;
    return (*yTop <= *yBot);
}

// ── eyelid & render ───────────────────────────────────────────────────────────

static int lidHeight(int x, int base, int leftW, int rightW) {
    float t = (float)(x - (CX - RR_W)) / (float)(2 * RR_W);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return base + (int)(leftW * (1-t)*(1-t) + rightW * t*t + 0.5f);
}

static void renderEye(const EyeParams& p) {
    clearCanvas();
    for (int x = CX - RR_W; x <= CX + RR_W && x < LCD_W; x++) {
        int sTop, sBot;
        if (!socketBound(x, p, &sTop, &sBot)) continue;
        lcd->drawLine(x, sTop, x, sBot, GLCD_COLOR_SET);

        int upH = lidHeight(x, p.upBase, p.upLeft, p.upRight);
        int loH = lidHeight(x, p.loBase, p.loLeft, p.loRight);
        int upY = sTop + upH;
        int loY = sBot  - loH;
        if (upY > sBot) upY = sBot;
        if (loY < sTop) loY = sTop;

        if (upY < loY)
            lcd->drawLine(x, upY, x, loY, GLCD_COLOR_CLEAR);

        int pdx = x - p.pupilCX;
        int pr2 = p.pupilR * p.pupilR - pdx * pdx;
        if (pr2 >= 0 && upY < loY) {
            int pry  = (int)sqrtf((float)pr2);
            int pTop = p.pupilCY - pry;
            int pBot = p.pupilCY + pry;
            if (pTop < upY + 1) pTop = upY + 1;
            if (pBot > loY - 1) pBot = loY - 1;
            if (pTop <= pBot)
                lcd->drawLine(x, pTop, x, pBot, GLCD_COLOR_SET);
        }
    }
    lcd->display();
}

// ── wander helpers ────────────────────────────────────────────────────────────

static int triWave(unsigned long ph, int amp, unsigned long period) {
    int t    = (int)(ph % period);
    int half = (int)(period >> 1);
    return (t < half) ? (-amp + t * (2*amp) / half)
                      : ( amp - (t-half) * (2*amp) / half);
}

static void wander(unsigned long ph,
                   int ampX, unsigned long pX1, unsigned long pX2,
                   int ampY, unsigned long pY1, unsigned long pY2,
                   int* offX, int* offY) {
    *offX = triWave(ph,          ampX,     pX1)
          + triWave(ph+pX1/2,   ampX/2,   pX2);
    *offY = triWave(ph,          ampY,     pY1)
          + triWave(ph+pY1/2,   ampY/2,   pY2);
}

// ── mood table ────────────────────────────────────────────────────────────────

enum Mood {
    NEUTRAL = 0, HAPPY, SAD, EXCITED, EVIL, ANGRY, SLEEPY, SURPRISED, CONFUSED,
    SCARED, SMUG, NERVOUS, LOVE, WINK, BORED,
    MOOD_COUNT
};

//                                upBas upL upR  loBas loL loR  pCX  pCY  pR  sTL sTR sBL sBR
static const EyeParams BASE[] = {
/* NEUTRAL   */ {  7,  0,  0,    5,  0,  0,   CX,  CY,  8,   0,  0,  0,  0 },
/* HAPPY     */ { 15,  0,  0,   13,  0,  0,   CX,  CY,  6,   0,  0,  0,  0 },
/* SAD       */ {  7,  0,  9,    5,  0,  9,   CX,CY+1,  7,   0,  5,  0,  0 },
/* EXCITED   */ {  2,  0,  0,    2,  0,  0,   CX,  CY, 13,   0,  0,  0,  0 },
/* EVIL      */ {  7, 10,  0,    4,  0,  0,   CX,  CY,  6,   8,  0,  0,  0 },
/* ANGRY     */ {  7,  9,  8,    4,  0,  0,   CX,  CY,  5,   6,  3,  0,  0 },
/* SLEEPY    */ { 18,  0,  0,    5,  0,  0,   CX,  CY,  5,   0,  0,  0,  0 },
/* SURPRISED */ {  2,  0,  0,    2,  0,  0,   CX,  CY, 13,   0,  0,  0,  0 },
/* CONFUSED  */ {  7,  7,  0,    5,  2,  0,   CX,CY-1,  7,   3,  0,  0,  0 },
/* SCARED    */ {  1,  0,  0,    1,  0,  0,   CX,  CY,  4,   0,  0,  0,  0 },
/* SMUG      */ { 10,  0,  5,    3,  0,  0,  CX+4, CY,  7,   0,  0,  3,  0 },
/* NERVOUS   */ {  8,  0,  0,    4,  0,  0,   CX,  CY,  7,   0,  0,  0,  0 },
/* LOVE      */ { 11,  0,  0,    8,  0,  0,   CX,CY-2, 13,   2,  2,  0,  0 },
/* WINK      */ {  0,  0,  0,    0,  0,  0,   CX,  CY,  8,   0,  0,  0,  0 },
/* BORED     */ { 12,  0,  0,    3,  0,  0,   CX,CY+2,  6,   3,  3,  0,  0 },
};

static EyeParams animate(Mood mood, unsigned long ph) {
    EyeParams p = BASE[mood];
    int wX = 0, wY = 0;

    switch (mood) {
    case NEUTRAL:
        p.upBase += triWave(ph, 1, 4200u);
        wander(ph, 8, 2300u, 1700u, 3, 3100u, 2000u, &wX, &wY);
        break;
    case HAPPY: {
        int sq = triWave(ph, 2, 1800u);
        p.upBase += sq;  p.loBase += sq;
        wander(ph, 4, 2800u, 1900u, 2, 3400u, 2300u, &wX, &wY);
        wY -= 1;
        break;
    }
    case SAD:
        p.upRight += triWave(ph, 3, 4600u);
        p.loRight += triWave(ph, 2, 4600u);
        wander(ph, 5, 4000u, 2900u, 2, 5000u, 3500u, &wX, &wY);
        wY += 2;
        p.pupilCY = CY + 1 + triWave(ph, 1, 3700u);
        break;
    case EXCITED: {
        int pulse = triWave(ph, 4, 420u);
        p.pupilR += pulse;
        p.upBase = 2 - (pulse > 2 ? 1 : 0);
        p.loBase = 2 - (pulse > 2 ? 1 : 0);
        wander(ph, 12, 870u, 610u, 5, 1100u, 730u, &wX, &wY);
        break;
    }
    case EVIL:
        p.upLeft += triWave(ph, 3, 3400u);
        wander(ph, 6, 5200u, 3900u, 1, 6000u, 4100u, &wX, &wY);
        break;
    case ANGRY: {
        int q = triWave(ph, 3, 650u);
        p.upLeft += q;  p.upRight += q;
        wander(ph, 3, 2800u, 2100u, 1, 3200u, 2400u, &wX, &wY);
        break;
    }
    case SLEEPY: {
        unsigned long t = ph % 5000u;
        if      (t < 3000u) p.upBase = 18 + (int)(t * 5 / 3000u);
        else if (t < 4500u) p.upBase = 23;
        else                p.upBase = 23 - (int)((t-4500u) * 5 / 500u);
        p.pupilR = 5 - (p.upBase > 20 ? 2 : 0);
        int close = (p.upBase > 18) ? (p.upBase - 18) : 0;
        p.sockTopLeft = p.sockTopRight = close * 2;
        p.sockBotLeft = p.sockBotRight = close;
        wander(ph, 3, 5500u, 4000u, 1, 6500u, 4800u, &wX, &wY);
        break;
    }
    case SURPRISED: {
        int fl = triWave(ph, 3, 480u);
        p.pupilR += fl;
        p.upBase = 2 - (fl > 1 ? 1 : 0);
        p.loBase = 2 - (fl > 1 ? 1 : 0);
        wander(ph, 11, 720u, 510u, 5, 860u, 620u, &wX, &wY);
        break;
    }
    case CONFUSED:
        p.upLeft += triWave(ph, 4, 2200u);
        wander(ph, 9, 1400u, 970u, 4, 1700u, 1200u, &wX, &wY);
        wY -= 1;
        break;
    case SCARED: {
        unsigned long ts = ph % 3200u;
        if (ts < 400u) { wX = 0; wY = 0; }
        else           wander(ph, 7, 580u, 370u, 4, 690u, 430u, &wX, &wY);
        p.upBase += triWave(ph, 1, 200u);
        break;
    }
    case SMUG:
        p.upRight += triWave(ph, 2, 5600u);
        wander(ph, 5, 6200u, 4500u, 1, 7100u, 5300u, &wX, &wY);
        wX += 3;
        break;
    case NERVOUS: {
        unsigned long tn = ph % 2200u;
        if      (tn < 80u)  { wX = -9; wY = -3; }
        else if (tn < 160u) { wX =  9; wY =  2; }
        else                wander(ph, 7, 480u, 310u, 3, 590u, 390u, &wX, &wY);
        p.upBase += triWave(ph, 2, 250u);
        int jitter = triWave(ph, 1, 120u);
        p.sockTopLeft  += jitter;
        p.sockTopRight += jitter;
        break;
    }
    case LOVE: {
        int pulse = triWave(ph, 2, 1600u);
        p.pupilR += pulse;
        p.upBase += triWave(ph, 1, 2500u);
        p.loBase += triWave(ph, 1, 2500u);
        wander(ph, 3, 3900u, 2800u, 2, 4300u, 3100u, &wX, &wY);
        wY -= 2;
        break;
    }
    case WINK: {
        unsigned long tw = ph % 4000u;
        int cr = 0;
        if      (tw < 180u)  cr = (int)(tw * 30 / 180u);
        else if (tw < 800u)  cr = 30;
        else if (tw < 980u)  cr = 30 - (int)((tw-800u) * 30 / 180u);
        p.upRight = cr;
        p.loRight = cr / 4;
        wander(ph, 5, 3200u, 2300u, 2, 3800u, 2700u, &wX, &wY);
        break;
    }
    case BORED: {
        p.upBase += triWave(ph, 2, 6200u);
        wander(ph, 4, 7200u, 5100u, 2, 8300u, 6100u, &wX, &wY);
        wY += 2;
        unsigned long tb = ph % 7000u;
        if (tb < 700u) {
            int bc = (tb < 350u) ? (int)(tb * 16 / 350u)
                                 : (int)((700u-tb) * 16 / 350u);
            if (bc > p.upBase) p.upBase = bc;
        }
        break;
    }
    default: break;
    }

    p.pupilCX = CX + wX;
    if (p.pupilCX < CX - (RR_W - RR_RC - p.pupilR))
        p.pupilCX = CX - (RR_W - RR_RC - p.pupilR);
    if (p.pupilCX > CX + (RR_W - RR_RC - p.pupilR))
        p.pupilCX = CX + (RR_W - RR_RC - p.pupilR);
    p.pupilCY += wY;

    if (p.upBase      < 0)  p.upBase      = 0;
    if (p.loBase      < 0)  p.loBase      = 0;
    if (p.upLeft      < 0)  p.upLeft      = 0;
    if (p.upRight     < 0)  p.upRight     = 0;
    if (p.loLeft      < 0)  p.loLeft      = 0;
    if (p.loRight     < 0)  p.loRight     = 0;
    if (p.pupilR      < 2)  p.pupilR      = 2;
    if (p.pupilR      > 16) p.pupilR      = 16;
    if (p.sockTopLeft  < 0) p.sockTopLeft  = 0;
    if (p.sockTopRight < 0) p.sockTopRight = 0;
    if (p.sockBotLeft  < 0) p.sockBotLeft  = 0;
    if (p.sockBotRight < 0) p.sockBotRight = 0;

    return p;
}

static EyeParams applyBlink(EyeParams p, unsigned long ph) {
    unsigned long t = ph % 4000u;
    if (t >= 200u) return p;
    int closeH;
    if      (t <  70u) closeH = (int)(t * 22 / 70u);
    else if (t < 130u) closeH = 22;
    else               closeH = 22 - (int)((t-130u) * 22 / 70u);
    if (closeH > p.upBase) {
        p.upBase  = closeH;
        p.loBase += closeH / 5;
        p.pupilR  = p.pupilR * (22 - closeH) / 22;
    }
    return p;
}

// ── module state ──────────────────────────────────────────────────────────────

static const char* MOOD_NAME[] = {
    "Neutral","Happy","Sad","Excited","Evil","Angry","Sleepy",
    "Surprised","Confused","Scared","Smug","Nervous","Love","Wink","Bored"
};

static Mood          _mood      = NEUTRAL;
static unsigned long _moodStart = 0;
static int           _moodsDone = 0;

const char* moodName() { return MOOD_NAME[(int)_mood]; }

void init(unsigned long now) {
    _mood      = NEUTRAL;
    _moodStart = now;
    _moodsDone = 0;
    Serial.println("[eye] init");
}

bool tick(unsigned long now) {
    if (now - _moodStart >= 5000u) {
        _mood      = (Mood)((int(_mood) + 1) % MOOD_COUNT);
        _moodStart = now;
        _moodsDone++;
        Serial.printf("[eye] mood %d\n", (int)_mood);
    }
    EyeParams p = animate(_mood, now - _moodStart);
    p = applyBlink(p, now - _moodStart);
    renderEye(p);
    delay(33);
    return (_moodsDone >= MOODS_PER_CYCLE);
}

} // namespace Eye
