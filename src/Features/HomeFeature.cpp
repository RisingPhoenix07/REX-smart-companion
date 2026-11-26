#include "HomeFeature.h"
#include "../Hardware/Hardware.h"
#include "../Hardware/Display.h"
#include "Config.h"

void drawHomeScreen() {
  tft.fillScreen(BG);
  drawHeader("HOME");

  drawPanel(35, 45, 250, 119);

  tft.setTextColor(CYAN);
  tft.setTextSize(3);
  tft.setCursor(119, 73);
  tft.print("REX");

  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(94, 112);
  tft.print("PERSONAL AI SYSTEM");

  tft.setTextColor(DIM_BLUE);
  tft.setCursor(111, 140);
  tft.print("GENERATION 2");

  drawHomeTiles();
}
