#pragma once
#include <lvgl.h>

enum RexPage : uint8_t {
  PAGE_HOME = 0,
  PAGE_APPS,
  PAGE_SYSTEM,
  PAGE_MODES,
  PAGE_POMODORO,
  PAGE_GAMES,
  PAGE_ALERTS,
  PAGE_SETTINGS
};

// Initializes all LVGL screens (Home + Pomodoro + sub-pages) and loads Home
void initializeScreens();

// Navigates to a specific page (resets keypad focus group cleanly)
void navigateToPage(RexPage page);

// Updates the 12-hour clock, date, Wi-Fi/BLE badge, and active Pomodoro timer
void updateHomeDashboard();

// Updates the bottom alert pop-up card on the Home screen
void setHomeStatusPreview(const char* tag, const char* message);

// Updates unread count badge and refreshes the Alerts list
void refreshAlertsUI(int unreadCount);

// Updates live Ubuntu PC telemetry shown in Monitor Mode
void updatePcTelemetry(int cpuPct, int ramPct, int tempC, const char* activeModeName);