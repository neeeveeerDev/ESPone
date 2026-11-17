#include "scene_custom_ir.h"
#include <FFat.h>
#include <math.h>
#include "ir_db.h"
#include "flipper_ir.h"
#include "ir_sender_hw.h"

namespace scene_custom_ir {

enum SubScene {
  SUB_CATEGORIES = 0,
  SUB_BUTTON_LIST,
  SUB_LEARNING
};

static SubScene s_sub = SUB_CATEGORIES;
static ir_db::UniversalCategory s_cat = ir_db::CAT_TV;


static int s_catSel = 0;
static float s_catBarY = 16.0f;
static float s_catScroll = 0.0f;
static float s_catTargetScroll = 0.0f;


static int s_btnSel = 0;
static float s_btnBarY = 16.0f;
static float s_btnScroll = 0.0f;
static float s_btnTargetScroll = 0.0f;

struct LearnedBtn {
  bool learned = false;
  flipper_ir::Signal sig;
};
static std::vector<LearnedBtn> s_buttons;

static int s_activeLearnIdx = -1;
static uint32_t s_feedbackUntil = 0;
static const char* s_feedbackMsg = nullptr;

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static void setupButtonsForCategory(ir_db::UniversalCategory cat) {
  s_buttons.clear();
  int count = ir_db::getCategoryButtonCount(cat);
  for (int i = 0; i < count; i++) {
    LearnedBtn lb;
    lb.learned = false;
    lb.sig.name = ir_db::getCategoryButtonName(cat, i);
    lb.sig.isRaw = false;
    s_buttons.push_back(lb);
  }
}

void open() {
  s_sub = SUB_CATEGORIES;
  s_catSel = 0;
  s_catBarY = startY;
  s_catScroll = 0.0f;
  s_catTargetScroll = 0.0f;
  s_btnSel = 0;
  s_btnBarY = startY;
  s_btnScroll = 0.0f;
  s_btnTargetScroll = 0.0f;
  s_activeLearnIdx = -1;
  s_feedbackMsg = nullptr;
  s_feedbackUntil = 0;
}

void close() {
  if (s_sub == SUB_LEARNING) {
    ir_sender_hw::stopReceiver();
  }
  s_buttons.clear();
  s_sub = SUB_CATEGORIES;
}

bool onBack() {
  if (s_sub == SUB_LEARNING) {
    ir_sender_hw::stopReceiver();
    s_sub = SUB_BUTTON_LIST;
    return true;
  }
  if (s_sub == SUB_BUTTON_LIST) {
    s_sub = SUB_CATEGORIES;
    return true;
  }
  return false;
}

static String generateNewRemotePath(ir_db::UniversalCategory cat) {
  const char* catName = ir_db::getCategoryName(cat);
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "%s", catName);
  for (int i = 0; prefix[i]; i++) prefix[i] = tolower(prefix[i]);

  for (int num = 1; num < 100; num++) {
    char p[48];
    snprintf(p, sizeof(p), "/ir/%s_%02d.ir", prefix, num);
    if (!FFat.exists(p)) {
      return String(p);
    }
  }
  return String("/ir/custom.ir");
}

static bool saveLearnedRemote() {
  int learnedCount = 0;
  for (const auto& b : s_buttons) {
    if (b.learned) learnedCount++;
  }
  if (learnedCount == 0) return false;

  String filePath = generateNewRemotePath(s_cat);
  bool first = true;
  for (const auto& b : s_buttons) {
    if (b.learned) {
      flipper_ir::saveSignalToFile(FFat, filePath.c_str(), b.sig, !first);
      first = false;
    }
  }
  return true;
}

void tick(uint32_t now) {
  if (s_sub == SUB_CATEGORIES) {
    float ds = s_catTargetScroll - s_catScroll;
    if (fabsf(ds) > 0.05f) s_catScroll += ds * 0.35f;
    else s_catScroll = s_catTargetScroll;

    float targetY = startY + (s_catSel - s_catScroll) * rowH;
    float dy = targetY - s_catBarY;
    if (fabsf(dy) > 0.1f) s_catBarY += dy * 0.35f;
    else s_catBarY = targetY;
  } else if (s_sub == SUB_BUTTON_LIST) {
    int total = (int)s_buttons.size() + 1;
    float ds = s_btnTargetScroll - s_btnScroll;
    if (fabsf(ds) > 0.05f) s_btnScroll += ds * 0.35f;
    else s_btnScroll = s_btnTargetScroll;

    float targetY = startY + (s_btnSel - s_btnScroll) * rowH;
    float dy = targetY - s_btnBarY;
    if (fabsf(dy) > 0.1f) s_btnBarY += dy * 0.35f;
    else s_btnBarY = targetY;

    if (s_feedbackMsg && now > s_feedbackUntil) {
      s_feedbackMsg = nullptr;
    }
  } else if (s_sub == SUB_LEARNING) {
    ir_sender_hw::IrDecodedResult res;
    if (ir_sender_hw::pollReceiver(res) && res.valid) {
      if (s_activeLearnIdx >= 0 && s_activeLearnIdx < (int)s_buttons.size()) {
        s_buttons[s_activeLearnIdx].learned = true;
        s_buttons[s_activeLearnIdx].sig.protocol = res.protoName;
        s_buttons[s_activeLearnIdx].sig.address = res.address;
        s_buttons[s_activeLearnIdx].sig.command = res.command;
      }
      ir_sender_hw::stopReceiver();
      s_sub = SUB_BUTTON_LIST;
      s_feedbackMsg = "Captured!";
      s_feedbackUntil = now + 1200;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_sub == SUB_CATEGORIES) {
    int count = (int)ir_db::CAT_COUNT;
    if (ev.key == BTN_UP) {
      if (s_catSel == 0) {
        s_catSel = count - 1;
        s_catTargetScroll = (count > visibleRows) ? (count - visibleRows) : 0;
      } else {
        s_catSel--;
        if (s_catSel < s_catTargetScroll) s_catTargetScroll = s_catSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_catSel == count - 1) {
        s_catSel = 0;
        s_catTargetScroll = 0.0f;
      } else {
        s_catSel++;
        if (s_catSel >= s_catTargetScroll + visibleRows) s_catTargetScroll = s_catSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_cat = (ir_db::UniversalCategory)s_catSel;
      setupButtonsForCategory(s_cat);
      s_sub = SUB_BUTTON_LIST;
      s_btnSel = 0;
      s_btnBarY = startY;
      s_btnScroll = 0.0f;
      s_btnTargetScroll = 0.0f;
    }
  } else if (s_sub == SUB_BUTTON_LIST) {
    int total = (int)s_buttons.size() + 1;

    if (ev.key == BTN_UP) {
      if (s_btnSel == 0) {
        s_btnSel = total - 1;
        s_btnTargetScroll = (total > visibleRows) ? (total - visibleRows) : 0;
      } else {
        s_btnSel--;
        if (s_btnSel < s_btnTargetScroll) s_btnTargetScroll = s_btnSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_btnSel == total - 1) {
        s_btnSel = 0;
        s_btnTargetScroll = 0.0f;
      } else {
        s_btnSel++;
        if (s_btnSel >= s_btnTargetScroll + visibleRows) s_btnTargetScroll = s_btnSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_btnSel < (int)s_buttons.size()) {

        s_activeLearnIdx = s_btnSel;
        s_sub = SUB_LEARNING;
        ir_sender_hw::startReceiver();
      } else {

        if (saveLearnedRemote()) {
          s_feedbackMsg = "Saved to /ir!";
          s_feedbackUntil = millis() + 2000;
        } else {
          s_feedbackMsg = "No buttons learned!";
          s_feedbackUntil = millis() + 1500;
        }
      }
    }
  } else if (s_sub == SUB_LEARNING) {
    if (ev.key == BTN_BACK) {
      ir_sender_hw::stopReceiver();
      s_sub = SUB_BUTTON_LIST;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  if (s_sub == SUB_CATEGORIES) {

    for (int i = 0; i < (int)ir_db::CAT_COUNT; i++) {
      int y = startY + (int)((i - s_catScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, ir_db::getCategoryName((ir_db::UniversalCategory)i));
    }


    int bY = (int)(s_catBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "CUSTOM IR");
    u8g2.setDrawColor(1);
  } else if (s_sub == SUB_BUTTON_LIST) {
    int total = (int)s_buttons.size() + 1;


    for (int i = 0; i < total; i++) {
      int y = startY + (int)((i - s_btnScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;

      if (i < (int)s_buttons.size()) {
        const auto& b = s_buttons[i];
        u8g2.drawStr(6, y + 10, b.sig.name.c_str());
        if (b.learned) {
          u8g2.drawStr(104, y + 10, "OK");
        } else {
          u8g2.drawStr(108, y + 10, "-");
        }
      } else {
        u8g2.drawStr(6, y + 10, "SAVE REMOTE");
      }
    }


    int bY = (int)(s_btnBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);

    char titleBuf[24];
    snprintf(titleBuf, sizeof(titleBuf), "%s REMOTE", ir_db::getCategoryName(s_cat));
    u8g2.drawStr(4, 11, titleBuf);

    if (s_feedbackMsg) {
      u8g2.setFont(u8g2_font_5x8_tr);
      int fw = u8g2.getStrWidth(s_feedbackMsg);
      u8g2.drawStr(124 - fw, 10, s_feedbackMsg);
    }
    u8g2.setDrawColor(1);
  } else if (s_sub == SUB_LEARNING) {

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "LEARN BUTTON");
    u8g2.setDrawColor(1);

    if (s_activeLearnIdx >= 0 && s_activeLearnIdx < (int)s_buttons.size()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      char bBuf[32];
      snprintf(bBuf, sizeof(bBuf), "Button: %s", s_buttons[s_activeLearnIdx].sig.name.c_str());
      u8g2.drawStr(4, 30, bBuf);

      int dots = (now / 350) % 4;
      char waitBuf[24];
      snprintf(waitBuf, sizeof(waitBuf), "Waiting signal%.*s", dots, "...");
      u8g2.drawStr(4, 46, waitBuf);
    }
  }
}

}
