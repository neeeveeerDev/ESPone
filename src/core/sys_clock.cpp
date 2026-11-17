#include "sys_clock.h"
#include <sys/time.h>
#include <Preferences.h>

namespace sys_clock {

static bool s_inited = false;
static int8_t s_tzHours = 3;

static void applyTimezone(int8_t tzHours) {
  char tzBuf[32];
  if (tzHours >= 0) {
    snprintf(tzBuf, sizeof(tzBuf), "UTC-%d", tzHours);
  } else {
    snprintf(tzBuf, sizeof(tzBuf), "UTC+%d", -tzHours);
  }
  setenv("TZ", tzBuf, 1);
  tzset();
}

void init() {
  if (s_inited) return;
  s_inited = true;

  Preferences prefs;
  prefs.begin("espone_clk", true);
  uint32_t savedEpoch = prefs.getUInt("epoch", 0);
  uint32_t savedMillis = prefs.getUInt("millis", 0);
  s_tzHours = (int8_t)prefs.getChar("tz", 3);
  prefs.end();

  if (s_tzHours < -12 || s_tzHours > 14) s_tzHours = 3;
  applyTimezone(s_tzHours);

  if (savedEpoch > 1700000000) {
    uint32_t elapsedSec = (millis() - savedMillis) / 1000;
    time_t cur = savedEpoch + elapsedSec;
    struct timeval tv = { cur, 0 };
    settimeofday(&tv, nullptr);
  } else {

    struct tm ti = {};
    ti.tm_year = 2026 - 1900;
    ti.tm_mon = 9;
    ti.tm_mday = 3;
    ti.tm_hour = 12;
    ti.tm_min = 0;
    ti.tm_sec = 0;
    time_t t = mktime(&ti);
    struct timeval tv = { t, 0 };
    settimeofday(&tv, nullptr);
  }
}

void saveTimeToNvs() {
  time_t cur = time(nullptr);
  Preferences prefs;
  prefs.begin("espone_clk", false);
  prefs.putUInt("epoch", (uint32_t)cur);
  prefs.putUInt("millis", millis());
  prefs.putChar("tz", (int8_t)s_tzHours);
  prefs.end();
}

void getTime(int& year, int& month, int& day, int& hour, int& min, int& sec) {
  if (!s_inited) init();
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  year = ti.tm_year + 1900;
  month = ti.tm_mon + 1;
  day = ti.tm_mday;
  hour = ti.tm_hour;
  min = ti.tm_min;
  sec = ti.tm_sec;
}

void setTime(int hour, int min, int sec) {
  if (!s_inited) init();
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  ti.tm_hour = hour;
  ti.tm_min = min;
  ti.tm_sec = sec;
  time_t newEpoch = mktime(&ti);
  struct timeval tv = { newEpoch, 0 };
  settimeofday(&tv, nullptr);
  saveTimeToNvs();
  Serial.printf("[sys_clock] Manual time set: %02d:%02d:%02d\n", hour, min, sec);
}

void setDate(int year, int month, int day) {
  if (!s_inited) init();
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  ti.tm_year = year - 1900;
  ti.tm_mon = month - 1;
  ti.tm_mday = day;
  time_t newEpoch = mktime(&ti);
  struct timeval tv = { newEpoch, 0 };
  settimeofday(&tv, nullptr);
  saveTimeToNvs();
  Serial.printf("[sys_clock] Manual date set: %04d-%02d-%02d\n", year, month, day);
}

void setTimezone(int8_t tzHours) {
  if (tzHours < -12) tzHours = -12;
  if (tzHours > 14) tzHours = 14;
  s_tzHours = tzHours;
  applyTimezone(s_tzHours);
  saveTimeToNvs();
  Serial.printf("[sys_clock] Timezone set to UTC%+d\n", s_tzHours);
}

int8_t getTimezone() {
  if (!s_inited) init();
  return s_tzHours;
}

void syncFromBle(uint32_t epochUtc, int8_t tzHours) {
  if (tzHours >= -12 && tzHours <= 14) {
    s_tzHours = tzHours;
    applyTimezone(s_tzHours);
  }
  struct timeval tv = { (time_t)epochUtc, 0 };
  settimeofday(&tv, nullptr);
  saveTimeToNvs();
  Serial.printf("[sys_clock] BLE Sync successful: epoch=%u, TZ=UTC%+d\n", epochUtc, s_tzHours);
}

}
