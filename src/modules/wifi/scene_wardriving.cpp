#include "scene_wardriving.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <algorithm>
#include "core/vfs_mgr.h"
#include "config.h"

namespace scene_wardriving {

enum WardriveState : uint8_t {
  STATE_MODE_MENU = 0,
  STATE_NO_GPS,
  STATE_WITH_GPS
};

struct WardriveAp {
  char bssid[18];
  char ssid[33];
  char authStr[10];
  uint8_t channel;
  int32_t rssi;
};

static const int kRowH = 12;
static const int kStartY = 16;

static WardriveState s_state = STATE_MODE_MENU;
static int s_menuSel = 0;
static float s_barY = (float)kStartY;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;


static std::vector<WardriveAp> s_loggedAps;
static uint32_t s_openCount = 0;
static int s_listSel = 0;


static bool s_scanning = false;
static bool s_savedNotice = false;
static uint32_t s_savedNoticeTime = 0;


static double s_gpsLat = 55.75583;
static double s_gpsLon = 37.61730;
static float s_gpsAlt = 156.0f;
static int s_gpsSats = 0;
static bool s_gpsFixed = false;
static bool s_usingFileTrack = false;
static char s_trackFileName[32] = "";
static HardwareSerial s_gpsSerial(1);

static const char* kMenuModes[] = {
  "No GPS",
  "With GPS"
};

static void drawHeader(U8G2& u8g2, const char* title, const char* badge = nullptr) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);

  if (badge && strlen(badge) > 0) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int bw = u8g2.getStrWidth(badge);
    u8g2.drawStr(124 - bw, 10, badge);
  }
  u8g2.setDrawColor(1);
}

static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / kRowH + 0.5f);
  if (sel < topIndex) {
    targetScroll = (float)(sel * kRowH);
  } else if (sel >= topIndex + maxLines) {
    targetScroll = (float)((sel - maxLines + 1) * kRowH);
  }

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * kRowH)) targetScroll = (float)(maxScroll * kRowH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) {
    scroll += ds * 0.35f;
  } else {
    scroll = targetScroll;
  }

  float targetBarY = (float)(kStartY + sel * kRowH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) {
    barY += dy * 0.4f;
  } else {
    barY = targetBarY;
  }
}

static const char* authToString(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
    default: return "ENC";
  }
}

static void checkGpsTrackFile() {
  s_usingFileTrack = false;
  s_trackFileName[0] = '\0';

  const char* candidates[] = {
    "/sd/coords.txt",
    "/sd/gps_track.txt",
    "/ffat/coords.txt",
    "/coords.txt"
  };

  for (const char* path : candidates) {
    File f = vfs::open(path, FILE_READ);
    if (f) {
      String line = f.readStringUntil('\n');
      f.close();
      line.trim();
      double lat = 0, lon = 0;
      if (sscanf(line.c_str(), "%lf,%lf", &lat, &lon) == 2 || sscanf(line.c_str(), "%lf %lf", &lat, &lon) == 2) {
        s_gpsLat = lat;
        s_gpsLon = lon;
        s_gpsFixed = true;
        s_usingFileTrack = true;
        snprintf(s_trackFileName, sizeof(s_trackFileName), "%s", strrchr(path, '/') + 1);
        return;
      }
    }
  }
}

static void initGpsUart() {
  checkGpsTrackFile();
  if (s_usingFileTrack) return;
  s_gpsSerial.begin(9600, SERIAL_8N1, UART_BRIDGE_PIN_RX, UART_BRIDGE_PIN_TX);
}

static void pollNmeaGps() {
  if (s_usingFileTrack) return;

  while (s_gpsSerial.available() > 0) {
    char c = s_gpsSerial.read();
    static char nmeaBuf[100];
    static int nmeaIdx = 0;

    if (c == '$') {
      nmeaIdx = 0;
      nmeaBuf[nmeaIdx++] = c;
    } else if (c == '\n' || c == '\r') {
      if (nmeaIdx > 0 && nmeaIdx < 99) {
        nmeaBuf[nmeaIdx] = '\0';
        if (strncmp(nmeaBuf, "$GPRMC", 6) == 0 || strncmp(nmeaBuf, "$GNRMC", 6) == 0) {
          char status = 'V';
          double rawLat = 0, rawLon = 0;
          char latDir = 'N', lonDir = 'E';
          char timeStr[16];
          if (sscanf(nmeaBuf, "%*[^,],%[^,],%c,%lf,%c,%lf,%c", timeStr, &status, &rawLat, &latDir, &rawLon, &lonDir) >= 6) {
            if (status == 'A') {
              int latDeg = (int)(rawLat / 100);
              double latMin = rawLat - (latDeg * 100);
              s_gpsLat = latDeg + (latMin / 60.0);
              if (latDir == 'S') s_gpsLat = -s_gpsLat;

              int lonDeg = (int)(rawLon / 100);
              double lonMin = rawLon - (lonDeg * 100);
              s_gpsLon = lonDeg + (lonMin / 60.0);
              if (lonDir == 'W') s_gpsLon = -s_gpsLon;

              s_gpsFixed = true;
            }
          }
        } else if (strncmp(nmeaBuf, "$GPGGA", 6) == 0 || strncmp(nmeaBuf, "$GNGGA", 6) == 0) {
          int fixQual = 0, sats = 0;
          float alt = 0;
          sscanf(nmeaBuf, "%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%d,%d,%*[^,],%f", &fixQual, &sats, &alt);
          s_gpsSats = sats;
          if (fixQual > 0) {
            s_gpsFixed = true;
            s_gpsAlt = alt;
          }
        }
      }
      nmeaIdx = 0;
    } else {
      if (nmeaIdx < 98) nmeaBuf[nmeaIdx++] = c;
    }
  }
}

static void saveToWigleCsv() {
  const char* filename = "/wigle.csv";
  bool isNew = !vfs::exists(filename);
  File f = vfs::open(filename, FILE_APPEND);
  if (f && isNew) {
    f.println("WigleWifi-1.4,appRelease=ESPone,model=ESP32-S3,release=1.0,device=ESPone,display=SSD1306,board=ESP32-S3");
    f.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
  }

  if (f) {
    for (const auto& ap : s_loggedAps) {
      f.printf("%s,%s,%s,2026-10-13 12:00:00,%d,%d,%.6f,%.6f,%.1f,5.0,WIFI\n",
        ap.bssid, ap.ssid, ap.authStr, (int)ap.channel, (int)ap.rssi, s_gpsLat, s_gpsLon, s_gpsAlt);
    }
    f.close();
    s_savedNotice = true;
    s_savedNoticeTime = millis();
  }
}

static void saveToSimpleCsv() {
  const char* filename = "/wardrive_nogps.csv";
  File f = vfs::open(filename, FILE_WRITE);

  if (f) {
    f.println("SSID,AUTH,BSSID,CHANNEL,RSSI");
    for (const auto& ap : s_loggedAps) {
      f.printf("\"%s\",%s,%s,%d,%d\n", ap.ssid, ap.authStr, ap.bssid, (int)ap.channel, (int)ap.rssi);
    }
    f.close();
    s_savedNotice = true;
    s_savedNoticeTime = millis();
  }
}

void open() {
  s_state = STATE_MODE_MENU;
  s_menuSel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_loggedAps.clear();
  s_openCount = 0;
  s_listSel = 0;
  s_savedNotice = false;
}

void close() {
  WiFi.scanDelete();
}

bool onBack() {
  if (s_state != STATE_MODE_MENU) {
    WiFi.scanDelete();
    s_state = STATE_MODE_MENU;
    s_barY = (float)kStartY;
    s_scroll = 0.0f;
    s_targetScroll = 0.0f;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_state == STATE_MODE_MENU) {
    updateScroll(s_menuSel, 2, s_scroll, s_targetScroll, s_barY);
    return;
  }

  if (s_state == STATE_WITH_GPS) {
    pollNmeaGps();
  }

  if (s_savedNotice && now - s_savedNoticeTime > 2000) {
    s_savedNotice = false;
  }


  if (!s_scanning) {
    s_scanning = true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true, true, false, 80);
  } else {
    int16_t res = WiFi.scanComplete();
    if (res >= 0) {
      s_scanning = false;
      for (int i = 0; i < res; i++) {
        String bssid = WiFi.BSSIDstr(i);
        bool exists = false;
        for (auto& ap : s_loggedAps) {
          if (strcmp(ap.bssid, bssid.c_str()) == 0) {
            exists = true;
            if (WiFi.RSSI(i) > ap.rssi) ap.rssi = WiFi.RSSI(i);
            break;
          }
        }
        if (!exists) {
          WardriveAp ap;
          snprintf(ap.bssid, sizeof(ap.bssid), "%s", bssid.c_str());
          String ssid = WiFi.SSID(i);
          snprintf(ap.ssid, sizeof(ap.ssid), "%s", ssid.length() > 0 ? ssid.c_str() : "<Hidden>");
          snprintf(ap.authStr, sizeof(ap.authStr), "%s", authToString(WiFi.encryptionType(i)));
          ap.channel = WiFi.channel(i);
          ap.rssi = WiFi.RSSI(i);
          if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) s_openCount++;
          s_loggedAps.push_back(ap);
        }
      }
      WiFi.scanDelete();
    }
  }

  if (s_state == STATE_NO_GPS) {
    int total = (int)s_loggedAps.size();
    if (total > 0) {
      if (s_listSel >= total) s_listSel = total - 1;
      updateScroll(s_listSel, total, s_scroll, s_targetScroll, s_barY);
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat && s_state == STATE_MODE_MENU) return;

  if (s_state == STATE_MODE_MENU) {
    if (ev.key == BTN_UP) {
      s_menuSel = (s_menuSel - 1 + 2) % 2;
    } else if (ev.key == BTN_DOWN) {
      s_menuSel = (s_menuSel + 1) % 2;
    } else if (ev.key == BTN_OK) {
      if (s_menuSel == 0) {
        s_state = STATE_NO_GPS;
      } else {
        s_state = STATE_WITH_GPS;
        initGpsUart();
      }
      s_loggedAps.clear();
      s_openCount = 0;
      s_listSel = 0;
      s_scanning = false;
      s_barY = (float)kStartY;
      s_scroll = 0.0f;
      s_targetScroll = 0.0f;
    }
  } else if (s_state == STATE_NO_GPS) {
    int total = (int)s_loggedAps.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_listSel = (s_listSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_listSel = (s_listSel + 1) % total;
      } else if (ev.key == BTN_OK && !ev.repeat) {
        saveToSimpleCsv();
      }
    }
  } else if (s_state == STATE_WITH_GPS) {
    if (ev.key == BTN_OK && !ev.repeat) {
      saveToWigleCsv();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_MODE_MENU) {
    drawHeader(u8g2, "WARDRIVING");

    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < 2; i++) {
      int y = kStartY + (int)(i * kRowH - s_scroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMenuModes[i]);
    }


    u8g2.setDrawColor(2);
    int curY = (int)(s_barY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();
    return;
  }




  if (s_state == STATE_NO_GPS) {
    char badge[16];
    if (s_savedNotice) {
      snprintf(badge, sizeof(badge), "SAVED!");
    } else {
      snprintf(badge, sizeof(badge), "APs:%d", (int)s_loggedAps.size());
    }
    drawHeader(u8g2, "WARDRIVE", badge);

    int total = (int)s_loggedAps.size();
    if (total == 0) {
      u8g2.setFont(u8g2_font_6x12_tr);
      int dots = (now / 350) % 4;
      char waitBuf[20];
      snprintf(waitBuf, sizeof(waitBuf), "Scanning%.*s", dots, "...");
      int tw = u8g2.getStrWidth(waitBuf);
      u8g2.drawStr((128 - tw) / 2, 40, waitBuf);
      return;
    }

    u8g2.setClipWindow(0, 14, 128, 64);

    for (int i = 0; i < total; i++) {
      int y = kStartY + (int)(i * kRowH - s_scroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      const auto& ap = s_loggedAps[i];


      char metaBuf[16];
      snprintf(metaBuf, sizeof(metaBuf), "%s %d", ap.authStr, (int)ap.rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      int mw = u8g2.getStrWidth(metaBuf);
      u8g2.drawStr(124 - mw, y + 9, metaBuf);


      u8g2.setFont(u8g2_font_6x12_tr);
      int maxW = 124 - mw - 8;
      char ssidLabel[32];
      strncpy(ssidLabel, ap.ssid, sizeof(ssidLabel) - 1);
      ssidLabel[sizeof(ssidLabel) - 1] = '\0';
      while (strlen(ssidLabel) > 0 && u8g2.getStrWidth(ssidLabel) > maxW) {
        ssidLabel[strlen(ssidLabel) - 1] = '\0';
      }
      u8g2.drawStr(6, y + 10, ssidLabel);
    }


    u8g2.setDrawColor(2);
    int curY = (int)(s_barY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();
    return;
  }




  if (s_state == STATE_WITH_GPS) {
    char badge[16];
    if (s_savedNotice) {
      snprintf(badge, sizeof(badge), "SAVED!");
    } else {
      snprintf(badge, sizeof(badge), "APs:%d", (int)s_loggedAps.size());
    }
    drawHeader(u8g2, "WARDRIVE GPS", badge);

    u8g2.setFont(u8g2_font_5x8_tr);

    char buf[40];
    if (s_usingFileTrack) {
      snprintf(buf, sizeof(buf), "SOURCE: %s", s_trackFileName);
      u8g2.drawStr(4, 23, buf);
    } else if (s_gpsFixed) {
      snprintf(buf, sizeof(buf), "GPS FIX (Sats: %d)", s_gpsSats);
      u8g2.drawStr(4, 23, buf);
    } else {
      snprintf(buf, sizeof(buf), "GPS: WAITING FIX...");
      u8g2.drawStr(4, 23, buf);
    }

    snprintf(buf, sizeof(buf), "LAT: %.5f", s_gpsLat);
    u8g2.drawStr(4, 33, buf);

    snprintf(buf, sizeof(buf), "LON: %.5f", s_gpsLon);
    u8g2.drawStr(4, 43, buf);

    snprintf(buf, sizeof(buf), "LOGGED: %d APs", (int)s_loggedAps.size());
    u8g2.drawStr(4, 53, buf);
  }
}

}
