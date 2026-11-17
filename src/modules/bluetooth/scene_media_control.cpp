#include "scene_media_control.h"
#include "ble_hid.h"
#include "core/ble_service.h"
#include <math.h>

namespace scene_media_control {

enum SubScene : uint8_t {
  SUB_MENU = 0,
  SUB_KEYBOARD,
  SUB_MEDIA,
  SUB_PRESENTER,
  SUB_SHORTCUTS,
  SUB_MOUSE,
  SUB_MOUSE_JIGGLER
};

static SubScene s_scene = SUB_MENU;
static const int kRowH = 12;
static const int kStartY = 16;


static const char* kModes[] = {
  "Keyboard",
  "Media",
  "Presenter",
  "Shortcuts",
  "Mouse",
  "Mouse Jiggler"
};
static const int kModeCount = sizeof(kModes) / sizeof(kModes[0]);
static int s_menuSel = 0;
static float s_menuBarY = (float)kStartY;
static float s_menuScroll = 0.0f;
static float s_menuTargetScroll = 0.0f;


enum KbMode : uint8_t {
  KB_MODE_UPPER = 0,
  KB_MODE_LOWER,
  KB_MODE_NUM,
  KB_MODE_SYM
};

static KbMode s_kbMode = KB_MODE_UPPER;
static int s_kbRow = 0;
static int s_kbCol = 0;
static uint8_t s_armedMod = 0;
static uint32_t s_lastSentFeedback = 0;
static char s_lastSentLabel[16] = "";



static const uint8_t s_icon_bksp[] = {
  0x7C, 0x00, 0x82, 0x00, 0xA9, 0x00, 0x91, 0x00, 0xA9, 0x00, 0x82, 0x00, 0x7C, 0x00
};

static const uint8_t s_icon_shift[] = {
  0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08
};

static const uint8_t s_icon_enter[] = {
  0x40, 0x40, 0x44, 0x46, 0x7F, 0x06, 0x04
};

static const uint8_t s_icon_globe[] = {
  0x3C, 0x66, 0xFF, 0xDB, 0xDB, 0xFF, 0x66, 0x3C
};

static const uint8_t s_icon_gear[] = {
  0x14, 0x2A, 0x5D, 0x36, 0x5D, 0x2A, 0x14
};

struct KeyPos {
  int8_t x;
  int8_t w;
};



static const KeyPos kRow0_pos[10] = {
  { 3, 11 }, { 15, 11 }, { 27, 11 }, { 40, 11 }, { 52, 11 },
  { 65, 11 }, { 77, 11 }, { 89, 11 }, { 102, 11 }, { 114, 11 }
};

static const KeyPos kRow1_pos[10] = {
  { 6, 11 }, { 18, 11 }, { 30, 11 }, { 42, 11 }, { 54, 11 },
  { 66, 11 }, { 78, 11 }, { 90, 11 }, { 102, 11 }, { 114, 11 }
};

static const KeyPos kRow2_pos[10] = {
  { 3, 15 }, { 19, 10 }, { 30, 10 }, { 41, 10 }, { 52, 10 },
  { 63, 10 }, { 74, 10 }, { 85, 10 }, { 96, 11 }, { 109, 16 }
};

static const KeyPos kRow3_pos[5] = {
  { 3, 16 }, { 20, 13 }, { 34, 58 }, { 93, 13 }, { 107, 18 }
};

static const int kRowKeyCounts[4] = { 10, 10, 10, 5 };
static const int kRowY[4] = { 16, 28, 40, 52 };


static const char* kRow0_upper[10] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
static const char* kRow1_upper[10] = { "A", "S", "D", "F", "G", "H", "J", "K", "L", "\b" };
static const char* kRow2_upper[10] = { "^", "Z", "X", "C", "V", "B", "N", "M", ",", "\n" };

static const char* kRow0_lower[10] = { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" };
static const char* kRow1_lower[10] = { "a", "s", "d", "f", "g", "h", "j", "k", "l", "\b" };
static const char* kRow2_lower[10] = { "^", "z", "x", "c", "v", "b", "n", "m", ",", "\n" };

static const char* kRow0_num[10]   = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
static const char* kRow1_num[10]   = { "-", "/", ":", ";", "(", ")", "$", "&", "@", "\b" };
static const char* kRow2_num[10]   = { "#", ".", ",", "?", "!", "'", "\"", "\\", "%", "\n" };

static const char* kRow0_sym[10]   = { "[", "]", "{", "}", "#", "%", "^", "*", "+", "=" };
static const char* kRow1_sym[10]   = { "_", "\\", "|", "~", "<", ">", "$", "`", ";", "\b" };
static const char* kRow2_sym[10]   = { "1", ".", ",", "?", "!", "'", "\"", ":", "/", "\n" };

static int getKeyCenterX(int row, int col) {
  const KeyPos* pos = nullptr;
  if (row == 0) pos = &kRow0_pos[col];
  else if (row == 1) pos = &kRow1_pos[col];
  else if (row == 2) pos = &kRow2_pos[col];
  else if (row == 3) pos = &kRow3_pos[col];
  if (!pos) return 64;
  return pos->x + pos->w / 2;
}

static int findClosestCol(int targetRow, int currentCenterX) {
  int count = kRowKeyCounts[targetRow];
  int bestCol = 0;
  int minDiff = 9999;
  for (int c = 0; c < count; c++) {
    int cx = getKeyCenterX(targetRow, c);
    int diff = abs(cx - currentCenterX);
    if (diff < minDiff) {
      minDiff = diff;
      bestCol = c;
    }
  }
  return bestCol;
}

static void handleKeyboardPress() {

  if (s_kbRow == 3) {
    if (s_kbCol == 0) {

      if (s_kbMode == KB_MODE_UPPER || s_kbMode == KB_MODE_LOWER) {
        s_kbMode = KB_MODE_NUM;
      } else {
        s_kbMode = KB_MODE_UPPER;
      }
      return;
    } else if (s_kbCol == 1) {

      ble_hid::sendKey(0x08, ble_hid::KEY_SPACE);
      snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "Language");
      s_lastSentFeedback = millis();
      return;
    } else if (s_kbCol == 2) {

      ble_hid::sendKey(s_armedMod, ble_hid::KEY_SPACE);
      s_armedMod = 0;
      snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "Space");
      s_lastSentFeedback = millis();
      return;
    } else if (s_kbCol == 3) {

      if (s_armedMod == 0) {
        s_armedMod = 0x01;
        snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "MOD: Ctrl");
      } else if (s_armedMod == 0x01) {
        s_armedMod = 0x04;
        snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "MOD: Alt");
      } else if (s_armedMod == 0x04) {
        s_armedMod = 0x08;
        snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "MOD: Win");
      } else if (s_armedMod == 0x08) {
        s_armedMod = 0;
        ble_hid::sendKey(0, ble_hid::KEY_TAB);
        snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "Tab");
      } else {
        s_armedMod = 0;
        snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "MOD: Off");
      }
      s_lastSentFeedback = millis();
      return;
    } else if (s_kbCol == 4) {

      s_scene = SUB_MENU;
      return;
    }
  }


  if (s_kbRow == 1 && s_kbCol == 9) {
    ble_hid::sendKey(s_armedMod, ble_hid::KEY_BACKSPACE);
    s_armedMod = 0;
    snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "Bksp");
    s_lastSentFeedback = millis();
    return;
  }


  if (s_kbRow == 2 && s_kbCol == 0) {
    if (s_kbMode == KB_MODE_UPPER) {
      s_kbMode = KB_MODE_LOWER;
      snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "lower");
    } else if (s_kbMode == KB_MODE_LOWER) {
      s_kbMode = KB_MODE_UPPER;
      snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "UPPER");
    } else if (s_kbMode == KB_MODE_NUM) {
      s_kbMode = KB_MODE_SYM;
    } else if (s_kbMode == KB_MODE_SYM) {
      s_kbMode = KB_MODE_NUM;
    }
    s_lastSentFeedback = millis();
    return;
  }


  if (s_kbRow == 2 && s_kbCol == 9) {
    ble_hid::sendKey(s_armedMod, ble_hid::KEY_ENTER);
    s_armedMod = 0;
    snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "Enter");
    s_lastSentFeedback = millis();
    return;
  }


  const char* keyLabel = "";
  if (s_kbMode == KB_MODE_UPPER) {
    if (s_kbRow == 0) keyLabel = kRow0_upper[s_kbCol];
    else if (s_kbRow == 1) keyLabel = kRow1_upper[s_kbCol];
    else if (s_kbRow == 2) keyLabel = kRow2_upper[s_kbCol];
  } else if (s_kbMode == KB_MODE_LOWER) {
    if (s_kbRow == 0) keyLabel = kRow0_lower[s_kbCol];
    else if (s_kbRow == 1) keyLabel = kRow1_lower[s_kbCol];
    else if (s_kbRow == 2) keyLabel = kRow2_lower[s_kbCol];
  } else if (s_kbMode == KB_MODE_NUM) {
    if (s_kbRow == 0) keyLabel = kRow0_num[s_kbCol];
    else if (s_kbRow == 1) keyLabel = kRow1_num[s_kbCol];
    else if (s_kbRow == 2) keyLabel = kRow2_num[s_kbCol];
  } else if (s_kbMode == KB_MODE_SYM) {
    if (s_kbRow == 0) keyLabel = kRow0_sym[s_kbCol];
    else if (s_kbRow == 1) keyLabel = kRow1_sym[s_kbCol];
    else if (s_kbRow == 2) keyLabel = kRow2_sym[s_kbCol];
  }

  if (keyLabel && strlen(keyLabel) == 1) {
    char c = keyLabel[0];
    if (s_armedMod != 0) {
      uint8_t keycode = 0;
      if (c >= 'a' && c <= 'z') keycode = 0x04 + (c - 'a');
      else if (c >= 'A' && c <= 'Z') keycode = 0x04 + (c - 'A');
      else if (c >= '1' && c <= '9') keycode = 0x1E + (c - '1');
      else if (c == '0') keycode = 0x27;
      if (keycode != 0) {
        ble_hid::sendKey(s_armedMod, keycode);
      } else {
        ble_hid::sendChar(c);
      }
      s_armedMod = 0;
    } else {
      ble_hid::sendChar(c);
    }
    snprintf(s_lastSentLabel, sizeof(s_lastSentLabel), "%s", keyLabel);
    s_lastSentFeedback = millis();
  }
}


static const char* kMediaCommands[] = {
  "Volume Up",
  "Volume Down",
  "Play / Pause",
  "Stop",
  "Next Track",
  "Prev Track",
  "Mute",
  "Brightness +",
  "Brightness -",
  "Screenshot"
};
static const int kMediaCount = sizeof(kMediaCommands) / sizeof(kMediaCommands[0]);
static int s_mediaSel = 0;
static float s_mediaBarY = (float)kStartY;
static float s_mediaScroll = 0.0f;
static float s_mediaTargetScroll = 0.0f;


static const char* kPresenterCommands[] = {
  "Next Slide",
  "Prev Slide",
  "Start Show (F5)",
  "Exit Show (Esc)",
  "Black Screen (B)",
  "White Screen (W)"
};
static const int kPresenterCount = sizeof(kPresenterCommands) / sizeof(kPresenterCommands[0]);
static int s_presSel = 0;
static float s_presBarY = (float)kStartY;
static float s_presScroll = 0.0f;
static float s_presTargetScroll = 0.0f;


static const char* kShortcutsCommands[] = {
  "Home Screen",
  "Back",
  "Lock Screen",
  "Camera Shutter",
  "Voice Assistant",
  "App Switcher"
};
static const int kShortcutsCount = sizeof(kShortcutsCommands) / sizeof(kShortcutsCommands[0]);
static int s_shortSel = 0;
static float s_shortBarY = (float)kStartY;
static float s_shortScroll = 0.0f;
static float s_shortTargetScroll = 0.0f;


static int s_mouseCurX = 64;
static int s_mouseCurY = 32;
static int s_mouseSubMode = 0;
static const char* kMouseSubModes[] = {"L-CLK", "R-CLK", "SCROLL"};
static bool s_mouseInToolbar = false;
static uint32_t s_mouseFeedbackUntil = 0;
static const char* s_mouseFeedbackText = nullptr;


static const int kJigglerIntervals[] = {5, 15, 30, 60};
static const int kJigglerIntervalCount = 4;
static int s_jigglerIntervalIdx = 1;
static bool s_jigglerActive = false;
static uint32_t s_jigglerLastTime = 0;
static uint32_t s_jigglerFlashUntil = 0;
static uint32_t s_jigglerCount = 0;

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

static void drawHeader(U8G2& u8g2, const char* title) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);
  u8g2.setDrawColor(1);
}

static void drawList(U8G2& u8g2, const char* const* items, int count, float scroll, float barY) {
  u8g2.setClipWindow(0, 14, 128, 64);
  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < count; i++) {
    int y = kStartY + (int)(i * kRowH - scroll + 0.5f);
    if (y < kStartY - kRowH || y > 64) continue;
    u8g2.drawStr(6, y + 10, items[i]);
  }


  u8g2.setDrawColor(2);
  int curY = (int)(barY + 0.5f);
  if (curY < kStartY) curY = kStartY;
  if (curY > 64 - kRowH) curY = 64 - kRowH;
  u8g2.drawBox(2, curY, 124, kRowH);
  u8g2.setDrawColor(1);
  u8g2.setMaxClipWindow();
}

void open() {
  s_scene = SUB_MENU;
  s_menuSel = 0;
  s_menuBarY = (float)kStartY;
  s_menuScroll = 0.0f;
  s_menuTargetScroll = 0.0f;

  s_mediaSel = 0;
  s_mediaBarY = (float)kStartY;
  s_mediaScroll = 0.0f;
  s_mediaTargetScroll = 0.0f;

  s_presSel = 0;
  s_presBarY = (float)kStartY;
  s_presScroll = 0.0f;
  s_presTargetScroll = 0.0f;

  s_shortSel = 0;
  s_shortBarY = (float)kStartY;
  s_shortScroll = 0.0f;
  s_shortTargetScroll = 0.0f;

  s_kbRow = 0;
  s_kbCol = 0;
  s_kbMode = KB_MODE_UPPER;
  s_armedMod = 0;
  s_lastSentFeedback = 0;
  s_lastSentLabel[0] = '\0';

  s_mouseCurX = 64;
  s_mouseCurY = 32;
  s_mouseSubMode = 0;
  s_mouseInToolbar = false;
  s_mouseFeedbackUntil = 0;
  s_mouseFeedbackText = nullptr;

  s_jigglerActive = false;
  s_jigglerCount = 0;
  s_jigglerFlashUntil = 0;
  s_jigglerIntervalIdx = 1;

  ble_service::resumeAdvertising();
}

void close() {
  s_jigglerActive = false;
  s_scene = SUB_MENU;
}

bool onBack() {
  if (s_scene != SUB_MENU) {
    if (s_scene == SUB_MOUSE_JIGGLER) {
      s_jigglerActive = false;
    }
    s_scene = SUB_MENU;
    return true;
  }

  return false;
}

void tick(uint32_t now) {
  if (s_mouseFeedbackText && now >= s_mouseFeedbackUntil) {
    s_mouseFeedbackText = nullptr;
  }
  if (s_jigglerFlashUntil > 0 && now >= s_jigglerFlashUntil) {
    s_jigglerFlashUntil = 0;
  }

  if (s_scene == SUB_MENU) {
    updateScroll(s_menuSel, kModeCount, s_menuScroll, s_menuTargetScroll, s_menuBarY);
  } else if (s_scene == SUB_MEDIA) {
    updateScroll(s_mediaSel, kMediaCount, s_mediaScroll, s_mediaTargetScroll, s_mediaBarY);
  } else if (s_scene == SUB_PRESENTER) {
    updateScroll(s_presSel, kPresenterCount, s_presScroll, s_presTargetScroll, s_presBarY);
  } else if (s_scene == SUB_SHORTCUTS) {
    updateScroll(s_shortSel, kShortcutsCount, s_shortScroll, s_shortTargetScroll, s_shortBarY);
  } else if (s_scene == SUB_MOUSE_JIGGLER) {
    if (s_jigglerActive) {
      uint32_t intervalMs = (uint32_t)kJigglerIntervals[s_jigglerIntervalIdx] * 1000UL;
      if (now - s_jigglerLastTime >= intervalMs) {
        s_jigglerLastTime = now;
        s_jigglerCount++;
        s_jigglerFlashUntil = now + 400;

        int8_t dir = (s_jigglerCount % 2 != 0) ? 2 : -2;
        ble_hid::mouseMove(dir, 0);
      }
    }
  }
}

void onKey(const KeyEvent& ev) {

  if (!ble_service::isConnected() && !ble_hid::isConnected() && s_scene == SUB_MENU) {
    return;
  }

  if (s_scene == SUB_MENU) {
    if (ev.key == BTN_UP) {
      s_menuSel = (s_menuSel - 1 + kModeCount) % kModeCount;
    } else if (ev.key == BTN_DOWN) {
      s_menuSel = (s_menuSel + 1) % kModeCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_menuSel == 0) s_scene = SUB_KEYBOARD;
      else if (s_menuSel == 1) s_scene = SUB_MEDIA;
      else if (s_menuSel == 2) s_scene = SUB_PRESENTER;
      else if (s_menuSel == 3) s_scene = SUB_SHORTCUTS;
      else if (s_menuSel == 4) {
        s_scene = SUB_MOUSE;
        s_mouseCurX = 64;
        s_mouseCurY = 32;
        s_mouseInToolbar = false;
        s_mouseSubMode = 0;
        s_mouseFeedbackText = nullptr;
      } else if (s_menuSel == 5) {
        s_scene = SUB_MOUSE_JIGGLER;
        s_jigglerActive = false;
        s_jigglerCount = 0;
        s_jigglerFlashUntil = 0;
      }
    }
  } else if (s_scene == SUB_KEYBOARD) {
    if (ev.key == BTN_UP) {
      if (s_kbRow > 0) {
        int cx = getKeyCenterX(s_kbRow, s_kbCol);
        s_kbRow--;
        s_kbCol = findClosestCol(s_kbRow, cx);
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_kbRow < 3) {
        int cx = getKeyCenterX(s_kbRow, s_kbCol);
        s_kbRow++;
        s_kbCol = findClosestCol(s_kbRow, cx);
      }
    } else if (ev.key == BTN_LEFT) {
      int count = kRowKeyCounts[s_kbRow];
      s_kbCol = (s_kbCol - 1 + count) % count;
    } else if (ev.key == BTN_RIGHT) {
      int count = kRowKeyCounts[s_kbRow];
      s_kbCol = (s_kbCol + 1) % count;
    } else if (ev.key == BTN_OK) {
      handleKeyboardPress();
    }
  } else if (s_scene == SUB_MEDIA) {
    if (ev.key == BTN_UP) {
      s_mediaSel = (s_mediaSel - 1 + kMediaCount) % kMediaCount;
    } else if (ev.key == BTN_DOWN) {
      s_mediaSel = (s_mediaSel + 1) % kMediaCount;
    } else if (ev.key == BTN_OK) {
      switch (s_mediaSel) {
        case 0: ble_hid::sendConsumer(ble_hid::MEDIA_VOL_UP); break;
        case 1: ble_hid::sendConsumer(ble_hid::MEDIA_VOL_DOWN); break;
        case 2: ble_hid::sendConsumer(ble_hid::MEDIA_PLAY_PAUSE); break;
        case 3: ble_hid::sendConsumer(ble_hid::MEDIA_STOP); break;
        case 4: ble_hid::sendConsumer(ble_hid::MEDIA_NEXT); break;
        case 5: ble_hid::sendConsumer(ble_hid::MEDIA_PREV); break;
        case 6: ble_hid::sendConsumer(ble_hid::MEDIA_MUTE); break;
        case 7: ble_hid::sendConsumer(ble_hid::MEDIA_BRIGHT_UP); break;
        case 8: ble_hid::sendConsumer(ble_hid::MEDIA_BRIGHT_DOWN); break;
        case 9:

          ble_hid::sendConsumer(ble_hid::MEDIA_SNAPSHOT);
          ble_hid::sendKey(0x08, ble_hid::KEY_PRINTSCREEN);
          break;
      }
      strncpy(s_lastSentLabel, kMediaCommands[s_mediaSel], sizeof(s_lastSentLabel) - 1);
      s_lastSentFeedback = millis();
    }
  } else if (s_scene == SUB_PRESENTER) {
    if (ev.key == BTN_UP) {
      s_presSel = (s_presSel - 1 + kPresenterCount) % kPresenterCount;
    } else if (ev.key == BTN_DOWN) {
      s_presSel = (s_presSel + 1) % kPresenterCount;
    } else if (ev.key == BTN_OK) {
      switch (s_presSel) {
        case 0: ble_hid::sendKey(0, ble_hid::KEY_RIGHT_ARROW); break;
        case 1: ble_hid::sendKey(0, ble_hid::KEY_LEFT_ARROW); break;
        case 2: ble_hid::sendKey(0, ble_hid::KEY_F5); break;
        case 3: ble_hid::sendKey(0, ble_hid::KEY_ESC); break;
        case 4: ble_hid::sendKey(0, 0x05); break;
        case 5: ble_hid::sendKey(0, 0x1A); break;
      }
      strncpy(s_lastSentLabel, kPresenterCommands[s_presSel], sizeof(s_lastSentLabel) - 1);
      s_lastSentFeedback = millis();
    }
  } else if (s_scene == SUB_SHORTCUTS) {
    if (ev.key == BTN_UP) {
      s_shortSel = (s_shortSel - 1 + kShortcutsCount) % kShortcutsCount;
    } else if (ev.key == BTN_DOWN) {
      s_shortSel = (s_shortSel + 1) % kShortcutsCount;
    } else if (ev.key == BTN_OK) {
      switch (s_shortSel) {
        case 0: ble_hid::sendConsumer(ble_hid::MEDIA_HOME); break;
        case 1: ble_hid::sendConsumer(ble_hid::MEDIA_BACK); break;
        case 2: ble_hid::sendConsumer(ble_hid::MEDIA_POWER); break;
        case 3: ble_hid::sendConsumer(ble_hid::MEDIA_VOL_UP); break;
        case 4: ble_hid::sendConsumer(ble_hid::MEDIA_VOICE_ASST); break;
        case 5: ble_hid::sendKey(0x04, ble_hid::KEY_TAB); break;
      }
      strncpy(s_lastSentLabel, kShortcutsCommands[s_shortSel], sizeof(s_lastSentLabel) - 1);
      s_lastSentFeedback = millis();
    }
  } else if (s_scene == SUB_MOUSE) {
    int step = ev.repeat ? 14 : 4;
    if (s_mouseInToolbar) {
      if (ev.key == BTN_UP && !ev.repeat) {
        s_mouseInToolbar = false;
      } else if (ev.key == BTN_LEFT && !ev.repeat) {
        s_mouseSubMode = (s_mouseSubMode - 1 + 3) % 3;
      } else if (ev.key == BTN_RIGHT && !ev.repeat) {
        s_mouseSubMode = (s_mouseSubMode + 1) % 3;
      } else if (ev.key == BTN_OK && !ev.repeat) {
        s_mouseInToolbar = false;
      }
    } else {
      if (s_mouseSubMode == 2) {

        if (ev.key == BTN_UP) {
          ble_hid::mouseScroll(1);
          s_mouseFeedbackText = "WHEEL UP";
          s_mouseFeedbackUntil = millis() + 300;
        } else if (ev.key == BTN_DOWN) {
          ble_hid::mouseScroll(-1);
          s_mouseFeedbackText = "WHEEL DN";
          s_mouseFeedbackUntil = millis() + 300;
        } else if (ev.key == BTN_LEFT) {
          s_mouseCurX -= (ev.repeat ? 8 : 4);
          if (s_mouseCurX < 8) s_mouseCurX = 8;
          ble_hid::mouseMove(-(ev.repeat ? 14 : 4), 0);
        } else if (ev.key == BTN_RIGHT) {
          s_mouseCurX += (ev.repeat ? 8 : 4);
          if (s_mouseCurX > 120) s_mouseCurX = 120;
          ble_hid::mouseMove((ev.repeat ? 14 : 4), 0);
        }
      } else {

        int dx = 0, dy = 0;
        if (ev.key == BTN_UP) dy = -step;
        else if (ev.key == BTN_DOWN) dy = step;
        else if (ev.key == BTN_LEFT) dx = -step;
        else if (ev.key == BTN_RIGHT) dx = step;

        if (dx != 0 || dy != 0) {
          ble_hid::mouseMove(dx, dy);
          s_mouseCurX += dx / 2;
          s_mouseCurY += dy / 2;
          if (s_mouseCurX < 8) s_mouseCurX = 8;
          if (s_mouseCurX > 120) s_mouseCurX = 120;
          if (s_mouseCurY < 18) s_mouseCurY = 18;
          if (s_mouseCurY > 44) {
            s_mouseCurY = 44;
            if (dy > 0 && !ev.repeat) {
              s_mouseInToolbar = true;
            }
          }
        }
      }

      if (ev.key == BTN_OK && !ev.repeat) {
        if (s_mouseSubMode == 0) {
          ble_hid::mouseClick(ble_hid::MOUSE_BUTTON_LEFT);
          s_mouseFeedbackText = "L-CLICK";
          s_mouseFeedbackUntil = millis() + 400;
        } else if (s_mouseSubMode == 1) {
          ble_hid::mouseClick(ble_hid::MOUSE_BUTTON_RIGHT);
          s_mouseFeedbackText = "R-CLICK";
          s_mouseFeedbackUntil = millis() + 400;
        } else if (s_mouseSubMode == 2) {
          ble_hid::mouseClick(ble_hid::MOUSE_BUTTON_MIDDLE);
          s_mouseFeedbackText = "M-CLICK";
          s_mouseFeedbackUntil = millis() + 400;
        }
      }
    }
  } else if (s_scene == SUB_MOUSE_JIGGLER) {
    if (ev.key == BTN_UP || ev.key == BTN_LEFT) {
      if (!s_jigglerActive && !ev.repeat) {
        s_jigglerIntervalIdx = (s_jigglerIntervalIdx - 1 + kJigglerIntervalCount) % kJigglerIntervalCount;
      }
    } else if (ev.key == BTN_DOWN || ev.key == BTN_RIGHT) {
      if (!s_jigglerActive && !ev.repeat) {
        s_jigglerIntervalIdx = (s_jigglerIntervalIdx + 1) % kJigglerIntervalCount;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_jigglerActive = !s_jigglerActive;
      if (s_jigglerActive) {
        s_jigglerLastTime = millis();
        s_jigglerFlashUntil = 0;
      }
    }
  }
}

static void drawKeyboard(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(3, 10, "Keyboard");


  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_armedMod != 0) {
    const char* mStr = (s_armedMod == 0x01) ? "[CTRL]" : ((s_armedMod == 0x04) ? "[ALT]" : "[WIN]");
    int mw = u8g2.getStrWidth(mStr);
    u8g2.drawStr(125 - mw, 10, mStr);
  } else if (now - s_lastSentFeedback < 1200 && strlen(s_lastSentLabel) > 0) {
    char fbuf[20];
    snprintf(fbuf, sizeof(fbuf), "> %s", s_lastSentLabel);
    int fw = u8g2.getStrWidth(fbuf);
    u8g2.drawStr(125 - fw, 10, fbuf);
  }


  u8g2.drawHLine(3, 13, 122);


  for (int r = 0; r < 4; r++) {
    int y = kRowY[r];
    int count = kRowKeyCounts[r];

    for (int c = 0; c < count; c++) {
      int x = 0, w = 0;
      if (r == 0) { x = kRow0_pos[c].x; w = kRow0_pos[c].w; }
      else if (r == 1) { x = kRow1_pos[c].x; w = kRow1_pos[c].w; }
      else if (r == 2) { x = kRow2_pos[c].x; w = kRow2_pos[c].w; }
      else if (r == 3) { x = kRow3_pos[c].x; w = kRow3_pos[c].w; }

      bool isSel = (s_kbRow == r && s_kbCol == c);


      if (isSel) {
        u8g2.setDrawColor(1);
        u8g2.drawRBox(x, y, w, 10, 2);
        u8g2.setDrawColor(0);
      } else {
        u8g2.setDrawColor(1);
        u8g2.drawRFrame(x, y, w, 10, 2);
      }


      if (r == 3) {
        if (c == 0) {

          const char* lbl = (s_kbMode == KB_MODE_UPPER || s_kbMode == KB_MODE_LOWER) ? "123" : "ABC";
          u8g2.setFont(u8g2_font_4x6_tr);
          int lw = u8g2.getStrWidth(lbl);
          u8g2.drawStr(x + (w - lw) / 2, y + 7, lbl);
        } else if (c == 1) {

          u8g2.drawXBMP(x + (w - 8) / 2, y + 1, 8, 8, s_icon_globe);
        } else if (c == 2) {

          int lineW = 18;
          u8g2.drawHLine(x + (w - lineW) / 2, y + 4, lineW);
          u8g2.drawHLine(x + (w - lineW) / 2, y + 5, lineW);
        } else if (c == 3) {

          u8g2.drawXBMP(x + (w - 8) / 2, y + 1, 8, 7, s_icon_gear);
        } else if (c == 4) {

          u8g2.setFont(u8g2_font_5x7_tr);
          int lw = u8g2.getStrWidth("OK");
          u8g2.drawStr(x + (w - lw) / 2, y + 8, "OK");
        }
      } else if (r == 1 && c == 9) {

        u8g2.drawXBMP(x + (w - 9) / 2, y + 1, 9, 7, s_icon_bksp);
      } else if (r == 2 && c == 0) {

        if (s_kbMode == KB_MODE_UPPER || s_kbMode == KB_MODE_LOWER) {
          u8g2.drawXBMP(x + (w - 7) / 2, y + 1, 7, 7, s_icon_shift);
        } else {
          const char* lbl = (s_kbMode == KB_MODE_NUM) ? "#+=" : "123";
          u8g2.setFont(u8g2_font_4x6_tr);
          int lw = u8g2.getStrWidth(lbl);
          u8g2.drawStr(x + (w - lw) / 2, y + 7, lbl);
        }
      } else if (r == 2 && c == 9) {

        u8g2.drawXBMP(x + (w - 8) / 2, y + 1, 8, 7, s_icon_enter);
      } else {

        const char* lbl = "";
        if (s_kbMode == KB_MODE_UPPER) {
          if (r == 0) lbl = kRow0_upper[c];
          else if (r == 1) lbl = kRow1_upper[c];
          else if (r == 2) lbl = kRow2_upper[c];
        } else if (s_kbMode == KB_MODE_LOWER) {
          if (r == 0) lbl = kRow0_lower[c];
          else if (r == 1) lbl = kRow1_lower[c];
          else if (r == 2) lbl = kRow2_lower[c];
        } else if (s_kbMode == KB_MODE_NUM) {
          if (r == 0) lbl = kRow0_num[c];
          else if (r == 1) lbl = kRow1_num[c];
          else if (r == 2) lbl = kRow2_num[c];
        } else if (s_kbMode == KB_MODE_SYM) {
          if (r == 0) lbl = kRow0_sym[c];
          else if (r == 1) lbl = kRow1_sym[c];
          else if (r == 2) lbl = kRow2_sym[c];
        }

        u8g2.setFont(u8g2_font_5x7_tr);
        int lw = u8g2.getStrWidth(lbl);
        u8g2.drawStr(x + (w - lw) / 2, y + 8, lbl);
      }
    }
  }

  u8g2.setDrawColor(1);
}

static void drawMouse(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "BLE MOUSE");

  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_mouseFeedbackText) {
    int fw = u8g2.getStrWidth(s_mouseFeedbackText);
    u8g2.drawStr(124 - fw, 10, s_mouseFeedbackText);
  } else {
    int mw = u8g2.getStrWidth(kMouseSubModes[s_mouseSubMode]);
    u8g2.drawStr(124 - mw, 10, kMouseSubModes[s_mouseSubMode]);
  }
  u8g2.setDrawColor(1);


  u8g2.drawFrame(4, 16, 120, 32);


  u8g2.drawDisc(s_mouseCurX, s_mouseCurY, 2);
  u8g2.drawHLine(s_mouseCurX - 4, s_mouseCurY, 9);
  u8g2.drawVLine(s_mouseCurX, s_mouseCurY - 4, 9);


  int btnW = 38;
  int btnH = 11;
  int btnY = 51;
  int xs[3] = { 4, 45, 86 };

  u8g2.setFont(u8g2_font_5x8_tr);
  for (int i = 0; i < 3; i++) {
    bool isSel = (s_mouseSubMode == i);
    if (isSel) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(xs[i], btnY, btnW, btnH);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
      u8g2.drawFrame(xs[i], btnY, btnW, btnH);
    }
    int tw = u8g2.getStrWidth(kMouseSubModes[i]);
    u8g2.drawStr(xs[i] + (btnW - tw) / 2, btnY + 8, kMouseSubModes[i]);
  }
  u8g2.setDrawColor(1);


  if (s_mouseInToolbar) {
    u8g2.setDrawColor(2);
    u8g2.drawFrame(xs[s_mouseSubMode] - 1, btnY - 1, btnW + 2, btnH + 2);
    u8g2.setDrawColor(1);
  }
}

static void drawMouseJiggler(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "JIGGLER");

  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_jigglerFlashUntil > 0) {
    u8g2.drawStr(124 - u8g2.getStrWidth("JIGGLED!"), 10, "JIGGLED!");
  } else {
    const char* st = s_jigglerActive ? "ACTIVE" : "STOPPED";
    u8g2.drawStr(124 - u8g2.getStrWidth(st), 10, st);
  }
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_6x12_tr);
  char intBuf[32];
  snprintf(intBuf, sizeof(intBuf), "Interval: < %ds >", kJigglerIntervals[s_jigglerIntervalIdx]);
  u8g2.drawStr(6, 28, intBuf);

  u8g2.setFont(u8g2_font_5x8_tr);
  char countBuf[32];
  snprintf(countBuf, sizeof(countBuf), "Jiggle count: %u", (unsigned int)s_jigglerCount);
  u8g2.drawStr(6, 39, countBuf);

  if (s_jigglerActive) {
    uint32_t intervalMs = (uint32_t)kJigglerIntervals[s_jigglerIntervalIdx] * 1000UL;
    uint32_t elapsed = now - s_jigglerLastTime;
    uint32_t remainSec = (intervalMs > elapsed) ? (intervalMs - elapsed) / 1000 + 1 : 0;
    char remBuf[32];
    snprintf(remBuf, sizeof(remBuf), "Next in: %u s", (unsigned int)remainSec);
    u8g2.drawStr(6, 49, remBuf);
  } else {
    u8g2.drawStr(6, 49, "Press OK to Start");
  }


  int btnW = 60;
  int btnH = 12;
  int btnX = 34;
  int btnY = 51;
  const char* btnLabel = s_jigglerActive ? "STOP" : "START";

  if (s_jigglerActive) {
    u8g2.drawFrame(btnX, btnY, btnW, btnH);
    u8g2.drawStr(btnX + (btnW - u8g2.getStrWidth(btnLabel)) / 2, btnY + 9, btnLabel);
  } else {
    u8g2.drawBox(btnX, btnY, btnW, btnH);
    u8g2.setDrawColor(0);
    u8g2.drawStr(btnX + (btnW - u8g2.getStrWidth(btnLabel)) / 2, btnY + 9, btnLabel);
    u8g2.setDrawColor(1);
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  if (!ble_service::isConnected() && !ble_hid::isConnected()) {
    drawHeader(u8g2, "MEDIA CONTROL");

    u8g2.setFont(u8g2_font_6x12_tr);
    int dots = (now / 350) % 4;
    char waitBuf[24];
    snprintf(waitBuf, sizeof(waitBuf), "Waiting pairing%.*s", dots, "...");
    int tw = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - tw) / 2, 36, waitBuf);

    u8g2.setFont(u8g2_font_5x8_tr);
    char nameBuf[32];
    snprintf(nameBuf, sizeof(nameBuf), "Device: %s", ble_service::getDeviceName());
    int nw = u8g2.getStrWidth(nameBuf);
    u8g2.drawStr((128 - nw) / 2, 50, nameBuf);
    return;
  }


  switch (s_scene) {
    case SUB_MENU:
      drawHeader(u8g2, "MEDIA CONTROL");
      drawList(u8g2, kModes, kModeCount, s_menuScroll, s_menuBarY);
      break;

    case SUB_KEYBOARD:
      drawKeyboard(u8g2, now);
      break;

    case SUB_MEDIA:
      drawHeader(u8g2, "MEDIA");
      drawList(u8g2, kMediaCommands, kMediaCount, s_mediaScroll, s_mediaBarY);
      break;

    case SUB_PRESENTER:
      drawHeader(u8g2, "PRESENTER");
      drawList(u8g2, kPresenterCommands, kPresenterCount, s_presScroll, s_presBarY);
      break;

    case SUB_SHORTCUTS:
      drawHeader(u8g2, "SHORTCUTS");
      drawList(u8g2, kShortcutsCommands, kShortcutsCount, s_shortScroll, s_shortBarY);
      break;

    case SUB_MOUSE:
      drawMouse(u8g2, now);
      break;

    case SUB_MOUSE_JIGGLER:
      drawMouseJiggler(u8g2, now);
      break;
  }
}

}
