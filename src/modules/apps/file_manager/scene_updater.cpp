#include "scene_updater.h"
#include <Update.h>
#include <Arduino.h>

namespace scene_updater {

enum State {
  STATE_CONFIRM = 0,
  STATE_STARTING,
  STATE_FLASHING,
  STATE_REBOOT,
  STATE_ERROR
};

static State s_state = STATE_CONFIRM;
static fm_fs::StorageType s_storage = fm_fs::STORAGE_NONE;
static String s_fullPath = "";
static String s_fileName = "";

static int s_selectedOption = 0;
static File s_file;
static size_t s_totalSize = 0;
static size_t s_bytesWritten = 0;
static int s_progress = 0;
static uint32_t s_rebootTimer = 0;
static String s_errorMsg = "";

static uint8_t s_chunkBuffer[4096];

void open(fm_fs::StorageType st, const char* fullPath, const char* fileName) {
  s_storage = st;
  s_fullPath = String(fullPath ? fullPath : "");
  s_fileName = String(fileName ? fileName : "");
  s_selectedOption = 0;
  s_state = STATE_CONFIRM;
  s_totalSize = 0;
  s_bytesWritten = 0;
  s_progress = 0;
  s_errorMsg = "";
}

void close() {
  if (s_file) {
    s_file.close();
  }
  if (s_state == STATE_FLASHING) {
    Update.abort();
  }
}

UpdaterResult onKey(const KeyEvent& ev) {
  if (s_state == STATE_CONFIRM) {
    if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT || ev.key == BTN_UP || ev.key == BTN_DOWN) {
      s_selectedOption = (s_selectedOption == 0) ? 1 : 0;
    } else if (ev.key == BTN_BACK) {
      return UPDATER_CANCEL;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_selectedOption == 1) {
        return UPDATER_CANCEL;
      } else {
        s_state = STATE_STARTING;
      }
    }
  } else if (s_state == STATE_ERROR) {
    if (ev.key == BTN_OK || ev.key == BTN_BACK) {
      return UPDATER_CANCEL;
    }
  }

  return UPDATER_RUNNING;
}

void tick(uint32_t now) {
  if (s_state == STATE_STARTING) {
    fs::FS* fs = fm_fs::getFS(s_storage);
    if (!fs) {
      s_state = STATE_ERROR;
      s_errorMsg = "Storage not available";
      return;
    }

    s_file = fs->open(s_fullPath.c_str(), "r");
    if (!s_file || s_file.isDirectory()) {
      s_state = STATE_ERROR;
      s_errorMsg = "Cannot open file";
      if (s_file) s_file.close();
      return;
    }

    s_totalSize = s_file.size();
    if (s_totalSize == 0) {
      s_state = STATE_ERROR;
      s_errorMsg = "File is empty";
      s_file.close();
      return;
    }

    if (!Update.begin(s_totalSize, U_FLASH)) {
      s_state = STATE_ERROR;
      s_errorMsg = "Not enough OTA space";
      s_file.close();
      return;
    }

    s_bytesWritten = 0;
    s_progress = 0;
    s_state = STATE_FLASHING;
    return;
  }

  if (s_state == STATE_FLASHING) {
    if (!s_file || !s_file.available()) {
      s_state = STATE_ERROR;
      s_errorMsg = "File read failed";
      s_file.close();
      return;
    }

    size_t toRead = sizeof(s_chunkBuffer);
    if (s_totalSize - s_bytesWritten < toRead) {
      toRead = s_totalSize - s_bytesWritten;
    }

    size_t r = s_file.read(s_chunkBuffer, toRead);
    if (r > 0) {
      size_t w = Update.write(s_chunkBuffer, r);
      if (w != r) {
        s_state = STATE_ERROR;
        s_errorMsg = "Flash write failed";
        s_file.close();
        return;
      }
      s_bytesWritten += w;
      s_progress = (int)((s_bytesWritten * 100ULL) / s_totalSize);
    }

    if (s_bytesWritten >= s_totalSize) {
      s_file.close();
      if (Update.end(true) && Update.isFinished()) {
        s_state = STATE_REBOOT;
        s_rebootTimer = millis();
      } else {
        s_state = STATE_ERROR;
        s_errorMsg = "Verification failed";
      }
    }
    return;
  }

  if (s_state == STATE_REBOOT) {
    if (millis() - s_rebootTimer >= 1500) {
      ESP.restart();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.setDrawColor(1);

  if (s_state == STATE_CONFIRM) {

    u8g2.setFont(u8g2_font_6x13B_tr);
    const char* qTitle = "Do you want update?";
    int tw = u8g2.getStrWidth(qTitle);
    int qx = (128 - tw) / 2;
    if (qx < 2) qx = 2;
    u8g2.drawStr(qx, 18, qTitle);


    u8g2.setFont(u8g2_font_5x7_tr);
    char fDisp[24];
    snprintf(fDisp, sizeof(fDisp), "%.22s", s_fileName.c_str());
    int fw = u8g2.getStrWidth(fDisp);
    int fx = (128 - fw) / 2;
    if (fx < 2) fx = 2;
    u8g2.drawStr(fx, 31, fDisp);


    const int btnW = 40;
    const int btnH = 15;
    const int yesX = 18;
    const int noX = 70;
    const int btnY = 42;

    u8g2.setFont(u8g2_font_6x12_tr);


    if (s_selectedOption == 0) {
      u8g2.setDrawColor(1);
      u8g2.drawRBox(yesX, btnY, btnW, btnH, 2);
      u8g2.setDrawColor(0);
      u8g2.drawStr(yesX + 10, btnY + 11, "YES");
      u8g2.setDrawColor(1);
    } else {
      u8g2.setDrawColor(1);
      u8g2.drawRFrame(yesX, btnY, btnW, btnH, 2);
      u8g2.drawStr(yesX + 10, btnY + 11, "YES");
    }


    if (s_selectedOption == 1) {
      u8g2.setDrawColor(1);
      u8g2.drawRBox(noX, btnY, btnW, btnH, 2);
      u8g2.setDrawColor(0);
      u8g2.drawStr(noX + 14, btnY + 11, "NO");
      u8g2.setDrawColor(1);
    } else {
      u8g2.setDrawColor(1);
      u8g2.drawRFrame(noX, btnY, btnW, btnH, 2);
      u8g2.drawStr(noX + 14, btnY + 11, "NO");
    }
    return;
  }

  if (s_state == STATE_STARTING || s_state == STATE_FLASHING) {

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* upTxt = "Updating...";
    int uw = u8g2.getStrWidth(upTxt);
    u8g2.drawStr((128 - uw) / 2, 18, upTxt);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* warnTxt = "Do not power off!";
    int ww = u8g2.getStrWidth(warnTxt);
    u8g2.drawStr((128 - ww) / 2, 28, warnTxt);


    u8g2.drawRFrame(14, 34, 100, 10, 2);
    int barW = (s_progress * 96) / 100;
    if (barW > 0) {
      if (barW > 96) barW = 96;
      u8g2.drawRBox(16, 36, barW, 6, 1);
    }


    char pctStr[16];
    snprintf(pctStr, sizeof(pctStr), "%d%%", s_progress);
    u8g2.setFont(u8g2_font_6x10_tr);
    int pw = u8g2.getStrWidth(pctStr);
    u8g2.drawStr((128 - pw) / 2, 57, pctStr);
    return;
  }

  if (s_state == STATE_REBOOT) {

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* rbTxt = "Reboot";
    int rw = u8g2.getStrWidth(rbTxt);
    u8g2.drawStr((128 - rw) / 2, 36, rbTxt);
    return;
  }

  if (s_state == STATE_ERROR) {
    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* errTitle = "Update Failed!";
    int ew = u8g2.getStrWidth(errTitle);
    u8g2.drawStr((128 - ew) / 2, 18, errTitle);

    u8g2.setFont(u8g2_font_6x10_tr);
    int mw = u8g2.getStrWidth(s_errorMsg.c_str());
    u8g2.drawStr((128 - mw) / 2, 34, s_errorMsg.c_str());

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okTxt = "[ Press OK to return ]";
    int ow = u8g2.getStrWidth(okTxt);
    u8g2.drawStr((128 - ow) / 2, 52, okTxt);
    return;
  }
}

}
