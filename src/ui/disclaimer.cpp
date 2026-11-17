#include "disclaimer.h"
#include "core/sys_storage.h"
#include <FFat.h>
#include <Preferences.h>
#include <math.h>

namespace disclaimer {

static bool s_accepted = false;
static bool s_inited = false;

static const char *kText[] = {"LEGAL DISCLAIMER",
                              "",
                              "ESPone firmware is",
                              "designed exclusively",
                              "for educational use,",
                              "authorized testing",
                              "and security research.",
                              "",
                              "The developers and",
                              "contributors bear NO",
                              "responsibility for any",
                              "unlawful actions,",
                              "misuse or damages",
                              "caused by this tool.",
                              "",
                              "You take full personal",
                              "responsibility for",
                              "compliance with all",
                              "applicable laws.",
                              "",
                              "--------------------"};
static const int kTextLineCount = sizeof(kText) / sizeof(kText[0]);


static bool s_checked = false;
static int s_focusCtrl = -1;

static int s_scrollLine = 0;
static float s_scrollSmooth = 0.0f;
static const int lineH = 10;
static const int startY = 16;
static const int visibleLines = 4;

static const char *s_warnMsg = nullptr;
static uint32_t s_warnUntil = 0;

void init() {
  if (s_inited)
    return;
  s_inited = true;

  Preferences prefs;
  prefs.begin("espone_set", true);
  s_accepted = prefs.getBool("disclaimer", false);
  prefs.end();

  s_checked = false;
  s_focusCtrl = -1;
  s_scrollLine = 0;
  s_scrollSmooth = 0.0f;
  s_warnMsg = nullptr;
  s_warnUntil = 0;
}

bool isAccepted() {
  if (!s_inited)
    init();
  return s_accepted;
}

void setAccepted(bool val) {
  s_accepted = val;
  Preferences prefs;
  prefs.begin("espone_set", false);
  prefs.putBool("disclaimer", val);
  prefs.end();


  sys_storage::init();
  File f = FFat.open("/system/sys.config", FILE_APPEND);
  if (f) {
    f.printf("disclaimer_accepted=%d\n", val ? 1 : 0);
    f.close();
  }
}

bool tick(uint32_t now) {
  float ds = (s_scrollLine * lineH) - s_scrollSmooth;
  if (fabsf(ds) > 0.1f)
    s_scrollSmooth += ds * 0.35f;
  else
    s_scrollSmooth = (s_scrollLine * lineH);

  if (s_warnMsg && now > s_warnUntil) {
    s_warnMsg = nullptr;
  }
  return fabsf(ds) > 0.1f || (s_warnMsg != nullptr);
}

void onKey(const KeyEvent &ev) {
  int maxScroll = kTextLineCount - visibleLines + 3;

  if (ev.key == BTN_UP) {
    if (s_focusCtrl == 1) {
      s_focusCtrl = 0;
    } else if (s_focusCtrl == 0) {
      s_focusCtrl = -1;
    } else if (s_scrollLine > 0) {
      s_scrollLine--;
    }
  } else if (ev.key == BTN_DOWN) {
    if (s_focusCtrl == -1) {
      if (s_scrollLine < maxScroll) {
        s_scrollLine++;
      } else {
        s_focusCtrl = 0;
      }
    } else if (s_focusCtrl == 0) {
      s_focusCtrl = 1;
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_focusCtrl == -1) {

      if (s_scrollLine < maxScroll) {
        s_scrollLine += 3;
        if (s_scrollLine >= maxScroll) {
          s_scrollLine = maxScroll;
          s_focusCtrl = 0;
        }
      } else {
        s_focusCtrl = 0;
      }
    } else if (s_focusCtrl == 0) {

      s_checked = !s_checked;
    } else if (s_focusCtrl == 1) {

      if (s_checked) {
        setAccepted(true);
      } else {
        s_warnMsg = "Check box first!";
        s_warnUntil = millis() + 1500;
      }
    }
  }
}

void draw(U8G2 &u8g2, uint32_t now) {

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setDrawColor(1);

  for (int i = 0; i < kTextLineCount; i++) {
    int y = startY + (int)(i * lineH - s_scrollSmooth + 0.5f);
    if (y < startY - lineH || y > 64)
      continue;
    u8g2.drawStr(6, y + 7, kText[i]);
  }


  int cbY = startY + (int)(kTextLineCount * lineH - s_scrollSmooth + 0.5f);
  int btnY = cbY + 12;


  if (cbY >= startY - lineH && cbY <= 64) {
    u8g2.drawFrame(6, cbY, 9, 9);
    if (s_checked) {

      u8g2.drawBox(8, cbY + 2, 5, 5);
    }
    u8g2.drawStr(19, cbY + 7, "I Agree & Accept");

    if (s_focusCtrl == 0) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(4, cbY - 1, 120, 11);
      u8g2.setDrawColor(1);
    }
  }


  if (btnY >= startY - lineH && btnY <= 64) {
    u8g2.drawStr(19, btnY + 7, "CONFIRM & ENTER");

    if (s_focusCtrl == 1) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(4, btnY - 1, 120, 11);
      u8g2.setDrawColor(1);
    }
  }


  int maxScroll = kTextLineCount - visibleLines + 3;
  int barH = 14;
  int barY =
      startY + (s_scrollLine * (46 - barH)) / (maxScroll > 0 ? maxScroll : 1);
  u8g2.drawVLine(127, startY, 48);
  u8g2.drawBox(126, barY, 2, barH);


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "DISCLAIMER");

  if (s_warnMsg) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int ww = u8g2.getStrWidth(s_warnMsg);
    u8g2.drawStr(124 - ww, 10, s_warnMsg);
  }
  u8g2.setDrawColor(1);
}

}
