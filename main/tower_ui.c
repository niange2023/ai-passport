// main/tower_ui.c
// 「AI 工位看板 (6 工位大字人话版)」LVGL 240x320 竖屏界面实现。
//
// 严格遵守运行时规则：
// - 所有 LVGL 对象在 tower_ui_init() 中一次性创建，tower_ui_render() 原地更新属性与文本，
//   运行期零 lv_obj_create / lv_obj_delete。
// - 一屏展示 2 列 × 3 行共 6 个大像素工位，直接显示「工具名·动作」与「等你确认/跑完了!/正在用/后台跑着/休息中」。
#include "tower_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"

#define COLOR_BG_MAIN       0x161E1B
#define COLOR_BG_BAR        0x0F1513
#define COLOR_BG_PANEL      0x202A25
#define COLOR_TEXT_MAIN     0xF2F6F3
#define COLOR_TEXT_SUB      0xA8B8AF
#define COLOR_TEXT_MUTED    0x78887F
#define COLOR_CURSOR_GOLD   0xFFE66D

#define COLOR_WAIT_BORDER   0xEB5F37
#define COLOR_WAIT_BG       0x3E1F1A
#define COLOR_DONE_BORDER   0x41C373
#define COLOR_DONE_BG       0x183625
#define COLOR_ACTIVE_BORDER 0xF0D250
#define COLOR_ACTIVE_BG     0x36331C
#define COLOR_RUN_BORDER    0x4B91EB
#define COLOR_RUN_BG        0x1A2838
#define COLOR_IDLE_BORDER   0x46504B
#define COLOR_IDLE_BG       0x1F2522

#define COLOR_DESK_WOOD     0x947E56
#define COLOR_MONITOR_CASE  0x2A322E
#define COLOR_SKIN          0xE0BC8E
#define COLOR_HAIR          0x3E4A3C

static uint32_t state_accent_color(uint8_t state)
{
    switch ((tower_state_t)state) {
    case TOWER_STATE_WAIT:
        return COLOR_WAIT_BORDER;
    case TOWER_STATE_DONE:
        return COLOR_DONE_BORDER;
    case TOWER_STATE_ACTIVE:
        return COLOR_ACTIVE_BORDER;
    case TOWER_STATE_RUN:
        return COLOR_RUN_BORDER;
    case TOWER_STATE_IDLE:
    default:
        return COLOR_IDLE_BORDER;
    }
}

static uint32_t state_bg_color(uint8_t state)
{
    switch ((tower_state_t)state) {
    case TOWER_STATE_WAIT:
        return COLOR_WAIT_BG;
    case TOWER_STATE_DONE:
        return COLOR_DONE_BG;
    case TOWER_STATE_ACTIVE:
        return COLOR_ACTIVE_BG;
    case TOWER_STATE_RUN:
        return COLOR_RUN_BG;
    case TOWER_STATE_IDLE:
    default:
        return COLOR_IDLE_BG;
    }
}

static lv_obj_t *pixel_box(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h,
    uint32_t bg_hex,
    int radius,
    uint32_t border_hex,
    int border_w
)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, border_w, 0);
    if (border_w > 0) {
        lv_obj_set_style_border_color(obj, lv_color_hex(border_hex), 0);
    }
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *text_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    uint32_t color_hex,
    int x,
    int y,
    int width,
    lv_text_align_t align
)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, width);
    return lbl;
}

static void set_visible(lv_obj_t *obj, bool visible)
{
    if (!obj) return;
    if (visible) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_battery_widget(tower_ui_t *ui)
{
    if (!ui || !ui->battery_fill || !ui->battery_label) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_obj_set_width(ui->battery_fill, 0);
        lv_label_set_text(ui->battery_label, "--");
        return;
    }
    if (soc > 100) soc = 100;
    int fill_w = (soc * 14 + 99) / 100;
    if (soc > 0 && fill_w < 2) fill_w = 2;
    lv_obj_set_width(ui->battery_fill, fill_w);
    lv_obj_set_style_bg_color(
        ui->battery_fill,
        lv_color_hex(soc < 20 ? COLOR_WAIT_BORDER : COLOR_DONE_BORDER),
        0
    );
    lv_label_set_text_fmt(ui->battery_label, "%d%%", soc);
}

static void on_battery_timer(lv_timer_t *timer)
{
    tower_ui_t *ui = (tower_ui_t *)lv_timer_get_user_data(timer);
    refresh_battery_widget(ui);
}

// 一次性创建单个大像素工位卡片 (108 x 84) 的子控件
static void init_desk_widgets(
    tower_ui_t *ui,
    lv_obj_t *parent,
    tower_desk_widgets_t *w,
    int x,
    int y
)
{
    w->card = pixel_box(parent, x, y, 108, 84, COLOR_RUN_BG, 8, COLOR_RUN_BORDER, 2);
    w->header_label = text_label(
        w->card, "Trae·写文章", ui->font_small, COLOR_TEXT_MAIN, 6, 3, 96, LV_TEXT_ALIGN_LEFT
    );

    // 办公桌面与右侧像素显示器
    w->desk = pixel_box(w->card, 8, 53, 92, 4, COLOR_DESK_WOOD, 1, 0, 0);
    w->monitor_case = pixel_box(w->card, 60, 30, 32, 21, COLOR_MONITOR_CASE, 2, COLOR_TEXT_MUTED, 1);
    w->monitor_screen = pixel_box(w->card, 63, 33, 26, 14, COLOR_RUN_BORDER, 1, 0, 0);
    w->monitor_scan = pixel_box(w->card, 66, 36, 18, 3, COLOR_TEXT_MAIN, 0, 0, 0);
    w->monitor_stand = pixel_box(w->card, 73, 50, 6, 4, COLOR_TEXT_MUTED, 0, 0, 0);

    // 左侧像素打工人小人
    w->body = pixel_box(w->card, 19, 40, 22, 14, COLOR_RUN_BORDER, 2, 0, 0);
    w->head = pixel_box(w->card, 22, 29, 16, 11, COLOR_SKIN, 2, 0, 0);
    w->hair = pixel_box(w->card, 21, 25, 18, 5, COLOR_HAIR, 1, 0, 0);
    w->eye = pixel_box(w->card, 32, 32, 3, 3, COLOR_BG_BAR, 0, 0, 0);
    w->arm_left = pixel_box(w->card, 42, 49, 5, 4, COLOR_SKIN, 2, 0, 0);
    w->hand_right = pixel_box(w->card, 49, 49, 5, 4, COLOR_SKIN, 2, 0, 0);

    // 紧急气泡与闲置睡眠符号
    w->bubble_box = pixel_box(w->card, 41, 20, 18, 16, COLOR_WAIT_BORDER, 3, COLOR_TEXT_MAIN, 1);
    w->bubble_label = text_label(
        w->bubble_box, "?", ui->font_small, COLOR_TEXT_MAIN, 0, 0, 16, LV_TEXT_ALIGN_CENTER
    );
    w->sleep_label = text_label(
        w->card, "zZ", ui->font_small, COLOR_TEXT_MUTED, 41, 26, 22, LV_TEXT_ALIGN_LEFT
    );

    // 底部中文状态胶囊条
    w->pill = pixel_box(w->card, 5, 60, 98, 19, COLOR_BG_BAR, 4, 0, 0);
    w->status_label = text_label(
        w->pill, "后台跑着", ui->font_small, COLOR_TEXT_MAIN, 2, 1, 94, LV_TEXT_ALIGN_CENTER
    );
}

// 原地更新单个像素工位卡片的状态、动画与中文文字
static void update_desk_widgets(
    tower_desk_widgets_t *w,
    uint8_t slot_idx,
    const tower_model_t *model
)
{
    const tower_slot_t *slot = &model->slots[slot_idx];
    bool is_selected = (slot_idx == model->selected_slot);
    uint32_t accent = state_accent_color(slot->state);
    uint32_t border_col = is_selected ? COLOR_CURSOR_GOLD : accent;
    bool is_urgent = (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE);
    bool pulse = is_urgent && ((model->anim_tick & 1U) != 0);

    lv_obj_set_style_bg_color(w->card, lv_color_hex(state_bg_color(slot->state)), 0);
    lv_obj_set_style_border_color(w->card, lv_color_hex(border_col), 0);
    lv_obj_set_style_border_width(w->card, is_selected ? 3 : 2, 0);

    char title_raw[24];
    tower_model_format_title(slot, title_raw, sizeof(title_raw));
    char header_buf[32];
    snprintf(
        header_buf,
        sizeof(header_buf),
        "%s%s",
        is_selected ? "▶" : "",
        title_raw
    );
    lv_label_set_text(w->header_label, header_buf);
    lv_obj_set_style_text_color(
        w->header_label,
        lv_color_hex(is_selected ? COLOR_CURSOR_GOLD : COLOR_TEXT_MAIN),
        0
    );

    // 只有需要用户注意/操作的紧急窗口才有屏幕闪烁与扫描线动画；其余窗口完全静止
    lv_obj_set_style_bg_color(
        w->monitor_screen,
        lv_color_hex(pulse ? COLOR_CURSOR_GOLD : accent),
        0
    );
    lv_obj_set_pos(w->monitor_scan, 66, pulse ? 34 : 39);

    if (slot->state == TOWER_STATE_IDLE) {
        // 休息中：完全静止，无任何动画
        lv_obj_set_pos(w->body, 20, 42);
        lv_obj_set_size(w->body, 18, 11);
        lv_obj_set_style_bg_color(w->body, lv_color_hex(COLOR_IDLE_BORDER), 0);

        lv_obj_set_pos(w->head, 22, 36);
        lv_obj_set_size(w->head, 15, 8);
        lv_obj_set_pos(w->hair, 21, 33);
        lv_obj_set_size(w->hair, 17, 4);

        set_visible(w->eye, false);
        set_visible(w->arm_left, false);
        set_visible(w->hand_right, false);
        set_visible(w->bubble_box, false);
        set_visible(w->sleep_label, true);
        lv_label_set_text(w->sleep_label, "zZ");
    } else if (is_urgent) {
        // 需要用户注意和操作的窗口（等你确认 / 跑完了!）：
        // 小人起立跳跃 + 大幅度高举手臂左右挥舞亮色手掌 + 气泡跳动闪烁
        int head_y = pulse ? 23 : 28;
        lv_obj_set_pos(w->hair, 21, head_y - 4);
        lv_obj_set_size(w->hair, 18, 5);
        lv_obj_set_pos(w->head, 22, head_y);
        lv_obj_set_size(w->head, 16, 11);
        lv_obj_set_pos(w->eye, 32, head_y + 3);
        set_visible(w->eye, true);

        lv_obj_set_pos(w->body, 19, head_y + 11);
        lv_obj_set_size(w->body, 22, pulse ? 19 : 15);
        lv_obj_set_style_bg_color(w->body, lv_color_hex(accent), 0);
        set_visible(w->sleep_label, false);

        // 高举过头顶的大手臂 (arm_left) + 醒目挥舞的大手掌/信号牌 (hand_right)
        lv_obj_set_pos(w->arm_left, pulse ? 9 : 4, pulse ? 18 : 25);
        lv_obj_set_size(w->arm_left, 8, pulse ? 22 : 18);
        lv_obj_set_style_bg_color(w->arm_left, lv_color_hex(COLOR_SKIN), 0);
        set_visible(w->arm_left, true);

        lv_obj_set_pos(w->hand_right, pulse ? 6 : 2, pulse ? 14 : 20);
        lv_obj_set_size(w->hand_right, 13, 8);
        lv_obj_set_style_bg_color(
            w->hand_right,
            lv_color_hex(pulse ? COLOR_CURSOR_GOLD : COLOR_TEXT_MAIN),
            0
        );
        set_visible(w->hand_right, true);

        // 醒目跳动的 ? / ✓ 提示气泡
        set_visible(w->bubble_box, true);
        lv_obj_set_pos(w->bubble_box, 41, pulse ? 16 : 22);
        lv_obj_set_style_bg_color(
            w->bubble_box,
            lv_color_hex(pulse ? COLOR_CURSOR_GOLD : accent),
            0
        );
        lv_obj_set_style_text_color(
            w->bubble_label,
            lv_color_hex(pulse ? COLOR_BG_BAR : COLOR_TEXT_MAIN),
            0
        );
        lv_label_set_text(
            w->bubble_label,
            (slot->state == TOWER_STATE_WAIT) ? "?" : "✓"
        );
    } else {
        // 无需注意的正常工作窗口（正在用 / 后台跑着）：完全静止，双手固定在桌面
        int head_y = 29;
        lv_obj_set_pos(w->hair, 21, head_y - 4);
        lv_obj_set_size(w->hair, 18, 5);
        lv_obj_set_pos(w->head, 22, head_y);
        lv_obj_set_size(w->head, 16, 11);
        lv_obj_set_pos(w->eye, 32, head_y + 3);
        set_visible(w->eye, true);

        lv_obj_set_pos(w->body, 19, head_y + 11);
        lv_obj_set_size(w->body, 22, 14);
        lv_obj_set_style_bg_color(w->body, lv_color_hex(accent), 0);
        set_visible(w->sleep_label, false);

        lv_obj_set_pos(w->arm_left, 42, 49);
        lv_obj_set_size(w->arm_left, 5, 4);
        lv_obj_set_style_bg_color(w->arm_left, lv_color_hex(COLOR_SKIN), 0);
        lv_obj_set_pos(w->hand_right, 49, 49);
        lv_obj_set_size(w->hand_right, 5, 4);
        lv_obj_set_style_bg_color(w->hand_right, lv_color_hex(COLOR_SKIN), 0);
        set_visible(w->arm_left, true);
        set_visible(w->hand_right, true);
        set_visible(w->bubble_box, false);
    }

    uint32_t pill_bg = (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE)
        ? accent
        : COLOR_BG_BAR;
    lv_obj_set_style_bg_color(w->pill, lv_color_hex(pill_bg), 0);

    uint32_t status_col = COLOR_TEXT_MAIN;
    if (slot->state == TOWER_STATE_IDLE) {
        status_col = COLOR_TEXT_MUTED;
    } else if (slot->state == TOWER_STATE_ACTIVE) {
        status_col = COLOR_CURSOR_GOLD;
    }
    lv_label_set_text(w->status_label, tower_model_state_text(slot->state));
    lv_obj_set_style_text_color(w->status_label, lv_color_hex(status_col), 0);
}

void tower_ui_render(tower_ui_t *ui, const tower_model_t *model)
{
    if (!ui || !model || !ui->screen) return;

    // 1. 顶部栏：直接用中文告诉你有几个工位在找你
    uint8_t urgent_total = (uint8_t)(model->wait_count + model->done_count);
    char top_buf[48];
    if (urgent_total > 0) {
        snprintf(top_buf, sizeof(top_buf), "有 %u 个工位在找你", (unsigned)urgent_total);
        lv_obj_set_style_text_color(ui->top_title_label, lv_color_hex(COLOR_CURSOR_GOLD), 0);
    } else {
        snprintf(top_buf, sizeof(top_buf), "AI 工位看板 · 全部正常");
        lv_obj_set_style_text_color(ui->top_title_label, lv_color_hex(COLOR_TEXT_MAIN), 0);
    }
    lv_label_set_text(ui->top_title_label, top_buf);
    refresh_battery_widget(ui);

    // 2. 连接说明页
    if (model->screen == TOWER_SCREEN_LINK) {
        set_visible(ui->main_layer, false);
        set_visible(ui->link_layer, true);

        const char *mode_desc = "状态：内置演示画面";
        if (model->link_mode == TOWER_LINK_USB) {
            mode_desc = "状态：已通过 USB 连上电脑";
        } else if (model->link_mode == TOWER_LINK_WIFI) {
            mode_desc = "状态：已通过 Wi-Fi 连上电脑";
        }
        lv_label_set_text(ui->link_mode_label, mode_desc);

        char seq_buf[48];
        snprintf(seq_buf, sizeof(seq_buf), "已同步刷新：%lu 次", (unsigned long)model->sync_seq);
        lv_label_set_text(ui->link_seq_label, seq_buf);

        if (ui->wifi_cfg && ui->wifi_cfg->configured) {
            char ssid_buf[48];
            snprintf(ssid_buf, sizeof(ssid_buf), "网络: %.24s", ui->wifi_cfg->ssid);
            lv_label_set_text(ui->link_ssid_label, ssid_buf);
            lv_label_set_text(ui->link_url_label, ui->wifi_cfg->url);
        } else {
            lv_label_set_text(ui->link_ssid_label, "当前使用 USB 数据线同步");
            lv_label_set_text(ui->link_url_label, "电脑运行 passport_tower.py");
        }

        lv_label_set_text(ui->footer_label, "按OK:立即刷新  长按OK:返回");
        lv_obj_set_style_text_color(ui->footer_label, lv_color_hex(COLOR_TEXT_SUB), 0);
        return;
    }

    set_visible(ui->link_layer, false);
    set_visible(ui->main_layer, true);

    // 3. 更新 2 列 × 3 行共 6 个大像素工位
    for (uint8_t idx = 0; idx < TOWER_SLOT_COUNT; idx++) {
        update_desk_widgets(&ui->desks[idx], idx, model);
    }

    // 4. 底部操作栏
    if (model->toast_ticks > 0 && model->toast_text[0] != '\0') {
        lv_label_set_text(ui->footer_label, model->toast_text);
        lv_obj_set_style_text_color(ui->footer_label, lv_color_hex(COLOR_CURSOR_GOLD), 0);
    } else {
        lv_label_set_text(ui->footer_label, "上下:选工位  OK:切过去  双击:已看");
        lv_obj_set_style_text_color(ui->footer_label, lv_color_hex(COLOR_TEXT_SUB), 0);
    }
}

void tower_ui_init(
    tower_ui_t *ui,
    const tower_model_t *model,
    const tower_wifi_cfg_t *wifi_cfg,
    const lv_font_t *font_small,
    const lv_font_t *font_body
)
{
    if (!ui || !model) return;
    memset(ui, 0, sizeof(*ui));
    ui->font_small = font_small;
    ui->font_body = font_body;
    ui->wifi_cfg = wifi_cfg;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG_MAIN), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    ui->screen = screen;

    // 顶部状态栏 (0..24px)
    lv_obj_t *bar = pixel_box(screen, 0, 0, 240, 24, COLOR_BG_BAR, 0, 0, 0);
    ui->top_title_label = text_label(
        bar, "有 2 个工位在找你", ui->font_small, COLOR_CURSOR_GOLD, 12, 4, 156, LV_TEXT_ALIGN_LEFT
    );
    lv_obj_t *bat_shell = pixel_box(bar, 170, 6, 18, 11, COLOR_BG_PANEL, 2, COLOR_TEXT_SUB, 1);
    pixel_box(bar, 188, 9, 2, 5, COLOR_TEXT_SUB, 0, 0, 0);
    ui->battery_fill = pixel_box(bat_shell, 1, 1, 12, 7, COLOR_DONE_BORDER, 1, 0, 0);
    ui->battery_label = text_label(
        bar, "--", ui->font_small, COLOR_TEXT_SUB, 192, 4, 36, LV_TEXT_ALIGN_LEFT
    );

    // 主视图层：2 列 × 3 行共 6 个大像素工位 (24..296px)
    ui->main_layer = pixel_box(screen, 0, 24, 240, 272, COLOR_BG_MAIN, 0, 0, 0);
    for (uint8_t idx = 0; idx < TOWER_SLOT_COUNT; idx++) {
        int row = idx / 2;
        int col = idx % 2;
        int cx = 8 + col * 116;
        int cy = 4 + row * 89;
        init_desk_widgets(ui, ui->main_layer, &ui->desks[idx], cx, cy);
    }

    // 连接说明页层（默认隐藏）
    ui->link_layer = pixel_box(screen, 0, 24, 240, 272, COLOR_BG_MAIN, 0, 0, 0);
    lv_obj_t *card1 = pixel_box(ui->link_layer, 10, 8, 220, 82, COLOR_BG_PANEL, 8, COLOR_RUN_BORDER, 2);
    text_label(card1, "电脑联动状态", ui->font_body, COLOR_CURSOR_GOLD, 10, 6, 200, LV_TEXT_ALIGN_LEFT);
    ui->link_mode_label = text_label(
        card1, "状态：内置演示画面", ui->font_small, COLOR_TEXT_MAIN, 10, 32, 200, LV_TEXT_ALIGN_LEFT
    );
    ui->link_seq_label = text_label(
        card1, "已同步刷新：0 次", ui->font_small, COLOR_TEXT_SUB, 10, 54, 200, LV_TEXT_ALIGN_LEFT
    );

    lv_obj_t *card2 = pixel_box(ui->link_layer, 10, 98, 220, 92, COLOR_BG_PANEL, 8, COLOR_DONE_BORDER, 1);
    text_label(card2, "同步方式", ui->font_body, COLOR_DONE_BORDER, 10, 6, 200, LV_TEXT_ALIGN_LEFT);
    ui->link_ssid_label = text_label(
        card2, "当前使用 USB 数据线同步", ui->font_small, COLOR_TEXT_MAIN, 10, 32, 200, LV_TEXT_ALIGN_LEFT
    );
    ui->link_url_label = text_label(
        card2, "电脑运行 passport_tower.py", ui->font_small, COLOR_TEXT_SUB, 10, 54, 200, LV_TEXT_ALIGN_LEFT
    );

    lv_obj_t *card3 = pixel_box(ui->link_layer, 10, 198, 220, 66, COLOR_BG_PANEL, 8, COLOR_IDLE_BORDER, 1);
    text_label(card3, "颜色含义说明", ui->font_small, COLOR_CURSOR_GOLD, 10, 6, 200, LV_TEXT_ALIGN_LEFT);
    text_label(card3, "橙色:等你确认  绿色:跑完了!", ui->font_small, COLOR_TEXT_SUB, 10, 26, 200, LV_TEXT_ALIGN_LEFT);
    text_label(card3, "黄色:正在用    蓝色:后台跑着", ui->font_small, COLOR_TEXT_SUB, 10, 44, 200, LV_TEXT_ALIGN_LEFT);
    set_visible(ui->link_layer, false);

    // 底部操作栏 (296..320px)
    lv_obj_t *footer = pixel_box(screen, 0, 296, 240, 24, COLOR_BG_BAR, 0, 0, 0);
    ui->footer_label = text_label(
        footer,
        "上下:选工位  OK:切过去  双击:已看",
        ui->font_small,
        COLOR_TEXT_SUB,
        10,
        4,
        220,
        LV_TEXT_ALIGN_CENTER
    );

    tower_ui_render(ui, model);
    lv_screen_load(screen);
    ui->battery_timer = lv_timer_create(on_battery_timer, 15000, ui);
}
