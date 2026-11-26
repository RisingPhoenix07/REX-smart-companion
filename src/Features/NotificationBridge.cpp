#include "NotificationBridge.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include "UI/BuddyWidget.h"
#include "UI/HomeScreen.h"

namespace {
  WebServer server(80);
  bool serverStarted = false;

  String pendingDesktopMode = "idle";
  uint32_t desktopModeSeq = 0;

  RexNotification history[MAX_NOTIFICATIONS];
  int totalStored = 0;
  int unreadCount = 0;

  // Lightweight JSON string field extractor (avoids extra library dependencies)
  String extractJsonField(const String& json, const char* key) {
    String pattern = String("\"") + key + "\"";
    int keyIdx = json.indexOf(pattern);
    if (keyIdx < 0) return "";

    int colonIdx = json.indexOf(':', keyIdx + pattern.length());
    if (colonIdx < 0) return "";

    int firstQuote = json.indexOf('"', colonIdx + 1);
    if (firstQuote < 0) return "";

    String result;
    for (int i = firstQuote + 1; i < static_cast<int>(json.length()); i++) {
      char c = json[i];
      if (c == '\\' && i + 1 < static_cast<int>(json.length())) {
        char next = json[i + 1];
        if (next == '"' || next == '\\' || next == '/') {
          result += next;
          i++;
          continue;
        } else if (next == 'n' || next == 'r') {
          result += ' ';
          i++;
          continue;
        }
      }
      if (c == '"') break;
      result += c;
    }
    return result;
  }

  void getCurrentTimeShort(char* buf, size_t len) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2)) {
      int h = timeinfo.tm_hour % 12;
      if (h == 0) h = 12;
      snprintf(buf, len, "%d:%02d%s", h, timeinfo.tm_min, (timeinfo.tm_hour >= 12) ? "p" : "a");
    } else {
      snprintf(buf, len, "now");
    }
  }

  void pushNotification(const String& app, const String& title, const String& body) {
    // Shift older notifications down (index 0 is always newest)
    for (int i = MAX_NOTIFICATIONS - 1; i > 0; i--) {
      history[i] = history[i - 1];
    }

    RexNotification& item = history[0];
    strlcpy(item.app, app.length() > 0 ? app.c_str() : "UBUNTU", sizeof(item.app));
    strlcpy(item.title, title.length() > 0 ? title.c_str() : "Notification", sizeof(item.title));
    strlcpy(item.body, body.c_str(), sizeof(item.body));
    getCurrentTimeShort(item.timeStr, sizeof(item.timeStr));
    item.valid = true;

    if (totalStored < MAX_NOTIFICATIONS) totalStored++;
    if (unreadCount < MAX_NOTIFICATIONS) unreadCount++;

    // Update Home Screen status strip
    char tagBuf[32];
    char msgBuf[96];
    snprintf(tagBuf, sizeof(tagBuf), "ALERT // %s", item.app);
    if (strlen(item.body) > 0) {
      snprintf(msgBuf, sizeof(msgBuf), "%s: %s", item.title, item.body);
    } else {
      snprintf(msgBuf, sizeof(msgBuf), "%s", item.title);
    }

    setHomeStatusPreview(tagBuf, msgBuf);
    refreshAlertsUI(unreadCount);

    // Trigger temporary ALERT expression without overwriting base mood
    triggerBuddyReaction(BUDDY_ALERT, 2600);

    Serial.printf("[NOTIFY] [%s] %s: %s\n", item.app, item.title, item.body);
  }

  void handleNotifyPost() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing body\"}");
      return;
    }

    const String payload = server.arg("plain");
    String app   = extractJsonField(payload, "app");
    String title = extractJsonField(payload, "title");
    String body  = extractJsonField(payload, "body");

    if (title.length() == 0 && body.length() == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"empty notification\"}");
      return;
    }

    pushNotification(app, title, body);
    server.send(200, "application/json", "{\"ok\":true}");
  }

  void handleStatusGet() {
    char resp[160];
    snprintf(
      resp, sizeof(resp),
      "{\"device\":\"REX-GEN2\",\"online\":true,\"unread\":%d,\"heap_kb\":%lu}",
      unreadCount,
      static_cast<unsigned long>(ESP.getFreeHeap() / 1024)
    );
    server.send(200, "application/json", resp);
  }

  void handleModeGet() {
    char json[96];
    snprintf(
      json, sizeof(json),
      "{\"mode\":\"%s\",\"seq\":%lu}",
      pendingDesktopMode.c_str(),
      static_cast<unsigned long>(desktopModeSeq)
    );
    server.send(200, "application/json", json);
  }

  void handleTelemetryPost() {
    const int cpu  = server.hasArg("cpu")  ? server.arg("cpu").toInt()  : 0;
    const int ram  = server.hasArg("ram")  ? server.arg("ram").toInt()  : 0;
    const int temp = server.hasArg("temp") ? server.arg("temp").toInt() : 0;
    const String m = server.hasArg("mode") ? server.arg("mode") : pendingDesktopMode;
    updatePcTelemetry(cpu, ram, temp, m.c_str());
    server.send(200, "text/plain", "OK");
  }
}

void triggerDesktopMode(const char* modeName) {
  pendingDesktopMode = modeName;
  desktopModeSeq++;
  Serial.printf("[MODE] Triggered -> %s (seq %lu)\n", modeName, static_cast<unsigned long>(desktopModeSeq));
}

void beginNotificationBridge() {
  for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
    history[i].valid = false;
  }

  server.on("/notify", HTTP_POST, handleNotifyPost);
  server.on("/status", HTTP_GET, handleStatusGet);
  server.on("/api/mode", HTTP_GET, handleModeGet);
  server.on("/api/telemetry", HTTP_POST, handleTelemetryPost);
}

void maintainNotificationBridge() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!serverStarted) {
    if (MDNS.begin("rex")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[BRIDGE] mDNS active: http://rex.local");
    }
    server.begin();
    serverStarted = true;
    Serial.print("[BRIDGE] Notification server listening at http://");
    Serial.println(WiFi.localIP());
  }

  server.handleClient();
}

int getNotificationCount() {
  return totalStored;
}

int getUnreadNotificationCount() {
  return unreadCount;
}

void markAllNotificationsRead() {
  unreadCount = 0;
  refreshAlertsUI(0);
}

void clearAllNotifications() {
  for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
    history[i].valid = false;
  }
  totalStored = 0;
  unreadCount = 0;
  setHomeStatusPreview("SYSTEM OPTIMIZED", "All alerts cleared. Ready when you are.");
  refreshAlertsUI(0);
}

void pushExternalNotification(const char* app, const char* title, const char* body, bool isCall) {
  pushNotification(String(app), String(title), String(body));
  if (isCall) {
    triggerBuddyReaction(BUDDY_ALERT, 5000);
  }
}

const RexNotification* getNotificationAt(int index) {
  if (index < 0 || index >= totalStored || !history[index].valid) {
    return nullptr;
  }
  return &history[index];
}