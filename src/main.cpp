// Emmie Lyn nameplate: cyd40 (Freenove ESP32 Display 4.0", ST7796 480x320, no PSRAM).
// Title cycles color every 5s; below it two kittens take turns batting a ball of yarn,
// drawn the same way as the Fish Tank's fish: ellipses + triangles, no bitmap assets.
#include <Arduino.h>
#include <TFT_eSPI.h>  // with LOAD_GFXFF, this already pulls in the bundled Custom fonts (Yellowtail_32 etc.)
#include <math.h>

#define AMP_EN 4  // speaker amp enable, active LOW; held HIGH (off) here

static inline uint16_t rgb(int r, int g, int b) {
  r = constrain(r, 0, 255); g = constrain(g, 0, 255); b = constrain(b, 0, 255);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline uint16_t shade(int r, int g, int b, int delta) { return rgb(r + delta, g + delta, b + delta); }

static const uint16_t BG_NAVY = rgb(22, 32, 68);  // dark navy, used instead of plain black

TFT_eSPI tft;
TFT_eSprite band(&tft);     // one strip sprite for the whole animated scene (no PSRAM, so keep it modest)
TFT_eSprite marquee(&tft);  // a thin sprite across the very top for the scrolling "Merry Christmas"

// ---------------------------------------------------------------- "Merry Christmas" marquee
static const int MARQUEE_H = 70;   // a tall strip across the very top for the (now bigger) scroller
static const float MARQUEE_SPEED = 60.0f;

// "Merry Christmas EVERYONE!!!!!!!!" scrolling across the very top, right to left, on a
// loop, at 2x text size - two colors in the one phrase, "EVERYONE" trailing right after.
static const char *MERRY_PART1 = "Merry    Christmas    ";
static const char *MERRY_PART2 = "EVERYONE!!!!!!!!";
static float merryWidth1 = 0, merryWidth = 0;
static const float SANTA_GAP = 36;       // a little breathing room before the sleigh train
static const float SANTA_GROUP_W = 170;  // reindeer + sleigh train riding right behind the text

// Three reindeer (Rudolph leading) pulling Santa's sleigh, drawn at the given scroll x.
static void drawSantaTrain(float xBase, float t) {
  uint16_t reindeer = rgb(120, 80, 50), antler = rgb(90, 60, 40), red = rgb(200, 30, 40);
  uint16_t skin = rgb(230, 190, 160), white = TFT_WHITE, runner = rgb(80, 80, 90);
  float baseY = MARQUEE_H - 14;
  for (int i = 0; i < 3; i++) {
    float rx = xBase + i * 38;
    marquee.drawLine(rx - 5, baseY, rx - 5, baseY + 10, reindeer);
    marquee.drawLine(rx + 5, baseY, rx + 5, baseY + 10, reindeer);
    marquee.fillCircle((int)rx - 5, (int)baseY + 10, 1, rgb(40, 30, 25));  // hoof
    marquee.fillCircle((int)rx + 5, (int)baseY + 10, 1, rgb(40, 30, 25));  // hoof
    marquee.fillEllipse((int)rx, (int)baseY - 6, 11, 7, reindeer);
    marquee.drawLine(rx + 9, baseY - 9, rx + 13, baseY - 5, reindeer);     // little tail
    marquee.fillCircle((int)rx - 12, (int)baseY - 10, 5, reindeer);
    marquee.drawLine(rx - 14, baseY - 14, rx - 18, baseY - 22, antler);
    marquee.drawLine(rx - 10, baseY - 14, rx - 8, baseY - 23, antler);
    marquee.drawLine(rx - 17, baseY - 19, rx - 19, baseY - 17, antler);   // antler fork
    marquee.drawLine(rx - 9, baseY - 20, rx - 6, baseY - 19, antler);     // antler fork
    marquee.fillCircle((int)rx - 15, (int)baseY - 11, 1, rgb(20, 20, 20));  // eye
    if (i == 0) marquee.fillCircle((int)rx - 17, (int)baseY - 10, 2, red);  // Rudolph's nose
    marquee.fillCircle((int)rx + 2, (int)baseY - 11, 1, rgb(220, 180, 60));  // a little harness bell
  }
  float sx = xBase + 3 * 38 + 10;
  // reins linking each reindeer to the next, and the last one to the sleigh's front
  uint16_t reinColor = rgb(70, 50, 35);
  for (int i = 0; i < 3; i++) {
    float rx = xBase + i * 38;
    float nextRx = (i < 2) ? xBase + (i + 1) * 38 - 12 : sx;
    marquee.drawLine(rx + 11, baseY - 6, nextRx, baseY - 6, reinColor);
  }
  uint16_t gold = rgb(220, 180, 60);
  marquee.fillRoundRect((int)sx, (int)baseY - 16, 34, 16, 4, red);
  marquee.drawLine(sx + 2, baseY - 14, sx + 32, baseY - 14, gold);   // gold trim
  marquee.drawLine(sx + 2, baseY - 2, sx + 32, baseY - 2, gold);     // gold trim
  marquee.drawLine(sx - 2, baseY, sx + 36, baseY, runner);
  marquee.drawLine(sx - 2, baseY + 1, sx + 36, baseY + 1, runner);   // thicker runner
  marquee.drawLine(sx + 36, baseY, sx + 41, baseY - 4, runner);      // curled front of the runner
  marquee.drawLine(sx + 41, baseY - 4, sx + 41, baseY - 8, runner);

  // Santa's bag of toys/presents, tucked in the back of the sleigh
  uint16_t sack = rgb(100, 65, 40), sackTie = rgb(60, 40, 25);
  marquee.fillCircle((int)sx + 6, (int)baseY - 13, 7, sack);
  marquee.fillCircle((int)sx + 5, (int)baseY - 19, 4, sack);
  marquee.drawLine(sx + 2, baseY - 21, sx + 8, baseY - 21, sackTie);

  float absS = 1.0f, sign = 1.0f;
  float bodyCY = baseY - 24;
  float headCY = bodyCY + (-8.0f) * absS * sign;

  marquee.fillEllipse((int)sx + 14, (int)bodyCY, 7, (int)(7 * absS), red);
  marquee.fillRect((int)sx + 11, (int)(bodyCY + 3 * absS * sign), 6, 2, rgb(40, 30, 25));   // belt
  marquee.fillCircle((int)sx + 14, (int)(bodyCY + 3 * absS * sign), 1, rgb(220, 180, 60));  // buckle
  marquee.fillEllipse((int)sx + 14, (int)headCY, 5, (int)(5 * absS), skin);
  marquee.fillEllipse((int)sx + 14, (int)(headCY + 4 * absS * sign), 4, (int)(3 * absS), white);  // beard
  float hatTopY = bodyCY + (-12.0f) * absS * sign;
  float hatBaseY = bodyCY + (-4.0f) * absS * sign;
  marquee.fillTriangle(sx + 8, hatBaseY, sx + 20, hatBaseY, sx + 14, hatTopY, red);
  marquee.fillRect((int)sx + 7, (int)(hatBaseY - 2 * sign), 14, 2, white);  // hat brim trim
  marquee.fillCircle((int)sx + 14, (int)hatTopY, 2, white);

  // a little speech bubble above Santa's hat
  float bx = sx + 34, by = 10;
  marquee.fillRoundRect((int)bx - 22, (int)by - 8, 44, 16, 5, white);
  marquee.fillTriangle(bx - 8, by + 6, bx + 2, by + 6, bx - 2, bodyCY - 10, white);
  marquee.setTextColor(TFT_BLACK, white);
  marquee.setTextDatum(MC_DATUM);
  marquee.drawString("HoHoHo", (int)bx, (int)by, 1);
  marquee.setTextDatum(ML_DATUM);
}

// Same letter-by-letter shade-hopping trick as the names, but left-anchored (for text
// that scrolls) instead of centered, and mixing pink and purple instead of one cat's colors.
static void drawTieDyeWordMarquee(const char *text, float xStart, float y, int phaseStart) {
  const int nShades = 5;
  const int order[nShades] = { 0, 3, 1, 4, 2 };
  const int shades[nShades][3] = {
    { 210, 30, 40 },    // christmas red
    { 40, 140, 65 },    // christmas green
    { 20, 100, 50 },    // deep christmas green
    { 245, 248, 250 },  // festive white
    { 150, 20, 30 },    // deep red
  };
  float x = xStart;
  int len = strlen(text);
  for (int i = 0; i < len; i++) {
    char buf[2] = { text[i], 0 };
    int w = marquee.textWidth(buf);
    const int *c = shades[order[(i + phaseStart) % nShades]];
    marquee.setTextColor(rgb(c[0], c[1], c[2]), BG_NAVY);
    marquee.drawString(buf, (int)x, (int)y);
    x += w;
  }
}

static void drawMarquee(float t) {
  float cycle = marquee.width() + merryWidth + SANTA_GAP + SANTA_GROUP_W;
  float x = marquee.width() - fmodf(t * MARQUEE_SPEED, cycle);

  marquee.fillSprite(BG_NAVY);
  marquee.setFreeFont(&Satisfy_24);
  marquee.setTextSize(2);
  marquee.setTextDatum(ML_DATUM);
  drawTieDyeWordMarquee(MERRY_PART1, x, MARQUEE_H / 2 + 2, 0);
  drawTieDyeWordMarquee(MERRY_PART2, x + merryWidth1, MARQUEE_H / 2 + 2, (int)strlen(MERRY_PART1));
  marquee.setFreeFont();
  marquee.setTextSize(1);
  drawSantaTrain(x + merryWidth + SANTA_GAP, t);
  for (int i = 0; i < 20; i++) {
    float tw = sinf(t * 8.0f + i * 1.9f);
    if (tw < 0.2f) continue;
    int px = (int)fmodf(i * 37.0f + t * 20.0f, marquee.width());
    int py = 3 + (i * 5) % (MARQUEE_H - 6);
    marquee.drawPixel(px, py, TFT_WHITE);
  }
  marquee.setFreeFont();
  marquee.setTextSize(1);
  marquee.setTextDatum(TL_DATUM);
  marquee.pushSprite(0, 0);
}

// ---------------------------------------------------------------- kitten + yarn scene
// Classic ESP32 (no PSRAM) can't do one big contiguous sprite for the whole scene (same
// lesson as Fish Tank/Deer Hunt's strip buffers), so the frame is drawn in two short
// horizontal strips reusing one small sprite, each pushed to its own row on the panel.
static const int STRIP_H = 80;              // 480x80x2 bytes = 75 KB per strip (one reused buffer)
static const int NUM_STRIPS = 3;            // extra strip opens room for the names + heart above the cats
static const int SCENE_Y0 = MARQUEE_H;      // panel row where strip 0 starts, right under the marquee
static int curOY = 0;                       // panel row the CURRENT strip's row 0 sits at
static inline int BY(float worldY) { return (int)lroundf(worldY) - curOY; }

static const float GROUND_Y = 272;      // world y of the floor line
static const float CAT_BASE_Y = 268;    // world y where paws sit
static const float CAT1_X = 128, CAT2_X = 352;  // left/right kitten centers

static float ballX = 240, ballVX = 0;
static const float BALL_Y = 236;
static const float BALL_MIN_X = CAT1_X + 78, BALL_MAX_X = CAT2_X - 78;

// one continuous 1.6s cycle: first half left kitten swipes, second half right kitten swipes
static const float SWIPE_PERIOD = 1.6f;
static bool impulseGivenThisHalf = false;

static void drawKitten(float cx, int dir, int r0, int g0, int b0, int rs, int gs, int bs,
                        float tailPhase, float pawReach, float t, float sparklePhase,
                        bool bowTie, int tieR, int tieG, int tieB,
                        bool hairBowOn, int hbR, int hbG, int hbB, bool tuxedo) {
  uint16_t body = rgb(r0, g0, b0);
  uint16_t light = shade(r0, g0, b0, 55);     // highlight / belly / cheeks / muzzle
  uint16_t dark = shade(r0, g0, b0, -60);     // shadow side / pupil-lid / toe lines
  uint16_t stripe = rgb(rs, gs, bs);          // the Cheshire Cat's contrasting stripe color
  uint16_t pink = rgb(235, 150, 160);

  auto fy = [&](float worldY) { return BY(worldY); };

  // tail: a smooth, plush, tapering swishing curve - tightly-overlapping circles with
  // no tufts or banding, so it reads as one solid tail rather than a string of segments
  for (int i = 0; i <= 24; i++) {
    float s = i / 24.0f;
    float px = cx - dir * (26 + 36 * s);
    float py = CAT_BASE_Y - 34 - 26 * sinf(s * 1.4f) + 6 * sinf(tailPhase + s * 2.6f) * s;
    int r = (int)lroundf(13 - 6 * s);
    band.fillCircle((int)px, fy(py), r, body);
  }
  // squiggly stripe marks all over the tail, instead of dots or rings straight across
  for (int m = 0; m < 7; m++) {
    float s0 = 0.08f + m * 0.12f;
    float prevX = 0, prevY = 0;
    for (int j = 0; j <= 4; j++) {
      float s = s0 + (j - 2) * 0.03f;
      float tx = cx - dir * (26 + 36 * s);
      float ty = CAT_BASE_Y - 34 - 26 * sinf(s * 1.4f) + 6 * sinf(tailPhase + s * 2.6f) * s;
      float wig = sinf(j * 2.2f + m * 1.7f) * 4.0f;
      float px = tx + wig, py = ty;
      if (j > 0) band.drawWideLine(prevX, fy(prevY), px, fy(py), 2, stripe, stripe);
      prevX = px; prevY = py;
    }
  }

  // body: base shape, a shadowed flank, a lit flank, pale belly
  band.fillEllipse((int)cx, fy(CAT_BASE_Y - 40), 31, 40, body);
  band.fillEllipse((int)cx - dir * 12, fy(CAT_BASE_Y - 46), 21, 35, dark);
  band.fillEllipse((int)cx + dir * 9, fy(CAT_BASE_Y - 50), 17, 29, light);
  band.fillEllipse((int)cx + dir * 4, fy(CAT_BASE_Y - 14), 16, 21, light);
  // wide Cheshire stripes curving over the back (calico markings - not for a tuxedo)
  if (!tuxedo) {
    for (int i = 0; i < 4; i++) {
      float sx = cx - dir * (20 - i * 12);
      band.drawWideLine(sx, fy(CAT_BASE_Y - 82), sx - dir * 6, fy(CAT_BASE_Y - 58), 5, stripe, stripe);
    }
    // a bigger patch over the haunch, like a calico's bolder markings
    band.fillEllipse((int)(cx - dir * 20), fy(CAT_BASE_Y - 30), 16, 20, stripe);
    band.fillEllipse((int)(cx - dir * 28), fy(CAT_BASE_Y - 14), 10, 12, stripe);
  }
  // a tuxedo's clean white chest bib, extended down into a white belly
  if (tuxedo) {
    band.fillEllipse((int)(cx + dir * 4), fy(CAT_BASE_Y - 20), 17, 30, TFT_WHITE);
    band.fillEllipse((int)(cx + dir * 2), fy(CAT_BASE_Y + 4), 12, 10, TFT_WHITE);
  }
  // fur texture along the body's edge and a fluffy chest ruff
  for (int i = 0; i < 10; i++) {
    float fx = cx - dir * 32 + dir * i * 7;
    float fy0 = CAT_BASE_Y - 14 + (i % 3) * 11;
    band.drawLine(fx, fy(fy0), fx + dir * 3, fy(fy0 - 5), (i % 2 == 0) ? dark : light);
  }
  if (!tuxedo) {
    for (int i = 0; i < 5; i++) {
      float fx = cx + dir * (2 - i) * 4;
      band.drawLine(fx, fy(CAT_BASE_Y - 18), fx, fy(CAT_BASE_Y - 26), light);
    }
  }
  // front paws with toe lines and little pads (tuxedo cats get clean white "mittens")
  for (int side = -1; side <= 1; side += 2) {
    int px = (int)(cx + side * 14);
    band.fillEllipse(px, fy(CAT_BASE_Y + 2), 10, 14, tuxedo ? TFT_WHITE : body);
    band.drawLine(px - 5, fy(CAT_BASE_Y + 10), px - 5, fy(CAT_BASE_Y + 15), dark);
    band.drawLine(px, fy(CAT_BASE_Y + 11), px, fy(CAT_BASE_Y + 16), dark);
    band.drawLine(px + 5, fy(CAT_BASE_Y + 10), px + 5, fy(CAT_BASE_Y + 15), dark);
    band.fillCircle(px, fy(CAT_BASE_Y + 6), 2, dark);  // paw pad
  }

  // head: slightly oval, with cheek fluff and a lit/shadow split
  float headX = cx + dir * 8;
  float headY = CAT_BASE_Y - 86;
  band.fillEllipse((int)headX, fy(headY), 33, 30, body);
  band.fillEllipse((int)headX - dir * 10, fy(headY + 2), 16, 18, dark);
  band.fillEllipse((int)headX + dir * 8, fy(headY - 4), 16, 18, light);
  band.fillEllipse((int)headX - dir * 24, fy(headY + 10), 11, 9, body);   // far cheek fluff
  band.fillEllipse((int)headX + dir * 24, fy(headY + 10), 11, 9, light); // near cheek fluff
  // ears: triangle with a slightly rounded tip and pink inner ear (rounder = cuter)
  for (int side = -1; side <= 1; side += 2) {
    float ex = headX + side * dir * 24;
    band.fillTriangle((int)(headX + side * dir * 10), fy(headY - 8),
                       (int)ex, fy(headY - 36),
                       (int)(headX + side * dir * 26), fy(headY - 4), body);
    band.fillTriangle((int)(headX + side * dir * 13), fy(headY - 12),
                       (int)ex, fy(headY - 27),
                       (int)(headX + side * dir * 21), fy(headY - 9), pink);
  }
  // one black ear and a face patch over that eye, calico-style (not for a tuxedo)
  if (!tuxedo) {
    float ex = headX - dir * 24;
    band.fillTriangle((int)(headX - dir * 10), fy(headY - 8),
                       (int)ex, fy(headY - 36),
                       (int)(headX - dir * 26), fy(headY - 4), stripe);
    band.fillTriangle((int)(headX - dir * 13), fy(headY - 12),
                       (int)ex, fy(headY - 27),
                       (int)(headX - dir * 21), fy(headY - 9), pink);
    band.fillEllipse((int)(headX - dir * 14), fy(headY - 4), 13, 16, stripe);
  }
  // a little hair bow on top of the head, between the ears (only if this kitten wears one)
  if (hairBowOn) {
    uint16_t hairBow = rgb(hbR, hbG, hbB);
    float hbx = headX - dir * 2, hby = headY - 28;
    band.fillTriangle((int)hbx - 1, fy(hby), (int)hbx - 11, fy(hby - 6), (int)hbx - 11, fy(hby + 6), hairBow);
    band.fillTriangle((int)hbx + 1, fy(hby), (int)hbx + 11, fy(hby - 6), (int)hbx + 11, fy(hby + 6), hairBow);
    band.fillCircle((int)hbx, fy(hby), 4, hairBow);
  }
  // muzzle (pale, or clean white chin for a tuxedo) under the nose
  band.fillEllipse((int)headX + dir * 2, fy(headY + 14), 13, 9, tuxedo ? TFT_WHITE : light);
  // blush - bigger and softer for a kawaii look
  band.fillCircle((int)headX - dir * 22, fy(headY + 12), 6, rgb(255, 180, 190));
  band.fillCircle((int)headX + dir * 22, fy(headY + 12), 6, rgb(255, 180, 190));
  // eyes: extra-big round and glossy, with a soft lash - the main "kawaii" feature
  for (int side = -1; side <= 1; side += 2) {
    int ex = (int)(headX + side * 9);
    int ey = (int)fy(headY - 1);
    band.fillCircle(ex, ey, 8, TFT_WHITE);
    band.fillCircle(ex, ey, 7, rgb(90, 60, 50));
    band.fillCircle(ex, ey + 1, 5, TFT_BLACK);
    band.fillCircle(ex - 2, ey - 2, 3, TFT_WHITE);     // big glossy highlight
    band.fillCircle(ex + 2, ey + 2, 1, rgb(230, 230, 230));  // tiny secondary sparkle
    band.drawLine(ex - 7, ey - 7, ex + 1, ey - 9, dark);     // soft upper lash
  }
  // a little twinkling sparkle accent that floats near the ear, kawaii-style
  float tw = sinf(t * 3.0f + sparklePhase);
  if (tw > 0.55f) {
    float spx = headX + dir * 30, spy = headY - 37 + sinf(t * 2.0f + sparklePhase) * 2;
    band.drawLine(spx - 4, fy(spy), spx + 4, fy(spy), TFT_WHITE);
    band.drawLine(spx, fy(spy - 4), spx, fy(spy + 4), TFT_WHITE);
    if (tw > 0.85f) band.fillCircle((int)spx, fy(spy), 1, TFT_WHITE);
  }
  // nose + a gentle smiling mouth
  band.fillTriangle((int)headX - 3, fy(headY + 8), (int)headX + 3, fy(headY + 8),
                     (int)headX, fy(headY + 12), pink);
  band.drawLine(headX, fy(headY + 12), headX - 2, fy(headY + 15), dark);
  band.drawLine(headX - 2, fy(headY + 15), headX - 7, fy(headY + 13), dark);
  band.drawLine(headX, fy(headY + 12), headX + 2, fy(headY + 15), dark);
  band.drawLine(headX + 2, fy(headY + 15), headX + 7, fy(headY + 13), dark);
  // whiskers
  for (int side = -1; side <= 1; side += 2)
    for (int w = -1; w <= 1; w++)
      band.drawLine(headX + side * dir * 12, fy(headY + 13 + w * 3),
                     headX + side * dir * 34, fy(headY + 10 + w * 6), TFT_LIGHTGREY);

  // a little bow tie at the neck, if this kitten wears one
  if (bowTie) {
    uint16_t tie = rgb(tieR, tieG, tieB);
    float by = CAT_BASE_Y - 58;
    band.fillTriangle((int)cx - 2, fy(by), (int)cx - 16, fy(by - 7), (int)cx - 16, fy(by + 7), tie);
    band.fillTriangle((int)cx + 2, fy(by), (int)cx + 16, fy(by - 7), (int)cx + 16, fy(by + 7), tie);
    band.fillCircle((int)cx, fy(by), 5, tie);
  }

  // swiping paw, reaches toward the ball between the cats
  if (pawReach > 0.5f) {
    float px = cx + dir * (20 + pawReach);
    band.fillEllipse((int)px, fy(CAT_BASE_Y - 28), 12, 10, body);
    band.drawLine(px, fy(CAT_BASE_Y - 20), px, fy(CAT_BASE_Y - 15), dark);
  }
}

static const int yarnPalette[][3] = {
  {150, 220, 255},  // sky ice blue
  {210, 225, 235},  // silver frost
  {100, 170, 200},  // steel teal
  {90, 220, 230},   // cyan
  {235, 245, 255},  // frost white
  {60, 90, 150},    // deep winter navy
  {130, 210, 200},  // arctic mint
  {60, 200, 180},   // aqua
};
static const int yarnPaletteLen = sizeof(yarnPalette) / sizeof(yarnPalette[0]);
static int yarnColorIdx = 0;

static void drawYarnBall(float x, float t) {
  const int R = 20;
  const int *yc = yarnPalette[yarnColorIdx];
  uint16_t base = rgb(yc[0], yc[1], yc[2]);
  uint16_t hi = shade(yc[0], yc[1], yc[2], 65);
  uint16_t lo = shade(yc[0], yc[1], yc[2], -65);
  uint16_t threadA = shade(yc[0], yc[1], yc[2], 55), threadB = shade(yc[0], yc[1], yc[2], -50);

  band.fillCircle((int)x, BY(BALL_Y), R, base);
  band.fillCircle((int)x - 6, BY(BALL_Y - 7), 8, hi);   // soft highlight, upper-left
  band.fillCircle((int)x + 7, BY(BALL_Y + 7), 9, lo);    // shadow side, lower-right
  band.fillCircle((int)x, BY(BALL_Y), R - 3, base);      // re-expose the base through the blobs' overlap

  // wound strands: bulging chords at varied angles, alternating thread shades
  for (int i = 0; i < 6; i++) {
    float a = (10 + i * 30.0f) * DEG_TO_RAD;
    float ax = x - R * cosf(a), ay = BALL_Y - R * sinf(a);
    float bx = x + R * cosf(a), by = BALL_Y + R * sinf(a);
    float bulge = ((i % 2) ? 1 : -1) * (7 + (i % 3) * 3);
    float mx = (ax + bx) / 2 - sinf(a) * bulge, my = (ay + by) / 2 + cosf(a) * bulge;
    uint16_t c = (i % 2) ? threadA : threadB;
    band.drawWideLine(ax, BY(ay), mx, BY(my), 2, c, c);
    band.drawWideLine(mx, BY(my), bx, BY(by), 2, c, c);
  }

  // a loose, gently waving trailing strand that frays at the tip (kept short)
  float px = x + 15, py = BALL_Y + 9;
  for (int i = 1; i <= 2; i++) {
    float nx = x + 15 + i * 4 + sinf(i * 1.3f) * 3, ny = BALL_Y + 9 + i * 5;
    band.drawWideLine(px, BY(py), nx, BY(ny), 2, threadB, threadB);
    px = nx; py = ny;
  }
  band.drawLine(px, BY(py), px - 3, BY(py + 4), threadB);
  band.drawLine(px, BY(py), px + 3, BY(py + 3), threadB);

  // shimmer: glints that drift slowly around the surface and twinkle
  for (int i = 0; i < 3; i++) {
    float ang = t * 1.2f + i * 2.1f;
    float sx = x + (R - 6) * cosf(ang);
    float sy = BALL_Y + (R - 6) * sinf(ang) * 0.6f - 4;  // flattened toward the upper, lit side
    float tw = sinf(t * 6.0f + i * 2.3f);
    if (tw > 0.15f) {
      band.fillCircle((int)sx, BY(sy), 1, TFT_WHITE);
      if (tw > 0.75f) {
        band.drawLine(sx - 3, BY(sy), sx + 3, BY(sy), TFT_WHITE);
        band.drawLine(sx, BY(sy - 3), sx, BY(sy + 3), TFT_WHITE);
      }
    }
  }
}

// Advances all the scene's state exactly once per frame (never inside the strip loop).
static void updateScene(float t, float &reachL, float &reachR) {
  float cyclePos = fmodf(t, SWIPE_PERIOD) / SWIPE_PERIOD;  // 0..1
  bool leftActive = cyclePos < 0.5f;
  float sub = leftActive ? cyclePos * 2.0f : (cyclePos - 0.5f) * 2.0f;  // 0..1 within this cat's turn
  float reach = sinf(sub * PI) * 46.0f;  // 0 at ends, peak mid-swipe
  if (reach < 0) reach = 0;
  reachL = leftActive ? reach : 0;
  reachR = leftActive ? 0 : reach;

  // apply one impulse per half-cycle, right as the paw passes the ball (sub ~ 0.5)
  static bool prevLeftActive = true;
  if (leftActive != prevLeftActive) impulseGivenThisHalf = false;
  prevLeftActive = leftActive;
  if (!impulseGivenThisHalf && sub > 0.48f && sub < 0.62f) {
    ballVX += leftActive ? 170.0f : -170.0f;
    impulseGivenThisHalf = true;
    int newIdx;
    do { newIdx = random(yarnPaletteLen); } while (newIdx == yarnColorIdx);
    yarnColorIdx = newIdx;
  }

  // ball physics: simple damped motion with a soft wall at each kitten
  static unsigned long lastPhys = 0;
  unsigned long now = millis();
  float dt = lastPhys ? (now - lastPhys) / 1000.0f : 0;
  lastPhys = now;
  if (dt > 0.08f) dt = 0.08f;
  ballX += ballVX * dt;
  ballVX *= 0.90f;
  if (ballX < BALL_MIN_X) { ballX = BALL_MIN_X; ballVX = fabsf(ballVX) * 0.5f; }
  if (ballX > BALL_MAX_X) { ballX = BALL_MAX_X; ballVX = -fabsf(ballVX) * 0.5f; }
}

// Glitter that drifts down through the whole scene, computed purely from time
// (no stored particle state needed) so it costs nothing extra to keep.
static void drawGlitterRain(float t) {
  const int N = 500;
  const float bandTotalH = NUM_STRIPS * STRIP_H;
  for (int i = 0; i < N; i++) {
    float speed = 30.0f + (i % 7) * 9.0f;
    float phase = i * 53.0f;
    float gy = SCENE_Y0 + fmodf(t * speed + phase, bandTotalH);
    float gx = fmodf(i * 17.3f + sinf(i * 1.7f) * 40.0f, 460.0f) + 10.0f;
    float tw = sinf(t * 9.0f + i * 1.3f);
    if (tw < -0.9f) continue;
    uint16_t c = (i % 3 == 0) ? TFT_WHITE : (i % 3 == 1) ? TFT_GOLD : rgb(255, 200, 230);
    int px = (int)gx, py = BY(gy);
    band.drawPixel(px, py, c);
    if (tw > 0.0f) {
      band.drawPixel(px + 1, py, c);
      band.drawPixel(px, py + 1, c);
    }
  }
}

// A few sparkle pixels scattered over a small rect, twinkling with time - used to make
// text or a shape read as "glittery" without needing per-pixel textures.
static void sparkleOverlay(float cx, float cy, float w, float h, float t, float phase, int n) {
  for (int i = 0; i < n; i++) {
    float gx = cx - w / 2 + fmodf(i * 37.3f + phase * 13.0f, w);
    float gy = cy - h / 2 + fmodf(i * 19.7f + phase * 7.0f, h);
    float tw = sinf(t * 8.0f + i * 1.7f + phase);
    if (tw < 0.2f) continue;
    int px = (int)gx, py = BY(gy);
    band.drawPixel(px, py, TFT_WHITE);
    if (tw > 0.75f) {
      band.drawPixel(px + 1, py, TFT_WHITE);
      band.drawPixel(px, py + 1, TFT_WHITE);
    }
  }
}

// A bigger heart, tie-dyed with every color the yarn ball cycles through: nested
// heart layers from the outside in, each one color of the palette.
static void drawGlitterHeart(float hx, float hy, float t) {
  float R = 14;
  for (int k = 0; k < yarnPaletteLen; k++) {
    float r = R * (1.0f - k * (0.78f / (yarnPaletteLen - 1)));
    const int *c = yarnPalette[k];
    uint16_t col = rgb(c[0], c[1], c[2]);
    band.fillCircle((int)(hx - r * 0.6f), BY(hy - r * 0.3f), (int)r, col);
    band.fillCircle((int)(hx + r * 0.6f), BY(hy - r * 0.3f), (int)r, col);
    band.fillTriangle((int)(hx - r * 1.5f), BY(hy - r * 0.1f), (int)(hx + r * 1.5f), BY(hy - r * 0.1f),
                       (int)hx, BY(hy + r * 1.6f), col);
  }
  sparkleOverlay(hx, hy, 34, 34, t, 20.0f, 14);
}

// Draws a word letter-by-letter, swirling between two colors (that cat's own feet and
// chest tones) for a tie-dye look - GFXFF fonts have no per-pixel fill, so this is the
// closest a bitmap-glyph font can get to tie-dye.
static void drawTieDyeWord(const char *text, float cx, float cy, const int palette[][3], int paletteLen) {
  // hop through the palette out of order so each letter clearly differs from its neighbor
  const int order[6] = { 0, 4, 2, 5, 1, 3 };
  int len = strlen(text);
  int totalW = band.textWidth(text);
  float x = cx - totalW / 2.0f;
  for (int i = 0; i < len; i++) {
    char buf[2] = { text[i], 0 };
    int w = band.textWidth(buf);
    const int *c = palette[order[i % 6] % paletteLen];
    band.setTextColor(rgb(c[0], c[1], c[2]), BG_NAVY);
    band.drawString(buf, (int)x, BY(cy));
    x += w;
  }
}

// "Biscuit" (pink kitten) and "Gwen" (purple kitten) in big, bold, glittery cursive,
// tie-dyed between each cat's own feet and chest colors, with a glittery heart between
// them, in the gap between the title and the cats' heads.
static const float NAME_Y = SCENE_Y0 + 28;
static void drawNames(float t) {
  band.setFreeFont(&Yellowtail_32);
  band.setTextDatum(ML_DATUM);

  // mostly winter, with a touch of Christmas red/green mixed in
  const int biscuitPalette[6][3] = {
    { 150, 220, 255 },  // sky ice blue
    { 235, 245, 255 },  // frost white
    { 60, 200, 180 },   // aqua
    { 210, 225, 235 },  // silver frost
    { 100, 170, 200 },  // steel teal
    { 130, 210, 200 },  // arctic mint
  };
  const int gwenPalette[6][3] = {
    { 100, 170, 200 },  // steel teal
    { 90, 220, 230 },   // cyan
    { 60, 90, 150 },    // deep winter navy
    { 130, 210, 200 },  // arctic mint
    { 150, 220, 255 },  // sky ice blue
    { 210, 225, 235 },  // silver frost
  };
  drawTieDyeWord("Biscuit", CAT1_X, NAME_Y, biscuitPalette, 6);
  sparkleOverlay(CAT1_X, NAME_Y, 100, 40, t, 0.0f, 14);

  drawTieDyeWord("Gwen", CAT2_X, NAME_Y, gwenPalette, 6);
  sparkleOverlay(CAT2_X, NAME_Y, 100, 40, t, 10.0f, 14);

  band.setFreeFont();
  band.setTextDatum(TL_DATUM);

  drawGlitterHeart((CAT1_X + CAT2_X) / 2, NAME_Y, t);
}

// A snowy ground: a drift of soft bumps along the floor line, dusted with twinkling glitter.
static void drawSnowGround(float t) {
  uint16_t snow = rgb(245, 248, 255);
  uint16_t snowShadow = rgb(205, 220, 240);
  band.fillRect(20, BY(GROUND_Y), 440, 16, snow);
  band.drawFastHLine(20, BY(GROUND_Y + 16), 440, snowShadow);
  for (int i = 0; i < 90; i++) {
    float gx = 20 + fmodf(i * 21.3f + t * 4.0f, 440.0f);
    float gy = GROUND_Y - 3 + sinf(i * 2.1f) * 3.0f + (i % 4) * 4.0f;
    float tw = sinf(t * 7.0f + i * 1.9f);
    if (tw < 0.05f) continue;
    uint16_t c = (i % 2 == 0) ? TFT_WHITE : rgb(200, 230, 255);
    int px = (int)gx, py = BY(gy);
    band.drawPixel(px, py, c);
    if (tw > 0.6f) band.drawPixel(px + 1, py, c);
  }
  // a few bigger six-point snowflakes scattered across the snow for variety
  for (int i = 0; i < 5; i++) {
    float sx = 40 + fmodf(i * 97.0f + t * 4.0f, 400.0f);
    float sy = GROUND_Y + 6 + (i % 3) * 3.0f;
    float tw = sinf(t * 3.0f + i * 2.2f);
    if (tw < 0.3f) continue;
    for (int a = 0; a < 180; a += 60) {
      float r = a * DEG_TO_RAD;
      band.drawLine(sx - 4 * cosf(r), BY(sy - 4 * sinf(r)), sx + 4 * cosf(r), BY(sy + 4 * sinf(r)), TFT_WHITE);
    }
  }
}

// A silver Christmas tree in the background, centered right behind the heart's spot,
// so the heart reads as the tree's star-topper (it's still a heart, not a star).
static void drawChristmasTree(float t) {
  const float cx = (CAT1_X + CAT2_X) / 2;
  const float apexY = NAME_Y + 14;
  uint16_t dark = rgb(150, 150, 160), light = rgb(220, 220, 228);
  float tops[4]    = { apexY,      apexY + 22, apexY + 50, apexY + 82  };
  float bottoms[4] = { apexY + 36, apexY + 72, apexY + 110, apexY + 150 };
  float halfWs[4]  = { 20, 34, 48, 62 };
  uint16_t needleDark = rgb(110, 110, 120);
  for (int i = 0; i < 4; i++) {
    float top = tops[i], bottom = bottoms[i], hw = halfWs[i];
    band.fillTriangle((int)cx, BY(top), (int)(cx - hw), BY(bottom), (int)(cx + hw), BY(bottom), dark);
    band.fillTriangle((int)(cx - 5), BY(top + 2), (int)(cx - hw + 8), BY(bottom - 2),
                       (int)(cx + hw * 0.3f), BY(bottom - 2), light);
    // needle texture: short diagonal ticks scattered over this tier
    for (int n = 0; n < 5; n++) {
      float frac = (n + 0.5f) / 5.0f;
      float nx = cx + (frac * 2.0f - 1.0f) * hw * 0.7f;
      float ny = top + (bottom - top) * (0.4f + 0.3f * sinf(n * 1.9f));
      band.drawLine(nx, BY(ny), nx + (n % 2 == 0 ? 4 : -4), BY(ny - 5), needleDark);
    }
    // a light dusting of snow resting along this tier's sloped edges
    for (int side = -1; side <= 1; side += 2)
      for (int n = 0; n < 3; n++) {
        float frac = (n + 0.5f) / 3.0f;
        float snx = cx + side * hw * frac * 0.85f;
        float sny = top + (bottom - top) * frac * 0.5f - 2.0f;
        band.fillEllipse((int)snx, BY(sny), 5, 2, rgb(245, 248, 255));
      }
  }
  band.fillRect((int)cx - 8, BY(bottoms[3] - 4), 16, 24, dark);
  // wood-grain on the trunk
  for (int g = 0; g < 3; g++)
    band.drawLine(cx - 6, BY(bottoms[3] + 2 + g * 6), cx + 6, BY(bottoms[3] + 4 + g * 6), needleDark);

  // ornaments: fixed little baubles, cycling through the same five colors the yarn ball does.
  // The last two sit at the yarn ball's own height, but off to the sides of its travel
  // path (BALL_MIN_X..BALL_MAX_X) so they never land on top of it.
  const int nOrn = 12;
  float ornX[nOrn] = { cx - 12, cx + 10, cx - 25, cx + 22, cx - 5, cx - 38, cx + 30, cx + 5, cx - 50, cx + 45,
                        BALL_MIN_X - 30, BALL_MAX_X + 30 };
  float ornY[nOrn] = { apexY + 20, apexY + 30, apexY + 55, apexY + 60, apexY + 50,
                        apexY + 95, apexY + 100, apexY + 105, apexY + 130, apexY + 135,
                        BALL_Y, BALL_Y };
  for (int i = 0; i < nOrn; i++) {
    const int *c = yarnPalette[i % yarnPaletteLen];
    band.fillCircle((int)ornX[i], BY(ornY[i]), 5, rgb(c[0], c[1], c[2]));
    band.fillCircle((int)ornX[i] - 1, BY(ornY[i]) - 1, 2, TFT_WHITE);
  }

  // string lights: one draped, sagging garland strip per tier (not just a straight line
  // down the middle) - all sharing the ball's CURRENT color, so they change together with it
  const int *lc = yarnPalette[yarnColorIdx];
  uint16_t lightColor = rgb(lc[0], lc[1], lc[2]);
  for (int tier = 0; tier < 4; tier++) {
    float midY = (tops[tier] + bottoms[tier]) / 2.0f + 8.0f;
    float hw = halfWs[tier];
    int nLights = 5 + tier;
    for (int j = 0; j < nLights; j++) {
      float frac = j / (float)(nLights - 1);
      float lx = cx + (frac * 2.0f - 1.0f) * hw * 0.8f;
      float sag = sinf(frac * PI) * (6 + tier * 2);
      float ly = midY + sag;
      float tw = sinf(t * 5.0f + tier * 2.1f + j * 1.1f);
      if (tw > -0.2f) band.fillCircle((int)lx, BY(ly), 4, lightColor);
    }
  }

  sparkleOverlay(cx, apexY + 80, 130, 180, t, 30.0f, 24);
}

// Presents piled under the tree, in the same five colors the yarn ball cycles through.
static void drawPresents(float t) {
  const float cx = (CAT1_X + CAT2_X) / 2;
  const float apexY = NAME_Y + 14;
  float baseY = apexY + 150 + 14;
  if (baseY > GROUND_Y - 6) baseY = GROUND_Y - 6;
  float xs[5]    = { cx - 55, cx - 28, cx,      cx + 28, cx + 55 };
  float sizes[5] = { 16,       20,      14,      22,      17      };
  for (int i = 0; i < 5; i++) {
    const int *c = yarnPalette[i % yarnPaletteLen];
    uint16_t box = rgb(c[0], c[1], c[2]);
    uint16_t ribbon = shade(c[0], c[1], c[2], 70);
    float s = sizes[i], px = xs[i], py = baseY - s * 0.5f;
    band.fillRect((int)(px - s / 2), BY(py - s / 2), (int)s, (int)s, box);
    band.fillRect((int)px - 1, BY(py - s / 2), 2, (int)s, ribbon);
    band.fillRect((int)(px - s / 2), BY(py) - 1, (int)s, 2, ribbon);
    band.fillTriangle((int)px - 4, BY(py - s / 2), (int)px + 4, BY(py - s / 2),
                       (int)px, BY(py - s / 2 - 6), ribbon);
    // a little gift tag dangling off one corner
    band.drawLine(px + s / 2, BY(py - s / 2), px + s / 2 + 4, BY(py - s / 2 - 4), rgb(230, 230, 220));
    band.fillRoundRect((int)(px + s / 2 + 2), BY(py - s / 2 - 8), 6, 5, 1, rgb(245, 240, 225));
  }
  sparkleOverlay(cx, baseY - 10, 140, 30, t, 40.0f, 10);
}

static void drawScene() {
  float t = millis() / 1000.0f;
  float reachL, reachR;
  updateScene(t, reachL, reachR);

  for (int strip = 0; strip < NUM_STRIPS; strip++) {
    curOY = SCENE_Y0 + strip * STRIP_H;
    band.fillSprite(BG_NAVY);
    drawSnowGround(t);
    drawChristmasTree(t);
    drawPresents(t);
    drawNames(t);
    drawGlitterRain(t);  // falls behind the cats and yarn, drawn before them
    // Cheshire Cat palette: hot-pink fur with deep-purple stripes, and the lavender reverse
    // Biscuit: a light-purple bow TIE at the neck (no hair bow). Quinn: a light-pink hair bow (no tie).
    // Biscuit: calico (white/cream with black patches, no orange). Gwen: solid black.
    // Biscuit: a tuxedo kitten (black with a white chest bib and white paws).
    drawKitten(CAT1_X, +1, 28, 26, 30, 32, 30, 34, sinf(t * 2.1f), reachL, t, 0.0f,
               true, 150, 220, 255, false, 0, 0, 0, true);
    drawKitten(CAT2_X, -1, 42, 40, 46, 70, 68, 74, sinf(t * 2.0f + 1.2f), reachR, t, 3.0f,
               false, 0, 0, 0, true, 60, 90, 150, false);
    drawYarnBall(ballX, t);
    band.pushSprite(0, curOY);
  }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, HIGH);

  tft.init();
  tft.setRotation(1);  // landscape, 480x320
  tft.fillScreen(BG_NAVY);

  Serial.printf("free heap before band sprite: %u\n", ESP.getFreeHeap());
  band.setColorDepth(16);
  if (!band.createSprite(480, STRIP_H)) {
    Serial.println("band.createSprite FAILED - out of memory");
  }
  marquee.setColorDepth(16);
  if (!marquee.createSprite(480, MARQUEE_H)) {
    Serial.println("marquee.createSprite FAILED - out of memory");
  }
  marquee.setFreeFont(&Satisfy_24);
  marquee.setTextSize(2);
  merryWidth1 = marquee.textWidth(MERRY_PART1);
  merryWidth = merryWidth1 + marquee.textWidth(MERRY_PART2);
  marquee.setTextSize(1);
  marquee.setFreeFont();
  Serial.printf("free heap after sprites: %u\n", ESP.getFreeHeap());
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastFrame = 0;
  if (now - lastFrame >= 50) {  // ~20 fps
    lastFrame = now;
    drawScene();
    drawMarquee(now / 1000.0f);
  }
}
