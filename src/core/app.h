#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "core/keys.h"








class App {
public:
  virtual const char* name() const = 0;
  virtual void open() {}
  virtual void close() {}
  virtual void draw(U8G2& u8g2, uint32_t now) = 0;
  virtual bool animates() const { return false; }
  virtual void onKey(const KeyEvent& ev) { (void)ev; }
  virtual bool onBack() { return false; }
  virtual void tick(uint32_t now) { (void)now; }
  virtual ~App() {}
};
