#pragma once
#include <lvgl.h>

enum PomodoroPose {
  POMO_POSE_IDLE_DESK,
  POMO_POSE_STUDY_BOOK,
  POMO_POSE_STUDY_LAPTOP,
  POMO_POSE_HEADING_BACK,
  POMO_POSE_CHILL_BREAK
};

// Builds the 196x88 multi-pose REX study & break scene inside `parent`
lv_obj_t* createPomodoroScene(lv_obj_t* parent);

// Sets the current pose (resets walk-back progress when entering POMO_POSE_HEADING_BACK)
void setPomodoroScenePose(PomodoroPose pose);

// Call periodically from updateHomeDashboard(); self-throttled to ~220ms
void updatePomodoroScene();