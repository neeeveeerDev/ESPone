#include "screenshot.h"
#include "vfs_mgr.h"
#include "display.h"

namespace screenshot {

#pragma pack(push, 1)
struct BmpHeader {
  // BITMAPFILEHEADER (14 bytes)
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;

  // BITMAPINFOHEADER (40 bytes)
  uint32_t biSize;
  int32_t  biWidth;
  int32_t  biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t  biXPelsPerMeter;
  int32_t  biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;

  // RGBQUAD Palette (8 bytes for 2 colors: 0=black, 1=white)
  uint8_t  palette[8];
};
#pragma pack(pop)

static uint32_t s_flashUntil = 0;
static uint32_t s_nextIndex = 1;

void init() {
  vfs::mkdir("/screenshots");
}

bool isFlashing() {
  return (s_flashUntil != 0 && millis() < s_flashUntil);
}

void clearFlash() {
  s_flashUntil = 0;
}

bool capture(String& outPath) {
  vfs::mkdir("/screenshots");

  // Find next available filename
  char filename[48];
  for (uint32_t i = s_nextIndex; i < 9999; i++) {
    snprintf(filename, sizeof(filename), "/screenshots/snap_%04u.bmp", (unsigned)i);
    if (!vfs::exists(filename)) {
      s_nextIndex = i + 1;
      break;
    }
  }

  File f = vfs::open(filename, FILE_WRITE);
  if (!f) {
    Serial.printf("[screenshot] Failed to open %s for writing\n", filename);
    return false;
  }

  // Populate BMP 1-bit Header
  BmpHeader hdr;
  memset(&hdr, 0, sizeof(hdr));

  hdr.bfType = 0x4D42; // "BM"
  hdr.bfSize = sizeof(BmpHeader) + 1024;
  hdr.bfOffBits = sizeof(BmpHeader);

  hdr.biSize = 40;
  hdr.biWidth = 128;
  hdr.biHeight = 64; // Standard bottom-up BMP
  hdr.biPlanes = 1;
  hdr.biBitCount = 1; // 1-bit monochrome
  hdr.biCompression = 0; // BI_RGB
  hdr.biSizeImage = 1024;
  hdr.biXPelsPerMeter = 2835; // 72 DPI
  hdr.biYPelsPerMeter = 2835;
  hdr.biClrUsed = 2;
  hdr.biClrImportant = 2;

  // Palette 0: Black (0, 0, 0, 0)
  hdr.palette[0] = 0x00;
  hdr.palette[1] = 0x00;
  hdr.palette[2] = 0x00;
  hdr.palette[3] = 0x00;

  // Palette 1: White (255, 255, 255, 0)
  hdr.palette[4] = 0xFF;
  hdr.palette[5] = 0xFF;
  hdr.palette[6] = 0xFF;
  hdr.palette[7] = 0x00;

  f.write((const uint8_t*)&hdr, sizeof(hdr));

  // Convert U8G2 vertical pages into horizontal row-major BMP scanlines
  // U8G2 buffer: 8 pages (0..7), 128 bytes each.
  // BMP row 0 is bottom (y=63), BMP row 63 is top (y=0).
  const uint8_t* u8g2Buf = display::get().getBufferPtr();
  uint8_t rowBuf[16];

  for (int r = 0; r < 64; r++) {
    int y = 63 - r; // Invert for bottom-up BMP
    int page = y / 8;
    int bit = y % 8;

    for (int bx = 0; bx < 16; bx++) {
      uint8_t bVal = 0;
      for (int b = 0; b < 8; b++) {
        int x = bx * 8 + b;
        uint8_t u8g2Byte = u8g2Buf[page * 128 + x];
        if (u8g2Byte & (1 << bit)) {
          bVal |= (1 << (7 - b)); // MSB-first pixel order in BMP
        }
      }
      rowBuf[bx] = bVal;
    }
    f.write(rowBuf, 16);
  }

  f.close();
  outPath = String(filename);
  s_flashUntil = millis() + 120; // Trigger a brief 120ms flash effect
  Serial.printf("[screenshot] Saved %s (%u bytes)\n", filename, (unsigned)sizeof(BmpHeader) + 1024);
  return true;
}

} // namespace screenshot
