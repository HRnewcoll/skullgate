/**
 * @file lv_conf.h
 * @brief LVGL 8.x configuration for SkullGate on ESP32.
 *
 * Memory budget:
 *   - 32 KB draw buffer (allocated in UiManager)
 *   - Minimal font set (Montserrat 14 + 20)
 *   - Basic widgets only (label, button, textarea, list)
 *
 * Keep RAM usage under 40 KB total for LVGL on ESP32 without PSRAM.
 * With PSRAM (ESP32-S3), increase LV_MEM_SIZE as needed.
 */

#if 1 /* Enable lv_conf.h processing */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOUR DEPTH
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
   MEMORY SETTINGS
 *====================*/
/* 32 KB heap for LVGL */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (32U * 1024U)

/* Stack size of the main LVGL task (not used in Arduino — single-threaded) */
#define LV_TASK_HANDLER_INCLUDE <stdint.h>

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 0

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/*====================
   ASSERTS
 *====================*/
#define LV_USE_ASSERT_NULL         1
#define LV_USE_ASSERT_MALLOC       1
#define LV_USE_ASSERT_OBJ          0
#define LV_USE_ASSERT_STYLE        0

/*====================
   FONT USAGE
 *====================*/
/* Built-in fonts */
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1   /* Default body text */
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1   /* Headings */
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_48  0

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGET USAGE
 *====================*/
#define LV_USE_ARC        0
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        0
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     1
#define LV_USE_TABLE      1
#define LV_USE_TEXTAREA   1
#define LV_USE_LIST       1
#define LV_USE_MSGBOX     0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_SPAN       0
#define LV_USE_METER      0
#define LV_USE_ANIMIMG    0
#define LV_USE_LED        0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   1   /* On-screen keyboard */
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_MENU       0

/*====================
   EXTRA COMPONENTS
 *====================*/
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY   0
#define LV_USE_GRIDNAV  0
#define LV_USE_FRAGMENT 0
#define LV_USE_MSGBOX   0
#define LV_USE_SPINBOX  0

/*====================
   THEME
 *====================*/
/* Use the default dark theme — suits cyber-deck aesthetic */
#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 1

/*====================
   LAYOUT
 *====================*/
#define LV_USE_FLEX  1
#define LV_USE_GRID  0

#endif /* LV_CONF_H */
#endif /* End of file guard */
