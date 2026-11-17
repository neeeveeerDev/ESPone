#include "apps.h"
#include <math.h>
#include "file_manager/file_manager.h"
#include "app_manager/app_manager.h"
#include "games/games_menu.h"
#include "clock_app.h"
#include "calendar_app.h"
#include "calculator_app.h"
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"

namespace {
static const char* kAppItems[] = {
  "File Manager",
  "App Manager",
  "Games",
  "Clock",
  "Calendar",
  "Calculator"
};
static const int kAppCount = sizeof(kAppItems) / sizeof(kAppItems[0]);
static const int rowH = 13;
static const int startY = 16;
static std::vector<app_registry::InstalledApp> s_dynApps;

static int getTotalAppCount() {
  return kAppCount + (int)s_dynApps.size();
}
}

void AppsApp::open() {
  s_dynApps = app_registry::getAppsForTarget("apps");
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = startY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.printf("[apps] open (dynamic apps: %d)\n", (int)s_dynApps.size());
}

void AppsApp::close() {
  if (scene_ == SCENE_FILE_MANAGER) {
    file_manager::close();
  } else if (scene_ == SCENE_APP_MANAGER) {
    app_manager::close();
  } else if (scene_ == SCENE_GAMES) {
    games_menu::close();
  } else if (scene_ == SCENE_CLOCK) {
    clock_app::close();
  } else if (scene_ == SCENE_CALENDAR) {
    calendar_app::close();
  } else if (scene_ == SCENE_CALCULATOR) {
    calculator_app::close();
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::close();
  }
  scene_ = SCENE_MENU;
}

bool AppsApp::onBack() {
  if (scene_ == SCENE_JS_APP) {
    js_runner::close();
    scene_ = SCENE_MENU;
    return true;
  }
  if (scene_ == SCENE_FILE_MANAGER) {
    if (file_manager::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_APP_MANAGER) {
    if (app_manager::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_GAMES) {
    if (games_menu::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_CLOCK) {
    if (clock_app::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_CALENDAR) {
    if (calendar_app::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_CALCULATOR) {
    if (calculator_app::onBack()) return true;
    scene_ = SCENE_MENU;
    return true;
  }
  return false;
}

void AppsApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    if (menuSel_ < targetScroll_) targetScroll_ = menuSel_;
    else if (menuSel_ >= targetScroll_ + 3) targetScroll_ = menuSel_ - 2;

    float dScroll = targetScroll_ - scroll_;
    if (fabsf(dScroll) > 0.02f) scroll_ += dScroll * 0.35f;
    else scroll_ = targetScroll_;

    float target = startY + (menuSel_ - scroll_) * rowH;
    float dy = target - barY_;
    if (fabsf(dy) > 0.1f) barY_ += dy * 0.35f;
    else barY_ = target;
  } else if (scene_ == SCENE_FILE_MANAGER) {
    file_manager::tick(now);
  } else if (scene_ == SCENE_APP_MANAGER) {
    app_manager::tick(now);
  } else if (scene_ == SCENE_GAMES) {
    games_menu::tick(now);
  } else if (scene_ == SCENE_CLOCK) {
    clock_app::tick(now);
  } else if (scene_ == SCENE_CALENDAR) {
    calendar_app::tick(now);
  } else if (scene_ == SCENE_CALCULATOR) {
    calculator_app::tick(now);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::tick(now);
  }
}

void AppsApp::onKey(const KeyEvent& ev) {
  int totalCount = getTotalAppCount();

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
      if (menuSel_ < kAppCount) {
        if (menuSel_ == 0) {
          scene_ = SCENE_FILE_MANAGER;
          file_manager::open();
        } else if (menuSel_ == 1) {
          scene_ = SCENE_APP_MANAGER;
          app_manager::open();
        } else if (menuSel_ == 2) {
          scene_ = SCENE_GAMES;
          games_menu::open();
        } else if (menuSel_ == 3) {
          scene_ = SCENE_CLOCK;
          clock_app::open();
        } else if (menuSel_ == 4) {
          scene_ = SCENE_CALENDAR;
          calendar_app::open();
        } else if (menuSel_ == 5) {
          scene_ = SCENE_CALCULATOR;
          calculator_app::open();
        }
      } else {

        int appIdx = menuSel_ - kAppCount;
        if (appIdx >= 0 && appIdx < (int)s_dynApps.size()) {
          scene_ = SCENE_JS_APP;
          js_runner::open(s_dynApps[appIdx]);
        }
      }
    }
  } else if (scene_ == SCENE_FILE_MANAGER) {
    file_manager::onKey(ev);
  } else if (scene_ == SCENE_APP_MANAGER) {
    app_manager::onKey(ev);
  } else if (scene_ == SCENE_GAMES) {
    games_menu::onKey(ev);
  } else if (scene_ == SCENE_CLOCK) {
    clock_app::onKey(ev);
  } else if (scene_ == SCENE_CALENDAR) {
    calendar_app::onKey(ev);
  } else if (scene_ == SCENE_CALCULATOR) {
    calculator_app::onKey(ev);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::onKey(ev);
  }
}

void AppsApp::draw(U8G2& u8g2, uint32_t now) {
  int totalCount = getTotalAppCount();

  if (scene_ == SCENE_MENU) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < totalCount; i++) {
      float itemY = startY + (i - scroll_) * rowH;
      if (itemY >= 12 && itemY <= 64) {
        if (i < kAppCount) {
          u8g2.drawStr(12, (int)itemY + 10, kAppItems[i]);
        } else {
          char title[24];
          snprintf(title, sizeof(title), "> %s", s_dynApps[i - kAppCount].menuTitle.c_str());
          u8g2.drawStr(12, (int)itemY + 10, title);
        }
      }
    }


    int bY = (int)(barY_ + 0.5f);
    if (bY >= 14 && bY <= 60) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(4, bY, 120, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "APPS");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_FILE_MANAGER) {
    file_manager::draw(u8g2, now);
  } else if (scene_ == SCENE_APP_MANAGER) {
    app_manager::draw(u8g2, now);
  } else if (scene_ == SCENE_GAMES) {
    games_menu::draw(u8g2, now);
  } else if (scene_ == SCENE_CLOCK) {
    clock_app::draw(u8g2, now);
  } else if (scene_ == SCENE_CALENDAR) {
    calendar_app::draw(u8g2, now);
  } else if (scene_ == SCENE_CALCULATOR) {
    calculator_app::draw(u8g2, now);
  } else if (scene_ == SCENE_JS_APP) {
    js_runner::draw(u8g2, now);
  }
}
