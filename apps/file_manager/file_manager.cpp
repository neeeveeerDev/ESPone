#include "file_manager.h"
#include "fm_fs.h"
#include "scene_storage.h"
#include "scene_browser.h"
#include "scene_operations.h"
#include "scene_create.h"
#include "scene_keyboard.h"
#include "scene_viewer.h"
#include "scene_updater.h"
#include "scene_app_installer.h"
#include "core/js/app_registry.h"
#include "core/js/js_runner.h"

namespace file_manager {

enum FmScene {
  SCENE_STORAGE = 0,
  SCENE_BROWSER,
  SCENE_OPERATIONS,
  SCENE_CREATE,
  SCENE_KEYBOARD,
  SCENE_VIEWER,
  SCENE_UPDATER,
  SCENE_APP_INSTALLER,
  SCENE_JS_APP
};

static FmScene s_scene = SCENE_STORAGE;
static fm_fs::StorageType s_storage = fm_fs::STORAGE_NONE;
static bool s_pendingCreateFile = false;

void open() {
  s_scene = SCENE_STORAGE;
  s_storage = fm_fs::STORAGE_NONE;
  scene_storage::open();
}

void close() {
  if (s_scene == SCENE_STORAGE) scene_storage::close();
  else if (s_scene == SCENE_BROWSER) scene_browser::close();
  else if (s_scene == SCENE_OPERATIONS) scene_operations::close();
  else if (s_scene == SCENE_CREATE) scene_create::close();
  else if (s_scene == SCENE_KEYBOARD) scene_keyboard::close();
  else if (s_scene == SCENE_VIEWER) scene_viewer::close();
  else if (s_scene == SCENE_UPDATER) scene_updater::close();
  else if (s_scene == SCENE_APP_INSTALLER) scene_app_installer::close();
  else if (s_scene == SCENE_JS_APP) js_runner::close();
  s_scene = SCENE_STORAGE;
}

bool onBack() {
  if (s_scene == SCENE_JS_APP) {
    js_runner::close();
    s_scene = SCENE_BROWSER;
    scene_browser::refresh();
    return true;
  }
  if (s_scene == SCENE_APP_INSTALLER) {
    scene_app_installer::close();
    s_scene = SCENE_BROWSER;
    return true;
  }
  if (s_scene == SCENE_UPDATER) {
    if (scene_updater::onKey(KeyEvent{ BTN_BACK, false }) == scene_updater::UPDATER_CANCEL) {
      scene_updater::close();
      s_scene = SCENE_BROWSER;
    }
    return true;
  }
  if (s_scene == SCENE_KEYBOARD) {
    scene_keyboard::close();
    s_scene = SCENE_BROWSER;
    return true;
  }
  if (s_scene == SCENE_VIEWER) {
    scene_viewer::close();
    s_scene = SCENE_BROWSER;
    return true;
  }
  if (s_scene == SCENE_OPERATIONS) {
    scene_operations::close();
    s_scene = SCENE_BROWSER;
    return true;
  }
  if (s_scene == SCENE_CREATE) {
    scene_create::close();
    s_scene = SCENE_BROWSER;
    return true;
  }
  if (s_scene == SCENE_BROWSER) {
    scene_browser::BrowserAction act = scene_browser::onKey(KeyEvent{ BTN_BACK, false });
    if (act == scene_browser::ACTION_BACK_TO_STORAGE) {
      scene_browser::close();
      s_scene = SCENE_STORAGE;
      scene_storage::open();
      return true;
    }
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_scene == SCENE_STORAGE) {
    scene_storage::tick(now);
    fm_fs::StorageType chosen = scene_storage::getSelectedStorage();
    if (chosen != fm_fs::STORAGE_NONE) {
      s_storage = chosen;
      scene_storage::resetSelection();
      s_scene = SCENE_BROWSER;
      scene_browser::open(s_storage);
    }
  } else if (s_scene == SCENE_BROWSER) {
    scene_browser::tick(now);
  } else if (s_scene == SCENE_OPERATIONS) {
    scene_operations::tick(now);
  } else if (s_scene == SCENE_CREATE) {
    scene_create::tick(now);
  } else if (s_scene == SCENE_KEYBOARD) {
    scene_keyboard::tick(now);
  } else if (s_scene == SCENE_VIEWER) {
    scene_viewer::tick(now);
  } else if (s_scene == SCENE_UPDATER) {
    scene_updater::tick(now);
  } else if (s_scene == SCENE_APP_INSTALLER) {
    scene_app_installer::tick(now);
  } else if (s_scene == SCENE_JS_APP) {
    if (!js_runner::isActive()) {
      s_scene = SCENE_BROWSER;
      scene_browser::refresh();
    } else {
      js_runner::tick(now);
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_scene == SCENE_STORAGE) {
    scene_storage::onKey(ev);
  } else if (s_scene == SCENE_BROWSER) {
    scene_browser::BrowserAction act = scene_browser::onKey(ev);
    if (act == scene_browser::ACTION_BACK_TO_STORAGE) {
      scene_browser::close();
      s_scene = SCENE_STORAGE;
      scene_storage::open();
    } else if (act == scene_browser::ACTION_INSTALL_APP) {
      s_scene = SCENE_APP_INSTALLER;
      String fp = scene_browser::getSelectedFullPath();
      const char* nm = scene_browser::getSelectedItemName();
      scene_app_installer::open(s_storage, fp.c_str(), nm);
    } else if (act == scene_browser::ACTION_RUN_JS) {
      String fp = scene_browser::getSelectedFullPath();
      const char* nm = scene_browser::getSelectedItemName();
      app_registry::InstalledApp app;
      app.id = nm;
      app.name = nm;
      app.entryPath = fp;
      app.basePath = scene_browser::getCurrentPath();
      app.isSd = (s_storage == fm_fs::STORAGE_SD);
      app.permissions = { "infrared", "subghz", "bluetooth", "gpio", "system", "fs" };
      js_runner::open(app);
      s_scene = SCENE_JS_APP;
    } else if (act == scene_browser::ACTION_UPDATE_FIRMWARE) {
      s_scene = SCENE_UPDATER;
      String fp = scene_browser::getSelectedFullPath();
      const char* nm = scene_browser::getSelectedItemName();
      scene_updater::open(s_storage, fp.c_str(), nm);
    } else if (act == scene_browser::ACTION_OPEN_FILE) {
      s_scene = SCENE_VIEWER;
      String fp = scene_browser::getSelectedFullPath();
      const char* nm = scene_browser::getSelectedItemName();
      scene_viewer::open(s_storage, fp.c_str(), nm);
    } else if (act == scene_browser::ACTION_OPEN_OPERATIONS) {
      s_scene = SCENE_OPERATIONS;
      const char* curDir = scene_browser::getCurrentPath();
      const char* nm = scene_browser::getSelectedItemName();
      bool isDir = scene_browser::isSelectedItemDir();
      size_t sz = scene_browser::getSelectedItemSize();
      scene_operations::open(s_storage, curDir, nm, isDir, sz);
    } else if (act == scene_browser::ACTION_OPEN_CREATE) {
      s_scene = SCENE_CREATE;
      scene_create::open();
    }
  } else if (s_scene == SCENE_OPERATIONS) {
    scene_operations::OpResult op = scene_operations::onKey(ev);
    if (op == scene_operations::OP_CLOSE) {
      scene_operations::close();
      s_scene = SCENE_BROWSER;
    } else if (op == scene_operations::OP_REFRESH) {
      scene_operations::close();
      s_scene = SCENE_BROWSER;
      scene_browser::refresh();
    } else if (op == scene_operations::OP_OPEN_VIEWER) {
      scene_operations::close();
      String fp = scene_browser::getSelectedFullPath();
      const char* nm = scene_browser::getSelectedItemName();
      if (scene_browser::isAppFile(nm)) {
        s_scene = SCENE_APP_INSTALLER;
        scene_app_installer::open(s_storage, fp.c_str(), nm);
      } else if (scene_browser::isJsFile(nm)) {
        app_registry::InstalledApp app;
        app.id = nm;
        app.name = nm;
        app.entryPath = fp;
        app.basePath = scene_browser::getCurrentPath();
        app.isSd = (s_storage == fm_fs::STORAGE_SD);
        app.permissions = { "infrared", "subghz", "bluetooth", "gpio", "system", "fs" };
        js_runner::open(app);
        s_scene = SCENE_JS_APP;
      } else {
        s_scene = SCENE_VIEWER;
        scene_viewer::open(s_storage, fp.c_str(), nm);
      }
    }
  } else if (s_scene == SCENE_CREATE) {
    scene_create::CreateTarget tgt = scene_create::onKey(ev);
    if (tgt == scene_create::CREATE_CANCEL) {
      scene_create::close();
      s_scene = SCENE_BROWSER;
    } else if (tgt == scene_create::CREATE_FOLDER) {
      scene_create::close();
      s_scene = SCENE_KEYBOARD;
      s_pendingCreateFile = false;
      scene_keyboard::open("NEW FOLDER", false);
    } else if (tgt == scene_create::CREATE_FILE) {
      scene_create::close();
      s_scene = SCENE_KEYBOARD;
      s_pendingCreateFile = true;
      scene_keyboard::open("NEW FILE", true);
    }
  } else if (s_scene == SCENE_KEYBOARD) {
    scene_keyboard::KeyboardResult kb = scene_keyboard::onKey(ev);
    if (kb == scene_keyboard::KB_CANCEL) {
      scene_keyboard::close();
      s_scene = SCENE_BROWSER;
    } else if (kb == scene_keyboard::KB_DONE) {
      String name = scene_keyboard::getResultString();
      name.trim();
      while (name.startsWith("/")) name.remove(0, 1);
      while (name.endsWith("/")) name.remove(name.length() - 1);
      if (name.length() > 0) {
        String fullPath = scene_browser::getCurrentPath();
        if (fullPath != "/") fullPath += "/";
        fullPath += name;

        if (s_pendingCreateFile) {
          fm_fs::createFile(s_storage, fullPath.c_str(), "");
        } else {
          fm_fs::createDir(s_storage, fullPath.c_str());
        }
      }
      scene_keyboard::close();
      s_scene = SCENE_BROWSER;
      scene_browser::refresh();
    }
  } else if (s_scene == SCENE_VIEWER) {
    if (scene_viewer::onKey(ev)) {
      scene_viewer::close();
      s_scene = SCENE_BROWSER;
    }
  } else if (s_scene == SCENE_UPDATER) {
    if (scene_updater::onKey(ev) == scene_updater::UPDATER_CANCEL) {
      scene_updater::close();
      s_scene = SCENE_BROWSER;
    }
  } else if (s_scene == SCENE_APP_INSTALLER) {
    scene_app_installer::InstallerResult res = scene_app_installer::onKey(ev);
    if (res == scene_app_installer::INSTALLER_CLOSE) {
      scene_app_installer::close();
      s_scene = SCENE_BROWSER;
      scene_browser::refresh();
    } else if (res == scene_app_installer::INSTALLER_LAUNCH_APP) {
      const char* id = scene_app_installer::getInstalledAppId();
      app_registry::refresh();
      const auto* app = app_registry::findAppById(id);
      scene_app_installer::close();
      if (app) {
        js_runner::open(*app);
        s_scene = SCENE_JS_APP;
      } else {
        s_scene = SCENE_BROWSER;
        scene_browser::refresh();
      }
    }
  } else if (s_scene == SCENE_JS_APP) {
    if (ev.key == BTN_BACK && !ev.repeat) {
      js_runner::close();
      s_scene = SCENE_BROWSER;
      scene_browser::refresh();
    } else {
      js_runner::onKey(ev);
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_scene == SCENE_STORAGE) {
    scene_storage::draw(u8g2, now);
  } else if (s_scene == SCENE_BROWSER) {
    scene_browser::draw(u8g2, now);
  } else if (s_scene == SCENE_OPERATIONS) {
    scene_operations::draw(u8g2, now);
  } else if (s_scene == SCENE_CREATE) {
    scene_create::draw(u8g2, now);
  } else if (s_scene == SCENE_KEYBOARD) {
    scene_keyboard::draw(u8g2, now);
  } else if (s_scene == SCENE_VIEWER) {
    scene_viewer::draw(u8g2, now);
  } else if (s_scene == SCENE_UPDATER) {
    scene_updater::draw(u8g2, now);
  } else if (s_scene == SCENE_APP_INSTALLER) {
    scene_app_installer::draw(u8g2, now);
  } else if (s_scene == SCENE_JS_APP) {
    js_runner::draw(u8g2, now);
  }
}

}
