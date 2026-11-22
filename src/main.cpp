// Emmie Lyn nameplate: cyd40 (Freenove ESP32 Display 4.0", ST7796 480x320, no PSRAM).
// Title cycles color every 5s; below it two kittens take turns batting a ball of yarn,
// drawn the same way as the Fish Tank's fish: ellipses + triangles, no bitmap assets.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

#define AMP_EN 4  // speaker amp enable, active LOW; held HIGH (off) here

TFT_eSPI tft;
TFT_eSprite band(&tft);  // one strip sprite for the whole animated scene (no PSRAM, so keep it modest)

// ---------------------------------------------------------------- title, color-cycling
static const uint16_t palette[] = {
  TFT_GOLD, TFT_CYAN, TFT_MAGENTA, TFT_GREEN, TFT_ORANGE, TFT_PINK, TFT_YELLOW, TFT_SKYBLUE
};
static const int paletteLen = sizeof(palette) / sizeof(palette[0]);
static int colorIdx = 0;
static unsigned long lastTitleChange = 0;
static const unsigned long TITLE_MS = 5000;
static const int TITLE_Y = 38;

static void drawTitle(uint16_t color) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawCentreString("Emmie Lyn", tft.width() / 2, TITLE_Y - tft.fontHeight(4) * 3 / 2, 4);
}

// ---------------------------------------------------------------- kitten + yarn scene
// Classic ESP32 (no PSRAM) can't do one big contiguous sprite for the whole scene (same
// lesson as Fish Tank/Deer Hunt's strip buffers), so the frame is drawn in two short
// horizontal strips reusing one small sprite, each pushed to its own row on the panel.
static const int STRIP_H = 80;              // 480x80x2 bytes = 75 KB per strip
static const int NUM_STRIPS = 2;
static const int SCENE_Y0 = 134;            // panel row where strip 0 starts
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

static void drawKitten(float cx, int dir, uint16_t body, uint16_t belly, float tailPhase, float pawReach) {
  // tail: a tapering, swishing curve of overlapping circles behind the cat
  for (int i = 0; i <= 8; i++) {
    float s = i / 8.0f;
    float px = cx - dir * (26 + 34 * s);
    float py = CAT_BASE_Y - 34 - 26 * sinf(s * 1.4f) + 14 * sinf(tailPhase + s * 2.6f) * s;
    int r = (int)lroundf(9 - 6 * s);
    band.fillCircle((int)px, BY(py), r, body);
  }
  // body + belly
  band.fillEllipse((int)cx, BY(CAT_BASE_Y - 42), 36, 46, body);
  band.fillEllipse((int)cx + dir * 2, BY(CAT_BASE_Y - 20), 20, 26, belly);
  // front paws (static)
  band.fillEllipse((int)(cx - 14), BY(CAT_BASE_Y + 2), 10, 14, body);
  band.fillEllipse((int)(cx + 14), BY(CAT_BASE_Y + 2), 10, 14, body);
  // head
  float headX = cx + dir * 8;
  float headY = CAT_BASE_Y - 92;
  band.fillCircle((int)headX, BY(headY), 28, body);
  // ears
  band.fillTriangle((int)(headX - dir * 24), BY(headY - 6),
                     (int)(headX - dir * 8), BY(headY - 34),
                     (int)(headX + dir * 4), BY(headY - 10), body);
  band.fillTriangle((int)(headX + dir * 24), BY(headY - 6),
                     (int)(headX + dir * 8), BY(headY - 34),
                     (int)(headX - dir * 4), BY(headY - 10), body);
  // eyes + nose
  band.fillCircle((int)(headX - dir * 8), BY(headY - 2), 3, TFT_BLACK);
  band.fillCircle((int)(headX + dir * 8), BY(headY - 2), 3, TFT_BLACK);
  band.fillTriangle((int)headX - 3, BY(headY + 8), (int)headX + 3, BY(headY + 8),
                     (int)headX, BY(headY + 13), TFT_PINK);
  // swiping paw, reaches toward the ball between the cats
  if (pawReach > 0.5f) {
    float px = cx + dir * (20 + pawReach);
    band.fillEllipse((int)px, BY(CAT_BASE_Y - 28), 12, 10, body);
  }
}

static void drawYarnBall(float x) {
  band.fillCircle((int)x, BY(BALL_Y), 20, TFT_MAROON);
  uint16_t thread = TFT_RED;
  for (int a = 0; a < 180; a += 36) {
    float r = a * DEG_TO_RAD;
    band.drawLine((int)(x - 20 * cosf(r)), BY(BALL_Y - 20 * sinf(r)),
                  (int)(x + 20 * cosf(r)), BY(BALL_Y + 20 * sinf(r)), thread);
  }
  // a loose trailing strand
  band.drawLine((int)x + 16, BY(BALL_Y + 10), (int)x + 30, BY(BALL_Y + 26), thread);
  band.drawLine((int)x + 30, BY(BALL_Y + 26), (int)x + 24, BY(BALL_Y + 40), thread);
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

static void drawScene() {
  float t = millis() / 1000.0f;
  float reachL, reachR;
  updateScene(t, reachL, reachR);

  for (int strip = 0; strip < NUM_STRIPS; strip++) {
    curOY = SCENE_Y0 + strip * STRIP_H;
    band.fillSprite(TFT_BLACK);
    band.drawFastHLine(20, BY(GROUND_Y), 440, TFT_DARKGREY);
    drawKitten(CAT1_X, +1, tft.color565(235, 140, 50), tft.color565(255, 210, 170),
               sinf(t * 2.1f), reachL);
    drawKitten(CAT2_X, -1, tft.color565(150, 150, 160), tft.color565(220, 220, 225),
               sinf(t * 2.0f + 1.2f), reachR);
    drawYarnBall(ballX);
    band.pushSprite(0, curOY);
  }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, HIGH);

  tft.init();
  tft.setRotation(1);  // landscape, 480x320
  tft.fillScreen(TFT_BLACK);

  drawTitle(palette[colorIdx]);
  lastTitleChange = millis();

  Serial.printf("free heap before band sprite: %u\n", ESP.getFreeHeap());
  band.setColorDepth(16);
  if (!band.createSprite(480, STRIP_H)) {
    Serial.println("band.createSprite FAILED - out of memory");
  }
  Serial.printf("free heap after band sprite: %u\n", ESP.getFreeHeap());
}

void loop() {
  unsigned long now = millis();
  if (now - lastTitleChange >= TITLE_MS) {
    colorIdx = (colorIdx + 1) % paletteLen;
    drawTitle(palette[colorIdx]);
    lastTitleChange += TITLE_MS;
  }

  static unsigned long lastFrame = 0;
  if (now - lastFrame >= 50) {  // ~20 fps
    lastFrame = now;
    drawScene();
  }
}
