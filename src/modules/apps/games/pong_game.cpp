#include "pong_game.h"
#include "config.h"

namespace pong_game {
namespace {

constexpr int PLAYER_PADDLE_H = 18;
constexpr int CPU_PADDLE_H = 12;
constexpr int PADDLE_W = 3;
constexpr int BALL_SIZE = 3;
constexpr float BALL_SPEED_X = 2.4f;
constexpr float BALL_SPEED_Y = 1.6f;
constexpr uint8_t MAX_SCORE = 5;

static float playerY = 24.0f;
static float cpuY = 24.0f;
static float ballX = 64.0f;
static float ballY = 32.0f;
static float vx = BALL_SPEED_X;
static float vy = BALL_SPEED_Y;
static uint8_t scorePlayer = 0;
static uint8_t scoreCpu = 0;
static bool gameOver = false;
static bool s_running = false;
static uint32_t lastTick = 0;
static float cpuOffset = 0.0f;

void resetBall(bool toPlayer) {
  ballX = 64.0f;
  ballY = 32.0f;
  vx = toPlayer ? -BALL_SPEED_X : BALL_SPEED_X;
  vy = (random(2) == 0 ? 1 : -1) * (random(10, 18) / 10.0f);
  cpuOffset = (float)random(-4, 5);
}

void resetGame() {
  playerY = 24.0f;
  cpuY = 24.0f;
  scorePlayer = 0;
  scoreCpu = 0;
  gameOver = false;
  resetBall(random(2) == 0);
  lastTick = millis();
}

}

void open() {
  s_running = true;
  randomSeed(millis());
  resetGame();
  Serial.println("[pong] open");
}

void close() {
  s_running = false;
  Serial.println("[pong] close");
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

  if (ev.key == BTN_UP) {
    playerY -= 4.0f;
    if (playerY < 1) playerY = 1;
  } else if (ev.key == BTN_DOWN) {
    playerY += 4.0f;
    if (playerY + PLAYER_PADDLE_H > 63) playerY = 63 - PLAYER_PADDLE_H;
  }
}

void tick(uint32_t now) {
  if (!s_running || gameOver) return;

  if (now - lastTick < 25) return;
  lastTick = now;


  if (digitalRead(PIN_BTN_UP) == LOW) {
    playerY -= 3.5f;
    if (playerY < 1) playerY = 1;
  }
  if (digitalRead(PIN_BTN_DOWN) == LOW) {
    playerY += 3.5f;
    if (playerY + PLAYER_PADDLE_H > 63) playerY = 63 - PLAYER_PADDLE_H;
  }


  ballX += vx;
  ballY += vy;


  if (ballY <= 1) {
    ballY = 1;
    vy = -vy;
  } else if (ballY + BALL_SIZE >= 63) {
    ballY = 63 - BALL_SIZE;
    vy = -vy;
  }



  float cpuCenter = cpuY + CPU_PADDLE_H / 2.0f + cpuOffset;
  float ballCenter = ballY + BALL_SIZE / 2.0f;
  if (ballX > 75.0f && vx > 0) {
    float cpuSpeed = 1.15f;
    if (cpuCenter < ballCenter - 1.5f) cpuY += cpuSpeed;
    else if (cpuCenter > ballCenter + 1.5f) cpuY -= cpuSpeed;
  }
  if (cpuY < 1) cpuY = 1;
  if (cpuY + CPU_PADDLE_H > 63) cpuY = 63 - CPU_PADDLE_H;


  if (ballX <= 2 + PADDLE_W && ballX >= 2) {
    if (ballY + BALL_SIZE >= playerY && ballY <= playerY + PLAYER_PADDLE_H) {
      ballX = 2 + PADDLE_W + 1;
      vx = -vx * 1.05f;
      if (vx > 4.5f) vx = 4.5f;
      float impact = (ballCenter - (playerY + PLAYER_PADDLE_H / 2.0f)) / (PLAYER_PADDLE_H / 2.0f);
      vy = impact * 3.0f;

      cpuOffset = (float)random(-8, 9);
    }
  }


  float cpuPaddleX = 126 - PADDLE_W;
  if (ballX + BALL_SIZE >= cpuPaddleX && ballX <= cpuPaddleX + PADDLE_W) {
    if (ballY + BALL_SIZE >= cpuY && ballY <= cpuY + CPU_PADDLE_H) {
      ballX = cpuPaddleX - BALL_SIZE - 1;
      vx = -vx * 1.03f;
      if (vx < -4.5f) vx = -4.5f;
      float impact = (ballCenter - (cpuY + CPU_PADDLE_H / 2.0f)) / (CPU_PADDLE_H / 2.0f);
      vy = impact * 2.2f;
      cpuOffset = (float)random(-5, 6);
    }
  }


  if (ballX > 128) {
    scorePlayer++;
    if (scorePlayer >= MAX_SCORE) gameOver = true;
    else resetBall(false);
  }

  else if (ballX < 0) {
    scoreCpu++;
    if (scoreCpu >= MAX_SCORE) gameOver = true;
    else resetBall(true);
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;

  u8g2.setDrawColor(1);


  for (int y = 0; y < 64; y += 6) {
    u8g2.drawVLine(64, y, 3);
  }


  u8g2.drawBox(2, (int)playerY, PADDLE_W, PLAYER_PADDLE_H);


  u8g2.drawBox(126 - PADDLE_W, (int)cpuY, PADDLE_W, CPU_PADDLE_H);


  u8g2.drawBox((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE);


  u8g2.setFont(u8g2_font_7x14B_tr);
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", scorePlayer);
  u8g2.drawStr(48, 14, buf);

  snprintf(buf, sizeof(buf), "%u", scoreCpu);
  u8g2.drawStr(74, 14, buf);


  if (gameOver) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(14, 18, 100, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(14, 18, 100, 28);

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* winStr = (scorePlayer >= MAX_SCORE) ? "YOU WIN!" : "CPU WINS!";
    u8g2.drawStr((128 - u8g2.getStrWidth(winStr)) / 2, 31, winStr);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okStr = "Press OK to Retry";
    u8g2.drawStr((128 - u8g2.getStrWidth(okStr)) / 2, 41, okStr);
  }
}

}
