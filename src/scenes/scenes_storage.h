#pragma once
#include <Arduino.h>

namespace scenes {

struct SceneItem {
  const char* title;
  uint8_t frameCount;
  uint16_t intervalMs;
  const uint8_t* const* frames;
};

uint8_t count();
const SceneItem* getScene(uint8_t idx);
const uint8_t* getFrame(uint8_t sceneIdx, uint8_t frameIdx);

}
