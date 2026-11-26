#include "AppState.h"
#include "Config.h"

AppPage currentPage = HOME;
int selectedTile = 0;
unsigned long bootTime = 0;
unsigned long lastRefresh = 0;
int gameScore = 0;

float smoothX = 0;
float smoothY = 0;
bool smoothingStarted = false;
bool wasTouched = false;
int touchX = 0;
int touchY = 0;
unsigned long lastTapTime = 0;

ButtonState upButton     = {BTN_UP, HIGH, HIGH, 0};
ButtonState downButton   = {BTN_DOWN, HIGH, HIGH, 0};
ButtonState selectButton = {BTN_SELECT, HIGH, HIGH, 0};
ButtonState backButton   = {BTN_BACK, HIGH, HIGH, 0};
