#include "ClockFeature.h"
#include <WiFi.h>
#include <time.h>
#include "../Hardware/Hardware.h"
#include "../Hardware/Display.h"
#include "Config.h"
#include "AppState.h"

void drawTimePage() {
  tft.fillScreen(BG);
  drawHeader("TIME");
  drawPanel(15, 45, 290, 150);

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(30, 57);
  tft.print("BANGLADESH TIME");

  tft.setTextSize(1);
  tft.setTextColor(GREY);
  tft.setCursor(30, 83);
  tft.print("NTP NETWORK CLOCK  |  UTC+6");

  updateTimeDisplay();
  drawHomeButton();
}

void updateTimeDisplay() {
  tft.fillRect(30, 96, 260, 14, PANEL);
  tft.setTextSize(1);

  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(CYAN);
    tft.setCursor(30, 99);
    tft.print("WIFI: CONNECTED");
  } else {
    tft.setTextColor(GREY);
    tft.setCursor(30, 99);
    tft.print("WIFI: DISCONNECTED");
  }

  struct tm timeInfo;
  const bool timeAvailable = getLocalTime(&timeInfo, 50);

  tft.fillRect(28, 113, 264, 39, PANEL);

  if (timeAvailable) {
    char clockBuffer[16];
    strftime(clockBuffer, sizeof(clockBuffer), "%H:%M:%S", &timeInfo);

    tft.setTextSize(3);
    tft.setTextColor(ICE);
    tft.setCursor(72, 120);
    tft.print(clockBuffer);

    char dateBuffer[40];
    strftime(dateBuffer, sizeof(dateBuffer), "%A, %d %B %Y", &timeInfo);

    tft.fillRect(30, 155, 260, 15, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(CYAN);
    tft.setCursor(30, 158);
    tft.print(dateBuffer);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(GREY);
    tft.setCursor(91, 124);
    tft.print("SYNCING...");

    tft.fillRect(30, 155, 260, 15, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(GREY);
    tft.setCursor(30, 158);
    tft.print("WAITING FOR NTP TIME");
  }

  const unsigned long elapsed = (millis() - bootTime) / 1000;
  const unsigned long hours = elapsed / 3600;
  const unsigned long minutes = (elapsed % 3600) / 60;
  const unsigned long seconds = elapsed % 60;

  char uptimeBuffer[24];
  snprintf(uptimeBuffer, sizeof(uptimeBuffer),
           "REX UPTIME: %02lu:%02lu:%02lu", hours, minutes, seconds);

  tft.fillRect(30, 174, 260, 13, PANEL);
  tft.setTextSize(1);
  tft.setTextColor(DIM_BLUE);
  tft.setCursor(30, 176);
  tft.print(uptimeBuffer);
}
