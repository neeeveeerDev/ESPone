#include "scene_tv_b_gone.h"
#include "ir_bitmaps.h"
#include "ir_sender_hw.h"
#include "world_ir_codes.h"

namespace scene_tv_b_gone {

enum ScreenState : uint8_t {
  STATE_REGION_LIST = 0,
  STATE_SWEEP_SCENE
};

enum Region : uint8_t {
  REG_EU = 0,
  REG_NA,
  REG_ALL,
  REG_COUNT
};

static const char* kRegionNames[REG_COUNT] = {
  "EU / World",
  "North America",
  "All Sweep"
};


static ScreenState s_state = STATE_REGION_LIST;
static int s_menuSel = 0;
static Region s_selectedRegion = REG_EU;
static bool s_running = false;
static int s_curIdx = 0;
static uint32_t s_lastSend = 0;
static uint8_t s_lastFreq = 38;


static uint8_t s_bitsleft_r = 0;
static uint8_t s_bits_r = 0;
static uint8_t s_code_ptr = 0;
static const tv_world_codes::IrCode* s_powerCode = nullptr;

static uint8_t read_bits(uint8_t count) {
  uint8_t tmp = 0;
  while (count--) {
    if (s_bitsleft_r == 0) {
      s_bits_r = s_powerCode->codes[s_code_ptr++];
      s_bitsleft_r = 8;
    }
    tmp = (tmp << 1) | ((s_bits_r >> --s_bitsleft_r) & 1);
  }
  return tmp;
}

static int getTotalCodesForRegion(Region reg) {
  using namespace tv_world_codes;
  switch (reg) {
    case REG_EU:
      return num_EUcodes + num_UniversalParsedCodes + num_UniversalRawCodes;
    case REG_NA:
      return num_NAcodes + num_UniversalParsedCodes + num_UniversalRawCodes;
    case REG_ALL:
      return num_EUcodes + num_NAcodes + num_UniversalParsedCodes + num_UniversalRawCodes;
    default:
      return 0;
  }
}

static void sendCodeAtIndex(Region reg, int idx) {
  using namespace tv_world_codes;
  uint16_t rawData[300];

  if (reg == REG_EU) {
    if (idx < num_EUcodes) {
      s_powerCode = EUpowerCodes[idx];
      const uint8_t freq = s_powerCode->timer_val;
      s_lastFreq = freq;
      const uint8_t numpairs = s_powerCode->numpairs;
      const uint8_t bitcompression = s_powerCode->bitcompression;
      s_code_ptr = 0;
      s_bitsleft_r = 0;
      s_bits_r = 0;
      for (uint8_t k = 0; k < numpairs; k++) {
        uint16_t ti = (read_bits(bitcompression)) * 2;
        rawData[k * 2] = s_powerCode->times[ti] * 10;
        rawData[(k * 2) + 1] = s_powerCode->times[ti + 1] * 10;
      }
      ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
      return;
    }
    idx -= num_EUcodes;
  } else if (reg == REG_NA) {
    if (idx < num_NAcodes) {
      s_powerCode = NApowerCodes[idx];
      const uint8_t freq = s_powerCode->timer_val;
      s_lastFreq = freq;
      const uint8_t numpairs = s_powerCode->numpairs;
      const uint8_t bitcompression = s_powerCode->bitcompression;
      s_code_ptr = 0;
      s_bitsleft_r = 0;
      s_bits_r = 0;
      for (uint8_t k = 0; k < numpairs; k++) {
        uint16_t ti = (read_bits(bitcompression)) * 2;
        rawData[k * 2] = s_powerCode->times[ti] * 10;
        rawData[(k * 2) + 1] = s_powerCode->times[ti + 1] * 10;
      }
      ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
      return;
    }
    idx -= num_NAcodes;
  } else {
    if (idx < num_EUcodes) {
      s_powerCode = EUpowerCodes[idx];
      const uint8_t freq = s_powerCode->timer_val;
      s_lastFreq = freq;
      const uint8_t numpairs = s_powerCode->numpairs;
      const uint8_t bitcompression = s_powerCode->bitcompression;
      s_code_ptr = 0;
      s_bitsleft_r = 0;
      s_bits_r = 0;
      for (uint8_t k = 0; k < numpairs; k++) {
        uint16_t ti = (read_bits(bitcompression)) * 2;
        rawData[k * 2] = s_powerCode->times[ti] * 10;
        rawData[(k * 2) + 1] = s_powerCode->times[ti + 1] * 10;
      }
      ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
      return;
    }
    idx -= num_EUcodes;

    if (idx < num_NAcodes) {
      s_powerCode = NApowerCodes[idx];
      const uint8_t freq = s_powerCode->timer_val;
      s_lastFreq = freq;
      const uint8_t numpairs = s_powerCode->numpairs;
      const uint8_t bitcompression = s_powerCode->bitcompression;
      s_code_ptr = 0;
      s_bitsleft_r = 0;
      s_bits_r = 0;
      for (uint8_t k = 0; k < numpairs; k++) {
        uint16_t ti = (read_bits(bitcompression)) * 2;
        rawData[k * 2] = s_powerCode->times[ti] * 10;
        rawData[(k * 2) + 1] = s_powerCode->times[ti + 1] * 10;
      }
      ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
      return;
    }
    idx -= num_NAcodes;
  }


  if (idx < num_UniversalParsedCodes) {
    s_powerCode = UniversalParsedCodes[idx];
    const uint8_t freq = s_powerCode->timer_val;
    s_lastFreq = freq;
    const uint8_t numpairs = s_powerCode->numpairs;
    const uint8_t bitcompression = s_powerCode->bitcompression;
    s_code_ptr = 0;
    s_bitsleft_r = 0;
    s_bits_r = 0;
    for (uint8_t k = 0; k < numpairs; k++) {
      uint16_t ti = (read_bits(bitcompression)) * 2;
      rawData[k * 2] = s_powerCode->times[ti] * 10;
      rawData[(k * 2) + 1] = s_powerCode->times[ti + 1] * 10;
    }
    ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
    return;
  }
  idx -= num_UniversalParsedCodes;


  if (idx < num_UniversalRawCodes) {
    const RawIrCode* rawCode = UniversalRawCodes[idx];
    const uint8_t freq = rawCode->timer_val;
    s_lastFreq = freq;
    const uint8_t numpairs = rawCode->numpairs;
    for (uint8_t k = 0; k < numpairs; k++) {
      rawData[k * 2] = (uint16_t)(rawCode->times[k * 2]);
      rawData[(k * 2) + 1] = (uint16_t)(rawCode->times[(k * 2) + 1]);
    }
    ir_sender_hw::sendRaw(rawData, numpairs * 2, freq);
  }
}

void open() {
  s_state = STATE_REGION_LIST;
  s_menuSel = 0;
  s_running = false;
  s_curIdx = 0;
  s_lastSend = 0;
  ir_sender_hw::init();
}

void close() {
  s_running = false;
  s_state = STATE_REGION_LIST;
}

bool onBack() {
  if (s_state == STATE_SWEEP_SCENE) {
    s_running = false;
    s_state = STATE_REGION_LIST;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_state != STATE_SWEEP_SCENE) return;
  if (!s_running) return;


  if (now - s_lastSend >= 35) {
    s_lastSend = now;
    int total = getTotalCodesForRegion(s_selectedRegion);
    if (s_curIdx < total) {
      sendCodeAtIndex(s_selectedRegion, s_curIdx);
      s_curIdx++;
    } else {

      s_running = false;
      s_curIdx = 0;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_REGION_LIST) {
    if (ev.key == BTN_UP) {
      s_menuSel = (s_menuSel > 0) ? s_menuSel - 1 : 2;
    } else if (ev.key == BTN_DOWN) {
      s_menuSel = (s_menuSel < 2) ? s_menuSel + 1 : 0;
    } else if (ev.key == BTN_OK) {
      s_selectedRegion = (Region)s_menuSel;
      s_state = STATE_SWEEP_SCENE;
      s_running = false;
      s_curIdx = 0;
    }
  } else if (s_state == STATE_SWEEP_SCENE) {
    if (ev.key == BTN_OK) {
      s_running = !s_running;
      if (s_running) {
        s_curIdx = 0;
        s_lastSend = millis() - 50;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_REGION_LIST) {



    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(3, 10, "SELECT REGION");

    u8g2.setDrawColor(1);

    const char* items[3] = {
      "EU / World",
      "North America",
      "All Sweep"
    };

    for (int i = 0; i < 3; i++) {
      int y = 27 + i * 13;
      if (i == s_menuSel) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(1, y - 10, 126, 12);
        u8g2.setDrawColor(0);
      } else {
        u8g2.setDrawColor(1);
      }
      u8g2.drawStr(6, y, items[i]);
    }
    u8g2.setDrawColor(1);
  } else {



    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(3, 11, "TV-B-GONE");

    u8g2.setDrawColor(1);

    int total = getTotalCodesForRegion(s_selectedRegion);


    u8g2.setFont(u8g2_font_6x12_tr);
    char buf[32];
    if (s_running) {
      snprintf(buf, sizeof(buf), "%d / %d (%dkHz)", s_curIdx + 1, total, s_lastFreq);
    } else {
      snprintf(buf, sizeof(buf), "%s", kRegionNames[s_selectedRegion]);
    }
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr((128 - w) / 2, 26, buf);


    if (total > 0) {
      u8g2.drawFrame(10, 30, 108, 5);
      int progW = (s_running ? s_curIdx * 104 : 0) / total;
      if (progW > 104) progW = 104;
      if (progW > 0) u8g2.drawBox(12, 32, progW, 2);
    }


    const int iconX = (128 - kPowerIconW) / 2;
    const int iconY = 39;

    if (s_running) {
      u8g2.drawXBMP(iconX, iconY, kPowerIconW, kPowerIconH, power_btn_active_bits);
    } else {
      u8g2.drawXBMP(iconX, iconY, kPowerIconW, kPowerIconH, power_btn_bits);
    }
  }
}

}
