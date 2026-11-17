#include "tetris_game.h"
#include "config.h"

namespace tetris_game {
namespace {

constexpr uint8_t BOARD_W = 10;
constexpr uint8_t BOARD_H = 18;
constexpr uint8_t CELL_SIZE = 3;
constexpr uint8_t BOARD_X = 14;
constexpr uint8_t BOARD_Y = 5;

static bool grid[BOARD_W][BOARD_H];
static int8_t curX = 4, curY = 0;
static uint8_t curType = 0, curRot = 0;
static uint8_t nextType = 0;
static uint16_t score = 0;
static uint16_t linesCleared = 0;
static uint32_t lastDrop = 0;
static uint16_t dropInterval = 450;
static bool gameOver = false;
static bool s_running = false;


static const int8_t TETROMINOES[7][4][2] = {
  { {0, 1}, {1, 1}, {2, 1}, {3, 1} },
  { {0, 0}, {0, 1}, {1, 1}, {2, 1} },
  { {2, 0}, {0, 1}, {1, 1}, {2, 1} },
  { {1, 0}, {2, 0}, {1, 1}, {2, 1} },
  { {1, 0}, {2, 0}, {0, 1}, {1, 1} },
  { {1, 0}, {0, 1}, {1, 1}, {2, 1} },
  { {0, 0}, {1, 0}, {1, 1}, {2, 1} }
};

void getRotatedBlock(uint8_t type, uint8_t rot, uint8_t idx, int8_t& ox, int8_t& oy) {
  int8_t x = TETROMINOES[type][idx][0];
  int8_t y = TETROMINOES[type][idx][1];

  if (type == 3) {
    ox = x; oy = y;
    return;
  }


  float cx = (type == 0) ? 1.5f : 1.0f;
  float cy = (type == 0) ? 0.5f : 1.0f;

  float px = x - cx;
  float py = y - cy;

  for (uint8_t r = 0; r < rot; r++) {
    float temp = px;
    px = -py;
    py = temp;
  }

  ox = (int8_t)(roundf(px + cx));
  oy = (int8_t)(roundf(py + cy));
}

bool collides(int8_t px, int8_t py, uint8_t rot) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getRotatedBlock(curType, rot, i, bx, by);
    int8_t x = px + bx;
    int8_t y = py + by;
    if (x < 0 || x >= BOARD_W || y >= BOARD_H) return true;
    if (y >= 0 && grid[x][y]) return true;
  }
  return false;
}

void spawnPiece() {
  curType = nextType;
  nextType = random(7);
  curRot = 0;
  curX = (curType == 0) ? 3 : 4;
  curY = 0;

  if (collides(curX, curY, curRot)) {
    gameOver = true;
  }
}

void lockPiece() {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getRotatedBlock(curType, curRot, i, bx, by);
    int8_t x = curX + bx;
    int8_t y = curY + by;
    if (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H) {
      grid[x][y] = true;
    }
  }


  uint8_t clearedNow = 0;
  for (int8_t y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (uint8_t x = 0; x < BOARD_W; x++) {
      if (!grid[x][y]) { full = false; break; }
    }
    if (full) {
      clearedNow++;
      for (int8_t ny = y; ny > 0; ny--) {
        for (uint8_t x = 0; x < BOARD_W; x++) {
          grid[x][ny] = grid[x][ny - 1];
        }
      }
      for (uint8_t x = 0; x < BOARD_W; x++) grid[x][0] = false;
      y++;
    }
  }

  if (clearedNow > 0) {
    linesCleared += clearedNow;
    score += (clearedNow == 1) ? 100 : (clearedNow == 2) ? 300 : (clearedNow == 3) ? 700 : 1500;
    if (dropInterval > 120) dropInterval -= (clearedNow * 8);
  }

  spawnPiece();
}

void resetGame() {
  for (uint8_t x = 0; x < BOARD_W; x++) {
    for (uint8_t y = 0; y < BOARD_H; y++) grid[x][y] = false;
  }
  score = 0;
  linesCleared = 0;
  dropInterval = 450;
  gameOver = false;
  lastDrop = millis();
  nextType = random(7);
  spawnPiece();
}

}

void open() {
  s_running = true;
  randomSeed(millis());
  resetGame();
  Serial.println("[tetris] open");
}

void close() {
  s_running = false;
  Serial.println("[tetris] close");
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

  if (ev.key == BTN_LEFT) {
    if (!collides(curX - 1, curY, curRot)) curX--;
  } else if (ev.key == BTN_RIGHT) {
    if (!collides(curX + 1, curY, curRot)) curX++;
  } else if (ev.key == BTN_UP && !ev.repeat) {
    uint8_t nRot = (curRot + 1) % 4;
    if (!collides(curX, curY, nRot)) curRot = nRot;
    else if (!collides(curX - 1, curY, nRot)) { curX--; curRot = nRot; }
    else if (!collides(curX + 1, curY, nRot)) { curX++; curRot = nRot; }
  } else if (ev.key == BTN_DOWN) {
    if (!collides(curX, curY + 1, curRot)) {
      curY++;
      score += 1;
    } else {
      lockPiece();
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {

    while (!collides(curX, curY + 1, curRot)) {
      curY++;
      score += 2;
    }
    lockPiece();
  }
}

void tick(uint32_t now) {
  if (!s_running || gameOver) return;

  uint16_t interval = dropInterval;
  if (digitalRead(PIN_BTN_DOWN) == LOW) {
    interval = 50;
  }

  if (now - lastDrop >= interval) {
    lastDrop = now;
    if (!collides(curX, curY + 1, curRot)) {
      curY++;
    } else {
      lockPiece();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;

  u8g2.setDrawColor(1);


  u8g2.drawFrame(BOARD_X - 2, BOARD_Y - 2, BOARD_W * CELL_SIZE + 4, BOARD_H * CELL_SIZE + 4);


  for (uint8_t x = 0; x < BOARD_W; x++) {
    for (uint8_t y = 0; y < BOARD_H; y++) {
      if (grid[x][y]) {
        u8g2.drawBox(BOARD_X + x * CELL_SIZE, BOARD_Y + y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
      }
    }
  }


  if (!gameOver) {
    for (uint8_t i = 0; i < 4; i++) {
      int8_t bx, by;
      getRotatedBlock(curType, curRot, i, bx, by);
      int8_t px = curX + bx;
      int8_t py = curY + by;
      if (px >= 0 && px < BOARD_W && py >= 0 && py < BOARD_H) {
        u8g2.drawBox(BOARD_X + px * CELL_SIZE, BOARD_Y + py * CELL_SIZE, CELL_SIZE, CELL_SIZE);
      }
    }
  }


  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(56, 14, "TETRIS");

  u8g2.setFont(u8g2_font_5x7_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "SCORE: %u", score);
  u8g2.drawStr(56, 26, buf);

  snprintf(buf, sizeof(buf), "LINES: %u", linesCleared);
  u8g2.drawStr(56, 36, buf);

  u8g2.drawStr(56, 48, "NEXT:");

  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getRotatedBlock(nextType, 0, i, bx, by);
    u8g2.drawBox(88 + bx * 3, 44 + by * 3, 3, 3);
  }


  if (gameOver) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 18, 108, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 18, 108, 28);

    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* goStr = "GAME OVER";
    u8g2.drawStr((128 - u8g2.getStrWidth(goStr)) / 2, 31, goStr);

    u8g2.setFont(u8g2_font_5x7_tr);
    const char* okStr = "Press OK to Retry";
    u8g2.drawStr((128 - u8g2.getStrWidth(okStr)) / 2, 41, okStr);
  }
}

}
