#pragma once
#include <Arduino.h>

namespace screenshot {

void init();

// Takes the current display buffer, converts it to 1-bit BMP, and saves via VFS
bool capture(String& outPath);

// Returns true if a flash effect should be drawn this frame
bool isFlashing();
void clearFlash();

} // namespace screenshot
