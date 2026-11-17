#include "ibutton.h"
#include <OneWire.h>
#include "config.h"
#include "modules/apps/file_manager/fm_fs.h"

namespace ibutton {




enum Scene {
  SCENE_MENU = 0,
  SCENE_READ,
  SCENE_COPY_CHOOSE,
  SCENE_COPY_FILE_SELECT,
  SCENE_COPY_READ_SOURCE,
  SCENE_COPY_WRITE_BLANK,
  SCENE_SAVE,
  SCENE_EMULATE_CHOOSE,
  SCENE_EMULATE_FILE_SELECT,
  SCENE_EMULATE_RUN,
  SCENE_UNIVERSAL_BRANDS,
  SCENE_UNIVERSAL_KEYS,
  SCENE_UNIVERSAL_ACTION
};

static const char* kMainMenuItems[] = {
  "Read",
  "Copy",
  "Save",
  "Emulate",
  "Universal"
};
static const int kMainMenuCount = sizeof(kMainMenuItems) / sizeof(kMainMenuItems[0]);


struct UniversalKeyItem {
  const char* name;
  uint8_t rom[8];
};

struct UniversalBrand {
  const char* brandName;
  const UniversalKeyItem* keys;
  int keyCount;
};

static const UniversalKeyItem kCyfralKeys[] = {
  { "Cyfral CCD-2094 Test", { 0x01, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0x3E } },
  { "Cyfral Universal 1",   { 0x01, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x0E } },
  { "Cyfral Service",       { 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x3C } }
};

static const UniversalKeyItem kEltisKeys[] = {
  { "Eltis Universal 1",    { 0x01, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x12 } },
  { "Eltis Universal 2",    { 0x01, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xA8 } },
  { "Eltis Master",         { 0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x3F } }
};

static const UniversalKeyItem kMetacomKeys[] = {
  { "Metacom MK2003 Master",{ 0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0xE1 } },
  { "Metacom Universal 1",  { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x9B } },
  { "Metacom Service",      { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xD7 } }
};

static const UniversalKeyItem kVizitKeys[] = {
  { "Vizit All-FF Master",  { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F } },
  { "Vizit All-00 Master",  { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D } },
  { "Vizit Universal Alt1", { 0x01, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0x82 } },
  { "Vizit Universal Alt2", { 0x01, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0xDF } },
  { "Vizit Service Code",   { 0x01, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xF8 } }
};

static const UniversalBrand kUniversalBrands[] = {
  { "Cyfral",  kCyfralKeys,  sizeof(kCyfralKeys) / sizeof(kCyfralKeys[0]) },
  { "Eltis",   kEltisKeys,   sizeof(kEltisKeys) / sizeof(kEltisKeys[0]) },
  { "Metacom", kMetacomKeys, sizeof(kMetacomKeys) / sizeof(kMetacomKeys[0]) },
  { "Vizit",   kVizitKeys,   sizeof(kVizitKeys) / sizeof(kVizitKeys[0]) }
};
static const int kBrandCount = sizeof(kUniversalBrands) / sizeof(kUniversalBrands[0]);




static Scene s_scene = SCENE_MENU;
static int s_menuSel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 16.0f;


struct IbtnFileInfo {
  char filename[32];
  fm_fs::StorageType storage;
};
static std::vector<IbtnFileInfo> s_fileList;
static int s_fileSel = 0;


static int s_brandSel = 0;
static int s_univKeySel = 0;
static int s_univActionSel = 0;


static uint8_t s_activeRom[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F };
static char s_activeKeyType[16] = "Dallas";
static bool s_activeRomValid = false;


static bool s_readFound = false;
static bool s_readCrcOk = false;
static int s_readActionSel = 0;


static int s_copySourceType = 0;
static bool s_copySourceReady = false;
static uint8_t s_sourceRom[8] = { 0 };
static enum CopyWriteStatus {
  COPY_IDLE = 0,
  COPY_WRITING,
  COPY_SUCCESS,
  COPY_FAILED
} s_copyStatus = COPY_IDLE;
static uint32_t s_copyStatusTime = 0;


static bool s_saveSuccess = false;
static char s_savedFileName[32] = "";


static uint32_t s_emulateHits = 0;
static uint32_t s_lastHitTime = 0;


static const int rowH = 12;
static const int startY = 16;

static void resetMenuScroll() {
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = (float)startY;
}

static void updateMenuScroll(int sel, int count, float& scroll, float& targetScroll, float& barY, int rH = rowH, int sY = startY, int maxLines = 4) {
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / rH);
  if (sel < topIndex) targetScroll = sel * rH;
  else if (sel >= topIndex + maxLines) targetScroll = (sel - maxLines + 1) * rH;

  if (targetScroll < 0) targetScroll = 0;
  if (targetScroll > maxScroll * rH) targetScroll = maxScroll * rH;

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) scroll += ds * 0.35f;
  else scroll = targetScroll;

  float targetBarY = sY + sel * rH - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) barY += dy * 0.4f;
  else barY = targetBarY;
}




static void ensureIbuttonDirs() {
  if (fm_fs::isStorageReady(fm_fs::STORAGE_INTERNAL)) {
    fm_fs::createDir(fm_fs::STORAGE_INTERNAL, "/ibutton");
  }
  if (fm_fs::isStorageReady(fm_fs::STORAGE_SD)) {
    fm_fs::createDir(fm_fs::STORAGE_SD, "/ibutton");
  }
}

static void reloadIbtnFiles() {
  s_fileList.clear();
  s_fileSel = 0;

  auto scan = [](fm_fs::StorageType st) {
    if (!fm_fs::isStorageReady(st)) return;
    std::vector<fm_fs::FileItem> items;
    if (fm_fs::listDir(st, "/ibutton", items)) {
      for (const auto& it : items) {
        if (!it.isDir && strstr(it.name, ".ibtn")) {
          IbtnFileInfo info;
          strncpy(info.filename, it.name, sizeof(info.filename) - 1);
          info.filename[sizeof(info.filename) - 1] = '\0';
          info.storage = st;
          s_fileList.push_back(info);
        }
      }
    }
  };

  scan(fm_fs::STORAGE_SD);
  scan(fm_fs::STORAGE_INTERNAL);
}


static bool saveFlipperIbtn(const char* filename, const char* keyType, const uint8_t* rom) {
  fm_fs::StorageType st = fm_fs::isStorageReady(fm_fs::STORAGE_SD) ?
                          fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
  fs::FS* fs = fm_fs::getFS(st);
  if (!fs) return false;

  ensureIbuttonDirs();
  char fullPath[64];
  snprintf(fullPath, sizeof(fullPath), "/ibutton/%s", filename);

  File f = fs->open(fullPath, "w");
  if (!f) return false;

  f.println("Filetype: Flipper iButton key");
  f.println("Version: 1");
  f.println("# Available types: Dallas, Cyfral, Metakom");
  f.printf("Key type: %s\n", keyType ? keyType : "Dallas");
  f.printf("Data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
    rom[0], rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7]);
  f.close();
  return true;
}


static bool parseFlipperIbtn(fm_fs::StorageType st, const char* path, char* outType, size_t maxType, uint8_t* outRom) {
  fs::FS* fs = fm_fs::getFS(st);
  if (!fs) return false;
  File f = fs->open(path, "r");
  if (!f) return false;

  memset(outRom, 0, 8);
  strncpy(outType, "Dallas", maxType);

  bool hasData = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("Key type:")) {
      String t = line.substring(9);
      t.trim();
      strncpy(outType, t.c_str(), maxType - 1);
    } else if (line.startsWith("Data:")) {
      String hexPart = line.substring(5);
      hexPart.trim();
      int byteIdx = 0;
      int len = hexPart.length();
      int i = 0;
      while (i < len && byteIdx < 8) {
        while (i < len && (hexPart[i] == ' ' || hexPart[i] == ':')) i++;
        if (i + 1 < len) {
          char bStr[3] = { hexPart[i], hexPart[i+1], 0 };
          outRom[byteIdx++] = (uint8_t)strtoul(bStr, nullptr, 16);
          i += 2;
        } else {
          break;
        }
      }
      if (byteIdx == 8) hasData = true;
    }
  }
  f.close();
  return hasData;
}




static bool readOneWireKey(uint8_t* outRom) {
  OneWire ds(PIN_IBUTTON);
  ds.reset_search();
  if (ds.search(outRom)) {
    bool all0 = true, all1 = true;
    for (int i = 0; i < 8; i++) {
      if (outRom[i] != 0x00) all0 = false;
      if (outRom[i] != 0xFF) all1 = false;
    }
    if (!all0 && !all1) return true;
  }


  if (ds.reset()) {
    ds.write(0x33);
    ds.read_bytes(outRom, 8);
    bool all0 = true, all1 = true;
    for (int i = 0; i < 8; i++) {
      if (outRom[i] != 0x00) all0 = false;
      if (outRom[i] != 0xFF) all1 = false;
    }
    if (!all0 && !all1) return true;
  }
  return false;
}


static bool writeRW1990Blank(const uint8_t* rom) {
  const int pin = PIN_IBUTTON;
  OneWire ds(pin);


  if (!ds.reset()) return false;
  ds.write(0xD1);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(10);
  pinMode(pin, INPUT_PULLUP);
  delay(10);


  if (!ds.reset()) return false;
  ds.write(0xD5);


  for (int b = 0; b < 8; b++) {
    uint8_t val = rom[b];
    for (int i = 0; i < 8; i++) {
      uint8_t bit = (val >> i) & 1;
      uint8_t inv = bit ? 0 : 1;

      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      delayMicroseconds(inv ? 60 : 10);
      pinMode(pin, INPUT_PULLUP);
      delayMicroseconds(inv ? 10 : 60);


      delay(10);
    }
  }


  if (ds.reset()) {
    ds.write(0xD1);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(10);
    pinMode(pin, INPUT_PULLUP);
    delay(10);
  }


  delay(30);
  uint8_t readback[8] = { 0 };
  if (!ds.reset()) return false;
  ds.write(0x33);
  ds.read_bytes(readback, 8);

  return (memcmp(rom, readback, 8) == 0);
}


static void runEmulationCycle(const uint8_t* rom) {
  const int pin = PIN_IBUTTON;
  pinMode(pin, INPUT_PULLUP);

  if (digitalRead(pin) != LOW) return;

  uint32_t tStart = micros();
  while (digitalRead(pin) == LOW) {
    if ((micros() - tStart) > 5000) return;
  }
  uint32_t resetDur = micros() - tStart;
  if (resetDur < 200) return;


  delayMicroseconds(25);


  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(120);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(20);


  uint8_t cmd = 0;
  for (int i = 0; i < 8; i++) {
    uint32_t tWait = micros();
    while (digitalRead(pin) == HIGH) {
      if ((micros() - tWait) > 1000) return;
    }
    delayMicroseconds(15);
    if (digitalRead(pin) == HIGH) cmd |= (1 << i);

    tWait = micros();
    while (digitalRead(pin) == LOW) {
      if ((micros() - tWait) > 200) break;
    }
    delayMicroseconds(2);
  }


  for (int byteIdx = 0; byteIdx < 8; byteIdx++) {
    uint8_t b = rom[byteIdx];
    for (int bitIdx = 0; bitIdx < 8; bitIdx++) {
      uint8_t bit = (b >> bitIdx) & 1;

      uint32_t tWait = micros();
      while (digitalRead(pin) == HIGH) {
        if ((micros() - tWait) > 1000) return;
      }

      if (bit == 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(35);
        pinMode(pin, INPUT_PULLUP);
      } else {

      }

      tWait = micros();
      while (digitalRead(pin) == LOW) {
        if ((micros() - tWait) > 200) break;
      }
      delayMicroseconds(2);
    }
  }

  s_emulateHits++;
  s_lastHitTime = millis();
}




void open() {
  ensureIbuttonDirs();
  s_scene = SCENE_MENU;
  s_menuSel = 0;
  resetMenuScroll();
  s_readFound = false;
  s_copyStatus = COPY_IDLE;
  s_copySourceReady = false;
  s_emulateHits = 0;
  s_saveSuccess = false;
  pinMode(PIN_IBUTTON, INPUT_PULLUP);
}

void close() {
  pinMode(PIN_IBUTTON, INPUT);
  s_scene = SCENE_MENU;
}

bool onBack() {
  resetMenuScroll();
  switch (s_scene) {
    case SCENE_MENU:
      return false;

    case SCENE_READ:
      if (s_readFound) {
        s_readFound = false;
        return true;
      }
      s_scene = SCENE_MENU;
      return true;

    case SCENE_COPY_CHOOSE:
    case SCENE_SAVE:
      s_scene = SCENE_MENU;
      return true;

    case SCENE_COPY_FILE_SELECT:
      s_scene = SCENE_COPY_CHOOSE;
      return true;

    case SCENE_COPY_READ_SOURCE:
      s_scene = SCENE_COPY_CHOOSE;
      return true;

    case SCENE_COPY_WRITE_BLANK:
      s_scene = (s_copySourceType == 0) ? SCENE_COPY_FILE_SELECT : SCENE_COPY_CHOOSE;
      s_copyStatus = COPY_IDLE;
      return true;

    case SCENE_EMULATE_CHOOSE:
      s_scene = SCENE_MENU;
      return true;

    case SCENE_EMULATE_FILE_SELECT:
      s_scene = SCENE_EMULATE_CHOOSE;
      return true;

    case SCENE_EMULATE_RUN:
      s_scene = SCENE_MENU;
      return true;

    case SCENE_UNIVERSAL_BRANDS:
      s_scene = SCENE_MENU;
      return true;

    case SCENE_UNIVERSAL_KEYS:
      s_scene = SCENE_UNIVERSAL_BRANDS;
      return true;

    case SCENE_UNIVERSAL_ACTION:
      s_scene = SCENE_UNIVERSAL_KEYS;
      return true;

    default:
      s_scene = SCENE_MENU;
      return true;
  }
}




void tick(uint32_t now) {

  if (s_scene == SCENE_MENU) {
    updateMenuScroll(s_menuSel, kMainMenuCount, s_scroll, s_targetScroll, s_barY);
  } else if (s_scene == SCENE_UNIVERSAL_BRANDS) {
    updateMenuScroll(s_brandSel, kBrandCount, s_scroll, s_targetScroll, s_barY);
  } else if (s_scene == SCENE_UNIVERSAL_KEYS) {
    int count = kUniversalBrands[s_brandSel].keyCount;
    updateMenuScroll(s_univKeySel, count, s_scroll, s_targetScroll, s_barY);
  } else if (s_scene == SCENE_COPY_FILE_SELECT || s_scene == SCENE_EMULATE_FILE_SELECT) {
    if (!s_fileList.empty()) {
      updateMenuScroll(s_fileSel, (int)s_fileList.size(), s_scroll, s_targetScroll, s_barY);
    }
  }


  if (s_scene == SCENE_READ && !s_readFound) {
    uint8_t rom[8];
    if (readOneWireKey(rom)) {
      memcpy(s_activeRom, rom, 8);
      s_activeRomValid = true;
      s_readCrcOk = (OneWire::crc8(rom, 7) == rom[7]);
      strncpy(s_activeKeyType, "Dallas", sizeof(s_activeKeyType));
      s_readFound = true;
      s_readActionSel = 0;
    }
  }


  if (s_scene == SCENE_COPY_READ_SOURCE) {
    uint8_t rom[8];
    if (readOneWireKey(rom)) {
      memcpy(s_sourceRom, rom, 8);
      s_copySourceReady = true;
      s_copyStatus = COPY_IDLE;
      s_scene = SCENE_COPY_WRITE_BLANK;
    }
  }


  if (s_scene == SCENE_COPY_WRITE_BLANK) {
    if (s_copyStatus == COPY_IDLE) {

      OneWire ds(PIN_IBUTTON);
      if (ds.reset()) {
        s_copyStatus = COPY_WRITING;
        bool ok = writeRW1990Blank(s_sourceRom);
        s_copyStatus = ok ? COPY_SUCCESS : COPY_FAILED;
        s_copyStatusTime = now;
      }
    }
  }


  if (s_scene == SCENE_SAVE && !s_saveSuccess) {
    uint8_t rom[8];
    if (readOneWireKey(rom)) {
      memcpy(s_activeRom, rom, 8);
      s_activeRomValid = true;
      snprintf(s_savedFileName, sizeof(s_savedFileName), "Key_%02X%02X%02X.ibtn",
        rom[1], rom[2], rom[3]);
      saveFlipperIbtn(s_savedFileName, "Dallas", rom);
      s_saveSuccess = true;
    }
  }


  if (s_scene == SCENE_EMULATE_RUN) {
    runEmulationCycle(s_activeRom);
  }
}




void onKey(const KeyEvent& ev) {
  if (ev.repeat && ev.key == BTN_OK) return;

  switch (s_scene) {
    case SCENE_MENU:
      if (ev.key == BTN_UP) {
        s_menuSel = (s_menuSel - 1 + kMainMenuCount) % kMainMenuCount;
      } else if (ev.key == BTN_DOWN) {
        s_menuSel = (s_menuSel + 1) % kMainMenuCount;
      } else if (ev.key == BTN_OK) {
        switch (s_menuSel) {
          case 0:
            s_readFound = false;
            s_scene = SCENE_READ;
            break;
          case 1:
            s_menuSel = 0;
            s_scene = SCENE_COPY_CHOOSE;
            break;
          case 2:
            s_saveSuccess = false;
            s_scene = SCENE_SAVE;
            break;
          case 3:
            s_menuSel = 0;
            s_scene = SCENE_EMULATE_CHOOSE;
            break;
          case 4:
            s_brandSel = 0;
            resetMenuScroll();
            s_scene = SCENE_UNIVERSAL_BRANDS;
            break;
        }
      }
      break;

    case SCENE_READ:
      if (s_readFound) {
        if (ev.key == BTN_LEFT) {
          s_readActionSel = (s_readActionSel - 1 + 3) % 3;
        } else if (ev.key == BTN_RIGHT) {
          s_readActionSel = (s_readActionSel + 1) % 3;
        } else if (ev.key == BTN_OK) {
          if (s_readActionSel == 0) {
            char fname[32];
            snprintf(fname, sizeof(fname), "Key_%02X%02X%02X.ibtn",
              s_activeRom[1], s_activeRom[2], s_activeRom[3]);
            saveFlipperIbtn(fname, s_activeKeyType, s_activeRom);
            s_readFound = false;
            s_saveSuccess = true;
            strncpy(s_savedFileName, fname, sizeof(s_savedFileName));
            s_scene = SCENE_SAVE;
          } else if (s_readActionSel == 1) {
            s_emulateHits = 0;
            s_scene = SCENE_EMULATE_RUN;
          } else if (s_readActionSel == 2) {
            s_readFound = false;
          }
        }
      }
      break;

    case SCENE_COPY_CHOOSE:
      if (ev.key == BTN_UP || ev.key == BTN_DOWN) {
        s_menuSel = 1 - s_menuSel;
      } else if (ev.key == BTN_OK) {
        if (s_menuSel == 0) {
          s_copySourceType = 0;
          reloadIbtnFiles();
          s_fileSel = 0;
          resetMenuScroll();
          s_scene = SCENE_COPY_FILE_SELECT;
        } else {
          s_copySourceType = 1;
          s_copySourceReady = false;
          s_scene = SCENE_COPY_READ_SOURCE;
        }
      }
      break;

    case SCENE_COPY_FILE_SELECT:
      if (s_fileList.empty()) {
        if (ev.key == BTN_OK || ev.key == BTN_BACK) {
          resetMenuScroll();
          s_scene = SCENE_COPY_CHOOSE;
        }
      } else {
        if (ev.key == BTN_UP) {
          s_fileSel = (s_fileSel - 1 + (int)s_fileList.size()) % (int)s_fileList.size();
        } else if (ev.key == BTN_DOWN) {
          s_fileSel = (s_fileSel + 1) % (int)s_fileList.size();
        } else if (ev.key == BTN_OK) {
          const auto& fi = s_fileList[s_fileSel];
          char path[64];
          snprintf(path, sizeof(path), "/ibutton/%s", fi.filename);
          char kType[16];
          if (parseFlipperIbtn(fi.storage, path, kType, sizeof(kType), s_sourceRom)) {
            s_copySourceReady = true;
            s_copyStatus = COPY_IDLE;
            s_scene = SCENE_COPY_WRITE_BLANK;
          }
        }
      }
      break;

    case SCENE_COPY_WRITE_BLANK:
      if (s_copyStatus == COPY_SUCCESS || s_copyStatus == COPY_FAILED) {
        if (ev.key == BTN_OK) {
          s_copyStatus = COPY_IDLE;
        }
      }
      break;

    case SCENE_SAVE:
      if (s_saveSuccess) {
        if (ev.key == BTN_OK) {
          s_saveSuccess = false;
        }
      }
      break;

    case SCENE_EMULATE_CHOOSE:
      if (ev.key == BTN_UP) {
        s_menuSel = (s_menuSel - 1 + 3) % 3;
      } else if (ev.key == BTN_DOWN) {
        s_menuSel = (s_menuSel + 1) % 3;
      } else if (ev.key == BTN_OK) {
        if (s_menuSel == 0) {
          reloadIbtnFiles();
          s_fileSel = 0;
          resetMenuScroll();
          s_scene = SCENE_EMULATE_FILE_SELECT;
        } else if (s_menuSel == 1) {
          s_brandSel = 0;
          resetMenuScroll();
          s_scene = SCENE_UNIVERSAL_BRANDS;
        } else if (s_menuSel == 2) {
          if (s_activeRomValid) {
            s_emulateHits = 0;
            s_scene = SCENE_EMULATE_RUN;
          }
        }
      }
      break;

    case SCENE_EMULATE_FILE_SELECT:
      if (s_fileList.empty()) {
        if (ev.key == BTN_OK || ev.key == BTN_BACK) {
          resetMenuScroll();
          s_scene = SCENE_EMULATE_CHOOSE;
        }
      } else {
        if (ev.key == BTN_UP) {
          s_fileSel = (s_fileSel - 1 + (int)s_fileList.size()) % (int)s_fileList.size();
        } else if (ev.key == BTN_DOWN) {
          s_fileSel = (s_fileSel + 1) % (int)s_fileList.size();
        } else if (ev.key == BTN_OK) {
          const auto& fi = s_fileList[s_fileSel];
          char path[64];
          snprintf(path, sizeof(path), "/ibutton/%s", fi.filename);
          char kType[16];
          if (parseFlipperIbtn(fi.storage, path, kType, sizeof(kType), s_activeRom)) {
            s_activeRomValid = true;
            s_emulateHits = 0;
            s_scene = SCENE_EMULATE_RUN;
          }
        }
      }
      break;

    case SCENE_EMULATE_RUN:
      if (ev.key == BTN_OK) {
        resetMenuScroll();
        s_scene = SCENE_MENU;
      }
      break;

    case SCENE_UNIVERSAL_BRANDS:
      if (ev.key == BTN_UP) {
        s_brandSel = (s_brandSel - 1 + kBrandCount) % kBrandCount;
      } else if (ev.key == BTN_DOWN) {
        s_brandSel = (s_brandSel + 1) % kBrandCount;
      } else if (ev.key == BTN_OK) {
        s_univKeySel = 0;
        resetMenuScroll();
        s_scene = SCENE_UNIVERSAL_KEYS;
      }
      break;

    case SCENE_UNIVERSAL_KEYS: {
      int count = kUniversalBrands[s_brandSel].keyCount;
      if (ev.key == BTN_UP) {
        s_univKeySel = (s_univKeySel - 1 + count) % count;
      } else if (ev.key == BTN_DOWN) {
        s_univKeySel = (s_univKeySel + 1) % count;
      } else if (ev.key == BTN_OK) {
        memcpy(s_activeRom, kUniversalBrands[s_brandSel].keys[s_univKeySel].rom, 8);
        s_activeRomValid = true;
        s_univActionSel = 0;
        s_scene = SCENE_UNIVERSAL_ACTION;
      }
      break;
    }

    case SCENE_UNIVERSAL_ACTION:
      if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT || ev.key == BTN_UP || ev.key == BTN_DOWN) {
        s_univActionSel = 1 - s_univActionSel;
      } else if (ev.key == BTN_OK) {
        if (s_univActionSel == 0) {
          s_emulateHits = 0;
          s_scene = SCENE_EMULATE_RUN;
        } else {
          char fname[32];
          snprintf(fname, sizeof(fname), "%s_%02X%02X.ibtn",
            kUniversalBrands[s_brandSel].brandName, s_activeRom[1], s_activeRom[2]);
          saveFlipperIbtn(fname, "Dallas", s_activeRom);
          s_saveSuccess = true;
          strncpy(s_savedFileName, fname, sizeof(s_savedFileName));
          s_scene = SCENE_SAVE;
        }
      }
      break;

    default:
      break;
  }
}




void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "iButton");
  u8g2.setDrawColor(1);


  u8g2.setClipWindow(0, 14, 128, 64);

  switch (s_scene) {
    case SCENE_MENU: {

      u8g2.setFont(u8g2_font_6x12_tr);
      for (int i = 0; i < kMainMenuCount; i++) {
        int y = startY + (int)(i * rowH - s_scroll + 0.5f);
        if (y < startY - rowH || y > 64) continue;
        u8g2.drawStr(6, y + 10, kMainMenuItems[i]);
      }


      u8g2.setDrawColor(2);
      u8g2.drawBox(2, (int)(s_barY + 0.5f), 124, rowH);
      u8g2.setDrawColor(1);
      break;
    }

    case SCENE_READ: {
      if (!s_readFound) {

        u8g2.setFont(u8g2_font_6x12_tr);
        const char* msg = "Touch iButton key";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 42, msg);
      } else {

        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(6, 25, "Key: Dallas DS1990");

        char hexStr[32];
        snprintf(hexStr, sizeof(hexStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
          s_activeRom[0], s_activeRom[1], s_activeRom[2], s_activeRom[3],
          s_activeRom[4], s_activeRom[5], s_activeRom[6], s_activeRom[7]);
        u8g2.drawStr(6, 36, hexStr);

        char crcStr[32];
        snprintf(crcStr, sizeof(crcStr), "Family: %02X  CRC: %s",
          s_activeRom[0], s_readCrcOk ? "[OK]" : "[FAIL]");
        u8g2.drawStr(6, 47, crcStr);


        const char* acts[] = { "Save", "Emulate", "Again" };
        int actX[] = { 6, 44, 92 };
        int actW[] = { 32, 44, 30 };
        for (int i = 0; i < 3; i++) {
          if (i == s_readActionSel) {
            u8g2.drawBox(actX[i], 51, actW[i], 11);
            u8g2.setDrawColor(0);
            u8g2.drawStr(actX[i] + 3, 60, acts[i]);
            u8g2.setDrawColor(1);
          } else {
            u8g2.drawStr(actX[i] + 3, 60, acts[i]);
          }
        }
      }
      break;
    }

    case SCENE_COPY_CHOOSE: {

      u8g2.setFont(u8g2_font_6x12_tr);
      static const char* kSources[] = { "From File", "From Key" };
      for (int i = 0; i < 2; i++) {
        u8g2.drawStr(6, startY + i * rowH + 10, kSources[i]);
      }

      u8g2.setDrawColor(2);
      u8g2.drawBox(2, startY + s_menuSel * rowH, 124, rowH);
      u8g2.setDrawColor(1);
      break;
    }

    case SCENE_COPY_FILE_SELECT:
    case SCENE_EMULATE_FILE_SELECT: {
      if (s_fileList.empty()) {

        u8g2.setFont(u8g2_font_6x12_tr);
        const char* msg = "Files not found";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 42, msg);
      } else {
        u8g2.setFont(u8g2_font_6x10_tr);
        for (size_t i = 0; i < s_fileList.size(); i++) {
          int y = startY + (int)(i * rowH - s_scroll + 0.5f);
          if (y < startY - rowH || y > 64) continue;
          const auto& it = s_fileList[i];
          char row[36];
          snprintf(row, sizeof(row), "%s %s",
            (it.storage == fm_fs::STORAGE_SD) ? "[SD]" : "[IN]", it.filename);
          u8g2.drawStr(6, y + 10, row);
        }


        u8g2.setDrawColor(2);
        u8g2.drawBox(2, (int)(s_barY + 0.5f), 124, rowH);
        u8g2.setDrawColor(1);
      }
      break;
    }

    case SCENE_COPY_READ_SOURCE: {
      u8g2.setFont(u8g2_font_6x12_tr);
      const char* msg = "Touch source key";
      int w = u8g2.getStrWidth(msg);
      u8g2.drawStr((128 - w) / 2, 42, msg);
      break;
    }

    case SCENE_COPY_WRITE_BLANK: {
      u8g2.setFont(u8g2_font_5x8_tr);
      char hexStr[32];
      snprintf(hexStr, sizeof(hexStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
        s_sourceRom[0], s_sourceRom[1], s_sourceRom[2], s_sourceRom[3],
        s_sourceRom[4], s_sourceRom[5], s_sourceRom[6], s_sourceRom[7]);
      u8g2.drawStr(6, 25, hexStr);

      if (s_copyStatus == COPY_IDLE) {
        u8g2.setFont(u8g2_font_6x12_tr);
        const char* msg = "Touch blank key";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 44, msg);
      } else if (s_copyStatus == COPY_WRITING) {
        u8g2.setFont(u8g2_font_6x12_tr);
        const char* msg = "Writing ROM bits...";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 44, msg);
      } else if (s_copyStatus == COPY_SUCCESS) {
        u8g2.drawBox(10, 32, 108, 22);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(26, 47, "Successful!");
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(26, 60, "Key Copied & Verified");
      } else if (s_copyStatus == COPY_FAILED) {
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(12, 44, "Write Failed!");
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(12, 56, "Not RW1990? [OK] Retry");
      }
      break;
    }

    case SCENE_SAVE: {
      if (!s_saveSuccess) {

        u8g2.setFont(u8g2_font_6x12_tr);
        const char* msg = "Touch key to save";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 42, msg);
      } else {
        u8g2.setFont(u8g2_font_6x10_tr);
        char hexStr[32];
        snprintf(hexStr, sizeof(hexStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
          s_activeRom[0], s_activeRom[1], s_activeRom[2], s_activeRom[3],
          s_activeRom[4], s_activeRom[5], s_activeRom[6], s_activeRom[7]);
        u8g2.drawStr(6, 25, hexStr);

        u8g2.drawBox(6, 31, 116, 18);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(10, 44, "Successful!");
        u8g2.setDrawColor(1);

        char fnStr[36];
        snprintf(fnStr, sizeof(fnStr), "Saved: %s", s_savedFileName);
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(6, 58, fnStr);
      }
      break;
    }

    case SCENE_EMULATE_CHOOSE: {

      u8g2.setFont(u8g2_font_6x12_tr);
      static const char* kEmOpts[] = {
        "From File",
        "From Universal",
        "Last Read Key"
      };
      for (int i = 0; i < 3; i++) {
        u8g2.drawStr(6, startY + i * rowH + 10, kEmOpts[i]);
      }

      u8g2.setDrawColor(2);
      u8g2.drawBox(2, startY + s_menuSel * rowH, 124, rowH);
      u8g2.setDrawColor(1);
      break;
    }

    case SCENE_EMULATE_RUN: {
      u8g2.setFont(u8g2_font_6x10_tr);
      char hexStr[32];
      snprintf(hexStr, sizeof(hexStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
        s_activeRom[0], s_activeRom[1], s_activeRom[2], s_activeRom[3],
        s_activeRom[4], s_activeRom[5], s_activeRom[6], s_activeRom[7]);
      u8g2.drawStr(6, 25, hexStr);

      u8g2.drawStr(6, 37, "Touch to intercom");

      char hitsStr[32];
      snprintf(hitsStr, sizeof(hitsStr), "Intercom hits: %lu", (unsigned long)s_emulateHits);
      u8g2.drawStr(6, 48, hitsStr);


      if (s_lastHitTime > 0 && (now - s_lastHitTime) < 300) {
        u8g2.drawBox(6, 52, 116, 11);
        u8g2.setDrawColor(0);
        u8g2.drawStr(12, 61, ">>> TRANSMITTING <<<");
        u8g2.setDrawColor(1);
      } else {
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(6, 60, "Touch to reader  |  [OK] Exit");
      }
      break;
    }

    case SCENE_UNIVERSAL_BRANDS: {
      u8g2.setFont(u8g2_font_6x12_tr);
      for (int i = 0; i < kBrandCount; i++) {
        int y = startY + (int)(i * rowH - s_scroll + 0.5f);
        if (y < startY - rowH || y > 64) continue;
        u8g2.drawStr(6, y + 10, kUniversalBrands[i].brandName);
      }
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, (int)(s_barY + 0.5f), 124, rowH);
      u8g2.setDrawColor(1);
      break;
    }

    case SCENE_UNIVERSAL_KEYS: {
      const auto& brand = kUniversalBrands[s_brandSel];
      u8g2.setFont(u8g2_font_6x10_tr);
      for (int i = 0; i < brand.keyCount; i++) {
        int y = startY + (int)(i * rowH - s_scroll + 0.5f);
        if (y < startY - rowH || y > 64) continue;
        u8g2.drawStr(6, y + 10, brand.keys[i].name);
      }
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, (int)(s_barY + 0.5f), 124, rowH);
      u8g2.setDrawColor(1);
      break;
    }

    case SCENE_UNIVERSAL_ACTION: {
      const auto& brand = kUniversalBrands[s_brandSel];
      const auto& key = brand.keys[s_univKeySel];

      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(6, 24, key.name);

      u8g2.setFont(u8g2_font_5x8_tr);
      char hexStr[32];
      snprintf(hexStr, sizeof(hexStr), "%02X %02X %02X %02X %02X %02X %02X %02X",
        s_activeRom[0], s_activeRom[1], s_activeRom[2], s_activeRom[3],
        s_activeRom[4], s_activeRom[5], s_activeRom[6], s_activeRom[7]);
      u8g2.drawStr(6, 36, hexStr);

      u8g2.setFont(u8g2_font_6x10_tr);
      const char* acts[] = { "EMULATE", "SAVE" };
      int actX[] = { 6, 66 };
      int actW = 56;
      for (int i = 0; i < 2; i++) {
        int strW = u8g2.getStrWidth(acts[i]);
        int textX = actX[i] + (actW - strW) / 2;
        if (i == s_univActionSel) {
          u8g2.drawBox(actX[i], 47, actW, 12);
          u8g2.setDrawColor(0);
          u8g2.drawStr(textX, 56, acts[i]);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(textX, 56, acts[i]);
        }
      }
      break;
    }

    default:
      break;
  }


  u8g2.setMaxClipWindow();
}

}
