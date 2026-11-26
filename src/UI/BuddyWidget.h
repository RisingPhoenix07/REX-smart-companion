#pragma once
#include <lvgl.h>
#include <stdint.h>

enum BuddyMood : uint8_t {
  BUDDY_IDLE = 0,
  BUDDY_BLINK,
  BUDDY_HAPPY,
  BUDDY_CURIOUS,
  BUDDY_SLEEPY,
  BUDDY_ALERT,
  BUDDY_THINKING,
  BUDDY_DISCONNECTED,
  BUDDY_STUDYING,
  BUDDY_BREAK
};

lv_obj_t* createBuddyWidget(lv_obj_t* parent);
void setBuddyBaseMood(BuddyMood mood);
BuddyMood getBuddyBaseMood();
void setBuddyStudyProp(bool useLaptop);
void triggerBuddyReaction(BuddyMood reaction, uint32_t durationMs = 2200);
void setBuddyAlertFocus(bool watchingAlert);
BuddyMood getBuddyActiveMood();
const char* getBuddyMoodName(BuddyMood mood);
void updateBuddyWidget();