#include "SettingsFeature.h"
#include "../Hardware/Hardware.h"
#include "../Hardware/Display.h"
#include "Config.h"

void drawSettingsPage() {
  tft.fillScreen(BG);
  drawHeader("SETTINGS");
  drawPanel(15, 45, 290, 150);

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(28, 58);
  tft.print("CONFIGURATION");

  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(30, 91);  tft.print("DISPLAY:  320 x 240");
  tft.setCursor(30, 115); tft.print("CONTROLLER: ILI9341");
  tft.setCursor(30, 139); tft.print("TOUCH: XPT2046");
  tft.setCursor(30, 163); tft.print("THEME: BLACK / CYAN");

  drawHomeButton();
}
