#include "bird_game.h"
#include "config.h"

namespace bird_game {
namespace {

constexpr int BIRD_W = 10;
constexpr int BIRD_H = 8;
constexpr int PIPE_W = 14;
constexpr int PIPE_GAP = 26;
constexpr int NUM_PIPES = 2;

static const unsigned char birdBmp1[] PROGMEM = {
  0x0e, 0x00, 0x15, 0x00, 0x60, 0x80, 0x81, 0x80, 0xfc, 0xc0, 0xfb, 0x80, 0x71, 0x00, 0x1e, 0x00
};

static const unsigned char birdBmp2[] PROGMEM = {
  0x0e, 0x00, 0x15, 0x00, 0x70, 0x80, 0xf9, 0x80, 0xfc, 0xc0, 0x83, 0x80, 0x71, 0x00, 0x1e, 0x00
};

struct Pipe {
  float x;
  int gapY;
  bool passed;
};

static float birdY = 24.0f;
static float birdVelocity = 0.0f;
static Pipe pipes[NUM_PIPES];
static uint16_t score = 0;
static bool gameOver = false;
static bool s_running = false;
static uint32_t lastTick = 0;
static uint8_t animFrame = 0;

void resetGame() {
  birdY = 24.0f;
  birdVelocity = 0.0f;
  score = 0;
  gameOver = false;
  lastTick = millis();

  pipes[0].x = 128.0f;
  pipes[0].gapY = random(12, 64 - PIPE_GAP - 12);
  pipes[0].passed = false;

  pipes[1].x = 128.0f + 70.0f;
  pipes[1].gapY = random(12, 64 - PIPE_GAP - 12);
  pipes[1].passed = false;
}

void jump() {
  birdVelocity = -3.2f;
}

}

void open() {
  s_running = true;
  randomSeed(millis());
  resetGame();
  Serial.println("[bird] open");
}

void close() {
  s_running = false;
  Serial.println("[bird] close");
}

bool isRunning() {
  return s_running;
}

bool onBack() {
  close();
  return false;
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_BACK && !ev.repeat) {
    close();
    return;
  }

  if (gameOver) {
    if (ev.key == BTN_OK && !ev.repeat) resetGame();
    return;
  }

  if ((ev.key == BTN_OK || ev.key == BTN_UP) && !ev.repeat) {
    jump();
  }
}

void tick(uint32_t now) {
  if (!s_running || gameOver) return;

  if (now - lastTick < 30) return;
  lastTick = now;

  animFrame++;


  birdVelocity += 0.28f;
  if (birdVelocity > 4.5f) birdVelocity = 4.5f;
  birdY += birdVelocity;

  if (birdY < 0) {
    birdY = 0;
    birdVelocity = 0;
  }
  if (birdY + BIRD_H >= 64) {
    birdY = 64 - BIRD_H;
    gameOver = true;
    return;
  }


  float speed = 1.6f + (score * 0.04f);
  if (speed > 3.2f) speed = 3.2f;

  for (int i = 0; i < NUM_PIPES; i++) {
    pipes[i].x -= speed;

    if (!pipes[i].passed && pipes[i].x + PIPE_W < 18) {
      pipes[i].passed = true;
      score++;
    }

    if (pipes[i].x + PIPE_W < 0) {

      int other = (i == 0) ? 1 : 0;
      pipes[i].x = pipes[other].x + 70.0f;
      pipes[i].gapY = random(10, 64 - PIPE_GAP - 10);
      pipes[i].passed = false;
    }


    const float birdX = 18.0f;
    if (birdX + BIRD_W >= pipes[i].x && birdX <= pipes[i].x + PIPE_W) {
      if (birdY < pipes[i].gapY || birdY + BIRD_H > pipes[i].gapY + PIPE_GAP) {
        gameOver = true;
        return;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;

  u8g2.setDrawColor(1);


  for (int i = 0; i < NUM_PIPES; i++) {
    int px = (int)pipes[i].x;
    if (px < -PIPE_W || px > 128) continue;

    int gapY = pipes[i].gapY;

    u8g2.drawBox(px, 0, PIPE_W, gapY);
    u8g2.drawBox(px - 1, gapY - 4, PIPE_W + 2, 4);


    int lowerY = gapY + PIPE_GAP;
    u8g2.drawBox(px, lowerY, PIPE_W, 64 - lowerY);
    u8g2.drawBox(px - 1, lowerY, PIPE_W + 2, 4);
  }


  const uint8_t* bmp = (animFrame / 4 % 2 == 0) ? birdBmp1 : birdBmp2;
  u8g2.drawBitmap(18, (int)birdY, 2, BIRD_H, bmp);


  u8g2.setFont(u8g2_font_7x14B_tr);
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", score);
  u8g2.drawStr((128 - u8g2.getStrWidth(buf)) / 2, 12, buf);


  if (gameOver) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(14, 18, 100, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(14, 18, 100, 28);

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* goStr = "GAME OVER";
    u8g2.drawStr((128 - u8g2.getStrWidth(goStr)) / 2, 31, goStr);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okStr = "Press OK to Retry";
    u8g2.drawStr((128 - u8g2.getStrWidth(okStr)) / 2, 41, okStr);
  }
}

}
