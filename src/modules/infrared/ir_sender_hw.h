#pragma once
#include <Arduino.h>
#include <vector>
#include "ir_db.h"

namespace ir_sender_hw {

struct IrDecodedResult {
  char protoName[16];
  uint16_t address;
  uint32_t command;
  uint32_t rawData;
  bool valid;
  std::vector<uint16_t> rawTimings;
};

void init();
uint8_t getTxPin();
void setTxPin(uint8_t pin);

void sendCode(const ir_db::IrCode& code);
void sendRaw(const uint16_t* timings, size_t length, uint8_t freqKHz = 38);


void startJammer();
void stopJammer();
void tickJammer();
bool isJamming();


void startReceiver();
void stopReceiver();
bool pollReceiver(IrDecodedResult& res);

}
