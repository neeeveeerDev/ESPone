#include "subghz_protocols.h"

namespace subghz_protocols {

bool encode(const String& protocol, uint64_t key, uint8_t bits, uint32_t te, std::vector<int32_t>& outTimings) {
  outTimings.clear();

  if (protocol.equalsIgnoreCase("Came")) {
    if (te == 0) te = 320;
    if (bits == 0) bits = 12;


    outTimings.push_back((int32_t)te);
    outTimings.push_back(-(int32_t)(te * 36));


    for (int i = bits - 1; i >= 0; i--) {
      bool bit = (key >> i) & 1;
      if (bit) {

        outTimings.push_back((int32_t)(te * 2));
        outTimings.push_back(-(int32_t)te);
      } else {

        outTimings.push_back((int32_t)te);
        outTimings.push_back(-(int32_t)(te * 2));
      }
    }

    outTimings.push_back((int32_t)te);
    outTimings.push_back(-(int32_t)(te * 36));
    return true;
  }

  if (protocol.equalsIgnoreCase("Nice FLO")) {
    if (te == 0) te = 700;
    if (bits == 0) bits = 12;


    outTimings.push_back((int32_t)te);
    outTimings.push_back(-(int32_t)(te * 36));

    for (int i = bits - 1; i >= 0; i--) {
      bool bit = (key >> i) & 1;
      if (bit) {

        outTimings.push_back((int32_t)te);
        outTimings.push_back(-(int32_t)(te * 2));
      } else {

        outTimings.push_back((int32_t)(te * 2));
        outTimings.push_back(-(int32_t)te);
      }
    }
    outTimings.push_back((int32_t)te);
    outTimings.push_back(-(int32_t)(te * 36));
    return true;
  }

  if (protocol.equalsIgnoreCase("Princeton")) {
    if (te == 0) te = 400;
    if (bits == 0) bits = 24;

    for (int i = bits - 1; i >= 0; i--) {
      bool bit = (key >> i) & 1;
      if (bit) {

        outTimings.push_back((int32_t)(te * 3));
        outTimings.push_back(-(int32_t)te);
      } else {

        outTimings.push_back((int32_t)te);
        outTimings.push_back(-(int32_t)(te * 3));
      }
    }

    outTimings.push_back((int32_t)te);
    outTimings.push_back(-(int32_t)(te * 31));
    return true;
  }

  if (protocol.equalsIgnoreCase("Linear")) {
    if (te == 0) te = 500;
    if (bits == 0) bits = 10;

    for (int i = bits - 1; i >= 0; i--) {
      bool bit = (key >> i) & 1;
      if (bit) {
        outTimings.push_back((int32_t)(te * 3));
        outTimings.push_back(-(int32_t)te);
      } else {
        outTimings.push_back((int32_t)te);
        outTimings.push_back(-(int32_t)(te * 3));
      }
    }
    outTimings.push_back(-(int32_t)(te * 40));
    return true;
  }

  return false;
}

bool decode(const int32_t* timings, size_t count, String& outProtocol, uint64_t& outKey, uint8_t& outBits, uint32_t& outTe) {
  if (count < 20 || !timings) return false;


  for (size_t s = 0; s < count - 25; s++) {
    if (timings[s] > 200 && timings[s] < 500 && timings[s+1] < -8000 && timings[s+1] > -16000) {

      uint32_t te = timings[s];
      uint64_t key = 0;
      bool ok = true;
      size_t pos = s + 2;

      for (int b = 0; b < 12; b++) {
        if (pos + 1 >= count) { ok = false; break; }
        int32_t mark = timings[pos++];
        int32_t space = -timings[pos++];

        if (mark > te * 1.5f && space < te * 1.5f) {
          key = (key << 1) | 1;
        } else if (mark < te * 1.5f && space > te * 1.5f) {
          key = (key << 1) | 0;
        } else {
          ok = false;
          break;
        }
      }

      if (ok) {
        outProtocol = "Came";
        outKey = key;
        outBits = 12;
        outTe = te;
        return true;
      }
    }
  }


  for (size_t s = 0; s < count - 48; s++) {

    if (timings[s] < -8000 && timings[s] > -18000) {
      size_t pos = s + 1;
      uint32_t te = 400;
      uint64_t key = 0;
      bool ok = true;

      for (int b = 0; b < 24; b++) {
        if (pos + 1 >= count) { ok = false; break; }
        int32_t mark = timings[pos++];
        int32_t space = -timings[pos++];

        if (mark > 700 && space < 600) {
          key = (key << 1) | 1;
        } else if (mark < 600 && space > 700) {
          key = (key << 1) | 0;
        } else {
          ok = false;
          break;
        }
      }

      if (ok) {
        outProtocol = "Princeton";
        outKey = key;
        outBits = 24;
        outTe = te;
        return true;
      }
    }
  }

  return false;
}

}
