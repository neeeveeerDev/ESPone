#pragma once
#include "core/app.h"

class MenuView;


namespace modules {
extern App* const kApps[];
extern const int kCount;

void initMenu(MenuView& menu);
void reloadMenu();
}
