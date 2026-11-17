#include "js_runner.h"
#include <FFat.h>
#include <SD.h>
#include <mjs.h>
#include <math.h>
#include "core/display.h"
#include "modules/infrared/ir_sender_hw.h"

namespace js_runner {

static struct mjs* s_mjs = nullptr;
static app_registry::InstalledApp s_activeApp;
static bool s_active = false;
static bool s_hasError = false;
static String s_errorMessage = "";


static uint8_t s_appFrameBuffer[1024];
static bool s_hasBuffer = false;


static bool checkPerm(const char* permName) {
  if (!s_activeApp.hasPermission(permName)) {
    Serial.printf("[Security Error] App '%s' lacks '%s' permission!\n",
                  s_activeApp.name.c_str(), permName);
    return false;
  }
  return true;
}






static void js_print(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    mjs_val_t v = mjs_arg(mjs, 0);
    if (mjs_is_string(v)) {
      size_t len = 0;
      const char* s = mjs_get_string(mjs, &v, &len);
      if (s) {
        Serial.write((const uint8_t*)s, len);
        Serial.println();
      }
    } else if (mjs_is_number(v)) {
      Serial.println(mjs_get_double(mjs, v));
    } else if (mjs_is_boolean(v)) {
      Serial.println(mjs_get_bool(mjs, v) ? "true" : "false");
    } else {
      Serial.println("[object]");
    }
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_math_random(struct mjs* mjs) {
  double r = ((double)esp_random()) / 4294967296.0;
  mjs_return(mjs, mjs_mk_number(mjs, r));
}


static void js_math_floor(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, floor(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_ceil(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, ceil(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_round(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, round(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_abs(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, fabs(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_min(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    double a = mjs_get_double(mjs, mjs_arg(mjs, 0));
    double b = mjs_get_double(mjs, mjs_arg(mjs, 1));
    mjs_return(mjs, mjs_mk_number(mjs, fmin(a, b)));
  } else if (n == 1) {
    mjs_return(mjs, mjs_arg(mjs, 0));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_max(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    double a = mjs_get_double(mjs, mjs_arg(mjs, 0));
    double b = mjs_get_double(mjs, mjs_arg(mjs, 1));
    mjs_return(mjs, mjs_mk_number(mjs, fmax(a, b)));
  } else if (n == 1) {
    mjs_return(mjs, mjs_arg(mjs, 0));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_sqrt(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, sqrt(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_sin(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, sin(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_math_cos(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    double v = mjs_get_double(mjs, mjs_arg(mjs, 0));
    mjs_return(mjs, mjs_mk_number(mjs, cos(v)));
  } else {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
  }
}


static void js_esp_millis(struct mjs* mjs) {
  mjs_return(mjs, mjs_mk_number(mjs, (double)millis()));
}


static void js_esp_delay(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n > 0) {
    int ms = mjs_get_int(mjs, mjs_arg(mjs, 0));
    if (ms > 0) delay(ms);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_clear(struct mjs* mjs) {
  display::get().clearBuffer();
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_print(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 3) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    mjs_val_t textVal = mjs_arg(mjs, 2);

    char buf[64];
    if (mjs_is_string(textVal)) {
      size_t len = 0;
      const char* s = mjs_get_string(mjs, &textVal, &len);
      if (s) {
        size_t cpyLen = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
        memcpy(buf, s, cpyLen);
        buf[cpyLen] = '\0';
        display::get().drawStr(x, y, buf);
      }
    } else if (mjs_is_number(textVal)) {
      snprintf(buf, sizeof(buf), "%g", mjs_get_double(mjs, textVal));
      display::get().drawStr(x, y, buf);
    }
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_box(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 4) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    int w = mjs_get_int(mjs, mjs_arg(mjs, 2));
    int h = mjs_get_int(mjs, mjs_arg(mjs, 3));
    display::get().drawBox(x, y, w, h);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_frame(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 4) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    int w = mjs_get_int(mjs, mjs_arg(mjs, 2));
    int h = mjs_get_int(mjs, mjs_arg(mjs, 3));
    display::get().drawFrame(x, y, w, h);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_line(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 4) {
    int x1 = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y1 = mjs_get_int(mjs, mjs_arg(mjs, 1));
    int x2 = mjs_get_int(mjs, mjs_arg(mjs, 2));
    int y2 = mjs_get_int(mjs, mjs_arg(mjs, 3));
    display::get().drawLine(x1, y1, x2, y2);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_circle(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 3) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    int r = mjs_get_int(mjs, mjs_arg(mjs, 2));
    display::get().drawCircle(x, y, r);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_disc(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 3) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    int r = mjs_get_int(mjs, mjs_arg(mjs, 2));
    display::get().drawDisc(x, y, r);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_draw_pixel(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    int x = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int y = mjs_get_int(mjs, mjs_arg(mjs, 1));
    display::get().drawPixel(x, y);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_set_draw_color(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 1) {
    int c = mjs_get_int(mjs, mjs_arg(mjs, 0));
    display::get().setDrawColor(c);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_set_font(struct mjs* mjs) {
  int n = mjs_nargs(mjs);
  if (n >= 1) {
    int fontId = mjs_get_int(mjs, mjs_arg(mjs, 0));
    switch (fontId) {
      case 1:  display::get().setFont(u8g2_font_5x8_tr); break;
      case 2:  display::get().setFont(u8g2_font_7x14B_tr); break;
      default: display::get().setFont(u8g2_font_6x12_tr); break;
    }
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_disp_update(struct mjs* mjs) {

  memcpy(s_appFrameBuffer, display::get().getBufferPtr(), 1024);
  s_hasBuffer = true;
  display::get().sendBuffer();
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_ir_send_nec(struct mjs* mjs) {
  if (!checkPerm("infrared")) {
    mjs_return(mjs, MJS_UNDEFINED);
    return;
  }
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    uint16_t addr = (uint16_t)mjs_get_int(mjs, mjs_arg(mjs, 0));
    uint32_t cmd = (uint32_t)mjs_get_int(mjs, mjs_arg(mjs, 1));
    ir_db::IrCode code;
    code.name = "JS NEC";
    code.proto = ir_db::IR_PROTO_NEC;
    code.address = addr;
    code.command = cmd;
    code.bits = 32;
    ir_sender_hw::sendCode(code);
    Serial.printf("[JS IR] Sent NEC: Addr=0x%04X Cmd=0x%04X\n", addr, cmd);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_gpio_pin_mode(struct mjs* mjs) {
  if (!checkPerm("gpio")) {
    mjs_return(mjs, MJS_UNDEFINED);
    return;
  }
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    int pin = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int mode = mjs_get_int(mjs, mjs_arg(mjs, 1));
    pinMode(pin, mode);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_gpio_digital_write(struct mjs* mjs) {
  if (!checkPerm("gpio")) {
    mjs_return(mjs, MJS_UNDEFINED);
    return;
  }
  int n = mjs_nargs(mjs);
  if (n >= 2) {
    int pin = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int val = mjs_get_int(mjs, mjs_arg(mjs, 1));
    digitalWrite(pin, val);
  }
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_gpio_digital_read(struct mjs* mjs) {
  if (!checkPerm("gpio")) {
    mjs_return(mjs, mjs_mk_number(mjs, 0));
    return;
  }
  int n = mjs_nargs(mjs);
  if (n >= 1) {
    int pin = mjs_get_int(mjs, mjs_arg(mjs, 0));
    int val = digitalRead(pin);
    mjs_return(mjs, mjs_mk_number(mjs, val));
    return;
  }
  mjs_return(mjs, mjs_mk_number(mjs, 0));
}


static void js_sys_reboot(struct mjs* mjs) {
  if (!checkPerm("system")) {
    mjs_return(mjs, MJS_UNDEFINED);
    return;
  }
  Serial.println("[JS System] Reboot requested");
  ESP.restart();
  mjs_return(mjs, MJS_UNDEFINED);
}


static void js_sys_free_heap(struct mjs* mjs) {
  mjs_return(mjs, mjs_mk_number(mjs, (double)ESP.getFreeHeap()));
}

static void registerBindings(struct mjs* mjs) {
  mjs_val_t global = mjs_get_global(mjs);


  mjs_set(mjs, global, "print", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_print));


  mjs_val_t math = mjs_mk_object(mjs);
  mjs_set(mjs, global, "Math", ~0, math);
  mjs_set(mjs, math, "floor", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_floor));
  mjs_set(mjs, math, "ceil", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_ceil));
  mjs_set(mjs, math, "round", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_round));
  mjs_set(mjs, math, "abs", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_abs));
  mjs_set(mjs, math, "min", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_min));
  mjs_set(mjs, math, "max", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_max));
  mjs_set(mjs, math, "sqrt", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_sqrt));
  mjs_set(mjs, math, "sin", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_sin));
  mjs_set(mjs, math, "cos", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_cos));
  mjs_set(mjs, math, "random", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_math_random));
  mjs_set(mjs, math, "PI", ~0, mjs_mk_number(mjs, 3.141592653589793));


  mjs_val_t esp = mjs_mk_object(mjs);
  mjs_set(mjs, global, "ESP", ~0, esp);

  mjs_set(mjs, esp, "millis", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_esp_millis));
  mjs_set(mjs, esp, "delay", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_esp_delay));


  mjs_val_t disp = mjs_mk_object(mjs);
  mjs_set(mjs, esp, "display", ~0, disp);
  mjs_set(mjs, disp, "clear", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_clear));
  mjs_set(mjs, disp, "print", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_print));
  mjs_set(mjs, disp, "drawBox", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_box));
  mjs_set(mjs, disp, "drawFrame", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_frame));
  mjs_set(mjs, disp, "drawLine", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_line));
  mjs_set(mjs, disp, "drawCircle", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_circle));
  mjs_set(mjs, disp, "drawDisc", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_disc));
  mjs_set(mjs, disp, "drawPixel", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_draw_pixel));
  mjs_set(mjs, disp, "setDrawColor", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_set_draw_color));
  mjs_set(mjs, disp, "setFont", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_set_font));
  mjs_set(mjs, disp, "update", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_disp_update));


  mjs_val_t ir = mjs_mk_object(mjs);
  mjs_set(mjs, esp, "ir", ~0, ir);
  mjs_set(mjs, ir, "sendNec", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_ir_send_nec));


  mjs_val_t gpio = mjs_mk_object(mjs);
  mjs_set(mjs, esp, "gpio", ~0, gpio);
  mjs_set(mjs, gpio, "pinMode", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_gpio_pin_mode));
  mjs_set(mjs, gpio, "digitalWrite", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_gpio_digital_write));
  mjs_set(mjs, gpio, "digitalRead", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_gpio_digital_read));


  mjs_val_t sys = mjs_mk_object(mjs);
  mjs_set(mjs, esp, "system", ~0, sys);
  mjs_set(mjs, sys, "reboot", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_sys_reboot));
  mjs_set(mjs, sys, "getFreeHeap", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t)js_sys_free_heap));
}

bool open(const app_registry::InstalledApp& app) {
  close();

  s_activeApp = app;
  s_active = true;
  s_hasError = false;
  s_errorMessage = "";
  s_hasBuffer = false;
  memset(s_appFrameBuffer, 0, sizeof(s_appFrameBuffer));

  Serial.printf("[JS Runner] Launching: %s (%s)\n", app.name.c_str(), app.entryPath.c_str());


  fs::FS* fs = app.isSd ? (fs::FS*)&SD : (fs::FS*)&FFat;
  if (!fs->exists(app.entryPath.c_str())) {
    s_hasError = true;
    s_errorMessage = "Script not found!";
    return false;
  }

  File f = fs->open(app.entryPath.c_str(), "r");
  if (!f) {
    s_hasError = true;
    s_errorMessage = "Cannot open script!";
    return false;
  }

  String code = f.readString();
  f.close();


  s_mjs = mjs_create();
  if (!s_mjs) {
    s_hasError = true;
    s_errorMessage = "Out of memory!";
    return false;
  }


  registerBindings(s_mjs);


  mjs_val_t res = MJS_UNDEFINED;
  mjs_err_t err = mjs_exec(s_mjs, code.c_str(), &res);
  if (err != MJS_OK) {
    s_hasError = true;
    s_errorMessage = String("Error: ") + mjs_strerror(s_mjs, err);
    Serial.printf("[JS Error] %s\n", s_errorMessage.c_str());
    return false;
  }

  return true;
}

void close() {
  if (s_mjs) {
    mjs_destroy(s_mjs);
    s_mjs = nullptr;
  }
  s_active = false;
  s_hasError = false;
  s_hasBuffer = false;
}

bool isActive() {
  return s_active;
}

void tick(uint32_t now) {
  if (!s_active || s_hasError || !s_mjs) return;


  mjs_val_t global = mjs_get_global(s_mjs);
  mjs_val_t fn = mjs_get(s_mjs, global, "onTick", ~0);
  if (mjs_is_function(fn)) {
    mjs_val_t res = MJS_UNDEFINED;
    mjs_err_t err = mjs_apply(s_mjs, &res, fn, MJS_UNDEFINED, 0, NULL);
    if (err != MJS_OK) {
      s_hasError = true;
      s_errorMessage = String("Tick Error: ") + mjs_strerror(s_mjs, err);
      Serial.println(s_errorMessage);
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_hasError) {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawStr(4, 9, "JS RUNNER ERROR");
    u8g2.setDrawColor(1);
    u8g2.drawStr(4, 28, s_errorMessage.c_str());
    u8g2.drawStr(4, 60, "BACK: EXIT");
    return;
  }

  if (!s_active || !s_mjs) return;


  mjs_val_t global = mjs_get_global(s_mjs);
  mjs_val_t fn = mjs_get(s_mjs, global, "onDraw", ~0);
  if (!mjs_is_function(fn)) {
    fn = mjs_get(s_mjs, global, "draw", ~0);
  }
  if (mjs_is_function(fn)) {
    mjs_val_t res = MJS_UNDEFINED;
    mjs_apply(s_mjs, &res, fn, MJS_UNDEFINED, 0, NULL);
    return;
  }


  if (s_hasBuffer) {
    memcpy(u8g2.getBufferPtr(), s_appFrameBuffer, 1024);
  }
}

void onKey(const KeyEvent& ev) {
  if (!s_active || !s_mjs) return;

  if (ev.key == BTN_BACK && !ev.repeat) {
    close();
    return;
  }

  if (s_hasError) return;


  const char* keyName = "NONE";
  if (ev.key == BTN_UP) keyName = "UP";
  else if (ev.key == BTN_DOWN) keyName = "DOWN";
  else if (ev.key == BTN_LEFT) keyName = "LEFT";
  else if (ev.key == BTN_RIGHT) keyName = "RIGHT";
  else if (ev.key == BTN_OK) keyName = "OK";

  mjs_val_t global = mjs_get_global(s_mjs);
  mjs_val_t fn = mjs_get(s_mjs, global, "onKey", ~0);
  if (mjs_is_function(fn)) {
    mjs_val_t args[2];
    args[0] = mjs_mk_string(s_mjs, keyName, ~0, 1);
    args[1] = mjs_mk_boolean(s_mjs, 1);
    mjs_val_t res = MJS_UNDEFINED;
    mjs_apply(s_mjs, &res, fn, MJS_UNDEFINED, 2, args);
  }
}

bool onBack() {
  if (s_active) {
    close();
    return true;
  }
  return false;
}

}
