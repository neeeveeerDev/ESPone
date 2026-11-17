#pragma once
#include <Arduino.h>

namespace sys_storage {


void init();


void loadConfig();


void saveConfig();

bool isSdMounted();

}
