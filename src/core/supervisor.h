#pragma once
#include <Arduino.h>

namespace supervisor {

void init();

// Called in main loop to notify supervisor that Core 1 is alive
void heartbeat(uint32_t now);

// Checks if user held BACK for 1.8+ seconds to force-kill current app
bool isForceKillRequested();
void clearForceKill();

// Hotkey screenshot request
bool isScreenshotRequested();
void clearScreenshotRequest();

} // namespace supervisor
