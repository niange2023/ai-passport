// main/tower_ui.h
// 「AI 工位看板 (6 工位大字人话版)」LVGL 240x320 界面渲染接口。
//
// 界面采用 2 列 × 3 行共 6 个大像素工位布局（全屏无重复区域、无英文缩写密码）：
// - 每个工位直接显示「工具名·动作类型」（如「Trae·写文章」「Cursor·写代码」）；
// - 中间绘制动态像素小人与电脑桌；
// - 底部胶囊条直接写中文大白话状态（「等你确认」「跑完了!」「正在用」「后台跑着」「休息中」）。
#pragma once

#include "lvgl.h"
#include "tower_model.h"
#include "tower_store.h"

typedef struct {
    lv_obj_t *card;
    lv_obj_t *header_label;
    lv_obj_t *desk;
    lv_obj_t *monitor_case;
    lv_obj_t *monitor_screen;
    lv_obj_t *monitor_scan;
    lv_obj_t *monitor_stand;
    lv_obj_t *hair;
    lv_obj_t *head;
    lv_obj_t *eye;
    lv_obj_t *body;
    lv_obj_t *arm_left;
    lv_obj_t *hand_right;
    lv_obj_t *bubble_box;
    lv_obj_t *bubble_label;
    lv_obj_t *sleep_label;
    lv_obj_t *pill;
    lv_obj_t *status_label;
} tower_desk_widgets_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *top_title_label;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_label;
    lv_timer_t *battery_timer;
    // 6 个大像素工位层 (2 列 × 3 行)
    lv_obj_t *main_layer;
    tower_desk_widgets_t desks[TOWER_SLOT_COUNT];
    // 连接状态说明层（长按 OK 切换）
    lv_obj_t *link_layer;
    lv_obj_t *link_mode_label;
    lv_obj_t *link_seq_label;
    lv_obj_t *link_ssid_label;
    lv_obj_t *link_url_label;
    // 底部操作栏
    lv_obj_t *footer_label;
    const lv_font_t *font_small;
    const lv_font_t *font_body;
    const tower_wifi_cfg_t *wifi_cfg;
} tower_ui_t;

void tower_ui_init(
    tower_ui_t *ui,
    const tower_model_t *model,
    const tower_wifi_cfg_t *wifi_cfg,
    const lv_font_t *font_small,
    const lv_font_t *font_body
);

void tower_ui_render(tower_ui_t *ui, const tower_model_t *model);
