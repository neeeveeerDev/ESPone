#include "scene_wifi_attacks.h"
#include "attacks/wifi_attacks_common.h"
#include "attacks/attack_evil_portal.h"
#include "attacks/attack_beacon_spam.h"
#include "attacks/attack_deauth.h"
#include "attacks/attack_handshake.h"
#include "attacks/attack_bruteforce.h"

namespace scene_wifi_attacks {

using namespace wifi_attacks_common;

enum SubAttack : uint8_t {
  SUB_MENU = 0,
  SUB_EVIL_PORTAL,
  SUB_BEACON_SPAM,
  SUB_DEAUTH,
  SUB_HANDSHAKE,
  SUB_BRUTEFORCE
};

static SubAttack s_subState = SUB_MENU;
static int       s_menuSel = 0;
static float     s_barY = (float)kStartY;
static float     s_menuScroll = 0.0f;
static float     s_menuTargetScroll = 0.0f;

static const char* kAttackItems[] = {
  "Evil Portal",
  "Beacon Spam",
  "Deauth Attack",
  "Handshake Cap",
  "Bruteforce"
};
static const int kAttackCount = sizeof(kAttackItems) / sizeof(kAttackItems[0]);

void open() {
  s_subState = SUB_MENU;
  s_menuSel = 0;
  s_barY = (float)kStartY;
  s_menuScroll = 0.0f;
  s_menuTargetScroll = 0.0f;
}

void close() {
  if (s_subState == SUB_EVIL_PORTAL) attack_evil_portal::close();
  else if (s_subState == SUB_BEACON_SPAM) attack_beacon_spam::close();
  else if (s_subState == SUB_DEAUTH) attack_deauth::close();
  else if (s_subState == SUB_HANDSHAKE) attack_handshake::close();
  else if (s_subState == SUB_BRUTEFORCE) attack_bruteforce::close();

  s_subState = SUB_MENU;
}

void tick(uint32_t now) {
  if (s_subState == SUB_MENU) {
    updateScroll(s_menuSel, kAttackCount, s_menuScroll, s_menuTargetScroll, s_barY);
  } else if (s_subState == SUB_EVIL_PORTAL) {
    attack_evil_portal::tick(now);
  } else if (s_subState == SUB_BEACON_SPAM) {
    attack_beacon_spam::tick(now);
  } else if (s_subState == SUB_DEAUTH) {
    attack_deauth::tick(now);
  } else if (s_subState == SUB_HANDSHAKE) {
    attack_handshake::tick(now);
  } else if (s_subState == SUB_BRUTEFORCE) {
    attack_bruteforce::tick(now);
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_subState == SUB_MENU) {
    if (ev.key == BTN_UP) {
      s_menuSel = (s_menuSel - 1 + kAttackCount) % kAttackCount;
    } else if (ev.key == BTN_DOWN) {
      s_menuSel = (s_menuSel + 1) % kAttackCount;
    } else if (ev.key == BTN_OK) {
      if (s_menuSel == 0) {
        attack_evil_portal::open();
        s_subState = SUB_EVIL_PORTAL;
      } else if (s_menuSel == 1) {
        attack_beacon_spam::open();
        s_subState = SUB_BEACON_SPAM;
      } else if (s_menuSel == 2) {
        attack_deauth::open();
        s_subState = SUB_DEAUTH;
      } else if (s_menuSel == 3) {
        attack_handshake::open();
        s_subState = SUB_HANDSHAKE;
      } else if (s_menuSel == 4) {
        attack_bruteforce::open();
        s_subState = SUB_BRUTEFORCE;
      }
    }
  } else if (s_subState == SUB_EVIL_PORTAL) {
    attack_evil_portal::onKey(ev);
  } else if (s_subState == SUB_BEACON_SPAM) {
    attack_beacon_spam::onKey(ev);
  } else if (s_subState == SUB_DEAUTH) {
    attack_deauth::onKey(ev);
  } else if (s_subState == SUB_HANDSHAKE) {
    attack_handshake::onKey(ev);
  } else if (s_subState == SUB_BRUTEFORCE) {
    attack_bruteforce::onKey(ev);
  }
}

bool onBack() {
  if (s_subState == SUB_EVIL_PORTAL) {
    if (!attack_evil_portal::onBack()) {
      attack_evil_portal::close();
      s_subState = SUB_MENU;
    }
    return true;
  } else if (s_subState == SUB_BEACON_SPAM) {
    if (!attack_beacon_spam::onBack()) {
      attack_beacon_spam::close();
      s_subState = SUB_MENU;
    }
    return true;
  } else if (s_subState == SUB_DEAUTH) {
    if (!attack_deauth::onBack()) {
      attack_deauth::close();
      s_subState = SUB_MENU;
    }
    return true;
  } else if (s_subState == SUB_HANDSHAKE) {
    if (!attack_handshake::onBack()) {
      attack_handshake::close();
      s_subState = SUB_MENU;
    }
    return true;
  } else if (s_subState == SUB_BRUTEFORCE) {
    if (!attack_bruteforce::onBack()) {
      attack_bruteforce::close();
      s_subState = SUB_MENU;
    }
    return true;
  }

  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_subState == SUB_EVIL_PORTAL) {
    attack_evil_portal::draw(u8g2, now);
    return;
  } else if (s_subState == SUB_BEACON_SPAM) {
    attack_beacon_spam::draw(u8g2, now);
    return;
  } else if (s_subState == SUB_DEAUTH) {
    attack_deauth::draw(u8g2, now);
    return;
  } else if (s_subState == SUB_HANDSHAKE) {
    attack_handshake::draw(u8g2, now);
    return;
  } else if (s_subState == SUB_BRUTEFORCE) {
    attack_bruteforce::draw(u8g2, now);
    return;
  }


  drawHeader(u8g2, "WIFI ATTACKS");

  u8g2.setClipWindow(0, 14, 128, 64);
  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < kAttackCount; i++) {
    int y = kStartY + (int)(i * kRowH - s_menuScroll + 0.5f);
    if (y < kStartY - kRowH || y > 64) continue;
    u8g2.drawStr(6, y + 10, kAttackItems[i]);
  }


  u8g2.setDrawColor(2);
  int curY = (int)(s_barY + 0.5f);
  if (curY < kStartY) curY = kStartY;
  if (curY > 64 - kRowH) curY = 64 - kRowH;
  u8g2.drawBox(2, curY, 124, kRowH);
  u8g2.setDrawColor(1);
  u8g2.setMaxClipWindow();
}

}
