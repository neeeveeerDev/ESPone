#include "app_registry.h"
#include <FFat.h>
#include <SD.h>
#include <ArduinoJson.h>

namespace app_registry {

static std::vector<InstalledApp> s_apps;
static bool s_initialized = false;

static void scanStorage(fs::FS& fs, bool isSd) {
  if (!fs.exists("/apps")) return;

  File appsDir = fs.open("/apps");
  if (!appsDir || !appsDir.isDirectory()) return;

  File child = appsDir.openNextFile();
  while (child) {
    if (child.isDirectory()) {
      const char* rawName = child.name();
      const char* slash = strrchr(rawName, '/');
      const char* baseName = slash ? (slash + 1) : rawName;
      if (strlen(baseName) == 0) {
        child = appsDir.openNextFile();
        continue;
      }

      String appFolder = String("/apps/") + baseName;
      String manifestPath = appFolder + "/info.json";

      if (fs.exists(manifestPath.c_str())) {
        File mf = fs.open(manifestPath.c_str(), "r");
        if (mf) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, mf);
          mf.close();

          if (!err) {
            InstalledApp app;
            app.id = doc["id"] | baseName;
            app.name = doc["name"] | app.id.c_str();
            app.version = doc["version"] | "1.0";
            app.author = doc["author"] | "Dev";
            app.description = doc["description"] | "";
            app.basePath = appFolder;

            const char* entry = doc["entry"] | "src/main.js";
            app.entryPath = appFolder + "/" + entry;

            app.targetMenu = doc["menu"]["target"] | doc["category"] | "apps";
            app.menuTitle = doc["menu"]["title"] | app.name.c_str();
            app.isSd = isSd;

            JsonArray perms = doc["permissions"].as<JsonArray>();
            for (JsonVariant p : perms) {
              app.permissions.push_back(p.as<const char*>());
            }

            s_apps.push_back(app);
            Serial.printf("[AppRegistry] Found: %s (%s) -> Menu: %s\n",
                          app.name.c_str(), app.id.c_str(), app.targetMenu.c_str());
          }
        }
      }
    }
    child = appsDir.openNextFile();
  }
}

void init() {
  if (s_initialized) return;
  refresh();
  s_initialized = true;
}

void refresh() {
  s_apps.clear();

  scanStorage(FFat, false);

  #ifdef PIN_SD_CS
  scanStorage(SD, true);
  #endif
}

const std::vector<InstalledApp>& getAllApps() {
  refresh();
  return s_apps;
}

std::vector<InstalledApp> getAppsForTarget(const char* target) {
  refresh();
  std::vector<InstalledApp> filtered;
  for (const auto& a : s_apps) {
    if (a.targetMenu.equalsIgnoreCase(target)) {
      filtered.push_back(a);
    }
  }
  return filtered;
}

const InstalledApp* findAppById(const char* id) {
  refresh();
  for (const auto& a : s_apps) {
    if (a.id.equalsIgnoreCase(id)) return &a;
  }
  return nullptr;
}

static fs::FS& getFS(bool isSd) {
#ifdef PIN_SD_CS
  if (isSd) return SD;
#endif
  return FFat;
}

static size_t calculateDirSize(fs::FS& fs, const String& path) {
  size_t total = 0;
  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  File child = dir.openNextFile();
  while (child) {
    const char* rawName = child.name();
    const char* slash = strrchr(rawName, '/');
    const char* baseName = slash ? (slash + 1) : rawName;
    if (baseName && strlen(baseName) > 0 && baseName[0] != '.') {
      String childPath = path + "/" + baseName;
      if (child.isDirectory()) {
        total += calculateDirSize(fs, childPath);
      } else {
        total += child.size();
      }
    }
    child = dir.openNextFile();
  }
  dir.close();
  return total;
}

static bool removeDirRecursive(fs::FS& fs, const String& path) {
  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) return false;
  File child = dir.openNextFile();
  while (child) {
    const char* rawName = child.name();
    const char* slash = strrchr(rawName, '/');
    const char* baseName = slash ? (slash + 1) : rawName;
    if (baseName && strlen(baseName) > 0 && baseName[0] != '.') {
      String childPath = path + "/" + baseName;
      if (child.isDirectory()) {
        removeDirRecursive(fs, childPath);
      } else {
        fs.remove(childPath.c_str());
      }
    }
    child = dir.openNextFile();
  }
  dir.close();
  return fs.rmdir(path.c_str());
}

size_t getAppSize(const InstalledApp& app) {
  fs::FS& fs = getFS(app.isSd);
  return calculateDirSize(fs, app.basePath);
}

bool deleteApp(const InstalledApp& app) {
  fs::FS& fs = getFS(app.isSd);
  bool ok = removeDirRecursive(fs, app.basePath);
  refresh();
  return ok;
}

}
