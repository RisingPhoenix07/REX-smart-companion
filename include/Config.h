#pragma once

// Hardware pin configuration
#define TFT_CS   13
#define TFT_DC   14
#define TFT_RST  27

#define TOUCH_CS  4
#define TOUCH_IRQ 34

#define BTN_UP     25
#define BTN_DOWN   26
#define BTN_SELECT 33
#define BTN_BACK   32

// Display dimensions (landscape)
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

// REX theme colors
#define BG          0x0000
#define PANEL       0x0924
#define PANEL_LIGHT 0x1234
#define CYAN        0x07FF
#define ICE         0xDFFF
#define BLUE        0x3DDF
#define DIM_BLUE    0x2A9F
#define WHITE       0xFFFF
#define GREY        0x8410

// Touch calibration — retained from the working V1.5 firmware
#define RAW_X_MIN 3797
#define RAW_X_MAX 453
#define RAW_Y_MIN 3655
#define RAW_Y_MAX 420

constexpr int MIN_PRESSURE = 250;
constexpr unsigned long TAP_DELAY = 250;
constexpr unsigned long DEBOUNCE_MS = 35;

// Bangladesh Standard Time (UTC+6)
constexpr long GMT_OFFSET_SEC = 6L * 3600L;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr unsigned long WIFI_RETRY_INTERVAL = 15000;

// --- REX Companion Identity ---
constexpr const char* DEVICE_NAME = "REX";
constexpr const char* USER_NAME   = "Rez...";

// Inside include/Config.h
constexpr unsigned long IDLE_QUOTE_INTERVAL_MS = 45000; // Rotate every 45s when idle

constexpr const char* REX_IDLE_QUOTES[] = {
  "All links synced, Rez.",
  "Systems nominal. Ready.",
  "Ubuntu & A13 standing by.",
  "Zero unread alerts.",
  "Keep building. Stay sharp."
};
constexpr size_t REX_IDLE_QUOTE_COUNT = sizeof(REX_IDLE_QUOTES) / sizeof(REX_IDLE_QUOTES[0]);
