#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "core/keys.h"
#include "app_registry.h"

namespace js_runner {

bool open(const app_registry::InstalledApp& app);
void close();
bool isActive();
void tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);
void onKey(const KeyEvent& ev);
bool onBack();

}
