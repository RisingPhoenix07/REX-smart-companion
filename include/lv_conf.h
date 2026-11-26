#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_CONF 1

/* Color & OS */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE

/* Memory: Use system malloc/free (LVGL 9.x) so .dram0.bss stays small */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Display & Software Renderer */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1

/* Fonts for Stage 1-3 REX UI */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/* Essential Widgets */
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_BAR 1
#define LV_USE_ARC 1
#define LV_USE_IMAGE 1
#define LV_USE_OBJ 1

/* Disable unused widgets to save flash & DRAM */
#define LV_USE_ANIMIMG    0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LINE       0
#define LV_USE_LIST       0
#define LV_USE_LOTTIE     0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TEXTAREA   0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/* Animations */
#define LV_USE_ANIM 1

/* Disable unused features to save flash & RAM */
#define LV_USE_LOG 0
#define LV_USE_FS_STDIO 0
#define LV_USE_FREETYPE 0
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif /* LV_CONF_H */