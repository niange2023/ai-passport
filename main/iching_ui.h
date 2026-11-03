// main/iching_ui.h
// 「知几 · 随身《周易》与破局签 (Pocket I-Ching & Oblique Oracle)」LVGL 240x320 界面接口。
//
// 视觉设计：
// - 遵循「一屏一事 · 大字留白」原则，采用古籍宣纸暖底 + 徽墨 + 朱砂印泥配色；
// - 六爻卦画以醒目朱墨几何长条绘制（阳爻实线、阴爻断线、老阳/老阴动爻以朱砂高亮）；
// - 使用 CONFIG_LV_USE_CLIB_MALLOC 与 lv_screen_load_anim(..., true) 安全切换页面。
#pragma once

#include "iching_model.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_label;
    lv_timer_t *battery_timer;
    const lv_font_t *font_small;  // 14px
    const lv_font_t *font_body;   // 16px
    const lv_font_t *font_title;  // 20px
} iching_ui_t;

void iching_ui_init(
    iching_ui_t *ui,
    const iching_model_t *model,
    const lv_font_t *font_small,
    const lv_font_t *font_body,
    const lv_font_t *font_title
);

void iching_ui_render(iching_ui_t *ui, const iching_model_t *model);
void iching_ui_show_sleeping(iching_ui_t *ui);
