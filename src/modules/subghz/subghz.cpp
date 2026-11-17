#include "subghz.h"
#include "cc1101_driver.h"
#include "core/sys_storage.h"
#include "scene_read.h"
#include "scene_read_raw.h"
#include "scene_saved.h"
#include "scene_custom_sub.h"
#include "scene_add_manually.h"
#include "scene_freq_analyzer.h"
#include "scene_spectrum.h"
#include "scene_radio_settings.h"
#include "scene_bruteforce.h"
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"
#include <math.h>
#include <vector>

namespace {
static const char* kMenuItems[] = {
  "Read",
  "Read RAW",
  "Bruteforce",
  "Spectrum",
  "Saved",
  "Custom Remote",
  "Add Manually",
  "Frequency Analyzer",
  "Radio Settings"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int rowH = 12;
static const int startY = 16;
static std::vector<app_registry::InstalledApp> s_subghzApps;

static int getTotalMenuCount() {
  return kMenuCount + (int)s_subghzApps.size();
}
}

void SubghzApp::open() {
  cc1101::init();
  sys_storage::init();
  s_subghzApps = app_registry::getAppsForTarget("subghz");
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = startY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.printf("[subghz] open (dynamic apps: %d)\n", (int)s_subghzApps.size());
}

void SubghzApp::close() {
  if (scene_ == SCENE_READ) scene_read::close();
  else if (scene_ == SCENE_READ_RAW) scene_read_raw::close();
  else if (scene_ == SCENE_SAVED) scene_saved::close();
  else if (scene_ == SCENE_CUSTOM_SUB) scene_custom_sub::close();
  else if (scene_ == SCENE_ADD_MANUALLY) scene_add_manually::close();
  else if (scene_ == SCENE_FREQ_ANALYZER) scene_freq_analyzer::close();
  else if (scene_ == SCENE_SPECTRUM) scene_spectrum::close();
  else if (scene_ == SCENE_RADIO_SETTINGS) scene_radio_settings::close();
  else if (scene_ == SCENE_BRUTEFORCE) scene_bruteforce::close();
  else if (scene_ == SCENE_JS_APP) js_runner::close();
  scene_ = SCENE_MENU;
}

bool SubghzApp::onBack() {
  if (scene_ == SCENE_JS_APP) {
    js_runner::close();
    scene_ = SCENE_MENU;
    return true;
  }

  if (scene_ == SCENE_SAVED) {
    if (scene_saved::onBack()) return true;
  } else if (scene_ == SCENE_CUSTOM_SUB) {
    if (scene_custom_sub::onBack()) return true;
  } else if (scene_ == SCENE_SPECTRUM) {
    if (scene_spectrum::onBack()) return true;
  } else if (scene_ == SCENE_BRUTEFORCE) {
    if (scene_bruteforce::onBack()) return true;
  }

  if (scene_ != SCENE_MENU) {
    if (scene_ == SCENE_READ) scene_read::close();
    else if (scene_ == SCENE_READ_RAW) scene_read_raw::close();
    else if (scene_ == SCENE_SAVED) scene_saved::close();
    else if (scene_ == SCENE_CUSTOM_SUB) scene_custom_sub::close();
    else if (scene_ == SCENE_ADD_MANUALLY) scene_add_manually::close();
    else if (scene_ == SCENE_FREQ_ANALYZER) scene_freq_analyzer::close();
    else if (scene_ == SCENE_SPECTRUM) scene_spectrum::close();
    else if (scene_ == SCENE_RADIO_SETTINGS) scene_radio_settings::close();
    else if (scene_ == SCENE_BRUTEFORCE) scene_bruteforce::close();

    scene_ = SCENE_MENU;
    return true;
  }

  return false;
}

void SubghzApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    int total = getTotalMenuCount();
    if (total == 0) return;

    if (ev.key == BTN_UP) {
      if (menuSel_ == 0) {
        menuSel_ = total - 1;
      } else {
        menuSel_--;
      }
    } else if (ev.key == BTN_DOWN) {
      if (menuSel_ == total - 1) {
        menuSel_ = 0;
      } else {
        menuSel_++;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (menuSel_ < kMenuCount) {
        switch (menuSel_) {
          case 0:
            scene_ = SCENE_READ;
            scene_read::open();
            break;
          case 1:
            scene_ = SCENE_READ_RAW;
            scene_read::open();
            scene_read_raw::open();
            break;
          case 2:
            scene_ = SCENE_BRUTEFORCE;
            scene_bruteforce::open();
            break;
          case 3:
            scene_ = SCENE_SPECTRUM;
            scene_spectrum::open();
            break;
          case 4:
            scene_ = SCENE_SAVED;
            scene_saved::open();
            break;
          case 5:
            scene_ = SCENE_CUSTOM_SUB;
            scene_custom_sub::open();
            break;
          case 6:
            scene_ = SCENE_ADD_MANUALLY;
            scene_add_manually::open();
            break;
          case 7:
            scene_ = SCENE_FREQ_ANALYZER;
            scene_freq_analyzer::open();
            break;
          case 8:
            scene_ = SCENE_RADIO_SETTINGS;
            scene_radio_settings::open();
            break;
        }
      } else {
        int idx = menuSel_ - kMenuCount;
        if (idx >= 0 && idx < (int)s_subghzApps.size()) {
          scene_ = SCENE_JS_APP;
          js_runner::open(s_subghzApps[idx]);
        }
      }
    }
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::onKey(ev);
  } else {
    switch (scene_) {
      case SCENE_READ:           scene_read::onKey(ev); break;
      case SCENE_READ_RAW:       scene_read_raw::onKey(ev); break;
      case SCENE_SAVED:          scene_saved::onKey(ev); break;
      case SCENE_CUSTOM_SUB:     scene_custom_sub::onKey(ev); break;
      case SCENE_ADD_MANUALLY:   scene_add_manually::onKey(ev); break;
      case SCENE_FREQ_ANALYZER:  scene_freq_analyzer::onKey(ev); break;
      case SCENE_SPECTRUM:       scene_spectrum::onKey(ev); break;
      case SCENE_RADIO_SETTINGS: scene_radio_settings::onKey(ev); break;
      case SCENE_BRUTEFORCE:     scene_bruteforce::onKey(ev); break;
      default: break;
    }
  }
}

void SubghzApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    int total = getTotalMenuCount();
    int maxLines = 4;
    int maxScroll = total - maxLines;
    if (maxScroll < 0) maxScroll = 0;

    int topIndex = (int)(targetScroll_ / rowH);
    if (menuSel_ < topIndex) targetScroll_ = menuSel_ * rowH;
    else if (menuSel_ >= topIndex + maxLines) targetScroll_ = (menuSel_ - maxLines + 1) * rowH;

    if (targetScroll_ < 0) targetScroll_ = 0;
    if (targetScroll_ > maxScroll * rowH) targetScroll_ = maxScroll * rowH;

    float ds = targetScroll_ - scroll_;
    if (fabsf(ds) > 0.1f) scroll_ += ds * 0.35f;
    else scroll_ = targetScroll_;

    float targetBarY = startY + menuSel_ * rowH - scroll_;
    float dy = targetBarY - barY_;
    if (fabsf(dy) > 0.1f) barY_ += dy * 0.4f;
    else barY_ = targetBarY;
  } else if (scene_ == SCENE_JS_APP) {
    if (!js_runner::isActive()) {
      scene_ = SCENE_MENU;
      return;
    }
    js_runner::tick(now);
  } else {
    switch (scene_) {
      case SCENE_READ:           scene_read::tick(now); break;
      case SCENE_READ_RAW:       scene_read_raw::tick(now); break;
      case SCENE_SAVED:          scene_saved::tick(now); break;
      case SCENE_CUSTOM_SUB:     scene_custom_sub::tick(now); break;
      case SCENE_ADD_MANUALLY:   scene_add_manually::tick(now); break;
      case SCENE_FREQ_ANALYZER:  scene_freq_analyzer::tick(now); break;
      case SCENE_SPECTRUM:       scene_spectrum::tick(now); break;
      case SCENE_RADIO_SETTINGS: scene_radio_settings::tick(now); break;
      case SCENE_BRUTEFORCE:     scene_bruteforce::tick(now); break;
      default: break;
    }
  }
}

void SubghzApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {
    int total = getTotalMenuCount();


    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < total; i++) {
      int y = startY + (int)(i * rowH - scroll_ + 0.5f);
      if (y < 14 || y > 64) continue;
      if (i < kMenuCount) {
        u8g2.drawStr(6, y + 10, kMenuItems[i]);
      } else {
        int idx = i - kMenuCount;
        u8g2.drawStr(6, y + 10, s_subghzApps[idx].menuTitle.c_str());
        u8g2.drawStr(110, y + 10, "JS");
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
    u8g2.drawStr(4, 11, "SUB-GHZ");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::draw(u8g2, now);
  } else {
    switch (scene_) {
      case SCENE_READ:           scene_read::draw(u8g2, now); break;
      case SCENE_READ_RAW:       scene_read_raw::draw(u8g2, now); break;
      case SCENE_SAVED:          scene_saved::draw(u8g2, now); break;
      case SCENE_CUSTOM_SUB:     scene_custom_sub::draw(u8g2, now); break;
      case SCENE_ADD_MANUALLY:   scene_add_manually::draw(u8g2, now); break;
      case SCENE_FREQ_ANALYZER:  scene_freq_analyzer::draw(u8g2, now); break;
      case SCENE_SPECTRUM:       scene_spectrum::draw(u8g2, now); break;
      case SCENE_RADIO_SETTINGS: scene_radio_settings::draw(u8g2, now); break;
      case SCENE_BRUTEFORCE:     scene_bruteforce::draw(u8g2, now); break;
      default: break;
    }
  }
}
