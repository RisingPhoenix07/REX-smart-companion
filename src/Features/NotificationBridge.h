#pragma once
#include <stdint.h>

struct RexNotification {
  char app[24];
  char title[40];
  char body[80];
  char timeStr[12];
  bool valid;
};

constexpr int MAX_NOTIFICATIONS = 8;

void beginNotificationBridge();
void maintainNotificationBridge();
void triggerDesktopMode(const char* modeName);

int getNotificationCount();
int getUnreadNotificationCount();
void markAllNotificationsRead();
void clearAllNotifications();
const RexNotification* getNotificationAt(int index); // 0 = newest

// Shared entry point for Wi-Fi (Ubuntu) and BLE (Android) notifications
void pushExternalNotification(const char* app, const char* title, const char* body, bool isCall = false);