#include "PomodoroScene.h"
#include <Arduino.h>

namespace {
  constexpr int SCENE_W = 196;
  constexpr int SCENE_H = 112; // Enlarged from 88 to fill vertical space cleanly

  lv_obj_t* sceneCard    = nullptr;
  lv_timer_t* sceneTimer = nullptr;

  // JARVIS Concentric HUD Rings
  lv_obj_t* hudOuterArc  = nullptr;
  lv_obj_t* hudInnerArc  = nullptr;

  // Charging Dock (visible on Break)
  lv_obj_t* dockPad      = nullptr;
  lv_obj_t* steamDot1    = nullptr;
  lv_obj_t* steamDot2    = nullptr;

  // Vector Robot Chassis & Articulated Claws
  lv_obj_t* rexHead      = nullptr;
  lv_obj_t* rexEarL      = nullptr;
  lv_obj_t* rexEarR      = nullptr;
  lv_obj_t* rexVisor     = nullptr;
  lv_obj_t* rexEyeL      = nullptr;
  lv_obj_t* rexEyeR      = nullptr;
  lv_obj_t* rexCheekL    = nullptr;
  lv_obj_t* rexCheekR    = nullptr;
  lv_obj_t* rexRearCore  = nullptr;
  lv_obj_t* rexThrusterL = nullptr;
  lv_obj_t* rexThrusterR = nullptr;
  lv_obj_t* rexClawL     = nullptr;
  lv_obj_t* rexClawR     = nullptr;

  // Desk Props (Book & Laptop)
  lv_obj_t* propGroup    = nullptr;
  lv_obj_t* bookBase     = nullptr;
  lv_obj_t* bookPageFlip = nullptr;
  lv_obj_t* laptopScreen = nullptr;
  lv_obj_t* laptopBase   = nullptr;
  lv_obj_t* codeBar1     = nullptr;
  lv_obj_t* codeBar2     = nullptr;

  PomodoroPose currentPose = POMO_POSE_IDLE_DESK;
  uint32_t animTick   = 0;
  int16_t  turnProg   = 0; // 0..256 fixed-point progress (0 = front desk, 256 = docked rear view)
  uint32_t lastColorAccent = 0xFFFFFFFF;

  // Dirty-guard helpers so SPI only redraws pixels that actually moved
  void setPosIfChanged(lv_obj_t* obj, int x, int y) {
    if (!obj) return;
    if (lv_obj_get_x(obj) != x || lv_obj_get_y(obj) != y) {
      lv_obj_set_pos(obj, x, y);
    }
  }

  void setSizeIfChanged(lv_obj_t* obj, int w, int h) {
    if (!obj) return;
    if (lv_obj_get_width(obj) != w || lv_obj_get_height(obj) != h) {
      lv_obj_set_size(obj, w, h);
    }
  }

  lv_obj_t* makeCleanBox(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
  }

  // Smooth triangular wave 0..amp..0 over `period` ticks
  int smoothWave(uint32_t tick, uint32_t period, int amp) {
    if (period < 2) return 0;
    const uint32_t half = period / 2;
    const uint32_t phase = tick % period;
    const uint32_t dist = (phase < half) ? phase : (period - phase);
    return static_cast<int>((dist * amp) / half);
  }

  void renderSceneFrame() {
    if (!sceneCard || !rexHead) return;
    if (lv_obj_get_screen(sceneCard) != lv_screen_active()) return;

    animTick++;

    const bool isBreakPose = (currentPose == POMO_POSE_HEADING_BACK ||
                              currentPose == POMO_POSE_CHILL_BREAK);

    // 1. Continuous ~22 FPS JARVIS HUD Ring Rotation
    const int16_t outerStart = (animTick * 4) % 360;
    const int16_t innerStart = 360 - ((animTick * 6) % 360);
    lv_arc_set_angles(hudOuterArc, outerStart, (outerStart + 135) % 360);
    lv_arc_set_angles(hudInnerArc, innerStart, (innerStart + 115) % 360);

    if (!isBreakPose) {
      // Smoothly return turnProg to 0 if resuming focus from break
      if (turnProg > 0) {
        turnProg = (turnProg > 18) ? (turnProg - 18) : 0;
      }

      const bool activeStudy = (currentPose != POMO_POSE_IDLE_DESK);
      const bool isLaptop    = (currentPose == POMO_POSE_STUDY_LAPTOP);
      const uint32_t accent  = activeStudy ? (isLaptop ? 0x00FF9C : 0x00E5FF) : 0xFF9E00;

      if (accent != lastColorAccent) {
        lastColorAccent = accent;
        lv_obj_set_style_border_color(sceneCard, lv_color_hex(accent), 0);
        lv_obj_set_style_arc_color(hudOuterArc, lv_color_hex(accent), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(hudInnerArc, lv_color_hex(activeStudy ? 0x00FFFF : 0xFF9E00), LV_PART_INDICATOR);
        lv_obj_set_style_border_color(rexHead, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_color(rexEarL, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_color(rexEarR, lv_color_hex(accent), 0);
        lv_obj_set_style_border_color(rexClawL, lv_color_hex(accent), 0);
        lv_obj_set_style_border_color(rexClawR, lv_color_hex(accent), 0);
      }

      lv_obj_add_flag(dockPad, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(steamDot1, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(steamDot2, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(rexRearCore, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(rexThrusterL, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(rexThrusterR, LV_OBJ_FLAG_HIDDEN);

      lv_obj_remove_flag(rexVisor, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(rexClawL, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(rexClawR, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(propGroup, LV_OBJ_FLAG_HIDDEN);

      // Smooth 4px vertical breathing float
      const int bobY = 14 + smoothWave(animTick, activeStudy ? 28 : 44, 4);
      setSizeIfChanged(rexHead, 68, 52);
      setPosIfChanged(rexHead, 64, bobY);

      setSizeIfChanged(rexVisor, 54, 34);
      setPosIfChanged(rexVisor, 5, 7);

      setPosIfChanged(rexEarL, 57, bobY + 14);
      setPosIfChanged(rexEarR, 132, bobY + 14);

      // Smooth Eye Blink & Reading Scan
      const uint32_t blinkMod = animTick % 72;
      if (blinkMod < 3) {
        setSizeIfChanged(rexEyeL, 15, 3);
        setSizeIfChanged(rexEyeR, 15, 3);
        setPosIfChanged(rexEyeL, 8, 16);
        setPosIfChanged(rexEyeR, 29, 16);
      } else {
        const int gazeX = activeStudy ? (7 + smoothWave(animTick, 36, 5)) : 9;
        const int gazeY = activeStudy ? 13 : 9;
        setSizeIfChanged(rexEyeL, 14, 14);
        setSizeIfChanged(rexEyeR, 14, 14);
        setPosIfChanged(rexEyeL, gazeX, gazeY);
        setPosIfChanged(rexEyeR, gazeX + 21, gazeY);
      }

      // Smooth Articulated Claw Motion
      const int clawLOff = activeStudy ? smoothWave(animTick, 14, 3) : 0;
      const int clawROff = activeStudy ? smoothWave(animTick + 7, 14, 3) : 0;
      setPosIfChanged(rexClawL, 48, 70 - clawLOff);
      setPosIfChanged(rexClawR, 134, 70 - clawROff);

      if (!isLaptop) {
        lv_obj_remove_flag(bookBase, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(laptopScreen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(laptopBase, LV_OBJ_FLAG_HIDDEN);

        // Smooth page-turn sweep every ~2.5s
        const uint32_t pageCycle = animTick % 56;
        int pageX = 36;
        if (activeStudy && pageCycle < 10) {
          pageX = 36 - static_cast<int>((pageCycle * 22) / 10);
        }
        setPosIfChanged(bookPageFlip, pageX, 2);
      } else {
        lv_obj_add_flag(bookBase, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(laptopScreen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(laptopBase, LV_OBJ_FLAG_HIDDEN);

        const int w1 = activeStudy ? (14 + smoothWave(animTick, 18, 26)) : 20;
        const int w2 = activeStudy ? (18 + smoothWave(animTick + 9, 22, 28)) : 28;
        setSizeIfChanged(codeBar1, w1, 3);
        setSizeIfChanged(codeBar2, w2, 3);
      }
    } else {
      // === SMOOTH 180-DEGREE TURN & ROLL TO CHARGING DOCK ===
      if (lastColorAccent != 0x00FF9C) {
        lastColorAccent = 0x00FF9C;
        lv_obj_set_style_border_color(sceneCard, lv_color_hex(0x00FF9C), 0);
        lv_obj_set_style_arc_color(hudOuterArc, lv_color_hex(0x00FF9C), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(hudInnerArc, lv_color_hex(0x7CFFC4), LV_PART_INDICATOR);
        lv_obj_set_style_border_color(rexHead, lv_color_hex(0x00FF9C), 0);
        lv_obj_set_style_bg_color(rexEarL, lv_color_hex(0x00FF9C), 0);
        lv_obj_set_style_bg_color(rexEarR, lv_color_hex(0x00FF9C), 0);
      }

      lv_obj_add_flag(propGroup, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(rexClawL, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(rexClawR, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(dockPad, LV_OBJ_FLAG_HIDDEN);

      if (turnProg < 256) {
        turnProg += 14; // Smooth ~0.8s turn & roll
        if (turnProg >= 256) {
          turnProg = 256;
          currentPose = POMO_POSE_CHILL_BREAK;
        }
      }

      // Smoothly interpolate X (64 -> 122), Size (68x52 -> 54x42), and Y
      const int headX = 64 + ((turnProg * 58) >> 8);
      const int headW = 68 - ((turnProg * 14) >> 8);
      const int headH = 52 - ((turnProg * 10) >> 8);
      const int baseY = 14 + ((turnProg * 28) >> 8);
      const int bobY  = (turnProg < 256)
        ? (baseY - smoothWave(turnProg / 14, 6, 5))
        : (baseY + smoothWave(animTick, 32, 3));

      setSizeIfChanged(rexHead, headW, headH);
      setPosIfChanged(rexHead, headX, bobY);
      setPosIfChanged(rexEarL, headX - 5, bobY + 12);
      setPosIfChanged(rexEarR, headX + headW - 2, bobY + 12);

      if (turnProg < 128) {
        // Front visor sweeps smoothly across the chassis edge during the turn
        lv_obj_remove_flag(rexVisor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(rexRearCore, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(rexThrusterL, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(rexThrusterR, LV_OBJ_FLAG_HIDDEN);

        const int visorW = 54 - ((turnProg * 48) >> 7);
        const int visorX = 5 + ((turnProg * 48) >> 7);
        setSizeIfChanged(rexVisor, visorW > 4 ? visorW : 4, 30);
        setPosIfChanged(rexVisor, visorX, 7);
      } else {
        // Rear chassis revealed: glowing spine reactor & twin thrusters
        lv_obj_add_flag(rexVisor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(rexRearCore, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(rexThrusterL, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(rexThrusterR, LV_OBJ_FLAG_HIDDEN);

        setPosIfChanged(rexRearCore, (headW - 18) / 2, 8);
        setPosIfChanged(rexThrusterL, 6, headH - 11);
        setPosIfChanged(rexThrusterR, headW - 14, headH - 11);
      }

      if (turnProg >= 256) {
        lv_obj_remove_flag(steamDot1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(steamDot2, LV_OBJ_FLAG_HIDDEN);
        const int p1 = (animTick % 24);
        const int p2 = ((animTick + 12) % 24);
        setPosIfChanged(steamDot1, 134, 34 - p1);
        setPosIfChanged(steamDot2, 160, 36 - p2);
      } else {
        lv_obj_add_flag(steamDot1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(steamDot2, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  void onSceneTimerTick(lv_timer_t* t) {
    LV_UNUSED(t);
    renderSceneFrame();
  }
}

lv_obj_t* createPomodoroScene(lv_obj_t* parent) {
  sceneCard = makeCleanBox(parent);
  lv_obj_set_size(sceneCard, SCENE_W, SCENE_H);
  lv_obj_set_style_bg_color(sceneCard, lv_color_hex(0x040B14), 0);
  lv_obj_set_style_border_color(sceneCard, lv_color_hex(0x00A8CC), 0);
  lv_obj_set_style_border_width(sceneCard, 1, 0);
  lv_obj_set_style_radius(sceneCard, 14, 0);

  // Subtle floor horizon line
  lv_obj_t* floorLine = makeCleanBox(sceneCard);
  lv_obj_set_size(floorLine, 176, 1);
  lv_obj_set_pos(floorLine, 10, 98);
  lv_obj_set_style_bg_color(floorLine, lv_color_hex(0x163859), 0);
  lv_obj_set_style_border_width(floorLine, 0, 0);

  // 1. JARVIS Outer Segmented HUD Ring (enlarged to match 196x112 card)
  hudOuterArc = lv_arc_create(sceneCard);
  lv_obj_remove_flag(hudOuterArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(hudOuterArc, 126, 126);
  lv_obj_set_pos(hudOuterArc, 35, -7);
  lv_arc_set_bg_angles(hudOuterArc, 0, 360);
  lv_arc_set_angles(hudOuterArc, 20, 150);
  lv_obj_set_style_opa(hudOuterArc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_width(hudOuterArc, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_width(hudOuterArc, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(hudOuterArc, lv_color_hex(0x0B2239), LV_PART_MAIN);
  lv_obj_set_style_arc_color(hudOuterArc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);

  // 2. JARVIS Inner Counter-Rotating Ring
  hudInnerArc = lv_arc_create(sceneCard);
  lv_obj_remove_flag(hudInnerArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(hudInnerArc, 98, 98);
  lv_obj_set_pos(hudInnerArc, 49, 7);
  lv_arc_set_bg_angles(hudInnerArc, 0, 360);
  lv_arc_set_angles(hudInnerArc, 190, 330);
  lv_obj_set_style_opa(hudInnerArc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_arc_width(hudInnerArc, 1, LV_PART_MAIN);
  lv_obj_set_style_arc_width(hudInnerArc, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(hudInnerArc, lv_color_hex(0x08192B), LV_PART_MAIN);
  lv_obj_set_style_arc_color(hudInnerArc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR);

  // 3. Wireless Charging Dock Pad (Right-rear, shown during Break)
  dockPad = makeCleanBox(sceneCard);
  lv_obj_set_size(dockPad, 56, 11);
  lv_obj_set_pos(dockPad, 121, 86);
  lv_obj_set_style_bg_color(dockPad, lv_color_hex(0x06241B), 0);
  lv_obj_set_style_border_color(dockPad, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(dockPad, 1, 0);
  lv_obj_set_style_radius(dockPad, 5, 0);
  lv_obj_add_flag(dockPad, LV_OBJ_FLAG_HIDDEN);

  steamDot1 = makeCleanBox(sceneCard);
  lv_obj_set_size(steamDot1, 5, 5);
  lv_obj_set_style_radius(steamDot1, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(steamDot1, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(steamDot1, 0, 0);
  lv_obj_add_flag(steamDot1, LV_OBJ_FLAG_HIDDEN);

  steamDot2 = makeCleanBox(sceneCard);
  lv_obj_set_size(steamDot2, 4, 4);
  lv_obj_set_style_radius(steamDot2, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(steamDot2, lv_color_hex(0x7CFFC4), 0);
  lv_obj_set_style_border_width(steamDot2, 0, 0);
  lv_obj_add_flag(steamDot2, LV_OBJ_FLAG_HIDDEN);

  // 4. Vector Robot Ear Pods
  rexEarL = makeCleanBox(sceneCard);
  lv_obj_set_size(rexEarL, 8, 22);
  lv_obj_set_pos(rexEarL, 57, 28);
  lv_obj_set_style_radius(rexEarL, 3, 0);
  lv_obj_set_style_bg_color(rexEarL, lv_color_hex(0x00A8CC), 0);
  lv_obj_set_style_border_width(rexEarL, 0, 0);

  rexEarR = makeCleanBox(sceneCard);
  lv_obj_set_size(rexEarR, 8, 22);
  lv_obj_set_pos(rexEarR, 132, 28);
  lv_obj_set_style_radius(rexEarR, 3, 0);
  lv_obj_set_style_bg_color(rexEarR, lv_color_hex(0x00A8CC), 0);
  lv_obj_set_style_border_width(rexEarR, 0, 0);

  // 5. Enlarged Vector Robot Head Chassis (68x52)
  rexHead = makeCleanBox(sceneCard);
  lv_obj_set_size(rexHead, 68, 52);
  lv_obj_set_pos(rexHead, 64, 14);
  lv_obj_set_style_radius(rexHead, 16, 0);
  lv_obj_set_style_bg_color(rexHead, lv_color_hex(0x0A1E32), 0);
  lv_obj_set_style_border_color(rexHead, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_border_width(rexHead, 2, 0);

  // Glossy Dark OLED Visor (54x34)
  rexVisor = makeCleanBox(rexHead);
  lv_obj_set_size(rexVisor, 54, 34);
  lv_obj_set_pos(rexVisor, 5, 7);
  lv_obj_set_style_radius(rexVisor, 10, 0);
  lv_obj_set_style_bg_color(rexVisor, lv_color_hex(0x020810), 0);
  lv_obj_set_style_border_color(rexVisor, lv_color_hex(0x0E3A5D), 0);
  lv_obj_set_style_border_width(rexVisor, 1, 0);

  // Expressive Vector Eyes (14x14)
  rexEyeL = makeCleanBox(rexVisor);
  lv_obj_set_size(rexEyeL, 14, 14);
  lv_obj_set_pos(rexEyeL, 8, 9);
  lv_obj_set_style_radius(rexEyeL, 4, 0);
  lv_obj_set_style_bg_color(rexEyeL, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_border_width(rexEyeL, 0, 0);

  rexEyeR = makeCleanBox(rexVisor);
  lv_obj_set_size(rexEyeR, 14, 14);
  lv_obj_set_pos(rexEyeR, 29, 9);
  lv_obj_set_style_radius(rexEyeR, 4, 0);
  lv_obj_set_style_bg_color(rexEyeR, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_border_width(rexEyeR, 0, 0);

  // Subtle Holographic Cheek Accents
  rexCheekL = makeCleanBox(rexVisor);
  lv_obj_set_size(rexCheekL, 6, 2);
  lv_obj_set_pos(rexCheekL, 6, 27);
  lv_obj_set_style_bg_color(rexCheekL, lv_color_hex(0x00A8CC), 0);
  lv_obj_set_style_border_width(rexCheekL, 0, 0);

  rexCheekR = makeCleanBox(rexVisor);
  lv_obj_set_size(rexCheekR, 6, 2);
  lv_obj_set_pos(rexCheekR, 40, 27);
  lv_obj_set_style_bg_color(rexCheekR, lv_color_hex(0x00A8CC), 0);
  lv_obj_set_style_border_width(rexCheekR, 0, 0);

  // Rear Chassis Elements
  rexRearCore = makeCleanBox(rexHead);
  lv_obj_set_size(rexRearCore, 18, 22);
  lv_obj_set_pos(rexRearCore, 18, 8);
  lv_obj_set_style_radius(rexRearCore, 5, 0);
  lv_obj_set_style_bg_color(rexRearCore, lv_color_hex(0x062B20), 0);
  lv_obj_set_style_border_color(rexRearCore, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(rexRearCore, 2, 0);
  lv_obj_add_flag(rexRearCore, LV_OBJ_FLAG_HIDDEN);

  rexThrusterL = makeCleanBox(rexHead);
  lv_obj_set_size(rexThrusterL, 8, 6);
  lv_obj_set_pos(rexThrusterL, 6, 31);
  lv_obj_set_style_radius(rexThrusterL, 2, 0);
  lv_obj_set_style_bg_color(rexThrusterL, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(rexThrusterL, 0, 0);
  lv_obj_add_flag(rexThrusterL, LV_OBJ_FLAG_HIDDEN);

  rexThrusterR = makeCleanBox(rexHead);
  lv_obj_set_size(rexThrusterR, 8, 6);
  lv_obj_set_pos(rexThrusterR, 40, 31);
  lv_obj_set_style_radius(rexThrusterR, 2, 0);
  lv_obj_set_style_bg_color(rexThrusterR, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(rexThrusterR, 0, 0);
  lv_obj_add_flag(rexThrusterR, LV_OBJ_FLAG_HIDDEN);

  // 6. Articulated Robot Claws
  rexClawL = makeCleanBox(sceneCard);
  lv_obj_set_size(rexClawL, 14, 11);
  lv_obj_set_pos(rexClawL, 48, 70);
  lv_obj_set_style_radius(rexClawL, 4, 0);
  lv_obj_set_style_bg_color(rexClawL, lv_color_hex(0x0D2B45), 0);
  lv_obj_set_style_border_color(rexClawL, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_border_width(rexClawL, 1, 0);

  rexClawR = makeCleanBox(sceneCard);
  lv_obj_set_size(rexClawR, 14, 11);
  lv_obj_set_pos(rexClawR, 134, 70);
  lv_obj_set_style_radius(rexClawR, 4, 0);
  lv_obj_set_style_bg_color(rexClawR, lv_color_hex(0x0D2B45), 0);
  lv_obj_set_style_border_color(rexClawR, lv_color_hex(0x00E5FF), 0);
  lv_obj_set_style_border_width(rexClawR, 1, 0);

  // 7. Desk Prop Assembly
  propGroup = makeCleanBox(sceneCard);
  lv_obj_set_size(propGroup, 82, 30);
  lv_obj_set_pos(propGroup, 57, 68);
  lv_obj_set_style_bg_opa(propGroup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(propGroup, 0, 0);

  // Open Book Prop
  bookBase = makeCleanBox(propGroup);
  lv_obj_set_size(bookBase, 70, 21);
  lv_obj_set_pos(bookBase, 6, 7);
  lv_obj_set_style_radius(bookBase, 4, 0);
  lv_obj_set_style_bg_color(bookBase, lv_color_hex(0x082038), 0);
  lv_obj_set_style_border_color(bookBase, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_border_width(bookBase, 2, 0);

  bookPageFlip = makeCleanBox(bookBase);
  lv_obj_set_size(bookPageFlip, 28, 13);
  lv_obj_set_pos(bookPageFlip, 36, 2);
  lv_obj_set_style_radius(bookPageFlip, 2, 0);
  lv_obj_set_style_bg_color(bookPageFlip, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_bg_opa(bookPageFlip, LV_OPA_50, 0);
  lv_obj_set_style_border_width(bookPageFlip, 0, 0);

  // Mini Laptop Prop
  laptopScreen = makeCleanBox(propGroup);
  lv_obj_set_size(laptopScreen, 58, 21);
  lv_obj_set_pos(laptopScreen, 12, 2);
  lv_obj_set_style_radius(laptopScreen, 4, 0);
  lv_obj_set_style_bg_color(laptopScreen, lv_color_hex(0x041322), 0);
  lv_obj_set_style_border_color(laptopScreen, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(laptopScreen, 2, 0);
  lv_obj_add_flag(laptopScreen, LV_OBJ_FLAG_HIDDEN);

  laptopBase = makeCleanBox(propGroup);
  lv_obj_set_size(laptopBase, 68, 6);
  lv_obj_set_pos(laptopBase, 7, 23);
  lv_obj_set_style_radius(laptopBase, 2, 0);
  lv_obj_set_style_bg_color(laptopBase, lv_color_hex(0x0D3559), 0);
  lv_obj_set_style_border_color(laptopBase, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_border_width(laptopBase, 1, 0);
  lv_obj_add_flag(laptopBase, LV_OBJ_FLAG_HIDDEN);

  codeBar1 = makeCleanBox(laptopScreen);
  lv_obj_set_size(codeBar1, 22, 3);
  lv_obj_set_pos(codeBar1, 6, 4);
  lv_obj_set_style_bg_color(codeBar1, lv_color_hex(0x00FF9C), 0);
  lv_obj_set_style_border_width(codeBar1, 0, 0);

  codeBar2 = makeCleanBox(laptopScreen);
  lv_obj_set_size(codeBar2, 32, 3);
  lv_obj_set_pos(codeBar2, 6, 11);
  lv_obj_set_style_bg_color(codeBar2, lv_color_hex(0x00FFFF), 0);
  lv_obj_set_style_border_width(codeBar2, 0, 0);

  // Dedicated 45ms LVGL Timer (~22 FPS) for silky animation
  if (!sceneTimer) {
    sceneTimer = lv_timer_create(onSceneTimerTick, 45, nullptr);
  }

  return sceneCard;
}

void setPomodoroScenePose(PomodoroPose pose) {
  currentPose = pose;
  renderSceneFrame();
}

void updatePomodoroScene() {
  // Driven automatically by the 45ms LVGL timer
}