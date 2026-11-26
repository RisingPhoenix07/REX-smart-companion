#include "SystemFeature.h"
#include "../Hardware/Hardware.h"
#include "../Hardware/Display.h"
#include "Config.h"
#include "AppState.h"

void drawSystemPage() {
  tft.fillScreen(BG);
  drawHeader("SYSTEM");
  drawPanel(15, 45, 290, 150);

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(27, 57);
  tft.print("ESP32 STATUS");

  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(30, 91);  tft.print("PROCESSOR:");
  tft.setCursor(30, 119); tft.print("CPU FREQUENCY:");
  tft.setCursor(30, 147); tft.print("FREE HEAP:");
  tft.setCursor(30, 175); tft.print("UPTIME:");

  updateSystemPage();
  drawHomeButton();
}

void updateSystemPage() {
  tft.setTextSize(1);

  tft.fillRect(145, 87, 145, 18, PANEL);
  tft.setCursor(145, 91);
  tft.setTextColor(CYAN);
  tft.print("ESP32 DUAL CORE");

  tft.fillRect(145, 115, 145, 18, PANEL);
  tft.setCursor(145, 119);
  tft.setTextColor(CYAN);
  tft.print(ESP.getCpuFreqMHz());
  tft.print(" MHz");

  tft.fillRect(145, 143, 145, 18, PANEL);
  tft.setCursor(145, 147);
  tft.setTextColor(CYAN);
  tft.print(ESP.getFreeHeap() / 1024);
  tft.print(" KB");

  tft.fillRect(145, 171, 145, 18, PANEL);
  tft.setCursor(145, 175);
  tft.setTextColor(CYAN);
  tft.print((millis() - bootTime) / 1000);
  tft.print(" SEC");
}
