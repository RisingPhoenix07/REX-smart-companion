#include "Display.h"
#include "Hardware.h"
#include "Config.h"
#include "AppState.h"

void drawHeader(const char *pageName) {
  tft.fillRect(0, 0, SCREEN_W, 34, BG);
  tft.setTextSize(1);
  tft.setTextColor(CYAN);
  tft.setCursor(10, 8);
  tft.print("REX");
  tft.setTextColor(WHITE);
  tft.setCursor(42, 8);
  tft.print("// ");
  tft.print(pageName);
  tft.setTextColor(DIM_BLUE);
  tft.setCursor(244, 8);
  tft.print("CORE ONLINE");
  tft.drawFastHLine(8, 29, 304, DIM_BLUE);
}

void drawPanel(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 8, PANEL);
  tft.drawRoundRect(x, y, w, h, 8, BLUE);
}

void drawHomeButton() {
  tft.fillRoundRect(12, 207, 80, 25, 5, PANEL_LIGHT);
  tft.drawRoundRect(12, 207, 80, 25, 5, CYAN);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(30, 216);
  tft.print("< HOME");
}

void drawTile(int x, const char *label, const char *symbol, bool selected) {
  const int y = 185;
  const int w = 58;
  const int h = 45;
  tft.fillRoundRect(x, y, w, h, 6, PANEL);
  const uint16_t borderColor = selected ? CYAN : BLUE;
  tft.drawRoundRect(x, y, w, h, 6, borderColor);
  if (selected) tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 5, DIM_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(x + 23, y + 5);
  tft.print(symbol);
  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(x + 5, y + 30);
  tft.print(label);
}

void drawHomeTiles() {
  drawTile(5,   "TIME",     "T", selectedTile == 0);
  drawTile(68,  "SYSTEM",   "S", selectedTile == 1);
  drawTile(131, "GAMES",    "G", selectedTile == 2);
  drawTile(194, "SETTINGS", "O", selectedTile == 3);
  drawTile(257, "BUDDY",    "B", selectedTile == 4);
}

void updateHomeSelection() {
  if (currentPage == HOME) drawHomeTiles();
}
