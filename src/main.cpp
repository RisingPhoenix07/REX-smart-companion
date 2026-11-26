#include <Arduino.h>
#include <lvgl.h>
#include "Config.h"
#include "AppState.h"
#include "Hardware/Hardware.h"
#include "Hardware/LvglDriver.h"
#include "UI/BuddyWidget.h"
#include "UI/HomeScreen.h"
#include "Features/NotificationBridge.h"
#include "Connectivity.h"
#include "Input.h"
#include "Features/BleBridge.h"

void setup() {
  Serial.begin(115200);
  initializeButtons();
  initializeHardware();
  bootTime = millis();
  beginBleBridge();
  beginConnectivity();
 

  initializeLvgl();
  initializeScreens();
  beginNotificationBridge();

  Serial.println("REX Generation 2 — Stage 3 Unified Home Dashboard Ready");
}

void loop() {
  maintainWiFi();
  updateBuddyWidget();
  updateLvgl();
  maintainNotificationBridge();
  maintainBleBridge();

  const unsigned long now = millis();
  if (now - lastRefresh >= 1000) {
    lastRefresh = now;
    updateHomeDashboard();
  }

  delay(5);
}