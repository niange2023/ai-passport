#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "sutra_model.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_label;
    lv_timer_t *battery_timer;
    const lv_font_t *font_small;
    const lv_font_t *font_body;
    const lv_font_t *font_title;
} sutra_ui_t;

void sutra_ui_init(sutra_ui_t *ui, const sutra_model_t *model,
                   const lv_font_t *font_small, const lv_font_t *font_body,
                   const lv_font_t *font_title);
void sutra_ui_render(sutra_ui_t *ui, const sutra_model_t *model);
void sutra_ui_show_sleeping(sutra_ui_t *ui);
