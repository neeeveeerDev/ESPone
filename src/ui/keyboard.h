#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <functional>
#include "core/keys.h"

namespace ui_keyboard {

enum Result {
  NONE = 0,
  DONE,
  CANCEL
};


using Callback = std::function<void(const String& text, bool ok)>;









void open(const char* title, Callback cb);
void open(const char* title, const char* defaultText, Callback cb);
void open(const char* title, const char* defaultText, bool isFile, Callback cb);


void openManual(const char* title, const char* defaultText = "", bool isFile = false);

bool isOpen();
bool isModal();
void close();


void tick(uint32_t now);
Result onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);


String getText();

}
