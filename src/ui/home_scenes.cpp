#include "home_scenes.h"
#include "scenes/scenes_storage.h"
#include "status_bar.h"
#include <Preferences.h>

namespace home_scenes {
namespace {

static uint8_t s_sceneIdx = 0;
static uint8_t s_frameIdx = 0;
static uint32_t s_lastFrameTime = 0;
static uint32_t s_bannerUntil = 0;

void saveCurrent() {
  Preferences prefs;
  prefs.begin("espone_sc", false);
  prefs.putUChar("cur", s_sceneIdx);
  prefs.end();
}

void loadCurrent() {
  Preferences prefs;
  prefs.begin("espone_sc", true);
  s_sceneIdx = prefs.getUChar("cur", 0);
  prefs.end();
  if (s_sceneIdx >= scenes::count()) {
    s_sceneIdx = 0;
  }
}

} // namespace

void init() {
  loadCurrent();
  s_frameIdx = 0;
  s_lastFrameTime = millis();
  s_bannerUntil = millis() + 1500;
  Serial.printf("[scenes] init scene #%d (%s)\n", s_sceneIdx, scenes::getScene(s_sceneIdx)->title);
}

void nextScene() {
  uint8_t total = scenes::count();
  if (total == 0) return;
  s_sceneIdx = (s_sceneIdx + 1) % total;
  s_frameIdx = 0;
  s_lastFrameTime = millis();
  saveCurrent();
  s_bannerUntil = millis() + 1400;
  Serial.printf("[scenes] switch -> #%d: %s\n", s_sceneIdx, scenes::getScene(s_sceneIdx)->title);
}

void prevScene() {
  uint8_t total = scenes::count();
  if (total == 0) return;
  s_sceneIdx = (s_sceneIdx == 0) ? (total - 1) : (s_sceneIdx - 1);
  s_frameIdx = 0;
  s_lastFrameTime = millis();
  saveCurrent();
  s_bannerUntil = millis() + 1400;
  Serial.printf("[scenes] switch -> #%d: %s\n", s_sceneIdx, scenes::getScene(s_sceneIdx)->title);
}

uint8_t currentSceneIndex() {
  return s_sceneIdx;
}

void tick(uint32_t now) {
  const scenes::SceneItem* sc = scenes::getScene(s_sceneIdx);
  if (!sc || sc->frameCount == 0) return;

  uint16_t interval = sc->intervalMs > 0 ? sc->intervalMs : 40;
  if (now - s_lastFrameTime >= interval) {
    s_lastFrameTime = now;
    s_frameIdx = (s_frameIdx + 1) % sc->frameCount;
  }
}

void onKey(const KeyEvent& ev) {
  if (!ev.repeat) {
    if (ev.key == BTN_LEFT) {
      prevScene();
    } else if (ev.key == BTN_RIGHT) {
      nextScene();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  const uint8_t* frameData = scenes::getFrame(s_sceneIdx, s_frameIdx);
  if (!frameData) return;

  // 1. Draw animated background scene (Spiral / Vortex)
  u8g2.setDrawColor(1);
  u8g2.drawBitmap(0, 0, 16, 64, frameData);

  // 2. Draw authentic Flipper Zero status bar (SD on left, Time + Battery on right)
  status_bar::draw(u8g2, now);

  // 3. Subtle clean indicator when switching scenes (Bottom center)
  if (now < s_bannerUntil) {
    const scenes::SceneItem* sc = scenes::getScene(s_sceneIdx);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d %s", s_sceneIdx + 1, scenes::count(), sc ? sc->title : "");
    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(buf);
    int bw = tw + 8;
    int bx = (128 - bw) / 2;
    int by = 50;

    u8g2.setDrawColor(0);
    u8g2.drawBox(bx, by, bw, 11);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(bx, by, bw, 11);
    u8g2.drawStr(bx + 4, by + 8, buf);
  }
}


} // namespace home_scenes
