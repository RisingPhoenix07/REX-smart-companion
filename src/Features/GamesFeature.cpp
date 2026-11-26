#include "GamesFeature.h"
#include "../Hardware/Hardware.h"
#include "../Hardware/Display.h"
#include "Config.h"
#include "AppState.h"

void drawGamesPage() {
  tft.fillScreen(BG);
  drawHeader("GAMES");
  drawPanel(15, 45, 290, 150);

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(30, 58);
  tft.print("ENERGY TAP");

  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(30, 87);
  tft.print("Tap the energy button to score.");

  drawGameButton();
  updateGameScore();
  drawHomeButton();
}

void drawGameButton() {
  tft.fillRoundRect(105, 117, 110, 50, 9, PANEL_LIGHT);
  tft.drawRoundRect(105, 117, 110, 50, 9, CYAN);

  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(131, 134);
  tft.print("TAP");
}

void updateGameScore() {
  tft.fillRect(225, 117, 65, 50, PANEL);

  tft.setTextSize(1);
  tft.setTextColor(ICE);
  tft.setCursor(229, 122);
  tft.print("SCORE");

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(244, 140);
  tft.print(gameScore);
}

void incrementGameScore() {
  ++gameScore;
  updateGameScore();
  Serial.print("Game score: ");
  Serial.println(gameScore);
}
