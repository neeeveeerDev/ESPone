#include "scene_copy.h"
#include <FFat.h>
#include "flipper_ir.h"
#include "ir_sender_hw.h"

namespace scene_copy {

static bool s_hasReceived = false;
static flipper_ir::Signal s_lastSignal;
static uint32_t s_savedFlashUntil = 0;
static int s_savedCount = 0;

void open() {
  s_hasReceived = false;
  s_savedFlashUntil = 0;
  s_savedCount = 0;
  ir_sender_hw::startReceiver();
}

void close() {
  ir_sender_hw::stopReceiver();
}

void tick(uint32_t now) {
  ir_sender_hw::IrDecodedResult res;
  if (ir_sender_hw::pollReceiver(res) && res.valid) {
    s_hasReceived = true;
    s_savedCount++;

    s_lastSignal.name = String("Btn_") + s_savedCount;
    s_lastSignal.protocol = res.protoName;
    s_lastSignal.address = res.address;
    s_lastSignal.command = res.command;
    s_lastSignal.rawTimings = res.rawTimings;
    s_lastSignal.isRaw = (strcmp(res.protoName, "UNKNOWN") == 0 || (res.address == 0 && res.command == 0));


    flipper_ir::saveSignalToFile(FFat, "/ir/captured.ir", s_lastSignal, true);
    s_savedFlashUntil = now + 2000;
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_OK && !ev.repeat) {

    if (s_hasReceived) {
      flipper_ir::transmitSignal(s_lastSignal);
      ir_sender_hw::startReceiver();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "COPY");
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x12_tr);

  if (!s_hasReceived) {
    int dots = (now / 350) % 4;
    char waitBuf[24];
    snprintf(waitBuf, sizeof(waitBuf), "Waiting signal%.*s", dots, "...");
    int w = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - w) / 2, 42, waitBuf);
  } else {
    char buf[36];
    snprintf(buf, sizeof(buf), "Proto: %s", s_lastSignal.protocol.c_str());
    u8g2.drawStr(4, 28, buf);

    snprintf(buf, sizeof(buf), "Cmd: 0x%04X Addr: 0x%02X",
             (unsigned int)s_lastSignal.command,
             (unsigned int)(s_lastSignal.address & 0xFF));
    u8g2.drawStr(4, 40, buf);

    u8g2.setFont(u8g2_font_5x8_tr);
    if (now < s_savedFlashUntil) {
      snprintf(buf, sizeof(buf), "Saved as %s into /ir!", s_lastSignal.name.c_str());
    } else {
      snprintf(buf, sizeof(buf), "Saved to /ir/captured.ir");
    }
    u8g2.drawStr(4, 56, buf);
  }
}

}
