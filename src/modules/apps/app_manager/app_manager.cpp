#include "app_manager.h"
#include <math.h>
#include <vector>
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"

namespace app_manager {

enum Scene {
  SCENE_LIST = 0,
  SCENE_DETAILS,
  SCENE_CONFIRM_DELETE,
  SCENE_RUNNING
};

static Scene s_scene = SCENE_LIST;
static std::vector<app_registry::InstalledApp> s_apps;
static int s_sel = 0;
static float s_barY = 16.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;

static const int kRowH = 12;
static const int kStartY = 16;
static const int kVisibleRows = 4;


static app_registry::InstalledApp s_currentApp;
static size_t s_currentSize = 0;
static int s_btnSel = 0;
static int s_confirmSel = 0;

static String formatSize(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < 1048576) {
    float kb = bytes / 1024.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f KB", kb);
    return String(buf);
  } else {
    float mb = bytes / 1048576.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f MB", mb);
    return String(buf);
  }
}

void open() {
  s_scene = SCENE_LIST;
  s_apps = app_registry::getAllApps();
  s_sel = 0;
  s_barY = kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_btnSel = 0;
  s_confirmSel = 0;
}

void close() {
  if (s_scene == SCENE_RUNNING) {
    js_runner::close();
  }
  s_scene = SCENE_LIST;
}

void tick(uint32_t now) {
  if (s_scene == SCENE_RUNNING) {
    js_runner::tick(now);
    return;
  }

  if (s_scene == SCENE_LIST && !s_apps.empty()) {
    if (s_sel < s_targetScroll) s_targetScroll = s_sel;
    else if (s_sel >= s_targetScroll + kVisibleRows) s_targetScroll = s_sel - kVisibleRows + 1;

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.05f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;

    float targetY = kStartY + (s_sel - s_scroll) * kRowH;
    float dy = targetY - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
    else s_barY = targetY;
  }
}

bool onBack() {
  if (s_scene == SCENE_RUNNING) {
    js_runner::close();
    s_scene = SCENE_DETAILS;
    return true;
  }
  if (s_scene == SCENE_CONFIRM_DELETE) {
    s_scene = SCENE_DETAILS;
    return true;
  }
  if (s_scene == SCENE_DETAILS) {
    s_scene = SCENE_LIST;
    return true;
  }
  return false;
}

void onKey(const KeyEvent& ev) {
  if (s_scene == SCENE_RUNNING) {
    js_runner::onKey(ev);
    return;
  }

  if (s_scene == SCENE_LIST) {
    int total = (int)s_apps.size();
    if (total == 0) return;

    if (ev.key == BTN_UP) {
      if (s_sel > 0) s_sel--;
      else s_sel = total - 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_sel < total - 1) s_sel++;
      else s_sel = 0;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_sel >= 0 && s_sel < total) {
        s_currentApp = s_apps[s_sel];
        s_currentSize = app_registry::getAppSize(s_currentApp);
        s_btnSel = 0;
        s_scene = SCENE_DETAILS;
      }
    }
  } else if (s_scene == SCENE_DETAILS) {
    if (ev.key == BTN_LEFT) {
      s_btnSel = 0;
    } else if (ev.key == BTN_RIGHT) {
      s_btnSel = 1;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_btnSel == 0) {

        s_scene = SCENE_RUNNING;
        js_runner::open(s_currentApp);
      } else {

        s_confirmSel = 0;
        s_scene = SCENE_CONFIRM_DELETE;
      }
    }
  } else if (s_scene == SCENE_CONFIRM_DELETE) {
    if (ev.key == BTN_LEFT) {
      s_confirmSel = 0;
    } else if (ev.key == BTN_RIGHT) {
      s_confirmSel = 1;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_confirmSel == 0) {
        s_scene = SCENE_DETAILS;
      } else {

        app_registry::deleteApp(s_currentApp);
        s_apps = app_registry::getAllApps();
        if (s_sel >= (int)s_apps.size()) {
          s_sel = s_apps.empty() ? 0 : (int)s_apps.size() - 1;
        }
        s_scene = SCENE_LIST;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_scene == SCENE_RUNNING) {
    js_runner::draw(u8g2, now);
    return;
  }

  if (s_scene == SCENE_LIST) {

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "APPS");

    int total = (int)s_apps.size();
    char cntStr[12];
    snprintf(cntStr, sizeof(cntStr), "%d", total);
    u8g2.setFont(u8g2_font_6x12_tr);
    int cw = u8g2.getStrWidth(cntStr);
    u8g2.drawStr(124 - cw, 11, cntStr);
    u8g2.setDrawColor(1);

    if (total == 0) {

      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(42, 38, "No Apps");
      return;
    }


    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < total; i++) {
      int y = kStartY + (int)((i - s_scroll) * kRowH + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      const auto& app = s_apps[i];

      u8g2.drawStr(6, y + 9, app.name.c_str());


      u8g2.setFont(u8g2_font_4x6_tr);
      int tw = u8g2.getStrWidth(app.targetMenu.c_str());
      u8g2.drawStr(122 - tw, y + 9, app.targetMenu.c_str());
      u8g2.setFont(u8g2_font_6x12_tr);
    }


    int bY = (int)(s_barY + 0.5f);
    if (bY >= kStartY - 2 && bY <= 64 - kRowH + 2) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, kRowH);
      u8g2.setDrawColor(1);
    }
  } else if (s_scene == SCENE_DETAILS) {

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, s_currentApp.name.c_str());
    u8g2.setDrawColor(1);


    u8g2.setFont(u8g2_font_6x10_tr);
    char buf[36];

    snprintf(buf, sizeof(buf), "Ver: %s", s_currentApp.version.c_str());
    u8g2.drawStr(6, 25, buf);

    snprintf(buf, sizeof(buf), "Author: %s", s_currentApp.author.c_str());
    u8g2.drawStr(6, 35, buf);

    String szStr = formatSize(s_currentSize);
    snprintf(buf, sizeof(buf), "Size: %s", szStr.c_str());
    u8g2.drawStr(6, 45, buf);


    u8g2.setFont(u8g2_font_6x12_tr);


    if (s_btnSel == 0) {
      u8g2.drawBox(6, 51, 54, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(18, 61, "Open");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(6, 51, 54, 12);
      u8g2.drawStr(18, 61, "Open");
    }


    if (s_btnSel == 1) {
      u8g2.drawBox(68, 51, 54, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(77, 61, "Delete");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(68, 51, 54, 12);
      u8g2.drawStr(77, 61, "Delete");
    }
  } else if (s_scene == SCENE_CONFIRM_DELETE) {

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "DELETE APP");
    u8g2.setDrawColor(1);


    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(6, 28, "Delete application?");
    u8g2.drawStr(6, 41, s_currentApp.name.c_str());


    if (s_confirmSel == 0) {
      u8g2.drawBox(6, 51, 54, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(14, 61, "Cancel");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(6, 51, 54, 12);
      u8g2.drawStr(14, 61, "Cancel");
    }

    if (s_confirmSel == 1) {
      u8g2.drawBox(68, 51, 54, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(77, 61, "Delete");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(68, 51, 54, 12);
      u8g2.drawStr(77, 61, "Delete");
    }
  }
}

}
