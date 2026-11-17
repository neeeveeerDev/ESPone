#include "arkanoid_game.h"
#include "config.h"

namespace arkanoid_game {
namespace {

constexpr uint8_t BRICK_ROWS = 4;
constexpr uint8_t BRICK_COLS = 8;
constexpr uint8_t BRICK_W = 13;
constexpr uint8_t BRICK_H = 4;
constexpr uint8_t BRICK_GAP = 1;
constexpr uint8_t BRICK_OFFSET_X = 8;
constexpr uint8_t BRICK_OFFSET_Y = 10;

constexpr uint8_t PADDLE_W = 22;
constexpr uint8_t PADDLE_H = 3;
constexpr uint8_t PADDLE_Y = 58;

static bool bricks[BRICK_ROWS][BRICK_COLS];
static float paddleX = 53.0f;
static float ballX = 64.0f;
static float ballY = 48.0f;
static float ballVx = 1.8f;
static float ballVy = -1.8f;
static uint16_t score = 0;
static uint8_t lives = 3;
static bool ballAttached = true;
static bool gameOver = false;
static bool gameWon = false;
static bool s_running = false;
static uint32_t lastTick = 0;

void resetBricks() {
  for (uint8_t r = 0; r < BRICK_ROWS; r++) {
    for (uint8_t c = 0; c < BRICK_COLS; c++) {
      bricks[r][c] = true;
    }
  }
}

void launchBall() {
  ballAttached = false;
  ballVx = (random(2) == 0 ? 1.6f : -1.6f);
  ballVy = -2.0f;
}

void resetBall() {
  ballAttached = true;
  ballX = paddleX + PADDLE_W / 2.0f;
  ballY = PADDLE_Y - 3;
}

void resetGame() {
  paddleX = 53.0f;
  score = 0;
  lives = 3;
  gameOver = false;
  gameWon = false;
  resetBricks();
  resetBall();
  lastTick = millis();
}

}

void open() {
  s_running = true;
  randomSeed(millis());
  resetGame();
  Serial.println("[arkanoid] open");
}

void close() {
  s_running = false;
  Serial.println("[arkanoid] close");
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

  if (gameOver || gameWon) {
    if (ev.key == BTN_OK && !ev.repeat) resetGame();
    return;
  }

  if (ballAttached && ev.key == BTN_OK && !ev.repeat) {
    launchBall();
    return;
  }

  if (ev.key == BTN_LEFT) {
    paddleX -= 5.0f;
    if (paddleX < 2) paddleX = 2;
  } else if (ev.key == BTN_RIGHT) {
    paddleX += 5.0f;
    if (paddleX + PADDLE_W > 126) paddleX = 126 - PADDLE_W;
  }
}

void tick(uint32_t now) {
  if (!s_running || gameOver || gameWon) return;

  if (now - lastTick < 22) return;
  lastTick = now;


  if (digitalRead(PIN_BTN_LEFT) == LOW) {
    paddleX -= 3.5f;
    if (paddleX < 2) paddleX = 2;
  }
  if (digitalRead(PIN_BTN_RIGHT) == LOW) {
    paddleX += 3.5f;
    if (paddleX + PADDLE_W > 126) paddleX = 126 - PADDLE_W;
  }
  if (ballAttached && digitalRead(PIN_BTN_OK) == LOW) {
    launchBall();
  }

  if (ballAttached) {
    ballX = paddleX + PADDLE_W / 2.0f;
    ballY = PADDLE_Y - 3;
    return;
  }


  ballX += ballVx;
  ballY += ballVy;


  if (ballX <= 2) {
    ballX = 2;
    ballVx = -ballVx;
  } else if (ballX >= 125) {
    ballX = 125;
    ballVx = -ballVx;
  }

  if (ballY <= 1) {
    ballY = 1;
    ballVy = -ballVy;
  }


  if (ballY >= PADDLE_Y - 2 && ballY <= PADDLE_Y + PADDLE_H && ballVy > 0) {
    if (ballX >= paddleX - 1 && ballX <= paddleX + PADDLE_W + 1) {
      ballY = PADDLE_Y - 2;
      ballVy = -abs(ballVy);

      float impact = (ballX - (paddleX + PADDLE_W / 2.0f)) / (PADDLE_W / 2.0f);
      ballVx = impact * 2.5f;
      if (abs(ballVx) < 0.6f) ballVx = (ballVx >= 0) ? 0.8f : -0.8f;
    }
  }


  bool anyBricks = false;
  for (uint8_t r = 0; r < BRICK_ROWS; r++) {
    for (uint8_t c = 0; c < BRICK_COLS; c++) {
      if (!bricks[r][c]) continue;
      anyBricks = true;

      int bx = BRICK_OFFSET_X + c * (BRICK_W + BRICK_GAP);
      int by = BRICK_OFFSET_Y + r * (BRICK_H + BRICK_GAP);

      if (ballX >= bx - 1 && ballX <= bx + BRICK_W + 1 &&
          ballY >= by - 1 && ballY <= by + BRICK_H + 1) {
        bricks[r][c] = false;
        score += (BRICK_ROWS - r) * 10;
        ballVy = -ballVy;
        goto brickChecked;
      }
    }
  }
brickChecked:

  if (!anyBricks) {
    gameWon = true;
    return;
  }


  if (ballY > 64) {
    if (lives > 1) {
      lives--;
      resetBall();
    } else {
      lives = 0;
      gameOver = true;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;

  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_5x7_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "SCORE: %u", score);
  u8g2.drawStr(2, 7, buf);

  snprintf(buf, sizeof(buf), "LIVES: %u", lives);
  u8g2.drawStr(88, 7, buf);

  u8g2.drawHLine(0, 8, 128);


  for (uint8_t r = 0; r < BRICK_ROWS; r++) {
    for (uint8_t c = 0; c < BRICK_COLS; c++) {
      if (bricks[r][c]) {
        int bx = BRICK_OFFSET_X + c * (BRICK_W + BRICK_GAP);
        int by = BRICK_OFFSET_Y + r * (BRICK_H + BRICK_GAP);
        u8g2.drawBox(bx, by, BRICK_W, BRICK_H);
      }
    }
  }


  u8g2.drawBox((int)paddleX, PADDLE_Y, PADDLE_W, PADDLE_H);


  u8g2.drawBox((int)ballX, (int)ballY, 2, 2);


  if (ballAttached && !gameOver && !gameWon) {
    u8g2.setFont(u8g2_font_5x7_tr);
    const char* pMsg = "Press OK to Launch";
    u8g2.drawStr((128 - u8g2.getStrWidth(pMsg)) / 2, 44, pMsg);
  }


  if (gameOver || gameWon) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(14, 18, 100, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(14, 18, 100, 28);

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* msg = gameWon ? "YOU WIN!" : "GAME OVER";
    u8g2.drawStr((128 - u8g2.getStrWidth(msg)) / 2, 31, msg);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okStr = "Press OK to Retry";
    u8g2.drawStr((128 - u8g2.getStrWidth(okStr)) / 2, 41, okStr);
  }
}

}
