#include "BuddyFeature.h"
#include <Arduino.h>
#include "../../Hardware/Hardware.h"
#include "../../Hardware/Display.h"
#include "AppState.h"
#include "Config.h"

namespace {
  enum BuddyMood : uint8_t {
    MOOD_IDLE,
    MOOD_HAPPY,
    MOOD_CURIOUS,
    MOOD_SLEEPY,
    MOOD_ALERT
  };

  BuddyMood mood = MOOD_IDLE;
  uint8_t bond = 0;
  uint8_t energy = 65;
  unsigned long moodUntil = 0;
  unsigned long lastAnimation = 0;
  unsigned long lastBlink = 0;
  bool eyesClosed = false;
  int gaze = 0;

  const char* moodName() {
    switch (mood) {
      case MOOD_HAPPY:   return "HAPPY";
      case MOOD_CURIOUS: return "CURIOUS";
      case MOOD_SLEEPY:  return "SLEEPY";
      case MOOD_ALERT:   return "ALERT";
      default:           return "IDLE";
    }
  }

  void drawBuddyFace() {
    // Redraw only the face viewport; the page frame and action buttons remain static.
    tft.fillRect(48, 57, 224, 92, PANEL);

    const uint16_t eyeColor = (mood == MOOD_ALERT) ? WHITE : ICE;
    const int eyeY = 99;
    const int leftX = 112;
    const int rightX = 208;

    // Eye sockets.
    if (eyesClosed || mood == MOOD_SLEEPY) {
      tft.drawLine(leftX - 19, eyeY, leftX + 19, eyeY, eyeColor);
      tft.drawLine(rightX - 19, eyeY, rightX + 19, eyeY, eyeColor);
      tft.drawLine(leftX - 14, eyeY + 3, leftX + 14, eyeY + 3, DIM_BLUE);
      tft.drawLine(rightX - 14, eyeY + 3, rightX + 14, eyeY + 3, DIM_BLUE);
    } else {
      const int eyeH = (mood == MOOD_ALERT) ? 39 : 32;
      tft.fillRoundRect(leftX - 21, eyeY - eyeH / 2, 42, eyeH, 12, eyeColor);
      tft.fillRoundRect(rightX - 21, eyeY - eyeH / 2, 42, eyeH, 12, eyeColor);

      int pupilOffset = gaze;
      if (mood == MOOD_CURIOUS) pupilOffset = 6;
      if (mood == MOOD_ALERT) pupilOffset = 0;

      tft.fillCircle(leftX + pupilOffset, eyeY, 8, BLUE);
      tft.fillCircle(rightX + pupilOffset, eyeY, 8, BLUE);
      tft.fillCircle(leftX + pupilOffset + 2, eyeY - 2, 3, BG);
      tft.fillCircle(rightX + pupilOffset + 2, eyeY - 2, 3, BG);
    }

    // Expressive eyebrows.
    if (mood == MOOD_CURIOUS) {
      tft.drawLine(leftX - 19, 73, leftX + 13, 68, CYAN);
      tft.drawLine(rightX - 15, 72, rightX + 18, 76, CYAN);
    } else if (mood == MOOD_ALERT) {
      tft.drawLine(leftX - 18, 70, leftX + 18, 70, CYAN);
      tft.drawLine(rightX - 18, 70, rightX + 18, 70, CYAN);
    } else if (mood != MOOD_SLEEPY) {
      tft.drawLine(leftX - 17, 73, leftX + 17, 73, DIM_BLUE);
      tft.drawLine(rightX - 17, 73, rightX + 17, 73, DIM_BLUE);
    }

    // Small pixel-style mouth, kept simple for legibility.
    if (mood == MOOD_HAPPY) {
      tft.drawLine(151, 132, 157, 137, CYAN);
      tft.drawLine(157, 137, 163, 137, CYAN);
      tft.drawLine(163, 137, 169, 132, CYAN);
    } else if (mood == MOOD_ALERT) {
      tft.drawRoundRect(155, 128, 10, 12, 4, CYAN);
    } else if (mood == MOOD_SLEEPY) {
      tft.drawFastHLine(153, 135, 14, DIM_BLUE);
    } else {
      tft.drawFastHLine(155, 134, 10, CYAN);
    }

    // Compact mood indicator beside the companion status line.
    tft.fillRect(235, 49, 58, 10, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(CYAN);
    tft.setCursor(241, 51);
    tft.print(moodName());
  }

  void drawBuddyStats() {
    tft.fillRect(26, 152, 268, 11, PANEL);
    tft.setTextSize(1);
    tft.setTextColor(ICE);
    tft.setCursor(31, 154);
    tft.print("BOND ");
    tft.print(bond);
    tft.print("%   ENERGY ");
    tft.print(energy);
    tft.print("%");
  }

  void setMood(BuddyMood nextMood, unsigned long durationMs) {
    mood = nextMood;
    moodUntil = millis() + durationMs;
    eyesClosed = false;
    drawBuddyFace();
  }

  void drawActionButton(int x, const char* label) {
    tft.fillRoundRect(x, 164, 86, 26, 6, PANEL_LIGHT);
    tft.drawRoundRect(x, 164, 86, 26, 6, CYAN);
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 27, 174);
    tft.print(label);
  }
}

void drawBuddyPage() {
  tft.fillScreen(BG);
  drawHeader("DESKTOP BUDDY");
  drawPanel(15, 45, 290, 150);

  tft.setTextSize(1);
  tft.setTextColor(DIM_BLUE);
  tft.setCursor(27, 51);
  tft.print("REX COMPANION // ONLINE");

  drawActionButton(55, "PET");
  drawActionButton(179, "FEED");
  drawHomeButton();

  mood = MOOD_IDLE;
  moodUntil = 0;
  lastAnimation = millis();
  lastBlink = millis();
  eyesClosed = false;
  gaze = 0;
  drawBuddyFace();
  drawBuddyStats();
}

void updateBuddy() {
  if (currentPage != BUDDY_PAGE) return;

  const unsigned long now = millis();

  if (mood != MOOD_IDLE && moodUntil != 0 && (long)(now - moodUntil) >= 0) {
    mood = MOOD_IDLE;
    moodUntil = 0;
    drawBuddyFace();
  }

  if (now - lastAnimation >= 900) {
    lastAnimation = now;
    gaze = (gaze == -5) ? 5 : -5;
    if (mood == MOOD_IDLE) drawBuddyFace();
  }

  if (!eyesClosed && now - lastBlink >= 3200) {
    eyesClosed = true;
    lastBlink = now;
    drawBuddyFace();
  } else if (eyesClosed && now - lastBlink >= 150) {
    eyesClosed = false;
    drawBuddyFace();
  }
}

void buddyPet() {
  if (currentPage != BUDDY_PAGE) return;
  if (bond < 99) bond = (bond > 89) ? 99 : bond + 10;
  setMood(MOOD_HAPPY, 3500);
  drawBuddyStats();
  Serial.println("REX Buddy: pet interaction");
}

void buddyFeed() {
  if (currentPage != BUDDY_PAGE) return;
  energy = (energy > 89) ? 100 : energy + 10;
  setMood(MOOD_HAPPY, 2500);
  drawBuddyStats();
  Serial.println("REX Buddy: feed interaction");
}

void buddyNextMood() {
  if (currentPage != BUDDY_PAGE) return;
  BuddyMood next = MOOD_CURIOUS;
  if (mood == MOOD_CURIOUS) next = MOOD_SLEEPY;
  else if (mood == MOOD_SLEEPY) next = MOOD_ALERT;
  else if (mood == MOOD_ALERT) next = MOOD_IDLE;
  setMood(next, 4000);
}

void handleBuddyTap(int x, int y) {
  if (currentPage != BUDDY_PAGE) return;

  if (y >= 160 && y <= 195) {
    if (x >= 45 && x <= 145) buddyPet();
    else if (x >= 175 && x <= 275) buddyFeed();
  } else if (y >= 65 && y <= 160) {
    buddyPet();
  }
}
