#include "BleBridge.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <sys/time.h>
#include "NotificationBridge.h"
#include "UI/BuddyWidget.h"
#include "UI/HomeScreen.h"
#include "Config.h"

namespace {
  // Nordic UART Service (NUS) UUIDs — compatible with Gadgetbridge, MacroDroid & nRF Connect
  constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  constexpr const char* NUS_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // Phone -> REX
  constexpr const char* NUS_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // REX -> Phone

  NimBLEServer* pServer = nullptr;
  NimBLECharacteristic* pTxChar = nullptr;
  volatile bool phoneConnected = false;
  bool lastPhoneConnected = false;

  // Thread-safe queue between NimBLE callback task and main LVGL loop() task
  constexpr size_t BLE_QUEUE_SIZE = 6;
  constexpr size_t BLE_MSG_MAX_LEN = 320;
  char msgQueue[BLE_QUEUE_SIZE][BLE_MSG_MAX_LEN];
  volatile size_t qHead = 0;
  volatile size_t qTail = 0;
  portMUX_TYPE bleMux = portMUX_INITIALIZER_UNLOCKED;

  String rxBuffer;

  void enqueueBleMessage(const String& msg) {
    if (msg.length() == 0) return;
    portENTER_CRITICAL(&bleMux);
    size_t nextHead = (qHead + 1) % BLE_QUEUE_SIZE;
    if (nextHead != qTail) {
      strncpy(msgQueue[qHead], msg.c_str(), BLE_MSG_MAX_LEN - 1);
      msgQueue[qHead][BLE_MSG_MAX_LEN - 1] = '\0';
      qHead = nextHead;
    }
    portEXIT_CRITICAL(&bleMux);
  }

  bool dequeueBleMessage(String& outMsg) {
    bool hasMsg = false;
    portENTER_CRITICAL(&bleMux);
    if (qTail != qHead) {
      outMsg = String(msgQueue[qTail]);
      qTail = (qTail + 1) % BLE_QUEUE_SIZE;
      hasMsg = true;
    }
    portEXIT_CRITICAL(&bleMux);
    return hasMsg;
  }

  void sendBleResponse(const char* json) {
    if (pTxChar && phoneConnected) {
      std::string payload(json);
      pTxChar->setValue(payload);
      pTxChar->notify();
    }
  }

  String extractField(const String& json, const char* key) {
    String pattern = String("\"") + key + "\"";
    int keyIdx = json.indexOf(pattern);
    if (keyIdx < 0) return "";
    int colonIdx = json.indexOf(':', keyIdx + pattern.length());
    if (colonIdx < 0) return "";
    int firstQuote = json.indexOf('"', colonIdx + 1);
    if (firstQuote < 0) return "";

    String out;
    for (int i = firstQuote + 1; i < static_cast<int>(json.length()); i++) {
      char c = json[i];
      if (c == '\\' && i + 1 < static_cast<int>(json.length())) {
        char n = json[i + 1];
        if (n == '"' || n == '\\' || n == '/') { out += n; i++; continue; }
        if (n == 'n' || n == 'r') { out += ' '; i++; continue; }
      }
      if (c == '"') break;
      out += c;
    }
    return out;
  }

  long extractLongField(const String& json, const char* key, long fallback = 0) {
    String pattern = String("\"") + key + "\"";
    int keyIdx = json.indexOf(pattern);
    if (keyIdx < 0) return fallback;
    int colonIdx = json.indexOf(':', keyIdx + pattern.length());
    if (colonIdx < 0) return fallback;

    int start = colonIdx + 1;
    while (start < static_cast<int>(json.length()) &&
           (json[start] == ' ' || json[start] == '"')) {
      start++;
    }

    int end = start;
    while (end < static_cast<int>(json.length()) &&
           (isDigit(json[end]) || json[end] == '-')) {
      end++;
    }

    if (end == start) return fallback;
    return json.substring(start, end).toInt();
  }

  void applyTimezoneHours(long tzHours) {
    char tzEnv[24];
    snprintf(tzEnv, sizeof(tzEnv), "UTC%+ld", -tzHours);
    setenv("TZ", tzEnv, 1);
    tzset();
  }

  void syncTimeFromPhone(long epochSec, long tzHours) {
    if (epochSec <= 1000000000L) return;

    struct timeval tv;
    tv.tv_sec = epochSec;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    applyTimezoneHours(tzHours);

    Serial.printf("[BLE TIME] Synced from phone: epoch=%ld, tz=%+ld\n", epochSec, tzHours);
    setHomeStatusPreview("TIME SYNC", "Clock synced with Galaxy A13");
  }

  void handleRemoteCommand(const String& cmd, const String& arg) {
    Serial.printf("[BLE CMD] cmd=%s arg=%s\n", cmd.c_str(), arg.c_str());

    if (cmd == "emote") {
      if (arg == "happy") triggerBuddyReaction(BUDDY_HAPPY, 3000);
      else if (arg == "alert") triggerBuddyReaction(BUDDY_ALERT, 3000);
      else triggerBuddyReaction(BUDDY_IDLE, 2000);
    }
    else if (cmd == "status") {
      setHomeStatusPreview("PHONE CMD", arg.c_str());
      triggerBuddyReaction(BUDDY_HAPPY, 1500);
    }
    else if (cmd == "ping") {
      setHomeStatusPreview("PING", "Galaxy A13 pinged REX!");
      triggerBuddyReaction(BUDDY_HAPPY, 2500);
      sendBleResponse("{\"t\":\"pong\",\"status\":\"ok\"}\n");
    }
    else if (cmd == "note") {
      pushExternalNotification("NOTE", "Phone Note", arg.c_str(), false);
    }
  }

  void handlePhoneSensor(const String& sensor, const String& value) {
    Serial.printf("[BLE SENSOR] %s = %s\n", sensor.c_str(), value.c_str());

    if (sensor == "steps") {
      char buf[48];
      snprintf(buf, sizeof(buf), "Steps today: %s", value.c_str());
      setHomeStatusPreview("ACTIVITY", buf);
    }
    else if (sensor == "battery") {
      char buf[48];
      snprintf(buf, sizeof(buf), "Galaxy A13 at %s%%", value.c_str());
      setHomeStatusPreview("PHONE BAT", buf);
      if (value.toInt() > 0 && value.toInt() <= 15) {
        triggerBuddyReaction(BUDDY_ALERT, 3000);
      }
    }
    else if (sensor == "location") {
      setHomeStatusPreview("ZONE", value.c_str());
      triggerBuddyReaction(BUDDY_HAPPY, 2000);
    }
    else if (sensor == "facedown") {
      setHomeStatusPreview("FOCUS", "Phone face-down: Focus active");
      triggerBuddyReaction(BUDDY_HAPPY, 2000);
    }
    else if (sensor == "light") {
      char buf[48];
      snprintf(buf, sizeof(buf), "Ambient light: %s lux", value.c_str());
      setHomeStatusPreview("LIGHT", buf);
    }
  }

  // Always runs safely on the main LVGL loop() thread via maintainBleBridge()
  void processIncomingBleMessage(String msg) {
    // Strip non-printable control characters like Espruino's \x10 (DLE) prefix
    String clean;
    clean.reserve(msg.length());
    for (size_t i = 0; i < msg.length(); i++) {
      if (static_cast<unsigned char>(msg[i]) >= 0x20) {
        clean += msg[i];
      }
    }
    msg = clean;
    msg.trim();
    if (msg.length() == 0) return;

    // Strip Gadgetbridge wrapper GB(...) if present
    if (msg.startsWith("GB(") && msg.endsWith(")")) {
      msg = msg.substring(3, msg.length() - 1);
    }

    Serial.printf("[BLE RX] %s\n", msg.c_str());

    // Gadgetbridge Espruino syntax: setTime(1794898860.45);
    int setTimeIdx = msg.indexOf("setTime(");
    if (setTimeIdx >= 0) {
      int openParen = msg.indexOf('(', setTimeIdx);
      int closeParen = msg.indexOf(')', openParen);
      if (openParen > 0 && closeParen > openParen) {
        long ts = msg.substring(openParen + 1, closeParen).toInt();
        syncTimeFromPhone(ts, GMT_OFFSET_SEC / 3600);
      }
      return;
    }

    // Gadgetbridge follow-up timezone command: E.setTimeZone(6);
    int tzIdx = msg.indexOf("E.setTimeZone(");
    if (tzIdx >= 0) {
      int openParen = msg.indexOf('(', tzIdx);
      int closeParen = msg.indexOf(')', openParen);
      if (openParen > 0 && closeParen > openParen) {
        long tz = msg.substring(openParen + 1, closeParen).toInt();
        applyTimezoneHours(tz);
        Serial.printf("[BLE TIME] Timezone updated: UTC%+ld\n", tz);
      }
      return;
    }

    const String type = extractField(msg, "t");

    // 1. JSON time sync packet: {"t":"time","ts":1794898860,"tz":6}
    if (type == "time") {
      long ts = extractLongField(msg, "ts", 0);
      long tz = extractLongField(msg, "tz", GMT_OFFSET_SEC / 3600);
      syncTimeFromPhone(ts, tz);
      return;
    }

    // 2. Phone -> REX remote control commands
    if (type == "rex") {
      String cmd = extractField(msg, "cmd");
      String arg = extractField(msg, "arg");
      if (cmd.length() > 0) {
        handleRemoteCommand(cmd, arg);
      }
      return;
    }

    // 3. Phone Sensor -> REX stream
    if (type == "sensor") {
      String s = extractField(msg, "s");
      String v = extractField(msg, "v");
      if (s.length() > 0) {
        handlePhoneSensor(s, v);
      }
      return;
    }

    // 4. Incoming phone calls
    if (type == "call") {
      const String cmd = extractField(msg, "cmd");
      if (cmd == "incoming") {
        String caller = extractField(msg, "name");
        if (caller.length() == 0) caller = extractField(msg, "number");
        if (caller.length() == 0) caller = "Incoming Call";
        pushExternalNotification("CALL", caller.c_str(), "Incoming phone call on Galaxy A13", true);
      }
      return;
    }

    // 5. Standard JSON or Gadgetbridge notify packet
    String app = extractField(msg, "app");
    if (app.length() == 0) app = extractField(msg, "src");
    if (app.length() == 0) app = "PHONE";

    String title = extractField(msg, "title");
    if (title.length() == 0) title = extractField(msg, "sender");

    String body = extractField(msg, "body");
    if (body.length() == 0) body = extractField(msg, "subject");

    if (title.length() > 0 || body.length() > 0) {
      pushExternalNotification(app.c_str(), title.c_str(), body.c_str(), false);
    }
  }

  class RexServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override {
      LV_UNUSED(s);
      LV_UNUSED(connInfo);
      phoneConnected = true;
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override {
      LV_UNUSED(s);
      LV_UNUSED(connInfo);
      LV_UNUSED(reason);
      phoneConnected = false;
      NimBLEDevice::getAdvertising()->start();
    }
  };

  class RexRxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
      LV_UNUSED(connInfo);
      std::string val = pChar->getValue();
      if (val.empty()) return;

      for (char c : val) {
        if (c == '\n' || c == '\r' || c == ';') {
          if (rxBuffer.length() > 0) {
            enqueueBleMessage(rxBuffer);
            rxBuffer = "";
          }
        } else if (rxBuffer.length() < BLE_MSG_MAX_LEN - 1) {
          rxBuffer += c;
        }
      }

      if (rxBuffer.endsWith("}") || rxBuffer.endsWith("})")) {
        enqueueBleMessage(rxBuffer);
        rxBuffer = "";
      }
    }
  };
}

void beginBleBridge() {
  if (NimBLEDevice::isInitialized()) {
    Serial.println("[BLE] NimBLE already initialized.");
    return;
  }

  NimBLEDevice::init("REX-WATCH");
  NimBLEDevice::setPower(3); // +3 dBm safe TX power alongside Wi-Fi

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new RexServerCallbacks());

  NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);

  pTxChar = pService->createCharacteristic(
    NUS_TX_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  NimBLECharacteristic* pRxChar = pService->createCharacteristic(
    NUS_RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pRxChar->setCallbacks(new RexRxCallbacks());

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.setName("REX-WATCH");
  pAdv->setAdvertisementData(advData);

  NimBLEAdvertisementData scanData;
  scanData.addServiceUUID(NUS_SERVICE_UUID);
  pAdv->setScanResponseData(scanData);

  if (pAdv->start()) {
    Serial.printf("[BLE] Advertising started: REX-WATCH (MAC: %s)\n",
                  NimBLEDevice::getAddress().toString().c_str());
  } else {
    Serial.println("[BLE] ERROR: Advertising failed to start!");
  }
}

void maintainBleBridge() {
  if (phoneConnected != lastPhoneConnected) {
    lastPhoneConnected = phoneConnected;
    if (phoneConnected) {
      Serial.println("[BLE] Galaxy A13 connected!");
      char msg[64];
      snprintf(msg, sizeof(msg), "Galaxy A13 paired, %s.", USER_NAME);
      setHomeStatusPreview("BLE LINKED", msg);
      triggerBuddyReaction(BUDDY_HAPPY, 2000);
    } else {
      Serial.println("[BLE] Phone disconnected, resuming advertising.");
      setHomeStatusPreview("BLE", "Phone disconnected. Advertising...");
    }
  }

  // Drain queued BLE packets safely on the main LVGL loop() thread
  String queuedMsg;
  while (dequeueBleMessage(queuedMsg)) {
    processIncomingBleMessage(queuedMsg);
  }
}

bool isBlePhoneConnected() {
  return phoneConnected;
}