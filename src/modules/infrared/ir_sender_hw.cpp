#include "ir_sender_hw.h"
#include "config.h"

#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <driver/gpio.h>

namespace ir_sender_hw {

static uint8_t s_txPin = PIN_IR_TX;
static bool s_inited = false;
static bool s_jamming = false;
static bool s_receiving = false;

static IRsend* s_irsend = nullptr;
static IRrecv* s_irrecv = nullptr;

static void configureTxPin(uint8_t pin) {
  if (s_irsend) {
    delete s_irsend;
    s_irsend = nullptr;
  }

  gpio_reset_pin((gpio_num_t)pin);
  s_irsend = new IRsend(pin, false);
  s_irsend->begin();

  gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_3);
}

void init() {
  if (s_inited) return;
  s_inited = true;

  configureTxPin(s_txPin);
  Serial.printf("[IR] Sender initialized on TX pin %d via IRremoteESP8266 (max-drive 40mA)\n", s_txPin);
}

uint8_t getTxPin() {
  return s_txPin;
}

void setTxPin(uint8_t pin) {
  s_txPin = pin;
  configureTxPin(pin);
  Serial.printf("[IR] Switched TX pin to %d\n", s_txPin);
}

void sendCode(const ir_db::IrCode& code) {
  if (!s_inited) init();


  bool wasRecv = s_receiving;
  if (wasRecv) stopReceiver();

  if (s_irsend) {
    switch (code.proto) {
      case ir_db::IR_PROTO_NEC:
        if (code.rawData) {
          s_irsend->sendNEC(code.rawData, 32, 2);
        } else {
          s_irsend->sendNEC(s_irsend->encodeNEC(code.address, code.command), 32, 2);
        }
        break;
      case ir_db::IR_PROTO_SAMSUNG:
        if (code.rawData) {
          s_irsend->sendSAMSUNG(code.rawData, 32, 2);
        } else {
          s_irsend->sendSAMSUNG(s_irsend->encodeSAMSUNG((uint8_t)code.address, (uint8_t)code.command), 32, 2);
        }
        break;
      case ir_db::IR_PROTO_SONY:
        s_irsend->sendSony(s_irsend->encodeSony(code.bits ? code.bits : 12, code.command, code.address), code.bits ? code.bits : 12, 3);
        break;
      case ir_db::IR_PROTO_RC5:
        s_irsend->sendRC5(code.rawData ? code.rawData : s_irsend->encodeRC5((uint8_t)code.address, (uint8_t)code.command), 12, 1);
        break;
      case ir_db::IR_PROTO_RC6:
        s_irsend->sendRC6(code.rawData ? code.rawData : s_irsend->encodeRC6(code.address, (uint8_t)code.command), 20, 1);
        break;
      case ir_db::IR_PROTO_PANASONIC:
        s_irsend->sendPanasonic64(s_irsend->encodePanasonic(code.address, 0, 0, code.command), 48, 1);
        break;
      case ir_db::IR_PROTO_LG:
        s_irsend->sendLG(code.rawData ? code.rawData : s_irsend->encodeLG(code.address, (uint16_t)code.command), 28, 2);
        break;
      case ir_db::IR_PROTO_SHARP:
        s_irsend->sendSharp(code.address, (uint16_t)code.command, code.bits ? code.bits : 15, 1);
        break;
      case ir_db::IR_PROTO_JVC:
        s_irsend->sendJVC(code.rawData ? code.rawData : s_irsend->encodeJVC((uint8_t)code.address, (uint8_t)code.command), 16, 1);
        break;
    }
  }

  delay(25);
  if (wasRecv) startReceiver();
}

void sendRaw(const uint16_t* timings, size_t length, uint8_t freqKHz) {
  if (!s_inited) init();
  bool wasRecv = s_receiving;
  if (wasRecv) stopReceiver();

  if (s_irsend && timings && length > 0) {
    uint16_t freqHz = (freqKHz > 0) ? (uint16_t)(freqKHz * 1000) : 38000;

    s_irsend->sendRaw(timings, (uint16_t)length, freqHz);
    delay(40);
    s_irsend->sendRaw(timings, (uint16_t)length, freqHz);
  }

  delay(25);
  if (wasRecv) startReceiver();
}

void startJammer() {
  s_jamming = true;
  tone(s_txPin, 38000);
}

void stopJammer() {
  s_jamming = false;
  noTone(s_txPin);
  configureTxPin(s_txPin);
}

void tickJammer() {
  if (!s_jamming) return;
  static uint32_t lastTog = 0;
  uint32_t now = micros();

  if (now - lastTog >= 350) {
    lastTog = now;
    uint32_t freq = 36000 + (now % 4000);
    tone(s_txPin, freq);
  }
}

bool isJamming() {
  return s_jamming;
}

void startReceiver() {
  if (s_receiving) return;
  s_receiving = true;
  if (!s_irrecv) {
    s_irrecv = new IRrecv(PIN_IR_RX);
  }
  s_irrecv->enableIRIn();
}

void stopReceiver() {
  if (!s_receiving) return;
  s_receiving = false;
  if (s_irrecv) {
    s_irrecv->disableIRIn();
  }
}

bool pollReceiver(IrDecodedResult& res) {
  if (!s_receiving || !s_irrecv) return false;

  decode_results results;
  if (s_irrecv->decode(&results)) {
    String pName = typeToString(results.decode_type);
    snprintf(res.protoName, sizeof(res.protoName), "%s", pName.c_str());
    res.address = results.address;
    res.command = results.command;
    res.rawData = (uint32_t)results.value;
    res.valid = (results.decode_type != decode_type_t::UNKNOWN || results.rawlen > 4);

    uint16_t* raw = resultToRawArray(&results);
    uint16_t rawCount = getCorrectedRawLength(&results);
    if (raw && rawCount > 0) {
      res.rawTimings.assign(raw, raw + rawCount);
      delete[] raw;
    } else {
      res.rawTimings.clear();
    }

    s_irrecv->resume();
    return true;
  }
  return false;
}

}
