#include "BuddyWidget.h"
#include <Arduino.h>
#include <cmath>

namespace {
  constexpr int WIDGET_W = 212;
  constexpr int WIDGET_H = 88;
  constexpr int LEFT_EYE_CX  = 60;
  constexpr int RIGHT_EYE_CX = 152;
  constexpr int EYE_CY       = 36;

  struct EyeTarget {
    float leftW, leftH;
    float rightW, rightH;
    float pupilX, pupilY;
    float topLid;
    float bottomLid;
    uint32_t glowHex;
    uint32_t coreHex;
    int16_t mouthStart;
    int16_t mouthEnd;
    int16_t mouthW;
    int16_t mouthH;
    int16_t mouthOffsetX;
    int16_t mouthOffsetY;
    int16_t eyeCenterY;
  };

  lv_obj_t* rootBox = nullptr;
  lv_obj_t* leftEye = nullptr;
  lv_obj_t* rightEye = nullptr;
  lv_obj_t* leftPupil = nullptr;
  lv_obj_t* rightPupil = nullptr;
  lv_obj_t* leftGlint = nullptr;
  lv_obj_t* rightGlint = nullptr;
  lv_obj_t* leftTopLid = nullptr;
  lv_obj_t* rightTopLid = nullptr;
  lv_obj_t* leftBotLid = nullptr;
  lv_obj_t* rightBotLid = nullptr;
  lv_obj_t* mouthArc = nullptr;

  // Study Props (Book & Mini Laptop)
  lv_obj_t* bookGroup = nullptr;
  lv_obj_t* bookPageFlip = nullptr;
  lv_obj_t* laptopGroup = nullptr;
  lv_obj_t* laptopBar1 = nullptr;
  lv_obj_t* laptopBar2 = nullptr;

  BuddyMood baseMood = BUDDY_IDLE;
  BuddyMood activeReaction = BUDDY_IDLE;
  bool hasReaction = false;
  uint32_t reactionEndMs = 0;
  bool watchingBottomAlert = false;
  bool studyUseLaptop = false;

  EyeTarget cur = {
    46, 38, 46, 38, 0, 0, 0.0f, 0.0f,
    0x00FFFF, 0xD8FFFF,
    55, 125, 24, 14, 0, 62, EYE_CY
  };

  uint32_t lastGlowHex = 0;
  uint32_t lastCoreHex = 0;
  int16_t lastMouthStart = -1;
  int16_t lastMouthEnd = -1;
  bool forceFirstRender = true;

  uint32_t lastFrameMs = 0;
  uint32_t nextBlinkMs = 0;
  uint32_t blinkEndMs = 0;
  bool isBlinking = false;

  uint32_t nextGazeMs = 0;
  uint32_t lastStudyStepMs = 0;
  float idleGazeX = 0.0f;
  float idleGazeY = 0.0f;
  float studyScanPhase = 0.0f;
  uint32_t propAnimStep = 0;

  float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
  }

  bool nearEq(float a, float b) {
    return fabsf(a - b) < 0.15f;
  }

  EyeTarget getTargetForMood(BuddyMood mood) {
    switch (mood) {
      case BUDDY_BLINK:
        return { 48, 38, 48, 38, idleGazeX, idleGazeY, 1.0f, 0.0f,
                 0x00FFFF, 0xD8FFFF, 60, 120, 22, 12, 0, 63,
                 static_cast<int16_t>(baseMood == BUDDY_STUDYING ? EYE_CY - 6 : EYE_CY) };
      case BUDDY_HAPPY:
        return { 50, 38, 50, 38, 0, -2, 0.04f, 0.62f,
                 0x00FFFF, 0xFFFFFF, 18, 162, 28, 16, 0, 60, EYE_CY };
      case BUDDY_CURIOUS:
        return { 50, 42, 40, 30, 5, -2, 0.06f, 0.0f,
                 0x00FFFF, 0xD8FFFF, 40, 115, 22, 12, 3, 63, EYE_CY };
      case BUDDY_SLEEPY:
        return { 46, 34, 46, 34, 0, 2, 0.58f, 0.12f,
                 0x2A9FFF, 0x88CCFF, 70, 110, 18, 10, 0, 64, EYE_CY };
      case BUDDY_ALERT:
        return { 52, 44, 52, 44, 0, 1, 0.0f, 0.0f,
                 0xFFFFFF, 0xFFFFFF, 0, 360, 12, 12, 0, 66, EYE_CY };
      case BUDDY_THINKING:
        return { 46, 36, 46, 36, idleGazeX, -5, 0.18f, 0.15f,
                 0x00E5FF, 0xD8FFFF, 65, 135, 22, 12, -3, 63, EYE_CY };
      case BUDDY_DISCONNECTED:
        return { 44, 34, 44, 34, -3, 3, 0.28f, 0.0f,
                 0xFF9E00, 0xFFD180, 205, 335, 24, 14, 0, 67, EYE_CY };
      case BUDDY_STUDYING:
        return { 44, 32, 44, 32, idleGazeX, 6.0f, 0.16f, 0.08f,
                 0x00E5FF, 0xD8FFFF, 65, 115, 18, 10, 0, 49, EYE_CY - 6 };
      case BUDDY_BREAK:
        return { 48, 34, 48, 34, 0.0f, -2.0f, 0.34f, 0.36f,
                 0x00FF9C, 0xD8FFF0, 30, 150, 26, 14, 0, 61, EYE_CY };
      case BUDDY_IDLE:
      default:
        return { 46, 38, 46, 38, idleGazeX, idleGazeY, 0.0f, 0.0f,
                 0x00FFFF, 0xD8FFFF, 55, 125, 24, 14, 0, 62, EYE_CY };
    }
  }

  void applyEyeGeometry(
      lv_obj_t* eye, lv_obj_t* pupil, lv_obj_t* glint,
      lv_obj_t* topLid, lv_obj_t* botLid,
      int cx, int cy, float w, float h,
      float px, float py, float topFrac, float botFrac,
      uint32_t glowHex, uint32_t coreHex, bool colorsChanged) {
    const int iw = static_cast<int>(w + 0.5f);
    const int ih = static_cast<int>(h + 0.5f);

    lv_obj_set_size(eye, iw, ih);
    lv_obj_set_pos(eye, cx - iw / 2, cy - ih / 2);

    if (colorsChanged) {
      lv_obj_set_style_bg_color(eye, lv_color_hex(coreHex), 0);
      lv_obj_set_style_border_color(eye, lv_color_hex(glowHex), 0);
    }

    const int pSize = 14;
    const int pLeft = (iw / 2) - (pSize / 2) + static_cast<int>(px);
    const int pTop  = (ih / 2) - (pSize / 2) + static_cast<int>(py);
    lv_obj_set_size(pupil, pSize, pSize);
    lv_obj_set_pos(pupil, pLeft, pTop);
    lv_obj_set_pos(glint, pLeft + 3, pTop + 3);

    const int topH = static_cast<int>(ih * topFrac);
    lv_obj_set_size(topLid, iw + 8, (topH > 0 ? topH : 0) + 4);
    lv_obj_set_pos(topLid, -4, -4);

    const int botH = static_cast<int>(ih * botFrac);
    lv_obj_set_size(botLid, iw + 8, botH + 6);
    lv_obj_set_pos(botLid, -4, ih - botH);
  }

  lv_obj_t* createEyeAssembly(lv_obj_t* parent, lv_obj_t** outPupil, lv_obj_t** outGlint,
                              lv_obj_t** outTopLid, lv_obj_t** outBotLid) {
    lv_obj_t* eye = lv_obj_create(parent);
    lv_obj_remove_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(eye, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(eye, 14, 0);
    lv_obj_set_style_border_width(eye, 2, 0);
    lv_obj_set_style_pad_all(eye, 0, 0);
    lv_obj_set_style_clip_corner(eye, true, 0);

    lv_obj_t* pupil = lv_obj_create(eye);
    lv_obj_remove_flag(pupil, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(pupil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0x051324), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);

    lv_obj_t* glint = lv_obj_create(eye);
    lv_obj_remove_flag(glint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(glint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(glint, 4, 4);
    lv_obj_set_style_radius(glint, 2, 0);
    lv_obj_set_style_bg_color(glint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(glint, 0, 0);

    lv_obj_t* topLid = lv_obj_create(eye);
    lv_obj_remove_flag(topLid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(topLid, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(topLid, lv_color_hex(0x050D16), 0);
    lv_obj_set_style_border_width(topLid, 0, 0);

    lv_obj_t* botLid = lv_obj_create(eye);
    lv_obj_remove_flag(botLid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(botLid, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(botLid, lv_color_hex(0x050D16), 0);
    lv_obj_set_style_border_width(botLid, 0, 0);
    lv_obj_set_style_radius(botLid, 18, 0);

    *outPupil = pupil;
    *outGlint = glint;
    *outTopLid = topLid;
    *outBotLid = botLid;
    return eye;
  }

  void createStudyProps(lv_obj_t* parent) {
    // --- PROP 1: OPEN GLOWING BOOK ---
    bookGroup = lv_obj_create(parent);
    lv_obj_remove_flag(bookGroup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bookGroup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bookGroup, 84, 26);
    lv_obj_set_pos(bookGroup, (WIDGET_W - 84) / 2, 58);
    lv_obj_set_style_bg_opa(bookGroup, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bookGroup, 0, 0);
    lv_obj_set_style_pad_all(bookGroup, 0, 0);
    lv_obj_add_flag(bookGroup, LV_OBJ_FLAG_HIDDEN);

    // Left Page
    lv_obj_t* leftPage = lv_obj_create(bookGroup);
    lv_obj_remove_flag(leftPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(leftPage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(leftPage, 39, 22);
    lv_obj_set_pos(leftPage, 2, 2);
    lv_obj_set_style_bg_color(leftPage, lv_color_hex(0x0A2640), 0);
    lv_obj_set_style_border_color(leftPage, lv_color_hex(0x7CC4FA), 0);
    lv_obj_set_style_border_width(leftPage, 2, 0);
    lv_obj_set_style_radius(leftPage, 4, 0);
    lv_obj_set_style_pad_all(leftPage, 0, 0);

    // Right Page
    lv_obj_t* rightPage = lv_obj_create(bookGroup);
    lv_obj_remove_flag(rightPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(rightPage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(rightPage, 39, 22);
    lv_obj_set_pos(rightPage, 43, 2);
    lv_obj_set_style_bg_color(rightPage, lv_color_hex(0x0A2640), 0);
    lv_obj_set_style_border_color(rightPage, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(rightPage, 2, 0);
    lv_obj_set_style_radius(rightPage, 4, 0);
    lv_obj_set_style_pad_all(rightPage, 0, 0);

    // Text lines on pages
    for (int i = 0; i < 3; i++) {
      lv_obj_t* l1 = lv_obj_create(leftPage);
      lv_obj_remove_flag(l1, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(l1, 24 - (i * 3), 2);
      lv_obj_set_pos(l1, 5, 4 + (i * 5));
      lv_obj_set_style_bg_color(l1, lv_color_hex(0x7CC4FA), 0);
      lv_obj_set_style_border_width(l1, 0, 0);

      lv_obj_t* l2 = lv_obj_create(rightPage);
      lv_obj_remove_flag(l2, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_size(l2, 22 - (i * 2), 2);
      lv_obj_set_pos(l2, 6, 4 + (i * 5));
      lv_obj_set_style_bg_color(l2, lv_color_hex(0x00FFFF), 0);
      lv_obj_set_style_border_width(l2, 0, 0);
    }

    // Animated page-flip highlight sheet
    bookPageFlip = lv_obj_create(bookGroup);
    lv_obj_remove_flag(bookPageFlip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bookPageFlip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bookPageFlip, 16, 20);
    lv_obj_set_pos(bookPageFlip, 43, 2);
    lv_obj_set_style_bg_color(bookPageFlip, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_bg_opa(bookPageFlip, LV_OPA_40, 0);
    lv_obj_set_style_border_width(bookPageFlip, 0, 0);
    lv_obj_set_style_radius(bookPageFlip, 3, 0);
    lv_obj_add_flag(bookPageFlip, LV_OBJ_FLAG_HIDDEN);

    // --- PROP 2: MINI GLOWING LAPTOP ---
    laptopGroup = lv_obj_create(parent);
    lv_obj_remove_flag(laptopGroup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(laptopGroup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(laptopGroup, 84, 28);
    lv_obj_set_pos(laptopGroup, (WIDGET_W - 84) / 2, 56);
    lv_obj_set_style_bg_opa(laptopGroup, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(laptopGroup, 0, 0);
    lv_obj_set_style_pad_all(laptopGroup, 0, 0);
    lv_obj_add_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN);

    // Laptop Lid / Back Screen (viewed from front, casting glow)
    lv_obj_t* screen = lv_obj_create(laptopGroup);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(screen, 64, 22);
    lv_obj_set_pos(screen, 10, 1);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07253D), 0);
    lv_obj_set_style_border_color(screen, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(screen, 2, 0);
    lv_obj_set_style_radius(screen, 4, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    // Glowing emblem / code bars on laptop
    laptopBar1 = lv_obj_create(screen);
    lv_obj_remove_flag(laptopBar1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(laptopBar1, 28, 3);
    lv_obj_set_pos(laptopBar1, 8, 5);
    lv_obj_set_style_bg_color(laptopBar1, lv_color_hex(0x00FF9C), 0);
    lv_obj_set_style_border_width(laptopBar1, 0, 0);

    laptopBar2 = lv_obj_create(screen);
    lv_obj_remove_flag(laptopBar2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(laptopBar2, 38, 3);
    lv_obj_set_pos(laptopBar2, 8, 11);
    lv_obj_set_style_bg_color(laptopBar2, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(laptopBar2, 0, 0);

    // Laptop Base
    lv_obj_t* base = lv_obj_create(laptopGroup);
    lv_obj_remove_flag(base, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(base, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(base, 78, 4);
    lv_obj_set_pos(base, 3, 23);
    lv_obj_set_style_bg_color(base, lv_color_hex(0x7CC4FA), 0);
    lv_obj_set_style_border_width(base, 0, 0);
    lv_obj_set_style_radius(base, 2, 0);
  }

  void updateStudyPropVisibility(BuddyMood effectiveMood) {
    const bool showProps = (effectiveMood == BUDDY_STUDYING);
    if (!bookGroup || !laptopGroup) return;

    if (!showProps) {
      if (!lv_obj_has_flag(bookGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(bookGroup, LV_OBJ_FLAG_HIDDEN);
      }
      if (!lv_obj_has_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN);
      }
      return;
    }

    if (studyUseLaptop) {
      if (!lv_obj_has_flag(bookGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(bookGroup, LV_OBJ_FLAG_HIDDEN);
      }
      if (lv_obj_has_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      if (!lv_obj_has_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(laptopGroup, LV_OBJ_FLAG_HIDDEN);
      }
      if (lv_obj_has_flag(bookGroup, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(bookGroup, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  void onFaceTapped(lv_event_t* e) {
    LV_UNUSED(e);
    triggerBuddyReaction(BUDDY_HAPPY, 2000);
  }
}

lv_obj_t* createBuddyWidget(lv_obj_t* parent) {
  rootBox = lv_obj_create(parent);
  lv_obj_set_size(rootBox, WIDGET_W, WIDGET_H);
  lv_obj_remove_flag(rootBox, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(rootBox, lv_color_hex(0x050D16), 0);
  lv_obj_set_style_bg_opa(rootBox, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(rootBox, lv_color_hex(0x163859), 0);
  lv_obj_set_style_border_width(rootBox, 1, 0);
  lv_obj_set_style_radius(rootBox, 14, 0);
  lv_obj_set_style_pad_all(rootBox, 0, 0);
  lv_obj_add_event_cb(rootBox, onFaceTapped, LV_EVENT_CLICKED, nullptr);

  leftEye = createEyeAssembly(rootBox, &leftPupil, &leftGlint, &leftTopLid, &leftBotLid);
  rightEye = createEyeAssembly(rootBox, &rightPupil, &rightGlint, &rightTopLid, &rightBotLid);

  mouthArc = lv_arc_create(rootBox);
  lv_obj_remove_flag(mouthArc, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_rotation(mouthArc, 0);
  lv_arc_set_bg_angles(mouthArc, 55, 125);
  lv_arc_set_angles(mouthArc, 0, 0);
  lv_obj_set_style_opa(mouthArc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_opa(mouthArc, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(mouthArc, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(mouthArc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_color(mouthArc, lv_color_hex(0x00FFFF), LV_PART_MAIN);

  createStudyProps(rootBox);

  const uint32_t now = millis();
  lastFrameMs = now;
  nextBlinkMs = now + 2600;
  nextGazeMs = now + 1200;
  lastStudyStepMs = now;
  forceFirstRender = true;
  return rootBox;
}

void setBuddyBaseMood(BuddyMood mood) {
  baseMood = mood;
  forceFirstRender = true;
}

BuddyMood getBuddyBaseMood() {
  return baseMood;
}

void setBuddyStudyProp(bool useLaptop) {
  studyUseLaptop = useLaptop;
  forceFirstRender = true;
}

void triggerBuddyReaction(BuddyMood reaction, uint32_t durationMs) {
  activeReaction = reaction;
  hasReaction = true;
  reactionEndMs = millis() + durationMs;
  isBlinking = false;
  forceFirstRender = true;
}

void setBuddyAlertFocus(bool watchingAlert) {
  watchingBottomAlert = watchingAlert;
  forceFirstRender = true;
}

BuddyMood getBuddyActiveMood() {
  if (hasReaction) return activeReaction;
  if (isBlinking) return BUDDY_BLINK;
  return baseMood;
}

const char* getBuddyMoodName(BuddyMood mood) {
  switch (mood) {
    case BUDDY_BLINK:        return "BLINK";
    case BUDDY_HAPPY:        return "HAPPY";
    case BUDDY_CURIOUS:      return "CURIOUS";
    case BUDDY_SLEEPY:       return "SLEEPY";
    case BUDDY_ALERT:        return "ALERT";
    case BUDDY_THINKING:     return "THINKING";
    case BUDDY_DISCONNECTED: return "CONCERNED";
    case BUDDY_STUDYING:     return "STUDYING";
    case BUDDY_BREAK:        return "BREAK";
    case BUDDY_IDLE:
    default:                 return "IDLE";
  }
}

void updateBuddyWidget() {
  if (!rootBox) return;
  if (lv_obj_get_screen(rootBox) != lv_screen_active()) {
    forceFirstRender = true;
    return;
  }

  const uint32_t now = millis();
  if (hasReaction && static_cast<int32_t>(now - reactionEndMs) >= 0) {
    hasReaction = false;
    forceFirstRender = true;
  }

  const BuddyMood currentMood = hasReaction ? activeReaction : baseMood;

  if (currentMood == BUDDY_STUDYING) {
    // Throttled to 250ms so SPI DMA stays fast and responsive
    if (now - lastStudyStepMs >= 250) {
      lastStudyStepMs = now;
      studyScanPhase += 0.45f;
      idleGazeX = sinf(studyScanPhase) * 6.0f;
      idleGazeY = 6.0f;

      propAnimStep++;
      if (!studyUseLaptop && bookPageFlip) {
        const uint32_t cycle = propAnimStep % 20;
        if (cycle == 0) {
          lv_obj_remove_flag(bookPageFlip, LV_OBJ_FLAG_HIDDEN);
          lv_obj_set_pos(bookPageFlip, 43, 2);
        } else if (cycle == 1) {
          lv_obj_set_pos(bookPageFlip, 25, 2);
        } else if (!lv_obj_has_flag(bookPageFlip, LV_OBJ_FLAG_HIDDEN)) {
          lv_obj_add_flag(bookPageFlip, LV_OBJ_FLAG_HIDDEN);
        }
      } else if (studyUseLaptop && laptopBar1 && laptopBar2) {
        const int w1 = 18 + static_cast<int>((sinf(studyScanPhase * 1.4f) + 1.0f) * 10.0f);
        const int w2 = 22 + static_cast<int>((cosf(studyScanPhase * 1.1f) + 1.0f) * 12.0f);
        lv_obj_set_width(laptopBar1, w1);
        lv_obj_set_width(laptopBar2, w2);
      }
    }
  } else if (now >= nextGazeMs) {
    if (currentMood == BUDDY_IDLE || currentMood == BUDDY_THINKING) {
      const int pick = random(0, 5);
      idleGazeX = (pick == 0) ? -5.0f : (pick == 1) ? 5.0f : 0.0f;
      idleGazeY = (pick == 2) ? -2.0f : (pick == 3) ? 2.0f : 0.0f;
    } else {
      idleGazeX = 0.0f;
      idleGazeY = 0.0f;
    }
    nextGazeMs = now + random(1500, 3200);
  }

  if (!hasReaction && currentMood != BUDDY_SLEEPY && currentMood != BUDDY_ALERT) {
    if (!isBlinking && now >= nextBlinkMs) {
      isBlinking = true;
      blinkEndMs = now + 130;
    } else if (isBlinking && now >= blinkEndMs) {
      isBlinking = false;
      nextBlinkMs = now + random(2500, 5000);
    }
  } else {
    isBlinking = false;
  }

  if (now - lastFrameMs < 55) return;
  lastFrameMs = now;

  const BuddyMood effectiveMood = isBlinking ? BUDDY_BLINK : currentMood;
  updateStudyPropVisibility(hasReaction ? activeReaction : baseMood);

  const EyeTarget target = getTargetForMood(effectiveMood);
  const float speed = isBlinking ? 0.6f : 0.28f;

  const bool settled =
      !forceFirstRender &&
      nearEq(cur.leftW, target.leftW) &&
      nearEq(cur.leftH, target.leftH) &&
      nearEq(cur.rightW, target.rightW) &&
      nearEq(cur.rightH, target.rightH) &&
      nearEq(cur.pupilX, target.pupilX) &&
      nearEq(cur.pupilY, target.pupilY) &&
      nearEq(cur.topLid, target.topLid) &&
      nearEq(cur.bottomLid, target.bottomLid) &&
      cur.eyeCenterY == target.eyeCenterY &&
      lastGlowHex == target.glowHex &&
      lastCoreHex == target.coreHex &&
      lastMouthStart == target.mouthStart &&
      lastMouthEnd == target.mouthEnd;

  if (settled) return;

  cur.leftW      = lerpf(cur.leftW, target.leftW, speed);
  cur.leftH      = lerpf(cur.leftH, target.leftH, speed);
  cur.rightW     = lerpf(cur.rightW, target.rightW, speed);
  cur.rightH     = lerpf(cur.rightH, target.rightH, speed);
  cur.pupilX     = lerpf(cur.pupilX, target.pupilX, speed);
  cur.pupilY     = lerpf(cur.pupilY, target.pupilY, speed);
  cur.topLid     = lerpf(cur.topLid, target.topLid, speed);
  cur.bottomLid  = lerpf(cur.bottomLid, target.bottomLid, speed);
  cur.eyeCenterY = target.eyeCenterY;

  const bool colorsChanged = forceFirstRender ||
      (lastGlowHex != target.glowHex) || (lastCoreHex != target.coreHex);

  applyEyeGeometry(leftEye, leftPupil, leftGlint, leftTopLid, leftBotLid,
                   LEFT_EYE_CX, cur.eyeCenterY, cur.leftW, cur.leftH,
                   cur.pupilX, cur.pupilY, cur.topLid, cur.bottomLid,
                   target.glowHex, target.coreHex, colorsChanged);

  applyEyeGeometry(rightEye, rightPupil, rightGlint, rightTopLid, rightBotLid,
                   RIGHT_EYE_CX, cur.eyeCenterY, cur.rightW, cur.rightH,
                   cur.pupilX, cur.pupilY, cur.topLid, cur.bottomLid,
                   target.glowHex, target.coreHex, colorsChanged);

  if (colorsChanged) {
    lv_obj_set_style_arc_color(mouthArc, lv_color_hex(target.glowHex), LV_PART_MAIN);
    lv_obj_set_style_border_color(rootBox,
      lv_color_hex(baseMood == BUDDY_STUDYING ? 0x00A8CC : (baseMood == BUDDY_BREAK ? 0x00B870 : 0x163859)), 0);
  }
  lv_obj_set_size(mouthArc, target.mouthW, target.mouthH);
  lv_obj_set_pos(mouthArc, (WIDGET_W - target.mouthW) / 2 + target.mouthOffsetX, target.mouthOffsetY);
  if (forceFirstRender || lastMouthStart != target.mouthStart || lastMouthEnd != target.mouthEnd) {
    lv_arc_set_bg_angles(mouthArc, target.mouthStart, target.mouthEnd);
    lastMouthStart = target.mouthStart;
    lastMouthEnd = target.mouthEnd;
  }

  lastGlowHex = target.glowHex;
  lastCoreHex = target.coreHex;
  forceFirstRender = false;
}