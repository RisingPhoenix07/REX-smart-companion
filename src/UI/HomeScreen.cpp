#include "HomeScreen.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "BuddyWidget.h"
#include "Hardware/LvglDriver.h"
#include "Features/NotificationBridge.h"
#include "AppState.h"
#include "Config.h"
#include "PomodoroScene.h"
#include "Features/BleBridge.h"

namespace {
  lv_obj_t* homeScr     = nullptr;
  lv_obj_t* systemScr   = nullptr;
  lv_obj_t* modesScr    = nullptr;
  lv_obj_t* pomodoroScr = nullptr;
  lv_obj_t* gamesScr    = nullptr;
  lv_obj_t* alertsScr   = nullptr;
  lv_obj_t* settingsScr = nullptr;

  lv_obj_t* buddyWidget    = nullptr;
  lv_obj_t* timeLabel      = nullptr;
  lv_obj_t* ampmDateLabel  = nullptr;
  lv_obj_t* netBadge       = nullptr;
  lv_obj_t* netDot         = nullptr;
  lv_obj_t* netTitleLabel  = nullptr;
  lv_obj_t* netSubLabel    = nullptr;
  lv_obj_t* statusTagLabel = nullptr;
  lv_obj_t* statusMsgLabel = nullptr;
  lv_obj_t* navBtns[6]     = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  lv_obj_t* alertsNavLabel = nullptr;

  // Dedicated Pomodoro Page Widgets
  lv_obj_t* pomoArc        = nullptr;
  lv_obj_t* pomoBigTimeLbl = nullptr;
  lv_obj_t* pomoPhaseLbl   = nullptr;
  lv_obj_t* pomoPropLbl    = nullptr;
  lv_obj_t* pomoStartBtn   = nullptr;
  lv_obj_t* pomoStartLbl   = nullptr;
  lv_obj_t* pomoSkipBtn    = nullptr;
  lv_obj_t* pomoResetBtn   = nullptr;
  lv_obj_t* pomoBackBtn    = nullptr;

  lv_obj_t* sysInfoLabel     = nullptr;
  lv_obj_t* modeBtnProject   = nullptr;
  lv_obj_t* modeBtnStudy     = nullptr;
  lv_obj_t* modeBtnMonitor   = nullptr;
  lv_obj_t* monitorStatsLbl  = nullptr;
  lv_obj_t* gameScoreLabel   = nullptr;
  lv_obj_t* alertsListBox    = nullptr;
  lv_obj_t* alertsClearBtn   = nullptr;
  lv_obj_t* settingsBackBtn  = nullptr;
  lv_obj_t* settingsMoodBtn  = nullptr;
  lv_obj_t* settingsMoodLbl  = nullptr;
  int lvglGameScore          = 0;

  // Pomodoro State Machine (25m Focus / 5m Break, alternating Book & Laptop)
  constexpr uint32_t REX_POMO_FOCUS_SECONDS = 25UL * 60UL;
  constexpr uint32_t REX_POMO_BREAK_SECONDS = 5UL * 60UL;

  bool pomoRunning = false;
  bool pomoIsBreak = false;
  uint32_t pomoTotalSec = REX_POMO_FOCUS_SECONDS;
  uint32_t pomoRemainingSec = REX_POMO_FOCUS_SECONDS;
  uint32_t pomoLastTickMs = 0;
  int pomoSessionCount = 1;

  constexpr BuddyMood SELECTABLE_MOODS[] = {
    BUDDY_IDLE, BUDDY_HAPPY, BUDDY_CURIOUS, BUDDY_SLEEPY, BUDDY_ALERT, BUDDY_THINKING, BUDDY_STUDYING
  };
  constexpr int SELECTABLE_MOOD_COUNT = sizeof(SELECTABLE_MOODS) / sizeof(SELECTABLE_MOODS[0]);

  int findMoodOptionIndex(BuddyMood m) {
    for (int i = 0; i < SELECTABLE_MOOD_COUNT; i++) {
      if (SELECTABLE_MOODS[i] == m) return i;
    }
    return 0;
  }

  int pcCpuPct = 0;
  int pcRamPct = 0;
  int pcTempC  = 0;
  RexPage activePage = PAGE_HOME;
  bool lastAnyOnline = false;
  bool lastWifiState = false;
  unsigned long lastNavMs = 0;

  const char* DAYS_UPPER[7] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"
  };

  const char* MONTHS_UPPER[12] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
  };

  constexpr const char* POMO_FOCUS_QUOTES[] = {
    "\"Deep focus unlocks flow.\"",
    "\"One block at a time.\"",
    "\"Stay sharp, build great things.\"",
    "\"Code clean, think clear.\"",
    "\"Progress over perfection.\"",
    "\"REX is locked in with you.\"",
    "\"Silence the noise, ship it.\"",
    "\"Small steps, massive wins.\""
  };
  constexpr int POMO_FOCUS_QUOTE_COUNT = sizeof(POMO_FOCUS_QUOTES) / sizeof(POMO_FOCUS_QUOTES[0]);

  constexpr const char* POMO_BREAK_QUOTES[] = {
    "\"Recharge your core!\"",
    "\"Rest eyes, stretch out.\"",
    "\"Hydrate & reset your mind.\"",
    "\"Great sprint! Take 5.\"",
    "\"Docking for quick energy.\""
  };
  constexpr int POMO_BREAK_QUOTE_COUNT = sizeof(POMO_BREAK_QUOTES) / sizeof(POMO_BREAK_QUOTES[0]);

  constexpr const char* POMO_IDLE_QUOTES[] = {
    "\"Ready when you are.\"",
    "\"Hit START to enter focus.\"",
    "\"Systems primed for study.\""
  };
  constexpr int POMO_IDLE_QUOTE_COUNT = sizeof(POMO_IDLE_QUOTES) / sizeof(POMO_IDLE_QUOTES[0]);

  int lastQuoteStateKey = -1;
  const char* activePomoQuote = POMO_IDLE_QUOTES[0];

  const char* getRandomPomoQuote(bool running, bool isBreak, int sessionNum) {
    // Only pick a new random quote when phase, running state, or session changes
    const int stateKey = (sessionNum << 2) | (isBreak ? 2 : 0) | (running ? 1 : 0);
    if (stateKey != lastQuoteStateKey) {
      lastQuoteStateKey = stateKey;
      if (!running) {
        activePomoQuote = POMO_IDLE_QUOTES[esp_random() % POMO_IDLE_QUOTE_COUNT];
      } else if (isBreak) {
        activePomoQuote = POMO_BREAK_QUOTES[esp_random() % POMO_BREAK_QUOTE_COUNT];
      } else {
        activePomoQuote = POMO_FOCUS_QUOTES[esp_random() % POMO_FOCUS_QUOTE_COUNT];
      }
    }
    return activePomoQuote;
  }

  void syncPomodoroUI() {
    const bool useLaptop = (pomoSessionCount % 2 == 0);
    const uint32_t mins = pomoRemainingSec / 60;
    const uint32_t secs = pomoRemainingSec % 60;
    const uint32_t accentHex = !pomoRunning ? 0xFF9E00 : (pomoIsBreak ? 0x00FF9C : 0x00FFFF);

    if (pomoBigTimeLbl) {
      lv_label_set_text_fmt(pomoBigTimeLbl, "%02lu:%02lu",
                            static_cast<unsigned long>(mins),
                            static_cast<unsigned long>(secs));
    }

    if (pomoArc) {
      const int32_t pct = (pomoTotalSec > 0)
        ? static_cast<int32_t>((pomoRemainingSec * 100UL) / pomoTotalSec)
        : 0;
      lv_arc_set_value(pomoArc, pct);
      lv_obj_set_style_arc_color(pomoArc, lv_color_hex(accentHex), LV_PART_INDICATOR);
    }

    if (pomoPhaseLbl) {
      if (pomoIsBreak) {
        lv_label_set_text_fmt(pomoPhaseLbl, "BREAK TIME // #%d DONE", pomoSessionCount);
      } else {
        lv_label_set_text_fmt(pomoPhaseLbl, "FOCUS SESSION #%d", pomoSessionCount);
      }
      lv_obj_set_style_text_color(pomoPhaseLbl, lv_color_hex(accentHex), 0);
    }

    if (pomoPropLbl) {
      lv_label_set_text(pomoPropLbl, getRandomPomoQuote(pomoRunning, pomoIsBreak, pomoSessionCount));
      lv_obj_set_style_text_color(pomoPropLbl, lv_color_hex(pomoIsBreak ? 0x00FF9C : 0x7CC4FA), 0);
    }

    if (!pomoRunning) {
      setPomodoroScenePose(POMO_POSE_IDLE_DESK);
    } else if (pomoIsBreak) {
      setPomodoroScenePose(POMO_POSE_HEADING_BACK);
    } else {
      setPomodoroScenePose(useLaptop ? POMO_POSE_STUDY_LAPTOP : POMO_POSE_STUDY_BOOK);
    }

    if (pomoStartLbl) {
      lv_label_set_text(pomoStartLbl, pomoRunning ? "PAUSE" : "START");
    }
  }

  void startPomodoroFocus(bool incrementSession) {
    if (incrementSession) {
      pomoSessionCount++;
    }
    pomoRunning = true;
    pomoIsBreak = false;
    pomoTotalSec = REX_POMO_FOCUS_SECONDS;
    pomoRemainingSec = REX_POMO_FOCUS_SECONDS;
    pomoLastTickMs = millis();

    const bool useLaptop = (pomoSessionCount % 2 == 0);
    setBuddyStudyProp(useLaptop);
    setBuddyBaseMood(BUDDY_STUDYING);
    triggerBuddyReaction(BUDDY_HAPPY, 1600);
    syncPomodoroUI();
  }

  void startPomodoroBreak() {
    pomoRunning = true;
    pomoIsBreak = true;
    pomoTotalSec = REX_POMO_BREAK_SECONDS;
    pomoRemainingSec = REX_POMO_BREAK_SECONDS;
    pomoLastTickMs = millis();

    setBuddyBaseMood(BUDDY_BREAK);
    triggerBuddyReaction(BUDDY_HAPPY, 2400);
    syncPomodoroUI();
  }

  void resetPomodoro() {
    pomoRunning = false;
    pomoIsBreak = false;
    pomoTotalSec = REX_POMO_FOCUS_SECONDS;
    pomoRemainingSec = REX_POMO_FOCUS_SECONDS;
    if (getBuddyBaseMood() == BUDDY_STUDYING || getBuddyBaseMood() == BUDDY_BREAK) {
      setBuddyBaseMood(BUDDY_IDLE);
    }
    syncPomodoroUI();
  }

  void updatePomodoroTimer() {
    if (!pomoRunning) return;

    const uint32_t now = millis();
    if (now - pomoLastTickMs >= 1000) {
      const uint32_t elapsedSec = (now - pomoLastTickMs) / 1000;
      pomoLastTickMs += elapsedSec * 1000;

      if (pomoRemainingSec > elapsedSec) {
        pomoRemainingSec -= elapsedSec;
      } else {
        pomoRemainingSec = 0;
        if (!pomoIsBreak) {
          startPomodoroBreak();
          setHomeStatusPreview("POMODORO DONE", "25m complete! REX is taking a 5m break.");
        } else {
          startPomodoroFocus(true);
          setHomeStatusPreview("BREAK OVER", "Next Pomodoro started with REX!");
        }
        return;
      }

      if (activePage == PAGE_POMODORO) {
        syncPomodoroUI();
      } else if (activePage == PAGE_HOME && statusTagLabel && statusMsgLabel) {
        const uint32_t mins = pomoRemainingSec / 60;
        const uint32_t secs = pomoRemainingSec % 60;
        lv_label_set_text(statusTagLabel, pomoIsBreak ? "POMODORO // BREAK" : "POMODORO // STUDY");
        lv_obj_set_style_text_color(statusTagLabel, lv_color_hex(pomoIsBreak ? 0x00FF9C : 0x00FFFF), 0);
        lv_label_set_text_fmt(statusMsgLabel, "%02lu:%02lu remaining — REX is %s",
                              static_cast<unsigned long>(mins),
                              static_cast<unsigned long>(secs),
                              pomoIsBreak ? "on break." : "studying with you.");
      }
    }
  }

  void onPomoStartPauseClicked(lv_event_t* e) {
    LV_UNUSED(e);
    pomoRunning = !pomoRunning;
    pomoLastTickMs = millis();

    if (pomoRunning) {
      const bool useLaptop = (pomoSessionCount % 2 == 0);
      setBuddyStudyProp(useLaptop);
      setBuddyBaseMood(pomoIsBreak ? BUDDY_BREAK : BUDDY_STUDYING);
    } else {
      setBuddyBaseMood(BUDDY_IDLE);
    }
    syncPomodoroUI();
  }

  void onPomoSkipClicked(lv_event_t* e) {
    LV_UNUSED(e);
    if (!pomoIsBreak) {
      startPomodoroBreak();
    } else {
      startPomodoroFocus(true);
    }
  }

  void onPomoResetClicked(lv_event_t* e) {
    LV_UNUSED(e);
    resetPomodoro();
  }

  bool pendingNavActive = false;
  RexPage pendingNavTarget = PAGE_HOME;

  void executeDeferredNav(void* userData) {
    LV_UNUSED(userData);
    pendingNavActive = false;
    navigateToPage(pendingNavTarget);
  }

  void safeNavigate(RexPage page) {
    const unsigned long now = millis();
    if (now - lastNavMs < 200 || pendingNavActive) return;
    lastNavMs = now;
    pendingNavTarget = page;
    pendingNavActive = true;
    lv_async_call(executeDeferredNav, nullptr);
  }

  void onNavBtnClicked(lv_event_t* e) {
    const uintptr_t target = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    safeNavigate(static_cast<RexPage>(target));
  }

  void onBackToHomeClicked(lv_event_t* e) {
    LV_UNUSED(e);
    safeNavigate(PAGE_HOME);
  }

  void onSubPageKey(lv_event_t* e) {
    if (lv_event_get_key(e) == LV_KEY_ESC) {
      safeNavigate(PAGE_HOME);
    }
  }

  void onModeSelectClicked(lv_event_t* e) {
    const char* mode = static_cast<const char*>(lv_event_get_user_data(e));
    triggerDesktopMode(mode);

    if (strcmp(mode, "study") == 0) {
      if (!pomoRunning) {
        startPomodoroFocus(false);
      }
      safeNavigate(PAGE_POMODORO);
    } else if (strcmp(mode, "project") == 0) {
      resetPomodoro();
      triggerBuddyReaction(BUDDY_HAPPY, 2000);
      safeNavigate(PAGE_HOME);
    } else if (strcmp(mode, "monitor") == 0 && monitorStatsLbl) {
      lv_label_set_text(monitorStatsLbl, "Waiting for Ubuntu telemetry stream...");
    }
  }

  void onGameTapClicked(lv_event_t* e) {
    LV_UNUSED(e);
    lvglGameScore++;
    if (gameScoreLabel) {
      lv_label_set_text_fmt(gameScoreLabel, "SCORE: %d", lvglGameScore);
    }
  }

  void onSettingsMoodCycle(lv_event_t* e) {
    LV_UNUSED(e);
    const int curIdx = findMoodOptionIndex(getBuddyBaseMood());
    const int nextIdx = (curIdx + 1) % SELECTABLE_MOOD_COUNT;
    const BuddyMood nextMood = SELECTABLE_MOODS[nextIdx];
    setBuddyBaseMood(nextMood);
    if (settingsMoodLbl) {
      lv_label_set_text_fmt(settingsMoodLbl, "MOOD: %s", getBuddyMoodName(nextMood));
    }
  }

  void onClearAlertsClicked(lv_event_t* e) {
    LV_UNUSED(e);
    clearAllNotifications();
    setHomeStatusPreview("SYSTEM OPTIMIZED", "Inbox cleared. Ready when you are.");
  }

  lv_obj_t* createSubPageShell(const char* pageTitle, lv_obj_t** outBackBtn) {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, pageTitle);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 10);

    lv_obj_t* backBtn = lv_button_create(scr);
    lv_obj_set_size(backBtn, 84, 30);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(backBtn, lv_color_hex(0x0A1E32), 0);
    lv_obj_set_style_border_color(backBtn, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(backBtn, 1, 0);
    lv_obj_set_style_outline_color(backBtn, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(backBtn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(backBtn, 2, LV_STATE_FOCUSED);

    lv_obj_add_event_cb(backBtn, onBackToHomeClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(backBtn, onSubPageKey, LV_EVENT_KEY, nullptr);

    lv_obj_t* lbl = lv_label_create(backBtn);
    lv_label_set_text(lbl, "< HOME");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);

    if (outBackBtn) *outBackBtn = backBtn;
    return scr;
  }

  void buildHomeScreen() {
    homeScr = lv_obj_create(nullptr);
    lv_obj_remove_flag(homeScr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(homeScr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(homeScr, LV_OPA_COVER, 0);

    // 1. Top-Left: 12-Hour Clock + AM/PM & Date
    timeLabel = lv_label_create(homeScr);
    lv_label_set_text(timeLabel, "--:--");
    lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(timeLabel, 12, 6);

    ampmDateLabel = lv_label_create(homeScr);
    lv_label_set_text(ampmDateLabel, "SYNCING CLOCK...");
    lv_obj_set_style_text_font(ampmDateLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ampmDateLabel, lv_color_hex(0x7CC4FA), 0);
    lv_obj_set_pos(ampmDateLabel, 12, 36);

    // 2. Top-Right: Connectivity Status Pill
    netBadge = lv_obj_create(homeScr);
    lv_obj_remove_flag(netBadge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(netBadge, 114, 42);
    lv_obj_set_pos(netBadge, 194, 8);
    lv_obj_set_style_bg_color(netBadge, lv_color_hex(0x071726), 0);
    lv_obj_set_style_border_color(netBadge, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(netBadge, 1, 0);
    lv_obj_set_style_radius(netBadge, 8, 0);
    lv_obj_set_style_pad_all(netBadge, 0, 0);

    netDot = lv_obj_create(netBadge);
    lv_obj_set_size(netDot, 8, 8);
    lv_obj_set_pos(netDot, 8, 9);
    lv_obj_set_style_radius(netDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(netDot, lv_color_hex(0xFF9E00), 0);
    lv_obj_set_style_border_width(netDot, 0, 0);

    netTitleLabel = lv_label_create(netBadge);
    lv_label_set_text(netTitleLabel, "OFFLINE");
    lv_obj_set_style_text_font(netTitleLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(netTitleLabel, lv_color_hex(0xFF9E00), 0);
    lv_obj_set_pos(netTitleLabel, 21, 6);

    netSubLabel = lv_label_create(netBadge);
    lv_label_set_text(netSubLabel, "BLE Ready");
    lv_obj_set_style_text_font(netSubLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(netSubLabel, lv_color_hex(0x9FB3C8), 0);
    lv_obj_set_pos(netSubLabel, 8, 22);

    // 3. Center: Reusable Animated Buddy Face (permanently parented to homeScr)
    buddyWidget = createBuddyWidget(homeScr);
    lv_obj_align(buddyWidget, LV_ALIGN_TOP_MID, 0, 56);

    // 4. Status / Notification Preview Strip
    lv_obj_t* statusCard = lv_obj_create(homeScr);
    lv_obj_remove_flag(statusCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(statusCard, 296, 38);
    lv_obj_set_pos(statusCard, 12, 150);
    lv_obj_set_style_bg_color(statusCard, lv_color_hex(0x061320), 0);
    lv_obj_set_style_border_color(statusCard, lv_color_hex(0x163859), 0);
    lv_obj_set_style_border_width(statusCard, 1, 0);
    lv_obj_set_style_radius(statusCard, 8, 0);
    lv_obj_set_style_pad_all(statusCard, 0, 0);

    statusTagLabel = lv_label_create(statusCard);
    lv_label_set_text(statusTagLabel, "SYSTEM OPTIMIZED");
    lv_obj_set_style_text_font(statusTagLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statusTagLabel, lv_color_hex(0x00FF9C), 0);
    lv_obj_set_pos(statusTagLabel, 10, 4);

    statusMsgLabel = lv_label_create(statusCard);
    lv_label_set_text_fmt(statusMsgLabel, "Hello, %s. Ready when you are.", USER_NAME);
    lv_obj_set_style_text_font(statusMsgLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(statusMsgLabel, lv_color_hex(0xD8FFFF), 0);
    lv_label_set_long_mode(statusMsgLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_width(statusMsgLabel, 276);
    lv_obj_set_pos(statusMsgLabel, 10, 19);

    // 5. Bottom Navigation Bar: SYS | MODES | POMO | GAME | ALRT | SET (6 x 48px)
    const char* navNames[6] = {"SYS", "MODES", "POMO", "GAME", "ALRT", "SET"};
    const RexPage navTargets[6] = {
      PAGE_SYSTEM, PAGE_MODES, PAGE_POMODORO, PAGE_GAMES, PAGE_ALERTS, PAGE_SETTINGS
    };

    for (int i = 0; i < 6; i++) {
      lv_obj_t* btn = lv_button_create(homeScr);
      lv_obj_set_size(btn, 48, 36);
      lv_obj_set_pos(btn, 7 + (i * 51), 196);
      const bool highlight = (i == 1 || i == 2);
      lv_obj_set_style_bg_color(btn, lv_color_hex(highlight ? 0x0D2B45 : 0x0A1E32), 0);
      lv_obj_set_style_border_color(btn, lv_color_hex(i == 2 ? 0x00FF9C : 0x00FFFF), 0);
      lv_obj_set_style_border_width(btn, 1, 0);
      lv_obj_set_style_radius(btn, 8, 0);
      lv_obj_set_style_pad_all(btn, 0, 0);
      lv_obj_set_style_outline_color(btn, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
      lv_obj_set_style_outline_width(btn, 2, LV_STATE_FOCUSED);
      lv_obj_set_style_outline_pad(btn, 2, LV_STATE_FOCUSED);

      lv_obj_add_event_cb(btn, onNavBtnClicked, LV_EVENT_CLICKED,
                          reinterpret_cast<void*>(static_cast<uintptr_t>(navTargets[i])));

      lv_obj_t* lbl = lv_label_create(btn);
      lv_label_set_text(lbl, navNames[i]);
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(lbl, lv_color_hex(i == 2 ? 0x00FF9C : 0xD8FFFF), 0);
      lv_obj_center(lbl);

      navBtns[i] = btn;
      if (i == 4) alertsNavLabel = lbl;
    }
  }

  void buildSubPages() {
    // SYSTEM PAGE
    systemScr = createSubPageShell("REX // SYSTEM DIAGNOSTICS", nullptr);
    sysInfoLabel = lv_label_create(systemScr);
    lv_label_set_text(sysInfoLabel, "Loading telemetry...");
    lv_obj_set_style_text_font(sysInfoLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sysInfoLabel, lv_color_hex(0xD8FFFF), 0);
    lv_obj_set_pos(sysInfoLabel, 14, 42);

    // MODES PAGE
    modesScr = createSubPageShell("REX // WORKSPACE MODES", nullptr);

    auto makeModeBtn = [&](const char* label, int x, int y, int w, uint32_t borderHex, const char* modeKey) {
      lv_obj_t* b = lv_button_create(modesScr);
      lv_obj_set_size(b, w, 38);
      lv_obj_set_pos(b, x, y);
      lv_obj_set_style_bg_color(b, lv_color_hex(0x0A1E32), 0);
      lv_obj_set_style_border_color(b, lv_color_hex(borderHex), 0);
      lv_obj_set_style_border_width(b, 1, 0);
      lv_obj_set_style_radius(b, 8, 0);
      lv_obj_set_style_outline_color(b, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
      lv_obj_set_style_outline_width(b, 2, LV_STATE_FOCUSED);
      lv_obj_add_event_cb(b, onModeSelectClicked, LV_EVENT_CLICKED, const_cast<char*>(modeKey));
      lv_obj_add_event_cb(b, onSubPageKey, LV_EVENT_KEY, nullptr);

      lv_obj_t* l = lv_label_create(b);
      lv_label_set_text(l, label);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_center(l);
      return b;
    };

    modeBtnProject = makeModeBtn("PROJECT\n(Zen + Code)", 12, 38, 144, 0x00FFFF, "project");
    modeBtnStudy   = makeModeBtn("STUDY + POMO\n(Zen + Files)", 164, 38, 144, 0x00FF9C, "study");
    modeBtnMonitor = makeModeBtn("MONITOR MODE (Live PC Stats)", 12, 84, 296, 0x7CC4FA, "monitor");

    lv_obj_t* monBox = lv_obj_create(modesScr);
    lv_obj_remove_flag(monBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(monBox, 296, 62);
    lv_obj_set_pos(monBox, 12, 130);
    lv_obj_set_style_bg_color(monBox, lv_color_hex(0x05111D), 0);
    lv_obj_set_style_border_color(monBox, lv_color_hex(0x163859), 0);
    lv_obj_set_style_border_width(monBox, 1, 0);
    lv_obj_set_style_radius(monBox, 8, 0);
    lv_obj_set_style_pad_all(monBox, 6, 0);

    monitorStatsLbl = lv_label_create(monBox);
    lv_label_set_text(monitorStatsLbl, "Select a mode above to launch workspace\nor open Study + Pomodoro page.");
    lv_obj_set_style_text_font(monitorStatsLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(monitorStatsLbl, lv_color_hex(0xD8FFFF), 0);

    // DEDICATED POMODORO STUDY PAGE
    pomodoroScr = createSubPageShell("REX // STUDY POMODORO", &pomoBackBtn);

    pomoPhaseLbl = lv_label_create(pomodoroScr);
    lv_label_set_text(pomoPhaseLbl, "FOCUS SESSION");
    lv_obj_set_style_text_font(pomoPhaseLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pomoPhaseLbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_pos(pomoPhaseLbl, 12, 30);

    // Multi-Pose Study & Break Scene on Left Side (from PomodoroScene.h)
    lv_obj_t* scene = createPomodoroScene(pomodoroScr);
    lv_obj_set_pos(scene, 12, 46);

    pomoPropLbl = lv_label_create(pomodoroScr);
    lv_label_set_text(pomoPropLbl, "\"Ready when you are.\"");
    lv_obj_set_style_text_font(pomoPropLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pomoPropLbl, lv_color_hex(0x7CC4FA), 0);
    lv_label_set_long_mode(pomoPropLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(pomoPropLbl, 296);
    lv_obj_set_pos(pomoPropLbl, 12, 168);

    // Circular Progress Arc + Countdown on Right Side
    pomoArc = lv_arc_create(pomodoroScr);
    lv_obj_remove_flag(pomoArc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(pomoArc, 94, 94);
    lv_obj_set_pos(pomoArc, 218, 44);
    lv_arc_set_rotation(pomoArc, 270);
    lv_arc_set_bg_angles(pomoArc, 0, 360);
    lv_arc_set_range(pomoArc, 0, 100);
    lv_arc_set_value(pomoArc, 100);
    lv_obj_set_style_opa(pomoArc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_arc_width(pomoArc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(pomoArc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(pomoArc, lv_color_hex(0x0D2B45), LV_PART_MAIN);
    lv_obj_set_style_arc_color(pomoArc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR);

    pomoBigTimeLbl = lv_label_create(pomoArc);
    lv_label_set_text(pomoBigTimeLbl, "25:00");
    lv_obj_set_style_text_font(pomoBigTimeLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pomoBigTimeLbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(pomoBigTimeLbl);

    // Bottom Controls next to < HOME (START/PAUSE, SKIP, RESET)
    auto makePomoBtn = [&](const char* txt, int x, int w, uint32_t borderHex, lv_event_cb_t cb, lv_obj_t** outLbl) {
      lv_obj_t* b = lv_button_create(pomodoroScr);
      lv_obj_set_size(b, w, 30);
      lv_obj_set_pos(b, x, 200);
      lv_obj_set_style_bg_color(b, lv_color_hex(0x0A1E32), 0);
      lv_obj_set_style_border_color(b, lv_color_hex(borderHex), 0);
      lv_obj_set_style_border_width(b, 1, 0);
      lv_obj_set_style_outline_color(b, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
      lv_obj_set_style_outline_width(b, 2, LV_STATE_FOCUSED);
      lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
      lv_obj_add_event_cb(b, onSubPageKey, LV_EVENT_KEY, nullptr);

      lv_obj_t* l = lv_label_create(b);
      lv_label_set_text(l, txt);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_center(l);
      if (outLbl) *outLbl = l;
      return b;
    };

    pomoStartBtn = makePomoBtn("START", 104, 72, 0x00FF9C, onPomoStartPauseClicked, &pomoStartLbl);
    pomoSkipBtn  = makePomoBtn("SKIP",  182, 58, 0x7CC4FA, onPomoSkipClicked, nullptr);
    pomoResetBtn = makePomoBtn("RESET", 246, 62, 0xFF9E00, onPomoResetClicked, nullptr);

    // GAMES PAGE
    gamesScr = createSubPageShell("REX // MINI-GAME", nullptr);
    gameScoreLabel = lv_label_create(gamesScr);
    lv_label_set_text(gameScoreLabel, "SCORE: 0");
    lv_obj_set_style_text_font(gameScoreLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(gameScoreLabel, lv_color_hex(0x00FFFF), 0);
    lv_obj_align(gameScoreLabel, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t* tapBtn = lv_button_create(gamesScr);
    lv_obj_set_size(tapBtn, 150, 48);
    lv_obj_align(tapBtn, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(tapBtn, lv_color_hex(0x0E3A5D), 0);
    lv_obj_set_style_border_color(tapBtn, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(tapBtn, 1, 0);
    lv_obj_add_event_cb(tapBtn, onGameTapClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(tapBtn, onSubPageKey, LV_EVENT_KEY, nullptr);
    lv_obj_t* tapLbl = lv_label_create(tapBtn);
    lv_label_set_text(tapLbl, "TAP / SELECT +1");
    lv_obj_center(tapLbl);

    // ALERTS PAGE
    alertsScr = createSubPageShell("REX // NOTIFICATION CENTRE", nullptr);

    alertsListBox = lv_obj_create(alertsScr);
    lv_obj_set_size(alertsListBox, 296, 152);
    lv_obj_set_pos(alertsListBox, 12, 36);
    lv_obj_set_style_bg_color(alertsListBox, lv_color_hex(0x040C14), 0);
    lv_obj_set_style_border_color(alertsListBox, lv_color_hex(0x163859), 0);
    lv_obj_set_style_border_width(alertsListBox, 1, 0);
    lv_obj_set_style_radius(alertsListBox, 8, 0);
    lv_obj_set_style_pad_all(alertsListBox, 6, 0);
    lv_obj_set_flex_flow(alertsListBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(alertsListBox, 6, 0);

    alertsClearBtn = lv_button_create(alertsScr);
    lv_obj_set_size(alertsClearBtn, 96, 30);
    lv_obj_align(alertsClearBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_set_style_bg_color(alertsClearBtn, lv_color_hex(0x1A1026), 0);
    lv_obj_set_style_border_color(alertsClearBtn, lv_color_hex(0xFF5C8A), 0);
    lv_obj_set_style_border_width(alertsClearBtn, 1, 0);
    lv_obj_set_style_outline_color(alertsClearBtn, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(alertsClearBtn, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(alertsClearBtn, onClearAlertsClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(alertsClearBtn, onSubPageKey, LV_EVENT_KEY, nullptr);

    lv_obj_t* clrLbl = lv_label_create(alertsClearBtn);
    lv_label_set_text(clrLbl, "CLEAR ALL");
    lv_obj_set_style_text_font(clrLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(clrLbl);

    refreshAlertsUI(0);

    // SETTINGS / BUDDY MOOD PAGE
    settingsScr = createSubPageShell("REX // BUDDY & SETTINGS", &settingsBackBtn);
    settingsMoodBtn = lv_button_create(settingsScr);
    lv_obj_set_size(settingsMoodBtn, 210, 44);
    lv_obj_align(settingsMoodBtn, LV_ALIGN_CENTER, 0, -6);
    lv_obj_set_style_bg_color(settingsMoodBtn, lv_color_hex(0x0A1E32), 0);
    lv_obj_set_style_border_color(settingsMoodBtn, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(settingsMoodBtn, 1, 0);
    lv_obj_set_style_outline_color(settingsMoodBtn, lv_color_hex(0x00FFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(settingsMoodBtn, 2, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(settingsMoodBtn, onSettingsMoodCycle, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(settingsMoodBtn, onSubPageKey, LV_EVENT_KEY, nullptr);

    settingsMoodLbl = lv_label_create(settingsMoodBtn);
    lv_label_set_text_fmt(settingsMoodLbl, "MOOD: %s", getBuddyMoodName(getBuddyBaseMood()));
    lv_obj_center(settingsMoodLbl);
  }
}

void initializeScreens() {
  buildHomeScreen();
  buildSubPages();
  syncPomodoroUI();
  navigateToPage(PAGE_HOME);
}

void navigateToPage(RexPage page) {
  activePage = page;
  lv_group_t* grp = getLvglInputGroup();

  switch (page) {
    case PAGE_SYSTEM:
      lv_screen_load(systemScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, lv_obj_get_child(systemScr, 1));
      }
      break;

    case PAGE_MODES:
      lv_screen_load(modesScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, modeBtnProject);
        lv_group_add_obj(grp, modeBtnStudy);
        lv_group_add_obj(grp, modeBtnMonitor);
        lv_group_add_obj(grp, lv_obj_get_child(modesScr, 1));
      }
      break;

    case PAGE_POMODORO:
      syncPomodoroUI();
      lv_screen_load(pomodoroScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        if (pomoStartBtn) lv_group_add_obj(grp, pomoStartBtn);
        if (pomoSkipBtn)  lv_group_add_obj(grp, pomoSkipBtn);
        if (pomoResetBtn) lv_group_add_obj(grp, pomoResetBtn);
        if (pomoBackBtn)  lv_group_add_obj(grp, pomoBackBtn);
      }
      break;

    case PAGE_GAMES:
      lvglGameScore = 0;
      if (gameScoreLabel) lv_label_set_text(gameScoreLabel, "SCORE: 0");
      lv_screen_load(gamesScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, lv_obj_get_child(gamesScr, 2));
        lv_group_add_obj(grp, lv_obj_get_child(gamesScr, 1));
      }
      break;

    case PAGE_ALERTS:
      markAllNotificationsRead();
      setHomeStatusPreview("SYSTEM OPTIMIZED", "All alerts reviewed.");
      lv_screen_load(alertsScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        lv_group_add_obj(grp, alertsClearBtn);
        lv_group_add_obj(grp, lv_obj_get_child(alertsScr, 1));
      }
      break;

    case PAGE_SETTINGS:
      if (settingsMoodLbl) {
        lv_label_set_text_fmt(settingsMoodLbl, "MOOD: %s", getBuddyMoodName(getBuddyBaseMood()));
      }
      lv_screen_load(settingsScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        if (settingsMoodBtn) lv_group_add_obj(grp, settingsMoodBtn);
        if (settingsBackBtn) lv_group_add_obj(grp, settingsBackBtn);
      }
      break;

    case PAGE_HOME:
    default:
      lv_screen_load(homeScr);
      if (grp) {
        lv_group_remove_all_objs(grp);
        for (int i = 0; i < 6; i++) {
          if (navBtns[i]) lv_group_add_obj(grp, navBtns[i]);
        }
      }
      break;
  }
}

void setHomeStatusPreview(const char* tag, const char* message) {
  if (statusTagLabel) {
    lv_label_set_text(statusTagLabel, tag);
    const bool isOk = (strcmp(tag, "SYSTEM OPTIMIZED") == 0 ||
                       strcmp(tag, "BLE LINKED") == 0 ||
                       strcmp(tag, "TIME SYNC") == 0);
    const bool isCall = (strcmp(tag, "CALL") == 0);
    lv_obj_set_style_text_color(statusTagLabel, lv_color_hex(isCall ? 0xFF5C8A : (isOk ? 0x00FF9C : 0x00FFFF)), 0);
  }
  if (statusMsgLabel) {
    lv_label_set_text(statusMsgLabel, message);
  }
}

void refreshAlertsUI(int unreadCount) {
  if (alertsNavLabel) {
    if (unreadCount > 0) {
      lv_label_set_text_fmt(alertsNavLabel, "AL(%d)", unreadCount);
    } else {
      lv_label_set_text(alertsNavLabel, "ALRT");
    }
  }

  if (!alertsListBox) return;
  lv_obj_clean(alertsListBox);

  const int count = getNotificationCount();
  if (count == 0) {
    lv_obj_t* emptyLbl = lv_label_create(alertsListBox);
    lv_label_set_text(emptyLbl, "Inbox empty.\nWaiting for Ubuntu / A13 bridge...");
    lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(emptyLbl, lv_color_hex(0x7CC4FA), 0);
    return;
  }

  for (int i = 0; i < count; i++) {
    const RexNotification* n = getNotificationAt(i);
    if (!n) continue;

    lv_obj_t* card = lv_obj_create(alertsListBox);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, 278, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x081A2C), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1E4976), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 6, 0);

    lv_obj_t* hdr = lv_label_create(card);
    lv_label_set_text_fmt(hdr, "[%s %s] %s", n->app, n->timeStr, n->title);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_width(hdr, 264);
    lv_label_set_long_mode(hdr, LV_LABEL_LONG_DOT);

    if (strlen(n->body) > 0) {
      lv_obj_t* bdy = lv_label_create(card);
      lv_label_set_text(bdy, n->body);
      lv_obj_set_style_text_font(bdy, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(bdy, lv_color_hex(0xD8FFFF), 0);
      lv_obj_set_width(bdy, 264);
      lv_label_set_long_mode(bdy, LV_LABEL_LONG_WRAP);
      lv_obj_set_pos(bdy, 0, 16);
    }
  }
}

void updateHomeDashboard() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";

    lv_label_set_text_fmt(timeLabel, "%d:%02d", hour12, timeinfo.tm_min);
    lv_label_set_text_fmt(
      ampmDateLabel,
      "%s %s, %d %s",
      ampm,
      DAYS_UPPER[timeinfo.tm_wday],
      timeinfo.tm_mday,
      MONTHS_UPPER[timeinfo.tm_mon]
    );
  }

  updatePomodoroTimer();
  updatePomodoroScene();

  const bool wifiOnline = (WiFi.status() == WL_CONNECTED);
  const bool bleOnline  = isBlePhoneConnected();
  const bool anyOnline  = (wifiOnline || bleOnline);

  if (anyOnline != lastAnyOnline || wifiOnline != lastWifiState) {
    lastAnyOnline = anyOnline;
    lastWifiState = wifiOnline;

    if (anyOnline) {
      lv_obj_set_style_bg_color(netDot, lv_color_hex(0x00FF9C), 0);
      lv_label_set_text(netTitleLabel, "ONLINE");
      lv_obj_set_style_text_color(netTitleLabel, lv_color_hex(0x00FF9C), 0);

      if (!pomoRunning) {
        if (wifiOnline) {
          setHomeStatusPreview("SYSTEM OPTIMIZED", "All links synced. Ready when you are.");
        } else {
          setHomeStatusPreview("BLE LINKED", "Galaxy A13 connected. Ready when you are.");
        }
      }

      if (getBuddyBaseMood() == BUDDY_DISCONNECTED) {
        setBuddyBaseMood(pomoRunning ? (pomoIsBreak ? BUDDY_BREAK : BUDDY_STUDYING) : BUDDY_IDLE);
      }
      triggerBuddyReaction(BUDDY_HAPPY, 1800);
    } else {
      lv_obj_set_style_bg_color(netDot, lv_color_hex(0xFF9E00), 0);
      lv_label_set_text(netTitleLabel, "OFFLINE");
      lv_obj_set_style_text_color(netTitleLabel, lv_color_hex(0xFF9E00), 0);
      if (!pomoRunning) {
        setHomeStatusPreview("NETWORK", "Links offline. Advertising BLE...");
        setBuddyBaseMood(BUDDY_DISCONNECTED);
      }
    }
  }

  if (netSubLabel) {
    if (bleOnline && wifiOnline) {
      lv_label_set_text(netSubLabel, "Wi-Fi + BLE");
    } else if (bleOnline) {
      lv_label_set_text(netSubLabel, "BLE Linked");
    } else {
      lv_label_set_text(netSubLabel, "BLE Ready");
    }
  }

  if (activePage == PAGE_SYSTEM && sysInfoLabel) {
    const unsigned long upSec = (millis() - bootTime) / 1000;
    lv_label_set_text_fmt(
      sysInfoLabel,
      "MCU: ESP32-WROOM-32 @ 240 MHz\n"
      "Free Heap: %lu KB\n"
      "Uptime: %lum %lus\n"
      "Wi-Fi: %s (%d dBm)\n"
      "BLE Phone: %s\n"
      "IP: %s",
      static_cast<unsigned long>(ESP.getFreeHeap() / 1024),
      upSec / 60,
      upSec % 60,
      wifiOnline ? "Connected" : "Offline",
      wifiOnline ? WiFi.RSSI() : 0,
      bleOnline ? "Paired (Galaxy A13)" : "Advertising",
      wifiOnline ? WiFi.localIP().toString().c_str() : "0.0.0.0"
    );
  }
}

void updatePcTelemetry(int cpuPct, int ramPct, int tempC, const char* activeModeName) {
  pcCpuPct = cpuPct;
  pcRamPct = ramPct;
  pcTempC  = tempC;

  if (monitorStatsLbl) {
    lv_label_set_text_fmt(
      monitorStatsLbl,
      "UBUNTU // %s\nCPU: %d%%   |   RAM: %d%%   |   TEMP: %d C",
      activeModeName,
      pcCpuPct,
      pcRamPct,
      pcTempC
    );
  }
}