#include "LvglDriver.h"
#include <Arduino.h>
#include <SPI.h>
#include "Hardware.h"
#include "Config.h"
#include "AppState.h"

namespace {
  // Partial draw buffer: 320 x 24 pixels x 2 bytes = 15,360 bytes
  constexpr uint32_t DRAW_BUF_LINES = 24;
  constexpr uint32_t DRAW_BUF_PIXELS = SCREEN_W * DRAW_BUF_LINES;
  uint16_t drawBuf[DRAW_BUF_PIXELS];

  lv_display_t* disp = nullptr;
  lv_indev_t* touchIndev = nullptr;
  lv_indev_t* keypadIndev = nullptr;
  lv_group_t* inputGroup = nullptr;

  unsigned long lastTickMs = 0;

  // --- Display Flush Callback ---
  void dispFlushCb(lv_display_t* display, const lv_area_t* area, uint8_t* pxMap) {
    const uint32_t w = (area->x2 - area->x1 + 1);
    const uint32_t h = (area->y2 - area->y1 + 1);
    const uint32_t pixelCount = w * h;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels(reinterpret_cast<uint16_t*>(pxMap), pixelCount, true, false);
    tft.endWrite();

    lv_display_flush_ready(display);
  }

  // --- Touch Helper & Callback ---
  int median3(int a, int b, int c) {
    if (a > b) { int t = a; a = b; b = t; }
    if (b > c) { int t = b; b = c; c = t; }
    if (a > b) { int t = a; a = b; b = t; }
    return b;
  }

  TS_Point getFilteredPoint() {
    TS_Point p1 = ts.getPoint();
    delayMicroseconds(500);
    TS_Point p2 = ts.getPoint();
    delayMicroseconds(500);
    TS_Point p3 = ts.getPoint();

    return TS_Point(
      median3(p1.x, p2.x, p3.x),
      median3(p1.y, p2.y, p3.y),
      median3(p1.z, p2.z, p3.z)
    );
  }

  void touchReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    LV_UNUSED(indev);
    if (!ts.touched()) {
      smoothingStarted = false;
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }

    const TS_Point p = getFilteredPoint();
    if (p.z < MIN_PRESSURE) {
      smoothingStarted = false;
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }

    int x = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, SCREEN_W - 1);
    int y = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, SCREEN_H - 1);
    x = constrain(x, 0, SCREEN_W - 1);
    y = constrain(y, 0, SCREEN_H - 1);

    if (!smoothingStarted) {
      smoothX = x;
      smoothY = y;
      smoothingStarted = true;
    } else {
      smoothX = 0.55f * smoothX + 0.45f * x;
      smoothY = 0.55f * smoothY + 0.45f * y;
    }

    touchX = static_cast<int>(smoothX);
    touchY = static_cast<int>(smoothY);

    data->point.x = touchX;
    data->point.y = touchY;
    data->state = LV_INDEV_STATE_PRESSED;
  }

  // --- Physical 4-Button Keypad Callback (Edge-Triggered) ---
  uint32_t lastKeyPressed = 0;

  bool buttonJustPressed(ButtonState& btn) {
    const bool reading = digitalRead(btn.pin);
    const unsigned long now = millis();

    if (reading != btn.lastReading) {
      btn.lastChange = now;
      btn.lastReading = reading;
    }

    if (now - btn.lastChange >= DEBOUNCE_MS) {
      if (reading != btn.stableState) {
        btn.stableState = reading;
        if (btn.stableState == LOW) {
          return true; // Fires ONCE per physical press
        }
      }
    }
    return false;
  }

  void keypadReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    LV_UNUSED(indev);
    if (buttonJustPressed(upButton)) {
      lastKeyPressed = LV_KEY_PREV;
      data->key = lastKeyPressed;
      data->state = LV_INDEV_STATE_PRESSED;
      Serial.println("[BTN] UP (GPIO 25)");
    } else if (buttonJustPressed(downButton)) {
      lastKeyPressed = LV_KEY_NEXT;
      data->key = lastKeyPressed;
      data->state = LV_INDEV_STATE_PRESSED;
      Serial.println("[BTN] DOWN (GPIO 26)");
    } else if (buttonJustPressed(selectButton)) {
      lastKeyPressed = LV_KEY_ENTER;
      data->key = lastKeyPressed;
      data->state = LV_INDEV_STATE_PRESSED;
      Serial.println("[BTN] SELECT (GPIO 33)");
    } else if (buttonJustPressed(backButton)) {
      lastKeyPressed = LV_KEY_ESC;
      data->key = lastKeyPressed;
      data->state = LV_INDEV_STATE_PRESSED;
      Serial.println("[BTN] BACK (GPIO 32)");
    } else {
      data->key = lastKeyPressed;
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
} // <--- Closing brace for anonymous namespace

void initializeLvgl() {
  lv_init();
  lastTickMs = millis();

  // 1. Register Display
  disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, dispFlushCb);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // 2. Register Touch Pointer
  touchIndev = lv_indev_create();
  lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touchIndev, touchReadCb);

  // 3. Register 4-Button Keypad & Focus Group
  inputGroup = lv_group_create();
  lv_group_set_default(inputGroup);

  keypadIndev = lv_indev_create();
  lv_indev_set_type(keypadIndev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(keypadIndev, keypadReadCb);
  lv_indev_set_group(keypadIndev, inputGroup);
}

void updateLvgl() {
  const unsigned long now = millis();
  const uint32_t elapsed = now - lastTickMs;
  if (elapsed > 0) {
    lv_tick_inc(elapsed);
    lastTickMs = now;
  }
  lv_timer_handler();
}

lv_group_t* getLvglInputGroup() {
  return inputGroup;
}