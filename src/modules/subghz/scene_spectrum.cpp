#include "scene_spectrum.h"
#include "cc1101_driver.h"
#include <string.h>
#include <math.h>

namespace scene_spectrum {

enum SceneState : uint8_t {
  STATE_MENU = 0,
  STATE_VIEW
};

enum SpectrumMode : uint8_t {
  MODE_WATERFALL = 0,
  MODE_BARS,
  MODE_MONITOR,
  MODE_COUNT
};

static const char* kModeItems[] = {
  "Waterfall",
  "Bars",
  "Monitor"
};
static const int kModeCount = sizeof(kModeItems) / sizeof(kModeItems[0]);

static const int kRowH = 13;
static const int kStartY = 16;

static SceneState s_state = STATE_MENU;
static int s_menuSel = 0;
static float s_barY = (float)kStartY;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;

static const int NUM_BINS = 64;

struct BandDef {
  const char* name;
  uint32_t startFreq;
  uint32_t stopFreq;
  uint32_t centerFreq;
};

static const BandDef kBands[] = {
  { "433M",      433050000, 434790000, 433920000 },
  { "315M",      314000000, 316000000, 315000000 },
  { "868M",      867500000, 869500000, 868350000 },
  { "915M",      914000000, 916000000, 915000000 },
  { "Wide 433",  430000000, 440000000, 434000000 },
  { "Wide 868",  860000000, 876000000, 868000000 },
  { "Wide 315",  305000000, 325000000, 315000000 }
};
static const int kBandCount = sizeof(kBands) / sizeof(kBands[0]);

static int s_bandIndex = 0;
static SpectrumMode s_mode = MODE_WATERFALL;
static bool s_isHold = false;

static int8_t s_currentRssi[NUM_BINS];
static int8_t s_peakRssi[NUM_BINS];
static uint32_t s_peakTime[NUM_BINS];

static uint32_t s_maxPeakFreq = 433920000;
static int8_t s_maxPeakRssi = -110;


static uint8_t s_waterfall[38][16];
static uint8_t s_waterfallLine = 0;


static int8_t s_monitorHistory[128];
static int s_monitorIdx = 0;

static uint32_t s_lastSweepTime = 0;

static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / kRowH + 0.5f);
  if (sel < topIndex) {
    targetScroll = (float)(sel * kRowH);
  } else if (sel >= topIndex + maxLines) {
    targetScroll = (float)((sel - maxLines + 1) * kRowH);
  }

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * kRowH)) targetScroll = (float)(maxScroll * kRowH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) {
    scroll += ds * 0.35f;
  } else {
    scroll = targetScroll;
  }

  float targetBarY = (float)(kStartY + sel * kRowH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) {
    barY += dy * 0.4f;
  } else {
    barY = targetBarY;
  }
}

static void resetSpectrum() {
  for (int i = 0; i < NUM_BINS; i++) {
    s_currentRssi[i] = -110;
    s_peakRssi[i] = -110;
    s_peakTime[i] = 0;
  }
  memset(s_waterfall, 0, sizeof(s_waterfall));
  for (int i = 0; i < 128; i++) {
    s_monitorHistory[i] = -110;
  }
  s_monitorIdx = 0;
  s_maxPeakFreq = kBands[s_bandIndex].centerFreq;
  s_maxPeakRssi = -110;
}

static void pushWaterfallLine() {
  memmove(&s_waterfall[1][0], &s_waterfall[0][0], sizeof(s_waterfall) - 16);
  memset(&s_waterfall[0][0], 0, 16);

  s_waterfallLine++;

  for (int i = 0; i < NUM_BINS; i++) {
    int rssi = s_currentRssi[i];
    int x0 = i * 2;
    int x1 = i * 2 + 1;

    bool p0 = false;
    bool p1 = false;

    if (rssi >= -55) {
      p0 = true;
      p1 = true;
    } else if (rssi >= -70) {
      p0 = ((s_waterfallLine & 3) != 0);
      p1 = true;
    } else if (rssi >= -82) {
      p0 = ((s_waterfallLine + i) % 2 == 0);
      p1 = ((s_waterfallLine + i) % 2 != 0);
    } else if (rssi >= -94) {
      p0 = ((s_waterfallLine + i) % 4 == 0);
      p1 = false;
    }

    if (p0) {
      s_waterfall[0][x0 / 8] |= (0x80 >> (x0 % 8));
    }
    if (p1) {
      s_waterfall[0][x1 / 8] |= (0x80 >> (x1 % 8));
    }
  }
}

static void performSweep() {
  const BandDef& b = kBands[s_bandIndex];
  uint32_t span = b.stopFreq - b.startFreq;
  uint32_t step = span / NUM_BINS;

  int8_t frameMaxRssi = -120;
  uint32_t frameMaxFreq = b.centerFreq;

  bool hw = cc1101::isHardwareDetected();
  uint32_t now = millis();

  for (int i = 0; i < NUM_BINS; i++) {
    uint32_t f = b.startFreq + (uint32_t)i * step;
    int rssi = -105;

    if (hw) {
      cc1101::setFrequency(f);
      delayMicroseconds(80);
      rssi = cc1101::getRssi();
    } else {
      rssi = -120;
    }

    if (rssi > -30) rssi = -30;
    if (rssi < -115) rssi = -115;

    s_currentRssi[i] = (int8_t)rssi;

    if (rssi > s_peakRssi[i]) {
      s_peakRssi[i] = (int8_t)rssi;
      s_peakTime[i] = now;
    } else if (now - s_peakTime[i] > 1200) {
      if (s_peakRssi[i] > -110) s_peakRssi[i]--;
    }

    if (rssi > frameMaxRssi) {
      frameMaxRssi = (int8_t)rssi;
      frameMaxFreq = f;
    }
  }

  s_maxPeakRssi = frameMaxRssi;
  s_maxPeakFreq = frameMaxFreq;

  s_monitorHistory[s_monitorIdx] = frameMaxRssi;
  s_monitorIdx = (s_monitorIdx + 1) % 128;

  pushWaterfallLine();
}

void open() {
  s_state = STATE_MENU;
  s_menuSel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_isHold = false;
  cc1101::init();
}

void close() {
  cc1101::stopReceive();
  s_state = STATE_MENU;
}

bool onBack() {
  if (s_state == STATE_VIEW) {
    s_state = STATE_MENU;
    cc1101::stopReceive();
    return true;
  }
  return false;
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_MENU) {
    if (ev.key == BTN_UP) {
      s_menuSel = (s_menuSel - 1 + kModeCount) % kModeCount;
    } else if (ev.key == BTN_DOWN) {
      s_menuSel = (s_menuSel + 1) % kModeCount;
    } else if (ev.key == BTN_OK) {
      s_mode = (SpectrumMode)s_menuSel;
      s_state = STATE_VIEW;
      s_isHold = false;
      resetSpectrum();
      cc1101::startReceive();
    }
  } else if (s_state == STATE_VIEW) {
    if (ev.key == BTN_LEFT) {
      if (s_mode == 0) s_mode = (SpectrumMode)(MODE_COUNT - 1);
      else s_mode = (SpectrumMode)(s_mode - 1);
    } else if (ev.key == BTN_RIGHT) {
      s_mode = (SpectrumMode)((s_mode + 1) % MODE_COUNT);
    } else if (ev.key == BTN_UP) {
      s_bandIndex = (s_bandIndex - 1 + kBandCount) % kBandCount;
      resetSpectrum();
    } else if (ev.key == BTN_DOWN) {
      s_bandIndex = (s_bandIndex + 1) % kBandCount;
      resetSpectrum();
    } else if (ev.key == BTN_OK) {
      s_isHold = !s_isHold;
    }
  }
}

void tick(uint32_t now) {
  if (s_state == STATE_MENU) {
    updateScroll(s_menuSel, kModeCount, s_scroll, s_targetScroll, s_barY);
  } else if (s_state == STATE_VIEW) {
    if (s_isHold) return;

    if (now - s_lastSweepTime >= 35) {
      s_lastSweepTime = now;
      performSweep();
    }
  }
}

static void drawHeader(U8G2& u8g2) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 13);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.drawStr(2, 10, kBands[s_bandIndex].name);


  char modeBuf[24];
  snprintf(modeBuf, sizeof(modeBuf), "<%s>", kModeItems[s_mode]);
  int mw = u8g2.getStrWidth(modeBuf);
  u8g2.drawStr((128 - mw) / 2, 10, modeBuf);


  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_isHold) {
    u8g2.drawStr(124 - u8g2.getStrWidth("HOLD"), 9, "HOLD");
  } else if (cc1101::isHardwareDetected()) {
    u8g2.drawStr(124 - u8g2.getStrWidth("LIVE"), 9, "LIVE");
  }
  u8g2.setDrawColor(1);
}

static void drawFooter(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setDrawColor(1);

  const BandDef& b = kBands[s_bandIndex];
  char buf[32];

  if (s_mode == MODE_MONITOR) {
    snprintf(buf, sizeof(buf), "%.3fMHz  %ddBm", (double)s_maxPeakFreq / 1000000.0, s_maxPeakRssi);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - w) / 2, 63, buf);
  } else {
    snprintf(buf, sizeof(buf), "%.2f", (double)b.startFreq / 1000000.0);
    u8g2.drawStr(1, 63, buf);

    snprintf(buf, sizeof(buf), "Pk:%.2fM %ddB", (double)s_maxPeakFreq / 1000000.0, s_maxPeakRssi);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - w) / 2, 63, buf);

    snprintf(buf, sizeof(buf), "%.2f", (double)b.stopFreq / 1000000.0);
    int sw = u8g2.getStrWidth(buf);
    u8g2.drawStr(127 - sw, 63, buf);
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  if (s_state == STATE_MENU) {

    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kModeCount; i++) {
      int y = kStartY + (int)(i * kRowH - s_scroll + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, kModeItems[i]);
    }


    int bY = (int)(s_barY + 0.5f);
    if (bY >= 14 && bY <= 64 - kRowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, kRowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "SPECTRUM");
    u8g2.setDrawColor(1);
    return;
  }


  drawHeader(u8g2);
  drawFooter(u8g2);

  switch (s_mode) {
    case MODE_WATERFALL: {
      u8g2.drawBitmap(0, 14, 16, 38, (const uint8_t*)s_waterfall);
      u8g2.drawHLine(0, 53, 128);

      if (s_maxPeakRssi > -95) {
        const BandDef& b = kBands[s_bandIndex];
        int peakX = (int)(((int64_t)(s_maxPeakFreq - b.startFreq) * 127) / (b.stopFreq - b.startFreq));
        if (peakX >= 1 && peakX <= 126) {
          u8g2.drawPixel(peakX, 52);
          u8g2.drawPixel(peakX - 1, 53);
          u8g2.drawPixel(peakX + 1, 53);
        }
      }
      break;
    }

    case MODE_BARS: {
      int gridY = 53 - map(-80, -110, -40, 0, 38);
      for (int gx = 0; gx < 128; gx += 4) {
        u8g2.drawPixel(gx, gridY);
      }

      for (int i = 0; i < NUM_BINS; i++) {
        int x = i * 2;
        int h = constrain(map(s_currentRssi[i], -110, -40, 0, 38), 0, 38);
        if (h > 0) {
          u8g2.drawVLine(x, 53 - h, h);
        }
        int ph = constrain(map(s_peakRssi[i], -110, -40, 0, 38), 0, 38);
        if (ph > 0) {
          u8g2.drawPixel(x, 53 - ph);
        }
      }
      u8g2.drawHLine(0, 54, 128);
      break;
    }

    case MODE_MONITOR: {
      int gridY = 53 - map(-80, -110, -40, 0, 38);
      for (int gx = 0; gx < 128; gx += 4) {
        u8g2.drawPixel(gx, gridY);
      }

      int prevX = 0;
      int prevY = 53 - constrain(map(s_monitorHistory[s_monitorIdx % 128], -110, -40, 0, 38), 0, 38);

      for (int x = 1; x < 128; x++) {
        int idx = (s_monitorIdx + x) % 128;
        int y = 53 - constrain(map(s_monitorHistory[idx], -110, -40, 0, 38), 0, 38);
        u8g2.drawLine(prevX, prevY, x, y);
        prevX = x;
        prevY = y;
      }
      u8g2.drawHLine(0, 54, 128);
      break;
    }

    default:
      break;
  }


  if (!cc1101::isHardwareDetected()) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 22, 108, 24);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 22, 108, 24);
    u8g2.setFont(u8g2_font_6x12_tr);
    const char* t1 = "NO CC1101 MODULE";
    u8g2.drawStr((128 - u8g2.getStrWidth(t1)) / 2, 34, t1);
    u8g2.setFont(u8g2_font_5x8_tr);
    const char* t2 = "Connect SPI Module";
    u8g2.drawStr((128 - u8g2.getStrWidth(t2)) / 2, 43, t2);
  }
}

}
