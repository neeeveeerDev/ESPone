#pragma once
#include <Arduino.h>
#include <vector>

namespace app_registry {

struct InstalledApp {
  String id;
  String name;
  String version;
  String author;
  String description;
  String entryPath;
  String basePath;
  String targetMenu;
  String menuTitle;
  std::vector<String> permissions;
  bool isSd;

  bool hasPermission(const char* perm) const {
    for (const auto& p : permissions) {
      if (p.equalsIgnoreCase(perm)) return true;
    }
    return false;
  }
};

void init();
void refresh();
const std::vector<InstalledApp>& getAllApps();
std::vector<InstalledApp> getAppsForTarget(const char* target);
const InstalledApp* findAppById(const char* id);
size_t getAppSize(const InstalledApp& app);
bool deleteApp(const InstalledApp& app);

}
