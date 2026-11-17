#pragma once
#include <Arduino.h>
#include <vector>

namespace subghz_protocols {


bool encode(const String& protocol, uint64_t key, uint8_t bits, uint32_t te, std::vector<int32_t>& outTimings);


bool decode(const int32_t* timings, size_t count, String& outProtocol, uint64_t& outKey, uint8_t& outBits, uint32_t& outTe);

}
