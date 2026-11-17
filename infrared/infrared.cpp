#include "infrared.h"
#include "ir_sender_hw.h"
#include "scene_universal.h"
#include "scene_tv_b_gone.h"
#include "scene_copy.h"
#include "scene_custom_ir.h"
#include "scene_sent.h"
#include "scene_jammer.h"
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"

namespace {
static const char* kMenuItems[] = {
  "Universal Remote",
  "TV-B-Gone",
  "Copy",
  "Custom IR",
  "Sent",
  "IR Jammer"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int rowH = 12;
static const int startY = 15;
static std::vector<app_registry::InstalledApp> s_irApps;

static int getTotalMenuCount() {
  return kMenuCount + (int)s_irApps.size();
}
}

void InfraredApp::open() {
  ir_sender_hw::init();
  s_irApps = app_registry::getAppsForTarget("infrared");
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = startY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.printf("[infrared] open (dynamic apps: %d)\n", (int)s_irApps.size());
}

void InfraredApp::close() {
  if (scene_ == SCENE_UNIVERSAL) scene_universal::close();
  else if (scene_ == SCENE_TV_B_GONE) scene_tv_b_gone::close();
  else if (scene_ == SCENE_COPY) scene_copy::close();
  else if (scene_ == SCENE_CUSTOM_IR) scene_custom_ir::close();
  else if (scene_ == SCENE_SENT) scene_sent::close();
  else if (scene_ == SCENE_JAMMER) scene_jammer::close();
  else if (scene_ == SCENE_JS_APP) js_runner::close();
  scene_ = SCENE_MENU;
}

bool InfraredApp::onBack() {
  if (scene_ == SCENE_JS_APP) {
    js_runner::close();
    scene_ = SCENE_MENU;
    return true;
  }
  if (scene_ == SCENE_UNIVERSAL) {
    if (scene_universal::onBack()) return true;
  } else if (scene_ == SCENE_TV_B_GONE) {
    if (scene_tv_b_gone::onBack()) return true;
  } else if (scene_ == SCENE_CUSTOM_IR) {
    if (scene_custom_ir::onBack()) return true;
  } else if (scene_ == SCENE_SENT) {
    if (scene_sent::onBack()) return true;
  }

  if (scene_ != SCENE_MENU) {
    if (scene_ == SCENE_UNIVERSAL) scene_universal::close();
    else if (scene_ == SCENE_TV_B_GONE) scene_tv_b_gone::close();
    else if (scene_ == SCENE_COPY) scene_copy::close();
    else if (scene_ == SCENE_CUSTOM_IR) scene_custom_ir::close();
    else if (scene_ == SCENE_SENT) scene_sent::close();
    else if (scene_ == SCENE_JAMMER) scene_jammer::close();

    scene_ = SCENE_MENU;
    return true;
  }
  return false;
}

void InfraredApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    if (menuSel_ < targetScroll_) targetScroll_ = menuSel_;
    else if (menuSel_ >= targetScroll_ + 4) targetScroll_ = menuSel_ - 3;

    float dScroll = targetScroll_ - scroll_;
    if (fabsf(dScroll) > 0.02f) scroll_ += dScroll * 0.35f;
    else scroll_ = targetScroll_;

    float target = startY + (menuSel_ - scroll_) * rowH;
    float dy = target - barY_;
    if (fabsf(dy) > 0.1f) barY_ += dy * 0.35f;
    else barY_ = target;
  } else if (scene_ == SCENE_UNIVERSAL) {
    scene_universal::tick(now);
  } else if (scene_ == SCENE_TV_B_GONE) {
    scene_tv_b_gone::tick(now);
  } else if (scene_ == SCENE_COPY) {
    scene_copy::tick(now);
  } else if (scene_ == SCENE_CUSTOM_IR) {
    scene_custom_ir::tick(now);
  } else if (scene_ == SCENE_SENT) {
    scene_sent::tick(now);
  } else if (scene_ == SCENE_JAMMER) {
    scene_jammer::tick(now);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::tick(now);
  }
}

void InfraredApp::onKey(const KeyEvent& ev) {
  int totalCount = getTotalMenuCount();

  if (scene_ == SCENE_MENU) {
    if (ev.key == BTN_UP) {
      if (menuSel_ > 0) {
        menuSel_--;
      } else {
        menuSel_ = totalCount > 0 ? totalCount - 1 : 0;
      }
    } else if (ev.key == BTN_DOWN) {
      if (menuSel_ < totalCount - 1) {
        menuSel_++;
      } else {
        menuSel_ = 0;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (menuSel_ < kMenuCount) {
        if (menuSel_ == 0) {
          scene_ = SCENE_UNIVERSAL;
          scene_universal::open();
        } else if (menuSel_ == 1) {
          scene_ = SCENE_TV_B_GONE;
          scene_tv_b_gone::open();
        } else if (menuSel_ == 2) {
          scene_ = SCENE_COPY;
          scene_copy::open();
        } else if (menuSel_ == 3) {
          scene_ = SCENE_CUSTOM_IR;
          scene_custom_ir::open();
        } else if (menuSel_ == 4) {
          scene_ = SCENE_SENT;
          scene_sent::open();
        } else if (menuSel_ == 5) {
          scene_ = SCENE_JAMMER;
          scene_jammer::open();
        }
      } else {

        int appIdx = menuSel_ - kMenuCount;
        if (appIdx >= 0 && appIdx < (int)s_irApps.size()) {
          scene_ = SCENE_JS_APP;
          js_runner::open(s_irApps[appIdx]);
        }
      }
    }
  } else if (scene_ == SCENE_UNIVERSAL) {
    scene_universal::onKey(ev);
  } else if (scene_ == SCENE_TV_B_GONE) {
    scene_tv_b_gone::onKey(ev);
  } else if (scene_ == SCENE_COPY) {
    scene_copy::onKey(ev);
  } else if (scene_ == SCENE_CUSTOM_IR) {
    scene_custom_ir::onKey(ev);
  } else if (scene_ == SCENE_SENT) {
    scene_sent::onKey(ev);
  } else if (scene_ == SCENE_JAMMER) {
    scene_jammer::onKey(ev);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::onKey(ev);
  }
}

void InfraredApp::draw(U8G2& u8g2, uint32_t now) {
  int totalCount = getTotalMenuCount();

  if (scene_ == SCENE_MENU) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < totalCount; i++) {
      int y = startY + (int)((i - scroll_) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;

      if (i < kMenuCount) {
        u8g2.drawStr(6, y + 10, kMenuItems[i]);
      } else {

        char title[24];
        snprintf(title, sizeof(title), "> %s", s_irApps[i - kMenuCount].menuTitle.c_str());
        u8g2.drawStr(6, y + 10, title);
      }
    }


    int bY = (int)(barY_ + 0.5f);
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
    u8g2.drawStr(4, 11, "INFRARED");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_UNIVERSAL) {
    scene_universal::draw(u8g2, now);
  } else if (scene_ == SCENE_TV_B_GONE) {
    scene_tv_b_gone::draw(u8g2, now);
  } else if (scene_ == SCENE_COPY) {
    scene_copy::draw(u8g2, now);
  } else if (scene_ == SCENE_CUSTOM_IR) {
    scene_custom_ir::draw(u8g2, now);
  } else if (scene_ == SCENE_SENT) {
    scene_sent::draw(u8g2, now);
  } else if (scene_ == SCENE_JAMMER) {
    scene_jammer::draw(u8g2, now);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::draw(u8g2, now);
  }
}
