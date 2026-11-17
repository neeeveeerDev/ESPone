#include "scene_keyboard.h"
#include "ui/keyboard.h"

namespace scene_keyboard {

void open(const char* title, bool isFile, const char* defaultName) {
  ui_keyboard::openManual(title, defaultName ? defaultName : "", isFile);
}

void close() {
  ui_keyboard::close();
}

void tick(uint32_t now) {
  ui_keyboard::tick(now);
}

KeyboardResult onKey(const KeyEvent& ev) {
  ui_keyboard::Result r = ui_keyboard::onKey(ev);
  if (r == ui_keyboard::DONE) return KB_DONE;
  if (r == ui_keyboard::CANCEL) return KB_CANCEL;
  return KB_NONE;
}

void draw(U8G2& u8g2, uint32_t now) {
  ui_keyboard::draw(u8g2, now);
}

String getResultString() {
  return ui_keyboard::getText();
}

}
