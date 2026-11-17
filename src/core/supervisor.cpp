#include "supervisor.h"
#include "config.h"

namespace supervisor {

static volatile uint32_t s_lastHeartbeat = 0;
static volatile bool s_forceKillRequested = false;
static volatile bool s_screenshotRequested = false;

static TaskHandle_t s_taskHandle = nullptr;

static void supervisorTask(void* param) {
  uint32_t backHoldStart = 0;
  uint32_t comboHoldStart = 0;
  bool comboTriggered = false;
  bool killTriggered = false;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(25));
    uint32_t now = millis();

    // 1. Emergency BACK button monitor (Force-Kill)
    bool backPressed = (digitalRead(PIN_BTN_BACK) == LOW);
    if (backPressed) {
      if (backHoldStart == 0) {
        backHoldStart = now;
        killTriggered = false;
      } else if (!killTriggered && (now - backHoldStart >= 1800)) {
        killTriggered = true;
        s_forceKillRequested = true;
        Serial.println("[supervisor] Emergency Force-Kill signal asserted!");
      } else if (now - backHoldStart >= 2800) {
        // Main thread is completely hung (deadlock) while holding BACK
        if (s_lastHeartbeat != 0 && (now - s_lastHeartbeat >= 2000)) {
          Serial.println("[supervisor] DEADLOCK DETECTED! Hard reset triggered.");
          esp_restart();
        }
      }
    } else {
      backHoldStart = 0;
      killTriggered = false;
    }

    // 2. Screenshot Hotkey combo (UP + OK held for 400ms)
    bool upPressed = (digitalRead(PIN_BTN_UP) == LOW);
    bool okPressed = (digitalRead(PIN_BTN_OK) == LOW);

    if (upPressed && okPressed) {
      if (comboHoldStart == 0) {
        comboHoldStart = now;
        comboTriggered = false;
      } else if (!comboTriggered && (now - comboHoldStart >= 400)) {
        comboTriggered = true;
        s_screenshotRequested = true;
        Serial.println("[supervisor] Screenshot combo detected (UP + OK)");
      }
    } else {
      comboHoldStart = 0;
      comboTriggered = false;
    }
  }
}

void init() {
  s_lastHeartbeat = millis();
  // Pin supervisor task to Core 0 (Core 1 runs Arduino loop)
  xTaskCreatePinnedToCore(
    supervisorTask,
    "SupervisorTask",
    3072,
    nullptr,
    2, // Priority
    &s_taskHandle,
    0  // Core 0
  );
  Serial.println("[supervisor] Independent Core 0 watchdog & emergency monitor started");
}

void heartbeat(uint32_t now) {
  s_lastHeartbeat = now;
}

bool isForceKillRequested() {
  return s_forceKillRequested;
}

void clearForceKill() {
  s_forceKillRequested = false;
}

bool isScreenshotRequested() {
  return s_screenshotRequested;
}

void clearScreenshotRequest() {
  s_screenshotRequested = false;
}

} // namespace supervisor
