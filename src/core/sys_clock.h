#pragma once
#include <Arduino.h>
#include <time.h>

namespace sys_clock {

void init();
void getTime(int& year, int& month, int& day, int& hour, int& min, int& sec);
void setTime(int hour, int min, int sec);
void setDate(int year, int month, int day);
void setTimezone(int8_t tzHours);
int8_t getTimezone();
void syncFromBle(uint32_t epochUtc, int8_t tzHours);
void saveTimeToNvs();

}
