#include "Navigation.h"
#include "AppState.h"
#include "Features/HomeFeature.h"
#include "Features/ClockFeature.h"
#include "Features/SystemFeature.h"
#include "Features/GamesFeature.h"
#include "Features/SettingsFeature.h"
#include "Features/Buddy/BuddyFeature.h"

void openPage(int page) {
  currentPage = (AppPage)page;
  switch (page) {
    case HOME:         drawHomeScreen(); break;
    case TIME_PAGE:    drawTimePage(); break;
    case SYSTEM_PAGE:  drawSystemPage(); break;
    case GAMES_PAGE:
      gameScore = 0;
      drawGamesPage();
      break;
    case SETTINGS_PAGE: drawSettingsPage(); break;
    case BUDDY_PAGE:    drawBuddyPage(); break;
  }
  lastRefresh = millis();
}

void selectHomeTile() {
  switch (selectedTile) {
    case 0: openPage(TIME_PAGE); break;
    case 1: openPage(SYSTEM_PAGE); break;
    case 2: openPage(GAMES_PAGE); break;
    case 3: openPage(SETTINGS_PAGE); break;
    case 4: openPage(BUDDY_PAGE); break;
  }
}
