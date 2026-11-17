#include "doom_game.h"
#include "config.h"
#include "core/display.h"
#include <math.h>

#define LEVEL_WIDTH_BASE 6
#define LEVEL_WIDTH (1 << LEVEL_WIDTH_BASE)
#define LEVEL_HEIGHT 57
#define LEVEL_SIZE (LEVEL_WIDTH / 2 * LEVEL_HEIGHT)
#define FRAME_TIME 66.666666
#define RES_DIVIDER 2
#define Z_RES_DIVIDER 2
#define DISTANCE_MULTIPLIER 20
#define MAX_RENDER_DEPTH 12
#define MAX_SPRITE_DEPTH 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define ZBUFFER_SIZE (SCREEN_WIDTH / Z_RES_DIVIDER)
#define RENDER_HEIGHT 56
#define HALF_WIDTH (SCREEN_WIDTH / 2)
#define GUN_TARGET_POS 18
#define GUN_SHOT_POS (GUN_TARGET_POS + 4)
#define ROT_SPEED .12
#define MOV_SPEED .2
#define MOV_SPEED_INV 5
#define JOGGING_SPEED .005
#define ENEMY_SPEED .02
#define FIREBALL_SPEED .2
#define FIREBALL_ANGLES 45
#define MAX_ENTITIES 10
#define MAX_STATIC_ENTITIES 28
#define MAX_ENTITY_DISTANCE 200
#define MAX_ENEMY_VIEW 80
#define ITEM_COLLIDER_DIST 6
#define ENEMY_COLLIDER_DIST 4
#define FIREBALL_COLLIDER_DIST 2
#define ENEMY_MELEE_DIST 6
#define FIRE_COOLDOWN_MS 500
#define ENEMY_MELEE_DAMAGE 8
#define ENEMY_FIREBALL_DAMAGE 20
#define GUN_MAX_DAMAGE 15
#define INTRO 0
#define GAME_PLAY 1

#include "types.h"
#include "entities.h"
#include "sprites.h"
#include "level.h"

#define doomSwap(a, b) do { auto temp = a; a = b; b = temp; } while (0)
#define doomSign(a, b) ((double)((a) > (b) ? 1 : ((b) > (a) ? -1 : 0)))


UID create_uid(EType type, uint8_t x, uint8_t y) {
  return ((y << LEVEL_WIDTH_BASE) | x) << 4 | type;
}

EType uid_get_type(UID uid) {
  return uid & 0x0F;
}

Coords create_coords(double x, double y) {
  return {x, y};
}

uint8_t coords_distance(Coords* a, Coords* b) {
  const double dx = a->x - b->x;
  const double dy = a->y - b->y;
  return sqrt(dx * dx + dy * dy) * DISTANCE_MULTIPLIER;
}

Entity create_entity(uint8_t type, uint8_t x, uint8_t y, uint8_t initialState, uint8_t initialHealth) {
  UID uid = create_uid(type, x, y);
  Coords pos = create_coords((double)x + .5, (double)y + .5);
  return {uid, pos, initialState, initialHealth, 0, 0};
}

StaticEntity create_static_entity(UID uid, uint8_t x, uint8_t y, bool active) {
  return {uid, x, y, active};
}

namespace doom_game {
namespace {

static U8G2* s_u8g2 = nullptr;

static uint8_t scene = INTRO;
static bool exitScene = false;
static bool invertScreen = false;
static uint8_t flashScreen = 0;
static Player player;
static Entity entity[MAX_ENTITIES];
static StaticEntity staticEntity[MAX_STATIC_ENTITIES];
static uint8_t numEntities = 0;
static uint8_t numStaticEntities = 0;
static double delta = 1;
static uint32_t lastFrameTime = 0;
static uint8_t zbuffer[ZBUFFER_SIZE];
static bool introDrawn = false;
static bool hudDrawn = false;
static uint8_t gunPos = 0;
static uint8_t fade = 0;
static double viewHeight = 0;
static double jogging = 0;
static bool fireWasPressed = false;
static unsigned long lastFireTime = 0;
static bool s_running = false;


static uint32_t s_vForwardUntil = 0;
static uint32_t s_vBackwardUntil = 0;
static uint32_t s_vLeftUntil = 0;
static uint32_t s_vRightUntil = 0;
static uint32_t s_vFireUntil = 0;
static bool s_vOkPress = false;
static bool s_vBackPress = false;

const static uint8_t PROGMEM bitMask[8] = {128, 64, 32, 16, 8, 4, 2, 1};

inline bool readBit(uint8_t value, uint8_t bit) {
  return (value & pgm_read_byte(bitMask + bit)) != 0;
}

inline void drawPixel(int16_t x, int16_t y, bool color, bool raycasterViewport = false) {
  if (!s_u8g2) return;
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= (raycasterViewport ? RENDER_HEIGHT : SCREEN_HEIGHT)) {
    return;
  }
  s_u8g2->setDrawColor(color ? 1 : 0);
  s_u8g2->drawPixel(x, y);
}

inline void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
  if (!s_u8g2) return;
  s_u8g2->setDrawColor(color ? 1 : 0);
  s_u8g2->drawBox(x, y, w, h);
}

inline void drawBitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h, uint8_t color) {
  if (!s_u8g2) return;
  s_u8g2->setDrawColor(color ? 1 : 0);
  s_u8g2->drawBitmap(x, y, (w + 7) / 8, h, bmp);
}

bool inputForward() {
  return (digitalRead(PIN_BTN_UP) == LOW) || (millis() < s_vForwardUntil);
}

bool inputBackward() {
  return (digitalRead(PIN_BTN_DOWN) == LOW && scene == GAME_PLAY && player.health > 0) || (millis() < s_vBackwardUntil);
}

bool inputLeft() {
  return (digitalRead(PIN_BTN_LEFT) == LOW) || (millis() < s_vLeftUntil);
}

bool inputRight() {
  return (digitalRead(PIN_BTN_RIGHT) == LOW) || (millis() < s_vRightUntil);
}

bool inputFire() {
  return (digitalRead(PIN_BTN_OK) == LOW) || (millis() < s_vFireUntil);
}

bool inputOkTrigger() {
  if (s_vOkPress) {
    s_vOkPress = false;
    return true;
  }
  static bool lastState = false;
  bool cur = digitalRead(PIN_BTN_OK) == LOW;
  bool trig = (cur && !lastState);
  lastState = cur;
  return trig;
}

bool inputBackTrigger() {
  if (s_vBackPress) {
    s_vBackPress = false;
    return true;
  }
  static bool lastState = false;
  bool cur = digitalRead(PIN_BTN_BACK) == LOW;
  bool trig = (cur && !lastState);
  lastState = cur;
  return trig;
}

bool getGradientPixel(uint8_t x, uint8_t y, uint8_t i) {
  if (i == 0) return false;
  if (i >= GRADIENT_COUNT - 1) return true;
  const uint8_t intensity = constrain(i, (uint8_t)0, (uint8_t)(GRADIENT_COUNT - 1));
  const uint8_t index = intensity * GRADIENT_WIDTH * GRADIENT_HEIGHT
    + y * GRADIENT_WIDTH % (GRADIENT_WIDTH * GRADIENT_HEIGHT)
    + x / GRADIENT_HEIGHT % GRADIENT_WIDTH;
  return readBit(pgm_read_byte(gradient + index), x % 8);
}

void fadeScreen(uint8_t intensity, bool color = false) {
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    for (uint8_t y = 0; y < SCREEN_HEIGHT; y++) {
      if (getGradientPixel(x, y, intensity)) {
        drawPixel(x, y, color, false);
      }
    }
  }
}

void drawVLine(uint8_t x, int16_t startY, int16_t endY, uint8_t intensity) {
  const int16_t lowerY = max<int16_t>(min<int16_t>(startY, endY), 0);
  const int16_t higherY = min<int16_t>(max<int16_t>(startY, endY), RENDER_HEIGHT - 1);
  for (int16_t y = lowerY; y <= higherY; y++) {
    for (uint8_t c = 0; c < RES_DIVIDER; c++) {
      if (getGradientPixel(x + c, y, intensity)) {
        drawPixel(x + c, y, true, true);
      }
    }
  }
}

void drawSprite(int16_t x, int16_t y, const uint8_t bitmap[], const uint8_t mask[],
                int16_t w, int16_t h, uint8_t sprite, double distance) {
  const uint8_t tw = max<int>(1, (double)w / distance);
  const uint8_t th = max<int>(1, (double)h / distance);
  const uint8_t byteWidth = w / 8;
  const uint8_t pixelSize = max<int>(1, 1.0 / distance);
  const uint16_t spriteOffset = byteWidth * h * sprite;

  if (zbuffer[min(max<int>(x, 0), ZBUFFER_SIZE - 1) / Z_RES_DIVIDER] < distance * DISTANCE_MULTIPLIER) {
    return;
  }

  for (uint8_t ty = 0; ty < th; ty += pixelSize) {
    if (y + ty < 0 || y + ty >= RENDER_HEIGHT) continue;
    const uint8_t sy = ty * distance;
    for (uint8_t tx = 0; tx < tw; tx += pixelSize) {
      if (x + tx < 0 || x + tx >= SCREEN_WIDTH) continue;
      const uint8_t sx = tx * distance;
      const uint16_t byteOffset = spriteOffset + sy * byteWidth + sx / 8;
      if (!readBit(pgm_read_byte(mask + byteOffset), sx % 8)) continue;
      const bool pixel = readBit(pgm_read_byte(bitmap + byteOffset), sx % 8);
      for (uint8_t ox = 0; ox < pixelSize; ox++) {
        for (uint8_t oy = 0; oy < pixelSize; oy++) {
          drawPixel(x + tx + ox, y + ty + oy, pixel, true);
        }
      }
    }
  }
}

void drawChar(int16_t x, int16_t y, char ch) {
  uint8_t c = 0;
  while (CHAR_MAP[c] != ch && CHAR_MAP[c] != '\0') c++;
  const uint8_t bOffset = c / 2;
  for (uint8_t line = 0; line < CHAR_HEIGHT; line++) {
    const uint8_t b = pgm_read_byte(bmp_font + (line * bmp_font_width + bOffset));
    for (uint8_t n = 0; n < CHAR_WIDTH; n++) {
      if (readBit(b, (c % 2 == 0 ? 0 : 4) + n)) {
        drawPixel(x + n, y + line, true, false);
      }
    }
  }
}

void drawText(int16_t x, int16_t y, const char* text, uint8_t space = 1) {
  int16_t pos = x;
  for (uint8_t i = 0; text[i] != '\0' && pos < SCREEN_WIDTH; i++) {
    drawChar(pos, y, text[i]);
    pos += CHAR_WIDTH + space;
  }
}

void drawText(uint8_t x, uint8_t y, uint8_t num) {
  char buf[4];
  itoa(num, buf, 10);
  drawText(x, y, buf);
}

uint8_t getBlockAt(const uint8_t level[], uint8_t x, uint8_t y) {
  if (x >= LEVEL_WIDTH || y >= LEVEL_HEIGHT) {
    return E_FLOOR;
  }
  return pgm_read_byte(level + (((LEVEL_HEIGHT - 1 - y) * LEVEL_WIDTH + x) / 2))
    >> (!(x % 2) * 4) & 0b1111;
}

void initializeLevel(const uint8_t level[]) {
  for (int y = LEVEL_HEIGHT - 1; y >= 0; y--) {
    for (uint8_t x = 0; x < LEVEL_WIDTH; x++) {
      if (getBlockAt(level, x, y) == E_PLAYER) {
        player = create_player(x, y);
        return;
      }
    }
  }
}

bool isSpawned(UID uid) {
  for (uint8_t i = 0; i < numEntities; i++) {
    if (entity[i].uid == uid) return true;
  }
  return false;
}

void spawnEntity(uint8_t type, uint8_t x, uint8_t y) {
  if (numEntities >= MAX_ENTITIES) return;
  if (type == E_ENEMY) entity[numEntities++] = create_enemy(x, y);
  else if (type == E_KEY) entity[numEntities++] = create_key(x, y);
  else if (type == E_MEDIKIT) entity[numEntities++] = create_medikit(x, y);
}

void spawnFireball(double x, double y) {
  if (numEntities >= MAX_ENTITIES) return;
  const UID uid = create_uid(E_FIREBALL, x, y);
  if (isSpawned(uid)) return;
  int16_t dir = FIREBALL_ANGLES + atan2(y - player.pos.y, x - player.pos.x) / PI * FIREBALL_ANGLES;
  if (dir < 0) dir += FIREBALL_ANGLES * 2;
  entity[numEntities++] = create_fireball(x, y, dir);
}

void removeEntity(UID uid) {
  uint8_t i = 0;
  bool found = false;
  while (i < numEntities) {
    if (!found && entity[i].uid == uid) {
      found = true;
      numEntities--;
    }
    if (found) entity[i] = entity[i + 1];
    i++;
  }
}

UID detectCollision(const uint8_t level[], Coords *pos, double relativeX, double relativeY, bool onlyWalls = false) {
  const uint8_t roundX = int(pos->x + relativeX);
  const uint8_t roundY = int(pos->y + relativeY);
  const uint8_t block = getBlockAt(level, roundX, roundY);
  if (block == E_WALL) {
    return create_uid(block, roundX, roundY);
  }
  if (onlyWalls) return UID_null;

  for (uint8_t i = 0; i < numEntities; i++) {
    if (&(entity[i].pos) == pos) continue;
    const uint8_t type = uid_get_type(entity[i].uid);
    if (type != E_ENEMY || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) continue;
    Coords newCoords = {entity[i].pos.x - relativeX, entity[i].pos.y - relativeY};
    const uint8_t distance = coords_distance(pos, &newCoords);
    if (distance < ENEMY_COLLIDER_DIST && distance < entity[i].distance) {
      return entity[i].uid;
    }
  }
  return UID_null;
}

UID updatePosition(const uint8_t level[], Coords *pos, double relativeX, double relativeY, bool onlyWalls = false) {
  const UID collideX = detectCollision(level, pos, relativeX, 0, onlyWalls);
  const UID collideY = detectCollision(level, pos, 0, relativeY, onlyWalls);
  if (!collideX) pos->x += relativeX;
  if (!collideY) pos->y += relativeY;
  return collideX || collideY || UID_null;
}

Coords translateIntoView(Coords *pos) {
  const double spriteX = pos->x - player.pos.x;
  const double spriteY = pos->y - player.pos.y;
  const double invDet = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
  return {
    invDet * (player.dir.y * spriteX - player.dir.x * spriteY),
    invDet * (-player.plane.y * spriteX + player.plane.x * spriteY)
  };
}

void fire() {
  int8_t targetIndex = -1;
  int16_t targetOffset = SCREEN_WIDTH;
  uint8_t targetDistance = 255;

  for (uint8_t i = 0; i < numEntities; i++) {
    if (uid_get_type(entity[i].uid) != E_ENEMY || entity[i].state == S_DEAD || entity[i].state == S_HIDDEN) continue;
    const Coords transform = translateIntoView(&(entity[i].pos));
    if (transform.y <= 0.1 || transform.y > MAX_SPRITE_DEPTH) continue;

    const int16_t spriteScreenX = HALF_WIDTH * (1.0 + transform.x / transform.y);
    const int16_t crosshairOffset = abs(spriteScreenX - HALF_WIDTH);
    const int16_t hitRadius = max<int16_t>(5, (BMP_IMP_WIDTH * .5) / transform.y);

    if (crosshairOffset <= hitRadius &&
        (targetIndex < 0 || crosshairOffset < targetOffset ||
         (crosshairOffset == targetOffset && entity[i].distance < targetDistance))) {
      targetIndex = i;
      targetOffset = crosshairOffset;
      targetDistance = entity[i].distance;
    }
  }

  if (targetIndex < 0) return;

  Entity &target = entity[targetIndex];
  const uint8_t damage = constrain(GUN_MAX_DAMAGE - (target.distance / 10), 5, GUN_MAX_DAMAGE);
  target.health = target.health > damage ? target.health - damage : 0;
  target.state = target.health == 0 ? S_DEAD : S_HIT;
  target.timer = target.health == 0 ? 6 : 4;
}

void shootGun() {
  gunPos = GUN_SHOT_POS;
  fire();
}

void updateHud() {
  fillRect(12, 57, 15, 6, 0);
  fillRect(50, 57, 8, 6, 0);
  drawText(12, 57, player.health);
  drawText(50, 57, player.keys);
}

void updateEntities(const uint8_t level[]) {
  uint8_t i = 0;
  while (i < numEntities) {
    entity[i].distance = coords_distance(&(player.pos), &(entity[i].pos));
    if (entity[i].timer > 0) entity[i].timer--;
    if (entity[i].distance > MAX_ENTITY_DISTANCE) {
      removeEntity(entity[i].uid);
      continue;
    }
    if (entity[i].state == S_HIDDEN) {
      i++;
      continue;
    }

    switch (uid_get_type(entity[i].uid)) {
      case E_ENEMY:
        if (entity[i].health == 0) {
          if (entity[i].state != S_DEAD) {
            entity[i].state = S_DEAD;
            entity[i].timer = 6;
          }
        } else if (entity[i].state == S_HIT || entity[i].state == S_FIRING) {
          if (entity[i].timer == 0) {
            entity[i].state = S_ALERT;
            entity[i].timer = 40;
          }
        } else if (entity[i].distance > ENEMY_MELEE_DIST && entity[i].distance < MAX_ENEMY_VIEW) {
          if (entity[i].state != S_ALERT) {
            entity[i].state = S_ALERT;
            entity[i].timer = 20;
          } else if (entity[i].timer == 0) {
            spawnFireball(entity[i].pos.x, entity[i].pos.y);
            entity[i].state = S_FIRING;
            entity[i].timer = 6;
          } else {
            updatePosition(level, &(entity[i].pos),
                           doomSign(player.pos.x, entity[i].pos.x) * ENEMY_SPEED * delta,
                           doomSign(player.pos.y, entity[i].pos.y) * ENEMY_SPEED * delta,
                           true);
          }
        } else if (entity[i].distance <= ENEMY_MELEE_DIST) {
          if (entity[i].state != S_MELEE) {
            entity[i].state = S_MELEE;
            entity[i].timer = 10;
          } else if (entity[i].timer == 0) {
            player.health = max<int>(0, player.health - ENEMY_MELEE_DAMAGE);
            entity[i].timer = 14;
            flashScreen = 1;
            updateHud();
          }
        } else {
          entity[i].state = S_STAND;
        }
        break;
      case E_FIREBALL:
        if (entity[i].distance < FIREBALL_COLLIDER_DIST) {
          player.health = max<int>(0, player.health - ENEMY_FIREBALL_DAMAGE);
          flashScreen = 1;
          updateHud();
          removeEntity(entity[i].uid);
          continue;
        } else if (updatePosition(level, &(entity[i].pos),
                                  cos((double)entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
                                  sin((double)entity[i].health / FIREBALL_ANGLES * PI) * FIREBALL_SPEED,
                                  true)) {
          removeEntity(entity[i].uid);
          continue;
        }
        break;
      case E_MEDIKIT:
        if (entity[i].distance < ITEM_COLLIDER_DIST) {
          entity[i].state = S_HIDDEN;
          player.health = min<int>(100, player.health + 50);
          updateHud();
          flashScreen = 1;
        }
        break;
      case E_KEY:
        if (entity[i].distance < ITEM_COLLIDER_DIST) {
          entity[i].state = S_HIDDEN;
          player.keys++;
          updateHud();
          flashScreen = 1;
        }
        break;
    }
    i++;
  }
}

void renderMap(const uint8_t level[], double currentViewHeight) {
  UID lastUid = UID_null;
  for (uint8_t x = 0; x < SCREEN_WIDTH; x += RES_DIVIDER) {
    const double cameraX = 2 * (double)x / SCREEN_WIDTH - 1;
    const double rayX = player.dir.x + player.plane.x * cameraX;
    const double rayY = player.dir.y + player.plane.y * cameraX;
    uint8_t mapX = uint8_t(player.pos.x);
    uint8_t mapY = uint8_t(player.pos.y);
    Coords mapCoords = {player.pos.x, player.pos.y};
    const double deltaX = abs(1 / rayX);
    const double deltaY = abs(1 / rayY);
    int8_t stepX, stepY;
    double sideX, sideY;

    if (rayX < 0) {
      stepX = -1;
      sideX = (player.pos.x - mapX) * deltaX;
    } else {
      stepX = 1;
      sideX = (mapX + 1.0 - player.pos.x) * deltaX;
    }
    if (rayY < 0) {
      stepY = -1;
      sideY = (player.pos.y - mapY) * deltaY;
    } else {
      stepY = 1;
      sideY = (mapY + 1.0 - player.pos.y) * deltaY;
    }

    uint8_t depth = 0;
    bool hit = false;
    bool side = false;
    while (!hit && depth < MAX_RENDER_DEPTH) {
      if (sideX < sideY) {
        sideX += deltaX;
        mapX += stepX;
        side = false;
      } else {
        sideY += deltaY;
        mapY += stepY;
        side = true;
      }
      const uint8_t block = getBlockAt(level, mapX, mapY);
      if (block == E_WALL) {
        hit = true;
      } else if (block == E_ENEMY || (block & 0b00001000)) {
        if (coords_distance(&(player.pos), &mapCoords) < MAX_ENTITY_DISTANCE) {
          const UID uid = create_uid(block, mapX, mapY);
          if (lastUid != uid && !isSpawned(uid)) {
            spawnEntity(block, mapX, mapY);
            lastUid = uid;
          }
        }
      }
      depth++;
    }

    if (hit) {
      const double distance = side == 0
        ? max<double>(1, (mapX - player.pos.x + (1 - stepX) / 2) / rayX)
        : max<double>(1, (mapY - player.pos.y + (1 - stepY) / 2) / rayY);
      zbuffer[x / Z_RES_DIVIDER] = min<int>(distance * DISTANCE_MULTIPLIER, 255);
      const uint8_t lineHeight = RENDER_HEIGHT / distance;
      drawVLine(x,
                currentViewHeight / distance - lineHeight / 2 + RENDER_HEIGHT / 2,
                currentViewHeight / distance + lineHeight / 2 + RENDER_HEIGHT / 2,
                GRADIENT_COUNT - int(distance / MAX_RENDER_DEPTH * GRADIENT_COUNT) - side * 2);
    }
  }
}

void sortEntities() {
  uint8_t gap = numEntities;
  bool swapped = false;
  while (gap > 1 || swapped) {
    gap = (gap * 10) / 13;
    if (gap == 9 || gap == 10) gap = 11;
    if (gap < 1) gap = 1;
    swapped = false;
    for (uint8_t i = 0; i < numEntities - gap; i++) {
      const uint8_t j = i + gap;
      if (entity[i].distance < entity[j].distance) {
        doomSwap(entity[i], entity[j]);
        swapped = true;
      }
    }
  }
}

void renderEntities(double currentViewHeight) {
  sortEntities();
  for (uint8_t i = 0; i < numEntities; i++) {
    if (entity[i].state == S_HIDDEN) continue;
    const Coords transform = translateIntoView(&(entity[i].pos));
    if (transform.y <= 0.1 || transform.y > MAX_SPRITE_DEPTH) continue;
    const int16_t spriteScreenX = HALF_WIDTH * (1.0 + transform.x / transform.y);
    const int8_t spriteScreenY = RENDER_HEIGHT / 2 + currentViewHeight / transform.y;
    if (spriteScreenX < -HALF_WIDTH || spriteScreenX > SCREEN_WIDTH + HALF_WIDTH) continue;

    switch (uid_get_type(entity[i].uid)) {
      case E_ENEMY: {
        uint8_t sprite = 0;
        if (entity[i].state == S_ALERT) sprite = int(millis() / 500) % 2;
        else if (entity[i].state == S_FIRING) sprite = 2;
        else if (entity[i].state == S_HIT) sprite = 3;
        else if (entity[i].state == S_MELEE) sprite = entity[i].timer > 10 ? 2 : 1;
        else if (entity[i].state == S_DEAD) sprite = entity[i].timer > 0 ? 3 : 4;
        drawSprite(spriteScreenX - BMP_IMP_WIDTH * .5 / transform.y,
                   spriteScreenY - 8 / transform.y,
                   bmp_imp_bits, bmp_imp_mask, BMP_IMP_WIDTH, BMP_IMP_HEIGHT, sprite, transform.y);
        break;
      }
      case E_FIREBALL:
        drawSprite(spriteScreenX - BMP_FIREBALL_WIDTH / 2 / transform.y,
                   spriteScreenY - BMP_FIREBALL_HEIGHT / 2 / transform.y,
                   bmp_fireball_bits, bmp_fireball_mask, BMP_FIREBALL_WIDTH, BMP_FIREBALL_HEIGHT, 0, transform.y);
        break;
      case E_MEDIKIT:
        drawSprite(spriteScreenX - BMP_ITEMS_WIDTH / 2 / transform.y,
                   spriteScreenY + 5 / transform.y,
                   bmp_items_bits, bmp_items_mask, BMP_ITEMS_WIDTH, BMP_ITEMS_HEIGHT, 0, transform.y);
        break;
      case E_KEY:
        drawSprite(spriteScreenX - BMP_ITEMS_WIDTH / 2 / transform.y,
                   spriteScreenY + 5 / transform.y,
                   bmp_items_bits, bmp_items_mask, BMP_ITEMS_WIDTH, BMP_ITEMS_HEIGHT, 1, transform.y);
        break;
    }
  }
}

void renderGun(uint8_t currentGunPos, double amountJogging) {
  const int16_t x = 48 + sin((double)millis() * JOGGING_SPEED) * 10 * amountJogging;
  const int16_t y = RENDER_HEIGHT - currentGunPos + abs(cos((double)millis() * JOGGING_SPEED)) * 8 * amountJogging;
  if (currentGunPos > GUN_SHOT_POS - 2) {
    drawBitmap(x + 6, y - 11, bmp_fire_bits, BMP_FIRE_WIDTH, BMP_FIRE_HEIGHT, 1);
  }
  const uint8_t clipHeight = max<int>(0, min<int>(y + BMP_GUN_HEIGHT, RENDER_HEIGHT) - y);
  drawBitmap(x, y, bmp_gun_mask, BMP_GUN_WIDTH, clipHeight, 0);
  drawBitmap(x, y, bmp_gun_bits, BMP_GUN_WIDTH, clipHeight, 1);
}

void renderHud() {
  drawText(2, 57, "{}", 0);
  drawText(40, 57, "[]", 0);
  updateHud();
}

void renderStats() {
  fillRect(58, 57, 70, 6, 0);
  double actualFps = (FRAME_TIME * delta > 0) ? (1000.0 / (FRAME_TIME * delta)) : 30.0;
  drawText(114, 57, (uint8_t)actualFps);
  drawText(82, 57, numEntities);
}

void resetGame() {
  scene = INTRO;
  exitScene = false;
  invertScreen = false;
  flashScreen = 0;
  numEntities = 0;
  numStaticEntities = 0;
  delta = 1;
  lastFrameTime = millis();
  memset(zbuffer, 0xFF, sizeof(zbuffer));
  introDrawn = false;
  hudDrawn = false;
  gunPos = 0;
  fade = GRADIENT_COUNT - 1;
  viewHeight = 0;
  jogging = 0;
  fireWasPressed = false;
  lastFireTime = 0;
  s_vForwardUntil = 0;
  s_vBackwardUntil = 0;
  s_vLeftUntil = 0;
  s_vRightUntil = 0;
  s_vFireUntil = 0;
  s_vOkPress = false;
  s_vBackPress = false;
}

void beginPlay() {
  numEntities = 0;
  numStaticEntities = 0;
  memset(zbuffer, 0xFF, sizeof(zbuffer));
  initializeLevel(sto_level_1);
  gunPos = 0;
  fade = GRADIENT_COUNT - 1;
  hudDrawn = false;
  viewHeight = 0;
  jogging = 0;
  fireWasPressed = false;
  lastFireTime = 0;
  lastFrameTime = millis();
  scene = GAME_PLAY;
  exitScene = false;
}

void drawIntro() {
  fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
  drawBitmap((SCREEN_WIDTH - BMP_LOGO_WIDTH) / 2,
             (SCREEN_HEIGHT - BMP_LOGO_HEIGHT) / 3,
             bmp_logo_bits, BMP_LOGO_WIDTH, BMP_LOGO_HEIGHT, 1);
  drawText((SCREEN_WIDTH - (8 * CHAR_WIDTH + 7)) / 2, SCREEN_HEIGHT * .8, "PRESS OK");
  introDrawn = true;
}

void rotatePlayer(double rotSpeed) {
  const double oldDirX = player.dir.x;
  player.dir.x = player.dir.x * cos(rotSpeed) - player.dir.y * sin(rotSpeed);
  player.dir.y = oldDirX * sin(rotSpeed) + player.dir.y * cos(rotSpeed);
  const double oldPlaneX = player.plane.x;
  player.plane.x = player.plane.x * cos(rotSpeed) - player.plane.y * sin(rotSpeed);
  player.plane.y = oldPlaneX * sin(rotSpeed) + player.plane.y * cos(rotSpeed);
}

void updatePhysics() {
  uint32_t nowMs = millis();
  if (nowMs - lastFrameTime > 0) {
    delta = (double)(nowMs - lastFrameTime) / FRAME_TIME;
    if (delta > 3.0) delta = 3.0;
  } else {
    delta = 1.0;
  }
  lastFrameTime = nowMs;

  if (player.health > 0) {
    if (inputForward()) {
      player.velocity += (MOV_SPEED - player.velocity) * .4;
      jogging = abs(player.velocity) * MOV_SPEED_INV;
    } else if (inputBackward()) {
      player.velocity += (-MOV_SPEED * 0.7 - player.velocity) * .4;
      jogging = abs(player.velocity) * MOV_SPEED_INV;
    } else {
      player.velocity *= .5;
      jogging = abs(player.velocity) * MOV_SPEED_INV;
    }

    if (inputRight()) {
      rotatePlayer(-ROT_SPEED * delta);
    } else if (inputLeft()) {
      rotatePlayer(ROT_SPEED * delta);
    }

    viewHeight = abs(sin((double)nowMs * JOGGING_SPEED)) * 6 * jogging;
    const bool firePressed = inputFire();
    if (gunPos > GUN_TARGET_POS) {
      gunPos--;
    } else if (gunPos < GUN_TARGET_POS) {
      gunPos += 2;
    }

    if (firePressed && !fireWasPressed) {
      fireWasPressed = true;
      if (lastFireTime == 0 || nowMs - lastFireTime >= FIRE_COOLDOWN_MS) {
        lastFireTime = nowMs;
        shootGun();
      }
    } else if (!firePressed) {
      fireWasPressed = false;
    }
  } else {
    if (viewHeight > -10) viewHeight--;
    if (gunPos > 1) gunPos -= 2;
  }

  if (abs(player.velocity) > 0.003) {
    updatePosition(sto_level_1, &(player.pos),
                   player.dir.x * player.velocity * delta,
                   player.dir.y * player.velocity * delta);
  } else {
    player.velocity = 0;
  }

  updateEntities(sto_level_1);
}

}

void open() {
  s_running = true;
  resetGame();
  display::setInvert(false);
  Serial.println("[doom] open");
}

void close() {
  s_running = false;
  display::setInvert(false);
  Serial.println("[doom] close");
}

bool isRunning() {
  return s_running;
}

bool onBack() {
  close();
  return false;
}

void onKey(const KeyEvent& ev) {
  uint32_t nowMs = millis();

  if (scene == INTRO) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_vOkPress = true;
    } else if (ev.key == BTN_BACK && !ev.repeat) {
      s_vBackPress = true;
    }
    return;
  }

  if (scene == GAME_PLAY) {
    if (player.health == 0) {
      if (ev.key == BTN_OK && !ev.repeat) {
        s_vOkPress = true;
      } else if (ev.key == BTN_BACK && !ev.repeat) {
        s_vBackPress = true;
      }
      return;
    }


    if (ev.key == BTN_UP) {
      s_vForwardUntil = nowMs + 280;
    } else if (ev.key == BTN_DOWN) {
      s_vBackwardUntil = nowMs + 280;
    } else if (ev.key == BTN_LEFT) {
      s_vLeftUntil = nowMs + 200;
    } else if (ev.key == BTN_RIGHT) {
      s_vRightUntil = nowMs + 200;
    } else if (ev.key == BTN_OK) {
      s_vFireUntil = nowMs + 200;
      if (lastFireTime == 0 || nowMs - lastFireTime >= FIRE_COOLDOWN_MS) {
        lastFireTime = nowMs;
        fireWasPressed = true;
        shootGun();
      }
    } else if (ev.key == BTN_BACK && !ev.repeat) {
      s_vBackPress = true;
    }
  }
}

void tick(uint32_t now) {
  (void)now;
  if (!s_running) return;

  if (scene == INTRO) {
    if (inputBackTrigger()) {
      close();
      return;
    }
    if (inputOkTrigger()) {
      beginPlay();
      return;
    }
  } else if (scene == GAME_PLAY) {
    if (player.health == 0) {
      if (inputBackTrigger()) {
        close();
        return;
      }
      if (inputOkTrigger()) {
        beginPlay();
        return;
      }
    } else {
      if (inputBackTrigger()) {
        close();
        return;
      }
    }
    updatePhysics();
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_running) return;
  s_u8g2 = &u8g2;

  if (scene == INTRO) {
    drawIntro();
  } else {
    fillRect(0, 0, SCREEN_WIDTH, RENDER_HEIGHT, 0);
    renderMap(sto_level_1, viewHeight);
    renderEntities(viewHeight);
    renderGun(gunPos, jogging);

    if (fade > 0) {
      fadeScreen(fade);
      fade--;
      if (fade == 0) {
        renderHud();
        hudDrawn = true;
      }
    } else {
      if (!hudDrawn) {
        renderHud();
        hudDrawn = true;
      }
      renderStats();
    }

    if (flashScreen > 0) {
      invertScreen = !invertScreen;
      flashScreen--;
    } else if (invertScreen) {
      invertScreen = false;
    }

    display::setInvert(invertScreen);
  }
}

}
