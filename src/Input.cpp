#include "Input.h"
#include <Arduino.h>
#include <SPI.h>
#include "Config.h"
#include "AppState.h"
#include "Hardware/Hardware.h"
#include "Hardware/Display.h"
#include "Navigation.h"
#include "Features/GamesFeature.h"
#include "Features/Buddy/BuddyFeature.h"

namespace {
  bool buttonPressed(ButtonState &button) {
    const bool reading = digitalRead(button.pin);
    const unsigned long now = millis();

    if (reading != button.lastReading) {
      button.lastChange = now;
      button.lastReading = reading;
    }

    if (now - button.lastChange >= DEBOUNCE_MS) {
      if (reading != button.stableState) {
        button.stableState = reading;
        if (button.stableState == LOW) {
          return true;
        }
      }
    }
    return false;
  }

  int median3(int a, int b, int c) {
    if (a > b) { int t = a; a = b; b = t; }
    if (b > c) { int t = b; b = c; c = t; }
    if (a > b) { int t = a; a = b; b = t; }
    return b;
  }

  TS_Point getFilteredPoint() {
    TS_Point p1 = ts.getPoint();
    delay(2);
    TS_Point p2 = ts.getPoint();
    delay(2);
    TS_Point p3 = ts.getPoint();

    return TS_Point(
      median3(p1.x, p2.x, p3.x),
      median3(p1.y, p2.y, p3.y),
      median3(p1.z, p2.z, p3.z)
    );
  }
}

void initializeButtons() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  upButton.lastReading = upButton.stableState = digitalRead(BTN_UP);
  downButton.lastReading = downButton.stableState = digitalRead(BTN_DOWN);
  selectButton.lastReading = selectButton.stableState = digitalRead(BTN_SELECT);
  backButton.lastReading = backButton.stableState = digitalRead(BTN_BACK);
}

bool readTouch() {
  if (!ts.touched()) {
    wasTouched = false;
    smoothingStarted = false;
    return false;
  }

  const TS_Point p = getFilteredPoint();
  if (p.z < MIN_PRESSURE) {
    return false;
  }

  int x = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 319);
  int y = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 239);

  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);

  if (!smoothingStarted) {
    smoothX = x;
    smoothY = y;
    smoothingStarted = true;
  } else {
    smoothX = 0.55f * smoothX + 0.45f * x;
    smoothY = 0.55f * smoothY + 0.45f * y;
  }

  touchX = (int)smoothX;
  touchY = (int)smoothY;

  if (!wasTouched && millis() - lastTapTime > TAP_DELAY) {
    wasTouched = true;
    lastTapTime = millis();
    return true;
  }

  wasTouched = true;
  return false;
}

void handleButtons() {
  // Read each button exactly once per loop so page-specific actions are not
  // accidentally consumed by the Home-page checks.
  const bool upPressed = buttonPressed(upButton);
  const bool downPressed = buttonPressed(downButton);
  const bool selectPressed = buttonPressed(selectButton);
  const bool backPressed = buttonPressed(backButton);

  if (currentPage == HOME && upPressed) {
    selectedTile--;
    if (selectedTile < 0) selectedTile = 4;
    updateHomeSelection();
    Serial.print("Selected tile: ");
    Serial.println(selectedTile);
  }

  if (currentPage == HOME && downPressed) {
    selectedTile++;
    if (selectedTile > 4) selectedTile = 0;
    updateHomeSelection();
    Serial.print("Selected tile: ");
    Serial.println(selectedTile);
  }

  if (selectPressed) {
    if (currentPage == HOME) {
      selectHomeTile();
    } else if (currentPage == GAMES_PAGE) {
      incrementGameScore();
    } else if (currentPage == BUDDY_PAGE) {
      buddyPet();
    }
  }

  if (currentPage == BUDDY_PAGE && upPressed) {
    buddyNextMood();
  }

  if (currentPage == BUDDY_PAGE && downPressed) {
    buddyFeed();
  }

  if (backPressed && currentPage != HOME) {
    openPage(HOME);
  }
}

void handleTap(int x, int y) {
  Serial.print("Tap: ");
  Serial.print(x);
  Serial.print(", ");
  Serial.println(y);

  if (currentPage == HOME) {
    if (y >= 180 && y <= 239) {
      if (x >= 5 && x < 63) {
        selectedTile = 0;
        openPage(TIME_PAGE);
      } else if (x >= 68 && x < 126) {
        selectedTile = 1;
        openPage(SYSTEM_PAGE);
      } else if (x >= 131 && x < 189) {
        selectedTile = 2;
        openPage(GAMES_PAGE);
      } else if (x >= 194 && x < 252) {
        selectedTile = 3;
        openPage(SETTINGS_PAGE);
      } else if (x >= 257 && x < 315) {
        selectedTile = 4;
        openPage(BUDDY_PAGE);
      }
    }
    return;
  }

  if (x <= 95 && y >= 195) {
    openPage(HOME);
    return;
  }

  if (currentPage == GAMES_PAGE &&
      x >= 105 && x <= 215 &&
      y >= 117 && y <= 167) {
    incrementGameScore();
  }

  if (currentPage == BUDDY_PAGE) {
    handleBuddyTap(x, y);
  }
}
