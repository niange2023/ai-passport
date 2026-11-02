// main/tower_ui.c
// 「3-B 双焦像素总控台 (Command Tower)」LVGL 240x320 竖屏界面实现。
//
// 严格遵守运行时规则：
// - 所有 LVGL 对象创建与更新均在持有 bsp_lvgl_lock() 的上下文中调用。
// - 重建 screen 前先清空引用，使用 lv_screen_load_anim(..., true) 自动释放旧 screen。
// - 所有文本标签显式绑定包含完整中文与 ASCII 字形的 tower_cjk_12 / tower_cjk_14 字体。
#include "tower_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"

// 调色板定义（深色像素总控台主题，高对比度区分五种窗口状态）
#define COLOR_BG_MAIN       0x161E1B
#define COLOR_BG_BAR        0x0F1513
#define COLOR_BG_PANEL      0x202A25
#define COLOR_TEXT_MAIN     0xF2F6F3
#define COLOR_TEXT_SUB      0xA8B8AF
#define COLOR_TEXT_MUTED    0x78887F
#define COLOR_CURSOR_GOLD   0xFFE66D

// 五种窗口状态的主题色（边框高亮色 & 卡片底色）
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

// 像素小人及工位家具颜色
#define COLOR_DESK_WOOD     0x947E56
#define COLOR_MONITOR_CASE  0x2A322E
#define COLOR_SKIN          0xE0BC8E
#define COLOR_HAIR          0x3E4A3C

// 根据状态枚举返回边框/强调色
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

// 根据状态枚举返回卡片背景色
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

// 根据状态枚举返回简短中文状态词
static const char *state_short_label(uint8_t state)
{
    switch ((tower_state_t)state) {
    case TOWER_STATE_WAIT:
        return "!等拍板";
    case TOWER_STATE_DONE:
        return "✓待收菜";
    case TOWER_STATE_ACTIVE:
        return "★当前窗";
    case TOWER_STATE_RUN:
        return "●运行中";
    case TOWER_STATE_IDLE:
    default:
        return "闲置";
    }
}

// 创建无内边距、不可滚动的矩形色块（用于卡片容器与像素画图元）
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

// 创建显式指定中文字体的单行或自动换行标签
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

// 刷新右上角电池电量图标与百分比（读取 bsp_battery_soc，-1 时降级显示 "--"）
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

// 绘制顶部状态栏 (240 x 24)
static void draw_top_bar(
    tower_ui_t *ui,
    lv_obj_t *screen,
    const tower_model_t *model
)
{
    lv_obj_t *bar = pixel_box(screen, 0, 0, 240, 24, COLOR_BG_BAR, 0, 0, 0);

    const char *link_tag = "演示";
    if (model->link_mode == TOWER_LINK_USB) {
        link_tag = "USB";
    } else if (model->link_mode == TOWER_LINK_WIFI) {
        link_tag = "WiFi";
    }

    char left_buf[48];
    snprintf(
        left_buf,
        sizeof(left_buf),
        "总控[%s] !%u ✓%u",
        link_tag,
        (unsigned)model->wait_count,
        (unsigned)model->done_count
    );
    text_label(bar, left_buf, ui->font_small, COLOR_TEXT_MAIN, 12, 4, 156, LV_TEXT_ALIGN_LEFT);

    // 右上角电量显示（避开圆角裁剪区域 x <= 226）
    lv_obj_t *bat_shell = pixel_box(bar, 170, 6, 18, 11, COLOR_BG_PANEL, 2, COLOR_TEXT_SUB, 1);
    pixel_box(bar, 188, 9, 2, 5, COLOR_TEXT_SUB, 0, 0, 0);
    ui->battery_fill = pixel_box(bat_shell, 1, 1, 12, 7, COLOR_DONE_BORDER, 1, 0, 0);
    ui->battery_label = text_label(
        bar, "--", ui->font_small, COLOR_TEXT_SUB, 192, 4, 36, LV_TEXT_ALIGN_LEFT
    );
    refresh_battery_widget(ui);
}

// 在上屏大工位卡片内绘制动态像素小人与工位家具（相对父卡片坐标）
static void draw_stage_pixel_scene(
    tower_ui_t *ui,
    lv_obj_t *card,
    const tower_slot_t *slot,
    uint8_t anim_tick
)
{
    uint32_t accent = state_accent_color(slot->state);
    bool pulse = (anim_tick & 1U) != 0;

    // 像素工位地板线与办公桌 (x=8..100, y=74..79)
    pixel_box(card, 8, 74, 92, 5, COLOR_DESK_WOOD, 1, 0, 0);

    // 右侧像素显示器 (x=58..92, y=46..72)
    pixel_box(card, 58, 46, 34, 26, COLOR_MONITOR_CASE, 2, COLOR_TEXT_MUTED, 1);
    pixel_box(card, 61, 49, 28, 18, accent, 1, 0, 0);
    // 显示器屏幕内动态代码扫描线
    int scan_y = pulse ? 53 : 59;
    pixel_box(card, 64, scan_y, 20, 3, COLOR_TEXT_MAIN, 0, 0, 0);
    // 显示器支架
    pixel_box(card, 72, 71, 6, 4, COLOR_TEXT_MUTED, 0, 0, 0);

    // 左侧像素打工人小人
    if (slot->state == TOWER_STATE_IDLE) {
        // 闲置状态：趴在桌面上休息 + 浮动 "z"
        pixel_box(card, 22, 60, 20, 14, COLOR_IDLE_BORDER, 2, 0, 0);
        pixel_box(card, 24, 54, 16, 9, COLOR_SKIN, 2, 0, 0);
        pixel_box(card, 23, 50, 18, 5, COLOR_HAIR, 1, 0, 0);
        text_label(
            card,
            pulse ? "zZ" : "z",
            ui->font_small,
            COLOR_TEXT_MUTED,
            42,
            34,
            20,
            LV_TEXT_ALIGN_LEFT
        );
        return;
    }

    // 正常坐姿/站姿头部与头发
    int head_y = (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE)
        ? (pulse ? 34 : 36)
        : 38;
    pixel_box(card, 23, head_y - 5, 20, 6, COLOR_HAIR, 1, 0, 0);
    pixel_box(card, 24, head_y, 18, 14, COLOR_SKIN, 2, 0, 0);
    // 像素眼睛
    pixel_box(card, 35, head_y + 4, 3, 3, COLOR_BG_BAR, 0, 0, 0);
    // 身体衬衫（颜色跟随状态）
    pixel_box(card, 21, head_y + 14, 24, 22, accent, 2, 0, 0);

    if (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE) {
        // 紧急状态（等拍板 / 待收菜）：高举左手上下挥动 + 头顶气泡
        int arm_y = pulse ? (head_y - 2) : (head_y + 4);
        pixel_box(card, 13, arm_y, 7, 16, COLOR_SKIN, 2, 0, 0);
        const char *bubble = (slot->state == TOWER_STATE_WAIT) ? "!" : "OK";
        lv_obj_t *tag = pixel_box(card, 44, 26, 22, 16, accent, 3, COLOR_TEXT_MAIN, 1);
        text_label(tag, bubble, ui->font_small, COLOR_TEXT_MAIN, 0, 0, 20, LV_TEXT_ALIGN_CENTER);
    } else {
        // 运行中 / 当前焦点：双手在桌面上交替敲键盘
        int left_hand_y = pulse ? 67 : 70;
        int right_hand_y = pulse ? 70 : 67;
        pixel_box(card, 43, left_hand_y, 6, 5, COLOR_SKIN, 1, 0, 0);
        pixel_box(card, 50, right_hand_y, 6, 5, COLOR_SKIN, 1, 0, 0);
    }
}

// 绘制上屏单个放大像素工位卡 (108 x 112)
static void draw_stage_card(
    tower_ui_t *ui,
    lv_obj_t *screen,
    int x,
    int y,
    uint8_t slot_idx,
    const tower_model_t *model
)
{
    const tower_slot_t *slot = &model->slots[slot_idx];
    bool is_selected = (slot_idx == model->selected_slot);
    uint32_t border_col = is_selected ? COLOR_CURSOR_GOLD : state_accent_color(slot->state);
    int border_w = is_selected ? 3 : 2;

    lv_obj_t *card = pixel_box(
        screen, x, y, 108, 112, state_bg_color(slot->state), 8, border_col, border_w
    );

    // 顶部工位编号与脱敏代号（例如 "#01 AIME"）
    char header_buf[24];
    snprintf(
        header_buf,
        sizeof(header_buf),
        "%s#%02u %s",
        is_selected ? "▶" : "",
        (unsigned)(slot_idx + 1U),
        slot->code
    );
    text_label(
        card,
        header_buf,
        ui->font_small,
        is_selected ? COLOR_CURSOR_GOLD : COLOR_TEXT_MAIN,
        6,
        4,
        96,
        LV_TEXT_ALIGN_LEFT
    );

    // 中部像素小人与工位动画
    draw_stage_pixel_scene(ui, card, slot, model->anim_tick);

    // 底部状态胶囊条
    uint32_t pill_bg = (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE)
        ? state_accent_color(slot->state)
        : COLOR_BG_BAR;
    lv_obj_t *pill = pixel_box(card, 5, 86, 98, 20, pill_bg, 4, 0, 0);

    char status_buf[32];
    snprintf(
        status_buf,
        sizeof(status_buf),
        "%s %um",
        state_short_label(slot->state),
        (unsigned)slot->minutes
    );
    text_label(
        pill,
        status_buf,
        ui->font_small,
        COLOR_TEXT_MAIN,
        2,
        2,
        94,
        LV_TEXT_ALIGN_CENTER
    );
}

// 绘制下屏 4x3 十二窗口全景状态矩阵 (y = 143..291)
static void draw_matrix_grid(
    tower_ui_t *ui,
    lv_obj_t *screen,
    const tower_model_t *model
)
{
    const int start_x = 8;
    const int start_y = 143;
    const int cell_w = 53;
    const int cell_h = 46;
    const int gap_x = 4;
    const int gap_y = 5;

    for (uint8_t idx = 0; idx < TOWER_SLOT_COUNT; idx++) {
        int row = idx / 4;
        int col = idx % 4;
        int cx = start_x + col * (cell_w + gap_x);
        int cy = start_y + row * (cell_h + gap_y);

        const tower_slot_t *slot = &model->slots[idx];
        bool is_sel = (idx == model->selected_slot);
        uint32_t accent = state_accent_color(slot->state);
        uint32_t border_col = is_sel ? COLOR_CURSOR_GOLD : accent;
        int border_w = is_sel ? 2 : 1;

        lv_obj_t *cell = pixel_box(
            screen, cx, cy, cell_w, cell_h, state_bg_color(slot->state), 5, border_col, border_w
        );

        // 左上角状态小圆点 + 工位代号
        pixel_box(cell, 4, 7, 5, 5, accent, 2, 0, 0);
        text_label(
            cell,
            slot->code,
            ui->font_small,
            is_sel ? COLOR_CURSOR_GOLD : COLOR_TEXT_MAIN,
            11,
            2,
            40,
            LV_TEXT_ALIGN_LEFT
        );

        // 第二行：状态符号 + 持续分钟数
        const char *sym = "";
        if (slot->state == TOWER_STATE_WAIT) sym = "!";
        else if (slot->state == TOWER_STATE_DONE) sym = "✓";
        else if (slot->state == TOWER_STATE_ACTIVE) sym = "★";
        else if (slot->state == TOWER_STATE_RUN) sym = "●";

        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%s%um", sym, (unsigned)slot->minutes);
        uint32_t time_col = (slot->state == TOWER_STATE_IDLE) ? COLOR_TEXT_MUTED : accent;
        if (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE) {
            time_col = COLOR_TEXT_MAIN;
        }
        text_label(
            cell,
            time_buf,
            ui->font_small,
            time_col,
            4,
            23,
            45,
            LV_TEXT_ALIGN_CENTER
        );
    }
}

// 绘制底部操作提示或瞬态反馈栏 (y = 296..320)
static void draw_footer_bar(
    tower_ui_t *ui,
    lv_obj_t *screen,
    const tower_model_t *model
)
{
    lv_obj_t *footer = pixel_box(screen, 0, 296, 240, 24, COLOR_BG_BAR, 0, 0, 0);

    if (model->toast_ticks > 0 && model->toast_text[0] != '\0') {
        text_label(
            footer,
            model->toast_text,
            ui->font_small,
            COLOR_CURSOR_GOLD,
            14,
            4,
            212,
            LV_TEXT_ALIGN_CENTER
        );
    } else {
        text_label(
            footer,
            "上下:选窗 OK:切窗口 双击:已收",
            ui->font_small,
            COLOR_TEXT_SUB,
            14,
            4,
            212,
            LV_TEXT_ALIGN_CENTER
        );
    }
}

// 绘制长按 OK 进入的「连接与同步状态页」
static void draw_link_screen(
    tower_ui_t *ui,
    lv_obj_t *screen,
    const tower_model_t *model
)
{
    draw_top_bar(ui, screen, model);

    lv_obj_t *card1 = pixel_box(
        screen, 10, 32, 220, 82, COLOR_BG_PANEL, 8, COLOR_RUN_BORDER, 2
    );
    text_label(card1, "双模实时联动状态", ui->font_body, COLOR_CURSOR_GOLD, 10, 6, 200, LV_TEXT_ALIGN_LEFT);

    const char *mode_desc = "当前：内置脱敏演示快照";
    if (model->link_mode == TOWER_LINK_USB) {
        mode_desc = "当前：USB-C 串口直连同步中";
    } else if (model->link_mode == TOWER_LINK_WIFI) {
        mode_desc = "当前：Wi-Fi 局域网轮询同步中";
    }
    text_label(card1, mode_desc, ui->font_small, COLOR_TEXT_MAIN, 10, 32, 200, LV_TEXT_ALIGN_LEFT);

    char seq_buf[48];
    snprintf(seq_buf, sizeof(seq_buf), "已接收同步帧：%lu 帧", (unsigned long)model->sync_seq);
    text_label(card1, seq_buf, ui->font_small, COLOR_TEXT_SUB, 10, 54, 200, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *card2 = pixel_box(
        screen, 10, 122, 220, 92, COLOR_BG_PANEL, 8, COLOR_DONE_BORDER, 1
    );
    text_label(card2, "Wi-Fi 局域网配置", ui->font_body, COLOR_DONE_BORDER, 10, 6, 200, LV_TEXT_ALIGN_LEFT);
    if (ui->wifi_cfg && ui->wifi_cfg->configured) {
        char ssid_buf[48];
        snprintf(ssid_buf, sizeof(ssid_buf), "网络: %.24s", ui->wifi_cfg->ssid);
        text_label(card2, ssid_buf, ui->font_small, COLOR_TEXT_MAIN, 10, 32, 200, LV_TEXT_ALIGN_LEFT);
        text_label(card2, ui->wifi_cfg->url, ui->font_small, COLOR_TEXT_SUB, 10, 54, 200, LV_TEXT_ALIGN_LEFT);
    } else {
        text_label(card2, "状态: 未配置 Wi-Fi (使用 USB)", ui->font_small, COLOR_TEXT_MAIN, 10, 32, 200, LV_TEXT_ALIGN_LEFT);
        text_label(card2, "Mac运行 passport_tower.py 即可", ui->font_small, COLOR_TEXT_SUB, 10, 54, 200, LV_TEXT_ALIGN_LEFT);
    }

    lv_obj_t *card3 = pixel_box(
        screen, 10, 222, 220, 66, COLOR_BG_PANEL, 8, COLOR_IDLE_BORDER, 1
    );
    text_label(card3, "零泄密安全保证", ui->font_small, COLOR_CURSOR_GOLD, 10, 6, 200, LV_TEXT_ALIGN_LEFT);
    text_label(card3, "仅传输应用代号与5态枚举", ui->font_small, COLOR_TEXT_SUB, 10, 26, 200, LV_TEXT_ALIGN_LEFT);
    text_label(card3, "绝不显示任何文档或任务名称", ui->font_small, COLOR_TEXT_SUB, 10, 44, 200, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *footer = pixel_box(screen, 0, 296, 240, 24, COLOR_BG_BAR, 0, 0, 0);
    text_label(
        footer,
        "单击OK:立即同步  长按OK:返回",
        ui->font_small,
        COLOR_TEXT_SUB,
        14,
        4,
        212,
        LV_TEXT_ALIGN_CENTER
    );
}

void tower_ui_render(tower_ui_t *ui, const tower_model_t *model)
{
    if (!ui || !model) return;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG_MAIN), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    ui->battery_fill = NULL;
    ui->battery_label = NULL;

    if (model->screen == TOWER_SCREEN_LINK) {
        draw_link_screen(ui, screen, model);
    } else {
        draw_top_bar(ui, screen, model);

        uint8_t left_slot = 0;
        uint8_t right_slot = 1;
        tower_model_get_stage_pair(model, &left_slot, &right_slot);

        draw_stage_card(ui, screen, 8, 26, left_slot, model);
        draw_stage_card(ui, screen, 124, 26, right_slot, model);
        draw_matrix_grid(ui, screen, model);
        draw_footer_bar(ui, screen, model);
    }

    if (ui->screen) {
        lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    } else {
        lv_screen_load(screen);
    }
    ui->screen = screen;
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
    tower_ui_render(ui, model);
    ui->battery_timer = lv_timer_create(on_battery_timer, 15000, ui);
}
