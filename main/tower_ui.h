// main/tower_ui.h
// 「3-B 双焦像素总控台 (Command Tower)」LVGL 240x320 界面渲染接口。
//
// 界面分为上下双焦结构：
// 1. 顶部状态栏 (0..24px)：标题 + 链路标志 + 紧急计数 + 右上角实时电量。
// 2. 上屏双焦大工位 (26..138px)：左工位固定锁定头号紧急窗口，右工位跟随当前光标或二号紧急窗口，
//    内含动态像素小人（举手呼救/报喜、敲键盘、趴桌休息）与像素显示器。
// 3. 下屏 4x3 十二窗口雷达矩阵 (142..292px)：12 个工作窗口状态色块与时长一览无余，全程零敏感文字。
// 4. 底部操作栏 (296..320px)：按键指引与切窗/收菜瞬态反馈。
#pragma once

#include "lvgl.h"
#include "tower_model.h"
#include "tower_store.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_label;
    lv_timer_t *battery_timer;
    const lv_font_t *font_small;
    const lv_font_t *font_body;
    const tower_wifi_cfg_t *wifi_cfg;
} tower_ui_t;

// 初始化 UI 上下文并构建首次屏幕对象树（调用方须持有 bsp_lvgl_lock）。
void tower_ui_init(
    tower_ui_t *ui,
    const tower_model_t *model,
    const tower_wifi_cfg_t *wifi_cfg,
    const lv_font_t *font_small,
    const lv_font_t *font_body
);

// 根据最新状态模型重建并刷新当前屏幕（调用方须持有 bsp_lvgl_lock）。
void tower_ui_render(tower_ui_t *ui, const tower_model_t *model);
