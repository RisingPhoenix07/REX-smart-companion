#include "Connectivity.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "Secrets.h"
#include "Config.h"

#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC 21600      // UTC+6 (Bangladesh Standard Time)
#endif

#ifndef DAYLIGHT_OFFSET_SEC
#define DAYLIGHT_OFFSET_SEC 0
#endif

namespace {
  unsigned long lastReconnectMs = 0;
  bool ntpStarted = false;
}

void beginConnectivity() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastReconnectMs = millis();
  Serial.printf("[WIFI] Connecting to %s...\n", WIFI_SSID);
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpStarted) {
      Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
      ntpStarted = true;
    }
    return;
  }

  if (millis() - lastReconnectMs >= 20000) {
    lastReconnectMs = millis();
    Serial.println("[WIFI] Reconnecting...");
    WiFi.reconnect();
  }
}

void maintainConnectivity() {
  maintainWiFi();
}