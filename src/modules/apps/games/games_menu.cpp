#include "games_menu.h"
#include "doom/doom_game.h"
#include "snake_game.h"
#include "tetris_game.h"
#include "arkanoid_game.h"
#include "pong_game.h"
#include "bird_game.h"
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"
#include <math.h>
#include <vector>

namespace games_menu {
namespace {

enum GameScene {
  SCENE_MENU = 0,
  SCENE_DOOM,
  SCENE_SNAKE,
  SCENE_TETRIS,
  SCENE_ARKANOID,
  SCENE_PONG,
  SCENE_BIRD,
  SCENE_JS_GAME
};

static GameScene s_scene = SCENE_MENU;
static int s_sel = 0;
static const char* kGamesList[] = {
  "DOOM",
  "Snake",
  "Tetris",
  "Arkanoid",
  "Pong",
  "Bird"
};
static const int kGamesCount = sizeof(kGamesList) / sizeof(kGamesList[0]);
static const int rowH = 14;
static const int startY = 18;
static float s_barY = 18.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;

static std::vector<app_registry::InstalledApp> s_gameApps;

static int getTotalGamesCount() {
  return kGamesCount + (int)s_gameApps.size();
}

}

void open() {
  s_gameApps = app_registry::getAppsForTarget("games");
  s_scene = SCENE_MENU;
  s_sel = 0;
  s_barY = startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  Serial.printf("[games] open (dynamic apps: %d)\n", (int)s_gameApps.size());
}

void close() {
  if (s_scene == SCENE_DOOM) doom_game::close();
  else if (s_scene == SCENE_SNAKE) snake_game::close();
  else if (s_scene == SCENE_TETRIS) tetris_game::close();
  else if (s_scene == SCENE_ARKANOID) arkanoid_game::close();
  else if (s_scene == SCENE_PONG) pong_game::close();
  else if (s_scene == SCENE_BIRD) bird_game::close();
  else if (s_scene == SCENE_JS_GAME) js_runner::close();

  s_scene = SCENE_MENU;
  Serial.println("[games] close");
}

bool onBack() {
  if (s_scene == SCENE_JS_GAME) {
    js_runner::close();
    s_scene = SCENE_MENU;
    return true;
  }

  if (s_scene != SCENE_MENU) {
    if (s_scene == SCENE_DOOM) doom_game::close();
    else if (s_scene == SCENE_SNAKE) snake_game::close();
    else if (s_scene == SCENE_TETRIS) tetris_game::close();
    else if (s_scene == SCENE_ARKANOID) arkanoid_game::close();
    else if (s_scene == SCENE_PONG) pong_game::close();
    else if (s_scene == SCENE_BIRD) bird_game::close();

    s_scene = SCENE_MENU;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_scene == SCENE_MENU) {
    int total = getTotalGamesCount();
    int maxScroll = total - 3;
    if (maxScroll < 0) maxScroll = 0;


    if (s_sel < s_targetScroll) {
      s_targetScroll = s_sel;
    } else if (s_sel >= s_targetScroll + 3) {
      s_targetScroll = s_sel - 2;
    }
    if (s_targetScroll > maxScroll) s_targetScroll = maxScroll;
    if (s_targetScroll < 0) s_targetScroll = 0;

    float dScroll = s_targetScroll - s_scroll;
    if (fabsf(dScroll) > 0.02f) s_scroll += dScroll * 0.35f;
    else s_scroll = s_targetScroll;


    float targetBar = startY + (s_sel - s_scroll) * rowH;
    float dy = targetBar - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
    else s_barY = targetBar;
  } else if (s_scene == SCENE_DOOM) {
    if (!doom_game::isRunning()) { s_scene = SCENE_MENU; return; }
    doom_game::tick(now);
  } else if (s_scene == SCENE_SNAKE) {
    if (!snake_game::isRunning()) { s_scene = SCENE_MENU; return; }
    snake_game::tick(now);
  } else if (s_scene == SCENE_TETRIS) {
    if (!tetris_game::isRunning()) { s_scene = SCENE_MENU; return; }
    tetris_game::tick(now);
  } else if (s_scene == SCENE_ARKANOID) {
    if (!arkanoid_game::isRunning()) { s_scene = SCENE_MENU; return; }
    arkanoid_game::tick(now);
  } else if (s_scene == SCENE_PONG) {
    if (!pong_game::isRunning()) { s_scene = SCENE_MENU; return; }
    pong_game::tick(now);
  } else if (s_scene == SCENE_BIRD) {
    if (!bird_game::isRunning()) { s_scene = SCENE_MENU; return; }
    bird_game::tick(now);
  } else if (s_scene == SCENE_JS_GAME) {
    if (!js_runner::isActive()) {
      s_scene = SCENE_MENU;
      return;
    }
    js_runner::tick(now);
  }
}

void onKey(const KeyEvent& ev) {
  if (s_scene == SCENE_MENU) {
    int total = getTotalGamesCount();
    if (total == 0) return;

    if (ev.key == BTN_UP) {
      if (s_sel > 0) s_sel--;
      else s_sel = total - 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_sel < total - 1) s_sel++;
      else s_sel = 0;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_sel < kGamesCount) {
        switch (s_sel) {
          case 0: s_scene = SCENE_DOOM; doom_game::open(); break;
          case 1: s_scene = SCENE_SNAKE; snake_game::open(); break;
          case 2: s_scene = SCENE_TETRIS; tetris_game::open(); break;
          case 3: s_scene = SCENE_ARKANOID; arkanoid_game::open(); break;
          case 4: s_scene = SCENE_PONG; pong_game::open(); break;
          case 5: s_scene = SCENE_BIRD; bird_game::open(); break;
        }
      } else {
        int idx = s_sel - kGamesCount;
        if (idx >= 0 && idx < (int)s_gameApps.size()) {
          s_scene = SCENE_JS_GAME;
          js_runner::open(s_gameApps[idx]);
        }
      }
    }
  } else if (s_scene == SCENE_DOOM) {
    doom_game::onKey(ev);
  } else if (s_scene == SCENE_SNAKE) {
    snake_game::onKey(ev);
  } else if (s_scene == SCENE_TETRIS) {
    tetris_game::onKey(ev);
  } else if (s_scene == SCENE_ARKANOID) {
    arkanoid_game::onKey(ev);
  } else if (s_scene == SCENE_PONG) {
    pong_game::onKey(ev);
  } else if (s_scene == SCENE_BIRD) {
    bird_game::onKey(ev);
  } else if (s_scene == SCENE_JS_GAME) {
    js_runner::onKey(ev);
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_scene == SCENE_MENU) {
    int total = getTotalGamesCount();


    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < total; i++) {
      int y = startY + (int)((i - s_scroll) * rowH + 0.5f);
      if (y < startY - rowH || y > 64) continue;
      if (i < kGamesCount) {
        u8g2.drawStr(12, y + 10, kGamesList[i]);
      } else {
        int idx = i - kGamesCount;
        u8g2.drawStr(12, y + 10, s_gameApps[idx].menuTitle.c_str());
        u8g2.drawStr(108, y + 10, "JS");
      }
    }


    int bY = (int)(s_barY + 0.5f);
    if (bY >= startY - 2 && bY <= 64 - rowH + 2) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(4, bY, 120, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "GAMES");
    u8g2.setDrawColor(1);
  } else if (s_scene == SCENE_DOOM) {
    doom_game::draw(u8g2, now);
  } else if (s_scene == SCENE_SNAKE) {
    snake_game::draw(u8g2, now);
  } else if (s_scene == SCENE_TETRIS) {
    tetris_game::draw(u8g2, now);
  } else if (s_scene == SCENE_ARKANOID) {
    arkanoid_game::draw(u8g2, now);
  } else if (s_scene == SCENE_PONG) {
    pong_game::draw(u8g2, now);
  } else if (s_scene == SCENE_BIRD) {
    bird_game::draw(u8g2, now);
  } else if (s_scene == SCENE_JS_GAME) {
    js_runner::draw(u8g2, now);
  }
}

}
