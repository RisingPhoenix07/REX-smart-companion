#pragma once
#include <Arduino.h>

enum AppPage {
  HOME,
  TIME_PAGE,
  SYSTEM_PAGE,
  GAMES_PAGE,
  SETTINGS_PAGE,
  BUDDY_PAGE
};

struct ButtonState {
  int pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChange;
};

extern AppPage currentPage;
extern int selectedTile;
extern unsigned long bootTime;
extern unsigned long lastRefresh;
extern int gameScore;

extern float smoothX;
extern float smoothY;
extern bool smoothingStarted;
extern bool wasTouched;
extern int touchX;
extern int touchY;
extern unsigned long lastTapTime;

extern ButtonState upButton;
extern ButtonState downButton;
extern ButtonState selectButton;
extern ButtonState backButton;
