#include "snake_game.h"
#include "config.h"

namespace snake_game {
namespace {

constexpr uint8_t SNAKE_TILE_SIZE = 4;
constexpr uint8_t SNAKE_GRID_WIDTH = 128 / SNAKE_TILE_SIZE;
constexpr uint8_t SNAKE_GRID_HEIGHT = 64 / SNAKE_TILE_SIZE;
constexpr uint16_t SNAKE_MAX_LENGTH = 128;

static uint8_t snakeAppleX = 0;
static uint8_t snakeAppleY = 0;
static int8_t snakeDirX = 1;
static int8_t snakeDirY = 0;
static uint8_t snakeHeadX = 16;
static uint8_t snakeHeadY = 8;
static uint16_t snakeLength = 3;
static uint16_t snakeScore = 0;
static uint8_t snakePosX[SNAKE_MAX_LENGTH];
static uint8_t snakePosY[SNAKE_MAX_LENGTH];
static uint32_t snakeFrameTime = 0;
static float snakeSpeed = 7.0f;
static bool snakeGameOver = false;
static bool s_running = false;

void spawnApple() {
  bool onSnake;
  uint8_t tries = 0;
  do {
    onSnake = false;
    snakeAppleX = random(2, SNAKE_GRID_WIDTH - 2);
    snakeAppleY = random(2, SNAKE_GRID_HEIGHT - 2);
    for (uint16_t i = 0; i < snakeLength; i++) {
      if (snakePosX[i] == snakeAppleX && snakePosY[i] == snakeAppleY) {
        onSnake = true;
        break;
      }
    }
    tries++;
  } while (onSnake && tries < 50);
}

void resetGame() {
  snakeHeadX = 16;
  snakeHeadY = 8;
  snakeLength = 3;
  snakeScore = 0;
  snakeDirX = 1;
  snakeDirY = 0;
  snakeSpeed = 7.0f;
  snakeGameOver = false;
  snakeFrameTime = millis();

  for (uint16_t i = 0; i < snakeLength; i++) {
    snakePosX[i] = snakeHeadX - i;
    snakePosY[i] = snakeHeadY;
  }
  spawnApple();
}

}

void open() {
  s_running = true;
  randomSeed(millis());
  resetGame();
  Serial.println("[snake] open");
}

void close() {
  s_running = false;
  Serial.println("[snake] close");
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

  if (snakeGameOver) {
    if (ev.key == BTN_OK && !ev.repeat) {
      resetGame();
    }
    return;
  }

  if (ev.key == BTN_UP && snakeDirY != 1) {
    snakeDirX = 0; snakeDirY = -1;
  } else if (ev.key == BTN_DOWN && snakeDirY != -1) {
    snakeDirX = 0; snakeDirY = 1;
  } else if (ev.key == BTN_LEFT && snakeDirX != 1) {
    snakeDirX = -1; snakeDirY = 0;
  } else if (ev.key == BTN_RIGHT && snakeDirX != -1) {
    snakeDirX = 1; snakeDirY = 0;
  }
}

void tick(uint32_t now) {
  if (!s_running) return;


  if (!snakeGameOver) {
    if (digitalRead(PIN_BTN_UP) == LOW && snakeDirY != 1) {
      snakeDirX = 0; snakeDirY = -1;
    } else if (digitalRead(PIN_BTN_DOWN) == LOW && snakeDirY != -1) {
      snakeDirX = 0; snakeDirY = 1;
    } else if (digitalRead(PIN_BTN_LEFT) == LOW && snakeDirX != 1) {
      snakeDirX = -1; snakeDirY = 0;
    } else if (digitalRead(PIN_BTN_RIGHT) == LOW && snakeDirX != -1) {
      snakeDirX = 1; snakeDirY = 0;
    }
  } else {
    if (digitalRead(PIN_BTN_OK) == LOW) {
      resetGame();
      return;
    }
  }

  if (snakeGameOver) return;

  uint32_t interval = (uint32_t)(1000.0f / snakeSpeed);
  if (now - snakeFrameTime < interval) return;
  snakeFrameTime = now;

  int nextX = (int)snakeHeadX + snakeDirX;
  int nextY = (int)snakeHeadY + snakeDirY;


  if (nextX < 1 || nextX >= SNAKE_GRID_WIDTH - 1 || nextY < 1 || nextY >= SNAKE_GRID_HEIGHT - 1) {
    snakeGameOver = true;
    return;
  }


  bool ateApple = (nextX == snakeAppleX && nextY == snakeAppleY);
  if (ateApple && snakeLength < SNAKE_MAX_LENGTH - 1) {
    snakeLength++;
    snakeScore++;
    snakeSpeed += 0.35f;
    spawnApple();
  }


  for (uint16_t i = snakeLength - 1; i > 0; i--) {
    snakePosX[i] = snakePosX[i - 1];
    snakePosY[i] = snakePosY[i - 1];
  }
  snakeHeadX = (uint8_t)nextX;
  snakeHeadY = (uint8_t)nextY;
  snakePosX[0] = snakeHeadX;
  snakePosY[0] = snakeHeadY;


  for (uint16_t i = 1; i < snakeLength; i++) {
    if (snakeHeadX == snakePosX[i] && snakeHeadY == snakePosY[i]) {
      snakeGameOver = true;
      return;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;

  u8g2.setDrawColor(1);

  u8g2.drawFrame(0, 0, 128, 64);


  u8g2.drawBox(snakeAppleX * SNAKE_TILE_SIZE, snakeAppleY * SNAKE_TILE_SIZE,
               SNAKE_TILE_SIZE, SNAKE_TILE_SIZE);


  for (uint16_t i = 0; i < snakeLength; i++) {
    if (i == 0) {
      u8g2.drawBox(snakePosX[i] * SNAKE_TILE_SIZE, snakePosY[i] * SNAKE_TILE_SIZE,
                   SNAKE_TILE_SIZE, SNAKE_TILE_SIZE);
    } else {
      u8g2.drawFrame(snakePosX[i] * SNAKE_TILE_SIZE, snakePosY[i] * SNAKE_TILE_SIZE,
                    SNAKE_TILE_SIZE, SNAKE_TILE_SIZE);
    }
  }


  u8g2.setFont(u8g2_font_5x7_tr);
  char scBuf[16];
  snprintf(scBuf, sizeof(scBuf), "%u", snakeScore);
  u8g2.drawStr(4, 9, scBuf);


  if (snakeGameOver) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(20, 18, 88, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(20, 18, 88, 28);

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* goStr = "GAME OVER";
    u8g2.drawStr((128 - u8g2.getStrWidth(goStr)) / 2, 31, goStr);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okStr = "Press OK to Retry";
    u8g2.drawStr((128 - u8g2.getStrWidth(okStr)) / 2, 41, okStr);
  }
}

}
