// Emmie Lyn's board: cyd40 (Freenove ESP32 Display 4.0", ST7796 480x320, no PSRAM).
// A Hamilton-the-musical-themed scene: a tall three-masted ship sailing into New York
// harbor at night, past a city skyline silhouette, under a sky full of bursting
// fireworks, with a little flag waving atop the mainmast. Drawn with TFT_eSPI vector
// primitives only (ellipses/triangles/lines, no bitmap assets), using the same
// memory-safe strip-sprite technique as the earlier scenes (classic ESP32 has no
// PSRAM, so one big contiguous sprite for the whole screen isn't an option).
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <math.h>

#define AMP_EN 4  // speaker amp enable, active LOW; now held LOW (on) for firework sound effects

static inline uint16_t rgb(int r, int g, int b) {
  r = constrain(r, 0, 255); g = constrain(g, 0, 255); b = constrain(b, 0, 255);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline uint16_t shade(int r, int g, int b, int delta) { return rgb(r + delta, g + delta, b + delta); }

TFT_eSPI tft;
TFT_eSprite band(&tft);  // one reused strip sprite for the whole scene (no PSRAM, so keep it modest)

// ---------------------------------------------------------------- scene strips
static const int STRIP_H = 80;
static const int NUM_STRIPS = 4;  // 80 x 4 = 320, the full panel height
static int curOY = 0;             // panel row the CURRENT strip's row 0 sits at
static inline int BY(float worldY) { return (int)lroundf(worldY) - curOY; }

static const float WATER_Y = 238;  // world y of the waterline

static void drawSky() {
  const int nBands = 6;
  for (int i = 0; i < nBands; i++) {
    float f = i / (float)(nBands - 1);
    int r = (int)(8 + f * (35 - 8));
    int g = (int)(10 + f * (28 - 10));
    int b = (int)(26 + f * (58 - 26));
    int y0 = (int)(i * WATER_Y / nBands);
    int y1 = (int)((i + 1) * WATER_Y / nBands);
    band.fillRect(0, BY(y0), 480, y1 - y0, rgb(r, g, b));
  }
}

static void drawCitySkyline() {
  uint16_t bld = rgb(14, 16, 32), bldLit = rgb(235, 200, 110);
  const float bx[10] = { 10, 45, 80, 120, 170, 230, 290, 340, 390, 440 };
  const float bh[10] = { 40, 65, 30, 80, 50, 95, 42, 70, 35, 55 };
  const float bw[10] = { 28, 22, 26, 24, 30, 26, 24, 22, 28, 24 };
  for (int i = 0; i < 10; i++) {
    float top = WATER_Y - bh[i];
    band.fillRect((int)(bx[i] - bw[i] / 2), BY(top), (int)bw[i], (int)bh[i] + 4, bld);
    for (int w = 0; w < 3; w++) {
      float wx = bx[i] - bw[i] / 2 + 4 + w * (bw[i] - 8) / 2.0f;
      if ((i + w) % 3 != 0) band.fillRect((int)wx, BY(top + 8 + (w % 2) * 14), 3, 4, bldLit);
    }
  }
}

// ---------------------------------------------------------------- fireworks
// A rotating playlist of song titles - the "titled" fireworks don't always show the
// same one; which title they show advances every time that firework cycles around.
static const char *SONG_TITLES[] = {
  "Alexander Hamilton", "My Shot", "The Schuyler Sisters", "Farmer Refuted",
  "You'll Be Back", "Right Hand Man", "Helpless", "Satisfied", "Wait For It",
  "Ten Duel Commandments", "Guns and Ships", "History Has Its Eyes On You",
  "Yorktown", "Non-Stop", "The Room Where It Happens", "Burn",
  "Dear Theodosia", "Who Tells Your Story",
};
static const int NUM_TITLES = 18;

struct FireworkDef { float x, y0, period, phase; int r, g, b; bool titled; };
static const int NUM_FW = 13;
static FireworkDef FW[NUM_FW];
static const int FW_COLORS[10][3] = {
  { 230, 60, 60 },    // red
  { 240, 200, 80 },   // gold
  { 80, 190, 220 },   // teal/cyan
  { 240, 240, 245 },  // white
  { 100, 140, 230 },  // blue
  { 190, 90, 220 },   // purple
  { 255, 120, 170 },  // pink
  { 255, 150, 60 },   // orange
  { 120, 220, 110 },  // green
  { 210, 210, 220 },  // silver
};

static void initFireworks() {
  // shuffle which launch-slot fires at which point in the timing sequence, so the
  // show doesn't always sweep left-to-right in the same order - a fresh random order
  // every time the board powers up
  int order[NUM_FW];
  for (int i = 0; i < NUM_FW; i++) order[i] = i;
  for (int i = NUM_FW - 1; i > 0; i--) {
    int j = random(i + 1);
    int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
  }
  for (int i = 0; i < NUM_FW; i++) {
    FW[i].x = 18 + i * 37.0f;
    FW[i].y0 = 45 + (i % 4) * 10;
    FW[i].period = 3.6f + (i % 5) * 0.15f;
    FW[i].phase = order[i] * 0.45f;
    const int *c = FW_COLORS[i % 10];
    FW[i].r = c[0]; FW[i].g = c[1]; FW[i].b = c[2];
    FW[i].titled = (i % 2 == 1);
  }
}
static const float RISE_DUR = 0.7f, BURST_DUR = 1.2f;
static const float MEGA_PERIOD = 10.0f;  // every 10s, every firework fires together at once

static void drawFireworks(float t) {
  // every MEGA_PERIOD seconds, all fireworks switch to a shared timeline so they all
  // rise and burst together in one grand synchronized salvo, then return to their own
  // staggered random timing for the rest of the cycle
  float masterT = fmodf(t, MEGA_PERIOD);
  bool mega = masterT < (RISE_DUR + BURST_DUR);
  for (int i = 0; i < NUM_FW; i++) {
    float localT = mega ? masterT : fmodf(t + FW[i].phase, FW[i].period);
    uint16_t bright = rgb(FW[i].r, FW[i].g, FW[i].b);
    uint16_t dim = shade(FW[i].r, FW[i].g, FW[i].b, -90);
    if (localT < RISE_DUR) {
      float p = localT / RISE_DUR;
      float yNow = WATER_Y - p * (WATER_Y - FW[i].y0);
      band.drawLine(FW[i].x, BY(WATER_Y), FW[i].x, BY(yNow), dim);
      band.fillCircle((int)FW[i].x, BY(yNow), 2, TFT_WHITE);
    } else if (localT < RISE_DUR + BURST_DUR) {
      float burstT = (localT - RISE_DUR) / BURST_DUR;
      const int nP = 14;
      for (int p = 0; p < nP; p++) {
        float ang = p * (360.0f / nP) * DEG_TO_RAD + i * 0.35f;
        float radius = burstT * 72.0f;
        float px = FW[i].x + radius * cosf(ang);
        float py = FW[i].y0 + radius * sinf(ang) * 0.85f + burstT * burstT * 20.0f;
        uint16_t c = (burstT > 0.6f) ? dim : bright;
        band.drawPixel((int)px, BY(py), c);
        float rPrev = fmaxf(radius - 10.0f, 0.0f);
        band.drawLine(FW[i].x + rPrev * cosf(ang), BY(FW[i].y0 + rPrev * sinf(ang) * 0.85f),
                       px, BY(py), c);
      }
    }
    // a few bursts spell out a song title in lights for a moment, right after bursting -
    // which title shows advances by one every time this firework comes back around
    if (FW[i].titled) {
      float textStart = RISE_DUR + 0.35f, textEnd = RISE_DUR + BURST_DUR + 0.5f;
      if (localT > textStart && localT < textEnd) {
        int cycleNum = (int)floorf((t + FW[i].phase) / FW[i].period);
        const char *title = SONG_TITLES[(cycleNum + i * 3) % NUM_TITLES];
        band.setTextColor(bright);
        band.setTextDatum(MC_DATUM);
        band.drawString(title, (int)FW[i].x, BY(FW[i].y0), 1);
        band.setTextDatum(TL_DATUM);
      }
    }
  }
}

// During the synchronized salvo, the fireworks' own sparks twinkle into the shape of
// an American flag across the sky - a dotted, flickering mosaic rather than a solid
// drawn graphic, so it reads as the fireworks themselves forming the picture.
static void drawFlagMoment(float t) {
  float masterT = fmodf(t, MEGA_PERIOD);
  if (masterT >= RISE_DUR + BURST_DUR + 0.6f) return;
  uint16_t red = rgb(255, 70, 80), white = rgb(255, 255, 255), blue = rgb(120, 150, 255);
  const int FX0 = 40, FY0 = 14, FLW = 400, FLH = 110, NSEG = 10, NSTRIPES = 13;
  float segW = FLW / (float)NSEG;
  float stripeH = FLH / (float)NSTRIPES;
  const int cantonSeg = 4;
  const float cStripes = 7;

  for (int s = 0; s < NSEG; s++) {
    float x0 = FX0 + s * segW, x1 = FX0 + (s + 1) * segW;
    float wave = (sinf(t * 2.2f + s * 0.75f) + sinf(t * 2.2f + (s + 1) * 0.75f)) * 3.5f;
    for (int i = 0; i < NSTRIPES; i++) {
      float y0 = FY0 + i * stripeH + wave, y1 = FY0 + (i + 1) * stripeH + wave;
      uint16_t c = (i % 2 == 0) ? red : white;
      for (int p = 0; p < 4; p++) {
        float fx = x0 + (p + 0.5f) / 4.0f * (x1 - x0);
        float fy = y0 + ((p % 2) + 0.5f) / 2.0f * (y1 - y0);
        float tw = sinf(t * 9.0f + s * 3.1f + i * 1.7f + p * 2.3f);
        if (tw < -0.2f) continue;
        band.drawPixel((int)fx, BY(fy), c);
        if (tw > 0.5f) {
          band.drawPixel((int)fx + 1, BY(fy), c);
          band.drawPixel((int)fx, BY(fy) + 1, c);
        }
      }
    }
  }
  // canton: a sparkling blue field
  for (int s = 0; s < cantonSeg; s++) {
    float x0 = FX0 + s * segW, x1 = FX0 + (s + 1) * segW;
    float wave = (sinf(t * 2.2f + s * 0.75f) + sinf(t * 2.2f + (s + 1) * 0.75f)) * 3.5f;
    float y0 = FY0 + wave, y1 = FY0 + cStripes * stripeH + wave;
    for (int p = 0; p < 6; p++) {
      float fx = x0 + (p % 3 + 0.5f) / 3.0f * (x1 - x0);
      float fy = y0 + (p / 3 + 0.5f) / 2.0f * (y1 - y0);
      if (sinf(t * 9.0f + s * 2.7f + p * 1.9f) < -0.2f) continue;
      band.drawPixel((int)fx, BY(fy), blue);
    }
  }
  // stars: twinkling little sparkle crosses laid out in a grid over the canton
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      float sx = FX0 + 10 + col * (cantonSeg * segW - 20) / 3.0f;
      float syBase = FY0 + 8 + row * (cStripes * stripeH - 16) / 3.0f;
      int seg = constrain((int)((sx - FX0) / segW), 0, cantonSeg - 1);
      float wv = sinf(t * 2.2f + seg * 0.75f) * 7.0f;
      float sy = syBase + wv;
      float tw = sinf(t * 10.0f + row * 1.3f + col * 2.1f);
      if (tw < -0.3f) continue;
      band.drawLine(sx - 3, BY(sy), sx + 3, BY(sy), white);
      band.drawLine(sx, BY(sy - 3), sx, BY(sy + 3), white);
      if (tw > 0.5f) band.fillCircle((int)sx, BY(sy), 1, white);
    }
  }
}

// ---------------------------------------------------------------- harbor water
static void drawWater(float t) {
  band.fillRect(0, BY(WATER_Y), 480, (NUM_STRIPS * STRIP_H) - (int)WATER_Y, rgb(10, 22, 34));
  band.drawFastHLine(0, BY(WATER_Y), 480, rgb(30, 55, 65));
  for (int i = 0; i < 60; i++) {
    float gx = fmodf(i * 8.1f + t * 6.0f, 480.0f);
    float gy = WATER_Y + 6 + (i % 7) * 6;
    if (sinf(t * 2.0f + i * 1.1f) > 0.3f) band.drawFastHLine((int)gx, BY(gy), 10, rgb(26, 48, 58));
  }
  // faint colored reflections beneath any firework currently bursting
  for (int i = 0; i < NUM_FW; i++) {
    float localT = fmodf(t + FW[i].phase, FW[i].period);
    if (localT < RISE_DUR || localT > RISE_DUR + BURST_DUR) continue;
    uint16_t dim = shade(FW[i].r, FW[i].g, FW[i].b, -110);
    for (int k = 0; k < 4; k++)
      band.drawPixel((int)(FW[i].x + (k - 2) * 5 + sinf(t * 3 + k) * 3), BY(WATER_Y + 10 + k * 8), dim);
  }
}

// ---------------------------------------------------------------- tall ship
static void drawShip(float t) {
  uint16_t hull = rgb(45, 32, 24), hullDark = shade(45, 32, 24, -18), hullLight = shade(45, 32, 24, 20);
  uint16_t sail = rgb(225, 218, 200), sailShadow = shade(225, 218, 200, -35);
  uint16_t mast = rgb(60, 45, 32);

  // slow drift across the harbor, wrapping around, plus a gentle bob
  float shipSpan = 560.0f;  // off-screen to off-screen, so it re-enters smoothly
  float shipX = fmodf(t * 35.0f, shipSpan) - 80.0f;
  float bob = sinf(t * 1.0f) * 2.5f;
  float hullY = WATER_Y - 6 + bob;

  // hull: a long curved shape with a raised bow and stern
  band.fillTriangle((int)(shipX - 70), BY(hullY), (int)(shipX + 70), BY(hullY), (int)shipX, BY(hullY + 20), hull);
  band.fillRect((int)(shipX - 70), BY(hullY - 10), 140, 12, hull);
  band.fillTriangle((int)(shipX + 70), BY(hullY - 10), (int)(shipX + 70), BY(hullY + 2), (int)(shipX + 90), BY(hullY - 14), hullDark);  // bow
  band.fillRect((int)(shipX - 76), BY(hullY - 20), 14, 16, hullDark);  // raised stern castle
  band.drawFastHLine((int)(shipX - 70), BY(hullY - 10), 140, hullLight);
  band.drawLine(shipX + 90, BY(hullY - 14), shipX + 104, BY(hullY - 16), mast);  // bowsprit

  // three masts with yards and sails, each sail gently luffing in the wind
  const float mastX[3] = { shipX - 40, shipX, shipX + 38 };
  const float mastH[3] = { 60, 78, 54 };
  for (int m = 0; m < 3; m++) {
    float topY = hullY - 10 - mastH[m];
    band.drawWideLine(mastX[m], BY(hullY - 10), mastX[m], BY(topY), 2, mast, mast);
    for (int yard = 0; yard < 2; yard++) {
      float yardY = topY + 14 + yard * 22;
      float billow = sinf(t * 2.0f + m * 1.3f + yard * 0.8f) * 4.0f;
      float halfW = 20.0f - yard * 4.0f;
      band.drawFastHLine((int)(mastX[m] - halfW), BY(yardY), (int)(halfW * 2), mast);
      band.fillTriangle((int)(mastX[m] - halfW + 2), BY(yardY + 2), (int)(mastX[m] + halfW - 2), BY(yardY + 2),
                         (int)(mastX[m] + billow), BY(yardY + 20), sail);
      band.fillTriangle((int)(mastX[m] - halfW + 2), BY(yardY + 2), (int)(mastX[m] + billow), BY(yardY + 20),
                         (int)(mastX[m] - halfW + 6 + billow * 0.5f), BY(yardY + 20), sailShadow);
    }
  }

  // a little flag waving atop the mainmast
  float flagBaseX = mastX[1], flagBaseY = hullY - 10 - mastH[1];
  for (int k = 0; k < 4; k++) {
    float fx0 = flagBaseX + k * 5, fx1 = flagBaseX + (k + 1) * 5;
    float wA = sinf(t * 5.0f + k * 1.0f) * 3.0f, wB = sinf(t * 5.0f + (k + 1) * 1.0f) * 3.0f;
    uint16_t c = (k % 2 == 0) ? rgb(195, 40, 55) : rgb(240, 240, 235);
    band.fillTriangle((int)fx0, BY(flagBaseY + wA - 4), (int)fx1, BY(flagBaseY + wB - 4), (int)fx1, BY(flagBaseY + wB + 4), c);
    band.fillTriangle((int)fx0, BY(flagBaseY + wA - 4), (int)fx1, BY(flagBaseY + wB + 4), (int)fx0, BY(flagBaseY + wA + 4), c);
  }

  // a few cabin lights glowing warm against the dark hull
  for (int i = 0; i < 3; i++)
    band.fillRect((int)(shipX - 60 + i * 20), BY(hullY - 7), 4, 4, rgb(235, 190, 110));
}

// ---------------------------------------------------------------- firework sound effects
// Real audio through the ESP32's internal DAC via I2S (same technique as Grey's Fish Tank/
// Deer Hunt boards): a dedicated task on core 1 plays back Grey's own recorded fireworks
// clip - a short single-pop snippet for each individual burst, the longer full clip for the
// grand synchronized salvo - scaling it by the chosen volume as real digital gain (not a PWM
// duty-cycle trick, which doesn't audibly change loudness on this amp), then dithering it
// down to the DAC's 8 bits the same way Fish Tank does. Only the volume-preview beep is
// still synthesized, since it's a UI sound rather than a firework sound.
static int fwPrevPhase[NUM_FW] = { 0 };

// Grey's recorded fireworks clip (trimmed, resampled to 16 kHz mono), embedded in flash:
// a short single-pop snippet for ordinary bursts, and the longer full clip for the salvo.
extern const uint8_t burst_start[] asm("_binary_src_burst_raw_start");
extern const uint8_t burst_end[] asm("_binary_src_burst_raw_end");
extern const uint8_t fireworks_start[] asm("_binary_src_fireworks_raw_start");
extern const uint8_t fireworks_end[] asm("_binary_src_fireworks_raw_end");
static const float FIREWORKS_SR = 16000.0f;

static volatile bool g_soundOn = true;
static volatile int g_volumeLevel = 3;  // 0..4
static const float VOLUME_GAIN[5] = { 0.10f, 0.22f, 0.40f, 0.62f, 0.90f };
static volatile int g_soundKind = 0;        // 0 none, 2 burst snippet, 3 preview beep, 4 full recorded clip
static volatile uint32_t g_triggerSeq = 0;  // bumped on every new trigger, even if kind repeats

static void loadSoundSettings() {
  Preferences p;
  p.begin("sound", true);
  g_soundOn = p.getBool("on", true);
  g_volumeLevel = p.getInt("vol", 3);
  p.end();
}
static void saveSoundSettings() {
  Preferences p;
  p.begin("sound", false);
  p.putBool("on", g_soundOn);
  p.putInt("vol", g_volumeLevel);
  p.end();
}

static void triggerSound(int kind) {
  g_soundKind = kind;
  g_triggerSeq++;
}

static void audioTask(void *) {
  const float SR = 44100.0f;
  static uint16_t out[256 * 2];
  uint32_t localSeq = 0;
  int localKind = 0;
  uint32_t samplePos = 0;
  float phase = 0;
  int32_t err = 0;

  for (;;) {
    for (int i = 0; i < 256; i++) {
      if (g_triggerSeq != localSeq) { localSeq = g_triggerSeq; localKind = g_soundKind; samplePos = 0; phase = 0; }
      float elapsed = samplePos / SR;
      float sampleF = 0;

      if (localKind == 2) {  // a single recorded pop, for each ordinary burst
        static const int32_t burstLen = (int32_t)((burst_end - burst_start) / 2);
        float srcPosF = samplePos * (FIREWORKS_SR / SR);
        int32_t i0 = (int32_t)srcPosF;
        if (i0 < burstLen - 1) {
          const int16_t *pcm = (const int16_t *)burst_start;
          float frac = srcPosF - i0;
          float s0 = pcm[i0] / 32768.0f, s1 = pcm[i0 + 1] / 32768.0f;
          sampleF = s0 + (s1 - s0) * frac;
        } else localKind = 0;

      } else if (localKind == 3) {  // short fixed-pitch volume-preview beep
        const float dur = 0.18f;
        if (elapsed < dur) {
          phase += 700.0f / SR; if (phase >= 1.0f) phase -= 1.0f;
          float env = fminf(1.0f, elapsed / 0.01f) * fminf(1.0f, (dur - elapsed) / 0.03f);
          sampleF = sinf(6.2831853f * phase) * env;
        } else localKind = 0;
      } else if (localKind == 4) {  // the real recorded fireworks clip, resampled up from 16 kHz
        static const int32_t fwLen = (int32_t)((fireworks_end - fireworks_start) / 2);
        float srcPosF = samplePos * (FIREWORKS_SR / SR);
        int32_t i0 = (int32_t)srcPosF;
        if (i0 < fwLen - 1) {
          const int16_t *pcm = (const int16_t *)fireworks_start;
          float frac = srcPosF - i0;
          float s0 = pcm[i0] / 32768.0f, s1 = pcm[i0 + 1] / 32768.0f;
          sampleF = s0 + (s1 - s0) * frac;
        } else localKind = 0;
      }
      if (localKind != 0) samplePos++;
      if (!g_soundOn) sampleF = 0;
      else sampleF *= VOLUME_GAIN[g_volumeLevel];

      int32_t m = (int32_t)(sampleF * 26000.0f);
      if (m > 32767) m = 32767; else if (m < -32768) m = -32768;
      // Dither down to the DAC's 8 bits with error feedback, exactly as Fish Tank does, so the
      // rounding noise moves to high frequencies instead of a hiss across the whole range.
      int32_t acc = m + err;
      int32_t q = (acc + 128) >> 8;
      if (q > 127) q = 127; else if (q < -128) q = -128;
      err = acc - (q << 8);
      if (err > 512) err = 512; else if (err < -512) err = -512;
      const uint16_t v = (uint16_t)((q + 128) << 8);  // DAC wants offset-binary in the high byte
      out[2 * i] = v; out[2 * i + 1] = v;
    }
    size_t written;
    i2s_write(I2S_NUM_0, out, sizeof(out), &written, portMAX_DELAY);
  }
}

static void startAudio() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 16;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("I2S install failed");
    return;
  }
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);  // cyd40: both DAC pins free (touch is on the display bus)
  xTaskCreatePinnedToCore(audioTask, "fwaudio", 4096, NULL, 2, NULL, 1);
}

static void previewVolume() { triggerSound(3); }

static bool wasMega = false;

static void updateFireworkSounds(float t) {
  float masterT = fmodf(t, MEGA_PERIOD);
  bool mega = masterT < (RISE_DUR + BURST_DUR);
  if (mega && !wasMega) triggerSound(4);  // the recorded fireworks clip, once per grand salvo
  wasMega = mega;
  for (int i = 0; i < NUM_FW; i++) {
    float localT = mega ? masterT : fmodf(t + FW[i].phase, FW[i].period);
    int phase = (localT < RISE_DUR) ? 1 : ((localT < RISE_DUR + BURST_DUR) ? 2 : 0);
    if (phase != fwPrevPhase[i]) {
      // during the mega salvo the recorded clip plays instead; otherwise each burst gets a
      // single recorded pop (no sound on the silent rise up)
      if (!mega && phase == 2) triggerSound(2);
      fwPrevPhase[i] = phase;
    }
  }
}

// ---------------------------------------------------------------- touch + settings dialog
static bool dialogOpen = false;
static bool wasTouched = false;
static const int DLG_W = 300, DLG_H = 180;
static const int DLG_X = (480 - DLG_W) / 2, DLG_Y = (320 - DLG_H) / 2;

static inline bool inRect(int px, int py, int x, int y, int w, int h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

static void drawDialog() {
  uint16_t panel = rgb(20, 24, 45), toggleOn = rgb(60, 160, 90), toggleOff = rgb(160, 60, 60), btn = rgb(70, 70, 92);
  tft.fillRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 10, panel);
  tft.drawRoundRect(DLG_X, DLG_Y, DLG_W, DLG_H, 10, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, panel);
  tft.drawString("Sound Settings", DLG_X + DLG_W / 2, DLG_Y + 20, 2);
  tft.drawLine(DLG_X + DLG_W - 28, DLG_Y + 10, DLG_X + DLG_W - 12, DLG_Y + 26, TFT_WHITE);
  tft.drawLine(DLG_X + DLG_W - 12, DLG_Y + 10, DLG_X + DLG_W - 28, DLG_Y + 26, TFT_WHITE);

  tft.fillRoundRect(DLG_X + 20, DLG_Y + 44, DLG_W - 40, 42, 8, g_soundOn ? toggleOn : toggleOff);
  tft.drawString(g_soundOn ? "Sound: ON" : "Sound: OFF", DLG_X + DLG_W / 2, DLG_Y + 65, 2);

  tft.drawString("Volume", DLG_X + DLG_W / 2, DLG_Y + 104, 1);
  tft.fillRoundRect(DLG_X + 20, DLG_Y + 116, 48, 42, 8, btn);
  tft.drawString("-", DLG_X + 44, DLG_Y + 137, 4);
  tft.fillRoundRect(DLG_X + DLG_W - 68, DLG_Y + 116, 48, 42, 8, btn);
  tft.drawString("+", DLG_X + DLG_W - 44, DLG_Y + 137, 4);
  for (int i = 0; i < 5; i++) {
    int bx = DLG_X + 82 + i * 26;
    tft.fillRoundRect(bx, DLG_Y + 126, 20, 22, 4, (i <= g_volumeLevel) ? rgb(240, 200, 80) : rgb(55, 55, 70));
  }
  tft.setTextDatum(TL_DATUM);
}

static void updateTouch(float t) {
  uint16_t tx, ty;
  bool touched = tft.getTouch(&tx, &ty, 350);  // a lighter threshold than the TFT_eSPI default (600)
  bool newTap = touched && !wasTouched;
  wasTouched = touched;
  if (!newTap) return;
  Serial.printf("tap at (%u,%u) dialogOpen=%d\n", tx, ty, dialogOpen);

  if (!dialogOpen) {
    dialogOpen = true;
    return;
  }
  if (!inRect(tx, ty, DLG_X, DLG_Y, DLG_W, DLG_H)) { Serial.println("  -> outside dialog, closing"); dialogOpen = false; return; }
  if (inRect(tx, ty, DLG_X + DLG_W - 30, DLG_Y + 8, 20, 20)) { Serial.println("  -> close button"); dialogOpen = false; return; }
  if (inRect(tx, ty, DLG_X + 20, DLG_Y + 44, DLG_W - 40, 42)) {
    Serial.println("  -> sound toggle");
    g_soundOn = !g_soundOn;
    saveSoundSettings();
    return;
  }
  if (inRect(tx, ty, DLG_X + 20, DLG_Y + 116, 48, 42)) {
    if (g_volumeLevel > 0) g_volumeLevel--;
    Serial.printf("  -> volume - (now %d)\n", g_volumeLevel);
    previewVolume();
    saveSoundSettings();
    return;
  }
  if (inRect(tx, ty, DLG_X + DLG_W - 68, DLG_Y + 116, 48, 42)) {
    if (g_volumeLevel < 4) g_volumeLevel++;
    Serial.printf("  -> volume + (now %d)\n", g_volumeLevel);
    previewVolume();
    saveSoundSettings();
    return;
  }
  Serial.println("  -> inside dialog, no button hit");
}

static void drawScene() {
  float t = millis() / 1000.0f;
  for (int strip = 0; strip < NUM_STRIPS; strip++) {
    curOY = strip * STRIP_H;
    drawSky();
    drawCitySkyline();
    drawFireworks(t);
    drawFlagMoment(t);
    drawWater(t);
    drawShip(t);
    band.pushSprite(0, curOY);
  }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, LOW);  // enable the speaker amp (active LOW)

  tft.init();
  tft.setRotation(1);  // landscape, 480x320
  tft.fillScreen(TFT_BLACK);

  // touch calibration: reuse it from flash if we have it, otherwise run the interactive
  // 3-point calibration once (tap each crosshair) and save it for next time
  uint16_t calData[5];
  Preferences calPrefs;
  calPrefs.begin("touchcal", true);
  size_t calN = calPrefs.getBytes("cal5", calData, sizeof(calData));
  calPrefs.end();
  if (calN == sizeof(calData)) {
    tft.setTouch(calData);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Tap each X to set up touch", 240, 20, 2);
    tft.setTextDatum(TL_DATUM);
    tft.calibrateTouch(calData, TFT_WHITE, TFT_RED, 15);
    Preferences calPrefs2;
    calPrefs2.begin("touchcal", false);
    calPrefs2.putBytes("cal5", calData, sizeof(calData));
    calPrefs2.end();
  }
  loadSoundSettings();
  startAudio();

  initFireworks();

  Serial.printf("free heap before sprite: %u\n", ESP.getFreeHeap());
  band.setColorDepth(16);
  if (!band.createSprite(480, STRIP_H)) {
    Serial.println("band.createSprite FAILED - out of memory");
  }
  Serial.printf("free heap after sprite: %u\n", ESP.getFreeHeap());
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastFrame = 0;
  if (now - lastFrame >= 50) {  // ~20 fps
    lastFrame = now;
    updateTouch(now / 1000.0f);
    updateFireworkSounds(now / 1000.0f);
    if (dialogOpen) {
      drawDialog();  // frozen behind the dialog, so it isn't repainted over every frame
    } else {
      drawScene();
    }
  }
}
