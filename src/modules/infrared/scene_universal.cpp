#include "scene_universal.h"
#include "ir_db.h"

namespace scene_universal {

enum UniState {
  ST_CATEGORIES = 0,
  ST_BRANDS,
  ST_BUTTONS
};

static UniState s_state = ST_CATEGORIES;
static int s_catSel = 0;
static float s_catBarY = 16.0f;
static float s_catScroll = 0.0f;
static float s_catTargetScroll = 0.0f;

static int s_brandSel = 0;
static float s_brandBarY = 16.0f;
static float s_brandScroll = 0.0f;
static float s_brandTargetScroll = 0.0f;

static int s_btnSel = 0;
static float s_btnScroll = 0.0f;
static float s_btnTargetScroll = 0.0f;
static float s_btnBarY = 16.0f;
static uint32_t s_txUntil = 0;

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

void open() {
  s_state = ST_CATEGORIES;
  s_catSel = 0;
  s_catBarY = startY;
  s_catScroll = 0.0f;
  s_catTargetScroll = 0.0f;
  s_brandSel = 0;
  s_brandBarY = startY;
  s_brandScroll = 0.0f;
  s_brandTargetScroll = 0.0f;
  s_btnSel = 0;
  s_btnScroll = 0.0f;
  s_btnTargetScroll = 0.0f;
  s_btnBarY = startY;
  s_txUntil = 0;
}

void close() {
  s_state = ST_CATEGORIES;
}

bool onBack() {
  if (s_state == ST_BUTTONS) {
    if ((ir_db::UniversalCategory)s_catSel == ir_db::CAT_PROJECTOR) {
      s_state = ST_BRANDS;
      s_brandBarY = startY + (s_brandSel - s_brandScroll) * rowH;
    } else {
      s_state = ST_CATEGORIES;
      s_catBarY = startY + (s_catSel - s_catScroll) * rowH;
    }
    return true;
  }
  if (s_state == ST_BRANDS) {
    s_state = ST_CATEGORIES;
    s_catBarY = startY + (s_catSel - s_catScroll) * rowH;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_state == ST_CATEGORIES) {
    float ds = s_catTargetScroll - s_catScroll;
    if (fabsf(ds) > 0.02f) s_catScroll += ds * 0.35f;
    else s_catScroll = s_catTargetScroll;

    float target = startY + (s_catSel - s_catScroll) * rowH;
    float dy = target - s_catBarY;
    if (fabsf(dy) > 0.1f) s_catBarY += dy * 0.35f;
    else s_catBarY = target;
  } else if (s_state == ST_BRANDS) {
    float ds = s_brandTargetScroll - s_brandScroll;
    if (fabsf(ds) > 0.02f) s_brandScroll += ds * 0.35f;
    else s_brandScroll = s_brandTargetScroll;

    float target = startY + (s_brandSel - s_brandScroll) * rowH;
    float dy = target - s_brandBarY;
    if (fabsf(dy) > 0.1f) s_brandBarY += dy * 0.35f;
    else s_brandBarY = target;
  } else {

    float dScroll = s_btnTargetScroll - s_btnScroll;
    if (fabsf(dScroll) > 0.02f) s_btnScroll += dScroll * 0.35f;
    else s_btnScroll = s_btnTargetScroll;


    float target = startY + (s_btnSel - s_btnScroll) * rowH;
    float dy = target - s_btnBarY;
    if (fabsf(dy) > 0.1f) s_btnBarY += dy * 0.35f;
    else s_btnBarY = target;
  }
}

void onKey(const KeyEvent& ev) {
  if (s_state == ST_CATEGORIES) {
    int total = ir_db::CAT_COUNT;
    if (ev.key == BTN_UP) {
      if (s_catSel == 0) {
        s_catSel = total - 1;
        s_catTargetScroll = (total > visibleRows) ? (total - visibleRows) : 0;
      } else {
        s_catSel--;
        if (s_catSel < s_catTargetScroll) s_catTargetScroll = s_catSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_catSel == total - 1) {
        s_catSel = 0;
        s_catTargetScroll = 0.0f;
      } else {
        s_catSel++;
        if (s_catSel >= s_catTargetScroll + visibleRows) s_catTargetScroll = s_catSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if ((ir_db::UniversalCategory)s_catSel == ir_db::CAT_PROJECTOR) {
        s_state = ST_BRANDS;
        s_brandSel = 0;
        s_brandScroll = 0.0f;
        s_brandTargetScroll = 0.0f;
        s_brandBarY = startY;
      } else {
        s_state = ST_BUTTONS;
        s_btnSel = 0;
        s_btnScroll = 0.0f;
        s_btnTargetScroll = 0.0f;
        s_btnBarY = startY;
      }
    }
  } else if (s_state == ST_BRANDS) {
    int total = ir_db::PROJ_COUNT;
    if (ev.key == BTN_UP) {
      if (s_brandSel == 0) {
        s_brandSel = total - 1;
        s_brandTargetScroll = (total > visibleRows) ? (total - visibleRows) : 0;
      } else {
        s_brandSel--;
        if (s_brandSel < s_brandTargetScroll) s_brandTargetScroll = s_brandSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_brandSel == total - 1) {
        s_brandSel = 0;
        s_brandTargetScroll = 0.0f;
      } else {
        s_brandSel++;
        if (s_brandSel >= s_brandTargetScroll + visibleRows) s_brandTargetScroll = s_brandSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_state = ST_BUTTONS;
      s_btnSel = 0;
      s_btnScroll = 0.0f;
      s_btnTargetScroll = 0.0f;
      s_btnBarY = startY;
    }
  } else {
    bool isProj = ((ir_db::UniversalCategory)s_catSel == ir_db::CAT_PROJECTOR);
    int total = isProj ? ir_db::getProjectorBrandButtonCount((ir_db::ProjectorBrand)s_brandSel)
                       : ir_db::getCategoryButtonCount((ir_db::UniversalCategory)s_catSel);
    if (ev.key == BTN_UP) {
      if (s_btnSel == 0) {
        s_btnSel = total - 1;
        s_btnTargetScroll = (total > visibleRows) ? (total - visibleRows) : 0;
      } else {
        s_btnSel--;
        if (s_btnSel < s_btnTargetScroll) s_btnTargetScroll = s_btnSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_btnSel == total - 1) {
        s_btnSel = 0;
        s_btnTargetScroll = 0.0f;
      } else {
        s_btnSel++;
        if (s_btnSel >= s_btnTargetScroll + visibleRows) s_btnTargetScroll = s_btnSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {

      s_txUntil = millis() + 450;
      if (isProj) {
        ir_db::sendProjectorBrandButton((ir_db::ProjectorBrand)s_brandSel, s_btnSel);
      } else {
        ir_db::sendCategoryButton((ir_db::UniversalCategory)s_catSel, s_btnSel);
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == ST_CATEGORIES) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < ir_db::CAT_COUNT; i++) {
      int y = startY + (int)((i - s_catScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, ir_db::getCategoryName((ir_db::UniversalCategory)i));
    }


    int bY = (int)(s_catBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "UNIVERSAL");
    u8g2.setDrawColor(1);
  } else if (s_state == ST_BRANDS) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < ir_db::PROJ_COUNT; i++) {
      int y = startY + (int)((i - s_brandScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, ir_db::getProjectorBrandName((ir_db::ProjectorBrand)i));
    }


    int bY = (int)(s_brandBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "PROJ BRANDS");
    u8g2.setDrawColor(1);
  } else {

    bool isProj = ((ir_db::UniversalCategory)s_catSel == ir_db::CAT_PROJECTOR);
    int total = isProj ? ir_db::getProjectorBrandButtonCount((ir_db::ProjectorBrand)s_brandSel)
                       : ir_db::getCategoryButtonCount((ir_db::UniversalCategory)s_catSel);


    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < total; i++) {
      int y = startY + (int)((i - s_btnScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;
      const char* name = isProj ? ir_db::getProjectorBrandButtonName((ir_db::ProjectorBrand)s_brandSel, i)
                                : ir_db::getCategoryButtonName((ir_db::UniversalCategory)s_catSel, i);
      u8g2.drawStr(6, y + 10, name);
    }


    int bY = (int)(s_btnBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);

    char titleBuf[24];
    if (isProj) {
      snprintf(titleBuf, sizeof(titleBuf), "%s", ir_db::getProjectorBrandName((ir_db::ProjectorBrand)s_brandSel));
    } else {
      snprintf(titleBuf, sizeof(titleBuf), "%s REMOTE", ir_db::getCategoryName((ir_db::UniversalCategory)s_catSel));
    }
    u8g2.drawStr(4, 11, titleBuf);


    if (now < s_txUntil) {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(108, 11, "TX");
    }

    u8g2.setDrawColor(1);
  }
}

}
