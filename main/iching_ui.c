// main/iching_ui.c
// 「知几 · 随身《周易》与破局签」LVGL 240x320 界面实现。
#include "iching_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "iching_content.h"

// 古籍宣纸与朱砂印泥配色
#define COLOR_PAPER      0xF4F0E6
#define COLOR_PAPER_DEEP 0xE6DFD1
#define COLOR_WHITE      0xFFFDF8
#define COLOR_INK        0x191816
#define COLOR_MUTED      0x747067
#define COLOR_VERMILION  0xB83A2E
#define COLOR_JADE       0x356A5A
#define COLOR_GOLD       0xB68934

static lv_obj_t *block(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    uint32_t color,
    int radius
)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    return object;
}

static lv_obj_t *label_wrap(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    uint32_t color,
    int x,
    int y,
    int width,
    lv_text_align_t align
)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(object, align, 0);
    lv_obj_set_style_text_line_space(object, 5, 0);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    return object;
}

static lv_obj_t *base_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    return screen;
}

static void battery_refresh_now(iching_ui_t *ui)
{
    if (!ui || !ui->battery_fill || !ui->battery_label) return;
    const int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_obj_set_width(ui->battery_fill, 0);
        lv_label_set_text(ui->battery_label, "--");
        return;
    }
    int width = (soc * 20 + 99) / 100;
    if (soc > 0 && width < 2) width = 2;
    lv_obj_set_width(ui->battery_fill, width);
    lv_obj_set_style_bg_color(
        ui->battery_fill,
        lv_color_hex(soc < 20 ? COLOR_VERMILION : COLOR_JADE),
        0
    );
    lv_label_set_text_fmt(ui->battery_label, "%d%%", soc);
}

static void on_battery_timer(lv_timer_t *timer)
{
    battery_refresh_now((iching_ui_t *)lv_timer_get_user_data(timer));
}

static void add_battery(iching_ui_t *ui, lv_obj_t *screen)
{
    lv_obj_t *body = block(screen, 183, 14, 24, 11, COLOR_PAPER, 2);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(COLOR_INK), 0);
    block(screen, 208, 17, 2, 5, COLOR_INK, 0);
    ui->battery_fill = block(body, 2, 2, 0, 5, COLOR_JADE, 1);
    ui->battery_label = label_wrap(
        screen, "--", ui->font_small, COLOR_MUTED, 172, 27, 48, LV_TEXT_ALIGN_CENTER
    );
    battery_refresh_now(ui);
}

static void add_header(
    iching_ui_t *ui,
    lv_obj_t *screen,
    const char *eyebrow,
    const char *title
)
{
    block(screen, 14, 14, 4, 30, COLOR_VERMILION, 1);
    label_wrap(screen, eyebrow, ui->font_small, COLOR_MUTED, 24, 10, 152, LV_TEXT_ALIGN_LEFT);
    label_wrap(screen, title, ui->font_title, COLOR_INK, 24, 26, 154, LV_TEXT_ALIGN_LEFT);
    block(screen, 14, 53, 212, 1, COLOR_PAPER_DEEP, 0);
    add_battery(ui, screen);
}

static void add_footer(iching_ui_t *ui, lv_obj_t *screen, const char *text)
{
    block(screen, 0, 292, 240, 28, COLOR_INK, 0);
    label_wrap(screen, text, ui->font_small, COLOR_WHITE, 8, 297, 224, LV_TEXT_ALIGN_CENTER);
}

// 绘制六爻卦象图（从上爻 row=0 到初爻 row=5）：
// 阳爻为整条实线，阴爻为左右两条断线；动爻或聚焦爻使用朱砂红高亮。
static void draw_hexagram_bars(
    lv_obj_t *parent,
    uint8_t mask6,
    uint8_t moving_mask,
    int highlight_line,
    int x,
    int y,
    int width,
    int bar_h,
    int gap_y
)
{
    int seg_w = (width - 14) / 2;
    for (int row = 0; row < 6; row++) {
        // 易经卦画自下而上：初爻在最底 (line_idx=0)，上爻在最顶 (line_idx=5)
        int line_idx = 5 - row;
        int cy = y + row * (bar_h + gap_y);
        bool is_yang = ((mask6 >> line_idx) & 1U) != 0;
        bool is_moving = ((moving_mask >> line_idx) & 1U) != 0;
        bool is_focus = (highlight_line == line_idx);

        uint32_t col = COLOR_INK;
        if (is_focus || is_moving) {
            col = COLOR_VERMILION;
        }

        if (is_yang) {
            block(parent, x, cy, width, bar_h, col, 1);
        } else {
            block(parent, x, cy, seg_w, bar_h, col, 1);
            block(parent, x + width - seg_w, cy, seg_w, bar_h, col, 1);
        }

        if (is_moving) {
            // 动爻右侧朱砂小圆点标记
            block(parent, x + width + 4, cy + 1, bar_h - 1, bar_h - 1, COLOR_VERMILION, 2);
        }
    }
}

// 1. 首页：四项大卡片入口
static void render_home_screen(iching_ui_t *ui, lv_obj_t *screen, const iching_model_t *model)
{
    add_header(ui, screen, "周易六十四卦 · 破局百签", "知几 · 随身易");

    const iching_hexagram_t *last_hex = iching_get_hexagram(model->primary_hex);
    char sub3[48];
    snprintf(sub3, sizeof(sub3), "续读：第%u卦 · %s", (unsigned)last_hex->number, last_hex->title);

    const char *titles[ICHING_HOME_ITEMS] = {
        "铜钱六爻起卦",
        "盲抽破局一签",
        "六十四卦全书",
        sub3,
    };
    const char *descs[ICHING_HOME_ITEMS] = {
        "三枚铜钱六次摇卦，观本卦与变爻",
        "百条第一性原理反问，一剑破除卡壳",
        "完整收录六十四卦、三百八十四爻辞",
        "回到上次占得的卦象与破局签",
    };

    for (uint8_t i = 0; i < ICHING_HOME_ITEMS; i++) {
        bool sel = (i == model->home_selection);
        int y = 60 + (int)i * 55;
        lv_obj_t *card = block(
            screen, 14, y, 212, 49, sel ? COLOR_WHITE : COLOR_PAPER_DEEP, 6
        );
        if (sel) {
            lv_obj_set_style_border_width(card, 2, 0);
            lv_obj_set_style_border_color(card, lv_color_hex(COLOR_VERMILION), 0);
            block(card, 0, 8, 4, 33, COLOR_VERMILION, 1);
        }
        label_wrap(
            card,
            titles[i],
            ui->font_body,
            sel ? COLOR_VERMILION : COLOR_INK,
            12,
            5,
            192,
            LV_TEXT_ALIGN_LEFT
        );
        label_wrap(
            card,
            descs[i],
            ui->font_small,
            COLOR_MUTED,
            12,
            26,
            192,
            LV_TEXT_ALIGN_LEFT
        );
    }

    add_footer(ui, screen, "上下:选模式  OK:进入  双击:速抽签");
}

// 2. 三枚铜钱六爻起卦仪式页
static void render_cast_screen(iching_ui_t *ui, lv_obj_t *screen, const iching_model_t *model)
{
    char eyebrow[64];
    if (model->cast_step < ICHING_LINE_COUNT) {
        snprintf(eyebrow, sizeof(eyebrow), "心中默念所问 · 第 %u / 6 爻", (unsigned)(model->cast_step + 1U));
    } else {
        snprintf(eyebrow, sizeof(eyebrow), "六爻既成 · 按 OK 观卦辞与破局签");
    }
    add_header(ui, screen, eyebrow, "三枚铜钱起卦");

    // 上半区：三枚古铜钱视觉呈现 (y=62..118)
    for (int c = 0; c < 3; c++) {
        int cx = 24 + c * 68;
        bool has_tossed = (model->cast_step > 0);
        bool is_heads = has_tossed ? (model->cast_coins[c] != 0) : (c != 1);
        uint32_t coin_col = is_heads ? COLOR_GOLD : COLOR_INK;

        lv_obj_t *coin = block(screen, cx, 62, 56, 52, COLOR_WHITE, 26);
        lv_obj_set_style_border_width(coin, 3, 0);
        lv_obj_set_style_border_color(coin, lv_color_hex(coin_col), 0);

        // 铜钱外圆内方孔与字/背标识
        lv_obj_t *hole = block(coin, 18, 16, 20, 20, COLOR_PAPER_DEEP, 2);
        lv_obj_set_style_border_width(hole, 1, 0);
        lv_obj_set_style_border_color(hole, lv_color_hex(coin_col), 0);
        label_wrap(
            coin,
            has_tossed ? (is_heads ? "字" : "背") : "易",
            ui->font_small,
            coin_col,
            0,
            17,
            50,
            LV_TEXT_ALIGN_CENTER
        );
    }

    // 下半区：六爻生成画布（自下而上逐爻点亮）
    lv_obj_t *board = block(screen, 14, 122, 212, 162, COLOR_WHITE, 8);
    lv_obj_set_style_border_width(board, 1, 0);
    lv_obj_set_style_border_color(board, lv_color_hex(COLOR_PAPER_DEEP), 0);

    static const char *const k_pos_names[6] = {
        "初爻", "二爻", "三爻", "四爻", "五爻", "上爻"
    };

    for (int row = 0; row < 6; row++) {
        int line_idx = 5 - row;
        int cy = 12 + row * 23;
        label_wrap(board, k_pos_names[line_idx], ui->font_small, COLOR_MUTED, 10, cy - 2, 36, LV_TEXT_ALIGN_LEFT);

        if ((uint8_t)line_idx < model->cast_step) {
            uint8_t v = model->cast_lines[line_idx];
            bool is_yang = (v == ICHING_VAL_YOUNG_YANG || v == ICHING_VAL_OLD_YANG);
            bool is_moving = (v == ICHING_VAL_OLD_YIN || v == ICHING_VAL_OLD_YANG);
            uint32_t bar_col = is_moving ? COLOR_VERMILION : COLOR_INK;

            if (is_yang) {
                block(board, 50, cy + 3, 96, 8, bar_col, 1);
            } else {
                block(board, 50, cy + 3, 42, 8, bar_col, 1);
                block(board, 104, cy + 3, 42, 8, bar_col, 1);
            }

            const char *val_tag = "少阳";
            if (v == ICHING_VAL_OLD_YANG) val_tag = "老阳○";
            else if (v == ICHING_VAL_OLD_YIN) val_tag = "老阴×";
            else if (v == ICHING_VAL_YOUNG_YIN) val_tag = "少阴";

            label_wrap(
                board,
                val_tag,
                ui->font_small,
                is_moving ? COLOR_VERMILION : COLOR_MUTED,
                152,
                cy - 2,
                54,
                LV_TEXT_ALIGN_LEFT
            );
        } else {
            // 尚未摇出的爻位虚线占位
            block(board, 50, cy + 6, 96, 2, COLOR_PAPER_DEEP, 0);
            label_wrap(board, "待摇", ui->font_small, COLOR_PAPER_DEEP, 152, cy - 2, 48, LV_TEXT_ALIGN_LEFT);
        }
    }

    if (model->cast_step < ICHING_LINE_COUNT) {
        add_footer(ui, screen, "按 OK 键：抛铜钱摇出一爻");
    } else {
        const iching_hexagram_t *hex = iching_get_hexagram(model->primary_hex);
        char foot[64];
        snprintf(foot, sizeof(foot), "得【%s】· 按 OK 解卦", hex->title);
        add_footer(ui, screen, foot);
    }
}

// 3. 卦象与爻辞品读页（4 个大字留白子页）
static void render_reading_screen(iching_ui_t *ui, lv_obj_t *screen, const iching_model_t *model)
{
    const iching_hexagram_t *hex = iching_get_hexagram(model->primary_hex);
    const iching_hexagram_t *changed = iching_get_hexagram(model->changed_hex);
    const iching_prompt_t *prompt = iching_get_prompt(model->oracle_index);

    char eyebrow[48];
    const char *tab_names[4] = { "壹·本卦大象", "贰·六爻爻辞", "叁·义理变卦", "肆·破局一问" };
    snprintf(
        eyebrow,
        sizeof(eyebrow),
        "第%u卦 · %s",
        (unsigned)hex->number,
        tab_names[model->reading_tab % 4U]
    );
    add_header(ui, screen, eyebrow, hex->title);

    if (model->reading_tab == 0U) {
        // 子页 1：左侧大卦画 + 右侧卦名与动爻提示 + 下方卦辞与大象传
        lv_obj_t *top_card = block(screen, 14, 60, 212, 82, COLOR_WHITE, 8);
        draw_hexagram_bars(top_card, hex->mask, model->moving_mask, -1, 14, 12, 72, 6, 4);

        char meta_buf[96];
        if (model->moving_mask != 0 && model->changed_hex != model->primary_hex) {
            snprintf(
                meta_buf,
                sizeof(meta_buf),
                "本卦：%s\n之卦：%s\n朱砂点为动爻",
                hex->name,
                changed->title
            );
        } else {
            snprintf(
                meta_buf,
                sizeof(meta_buf),
                "卦名：%s\n六爻安静\n以本卦卦辞断之",
                hex->name
            );
        }
        label_wrap(top_card, meta_buf, ui->font_small, COLOR_INK, 102, 10, 102, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *body_card = block(screen, 14, 148, 212, 136, COLOR_WHITE, 8);
        block(body_card, 0, 10, 4, 116, COLOR_VERMILION, 1);

        char text_buf[256];
        snprintf(
            text_buf,
            sizeof(text_buf),
            "【卦辞】%s\n【象曰】%s",
            hex->judgment,
            hex->image
        );
        label_wrap(body_card, text_buf, ui->font_body, COLOR_INK, 12, 8, 192, LV_TEXT_ALIGN_LEFT);
        add_footer(ui, screen, "上下:换卦  OK:看六爻辞  长按:首页");

    } else if (model->reading_tab == 1U) {
        // 子页 2：三百八十四爻逐爻品读（上下键切换初爻..上爻）
        uint8_t ln_idx = model->focus_line % ICHING_LINE_COUNT;
        const iching_line_t *ln = &hex->lines[ln_idx];
        bool is_moving = ((model->moving_mask >> ln_idx) & 1U) != 0;

        lv_obj_t *top_strip = block(screen, 14, 60, 212, 54, COLOR_WHITE, 8);
        draw_hexagram_bars(top_strip, hex->mask, model->moving_mask, (int)ln_idx, 12, 6, 56, 4, 3);

        char line_title[64];
        snprintf(
            line_title,
            sizeof(line_title),
            "%s %s\n第 %u / 6 爻",
            ln->label,
            is_moving ? "【动爻】" : "（静爻）",
            (unsigned)(ln_idx + 1U)
        );
        label_wrap(
            top_strip,
            line_title,
            ui->font_body,
            is_moving ? COLOR_VERMILION : COLOR_INK,
            82,
            6,
            122,
            LV_TEXT_ALIGN_LEFT
        );

        lv_obj_t *yao_card = block(screen, 14, 120, 212, 164, COLOR_WHITE, 8);
        lv_obj_set_style_border_width(yao_card, 1, 0);
        lv_obj_set_style_border_color(yao_card, lv_color_hex(is_moving ? COLOR_VERMILION : COLOR_PAPER_DEEP), 0);

        char yao_buf[256];
        if (hex->extra[0] != '\0' && ln_idx == 5U) {
            snprintf(
                yao_buf,
                sizeof(yao_buf),
                "【%s】%s\n【象曰】%s\n【%s】",
                ln->label,
                ln->text,
                ln->xiang,
                hex->extra
            );
        } else {
            snprintf(
                yao_buf,
                sizeof(yao_buf),
                "【%s】%s\n\n【象曰】%s",
                ln->label,
                ln->text,
                ln->xiang
            );
        }
        label_wrap(yao_card, yao_buf, ui->font_body, COLOR_INK, 12, 10, 188, LV_TEXT_ALIGN_LEFT);
        add_footer(ui, screen, "上下:选初至上爻  OK:看义理与变卦");

    } else if (model->reading_tab == 2U) {
        // 子页 3：现代义理心要 + 之卦（变卦）对照
        lv_obj_t *card1 = block(screen, 14, 60, 212, 112, COLOR_WHITE, 8);
        block(card1, 0, 10, 4, 92, COLOR_JADE, 1);
        label_wrap(card1, "【义理心要】", ui->font_small, COLOR_JADE, 12, 8, 188, LV_TEXT_ALIGN_LEFT);
        label_wrap(card1, hex->summary, ui->font_body, COLOR_INK, 12, 30, 188, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *card2 = block(screen, 14, 178, 212, 106, COLOR_WHITE, 8);
        if (model->moving_mask != 0 && model->changed_hex != model->primary_hex) {
            char chg_title[64];
            snprintf(chg_title, sizeof(chg_title), "【之卦】第%u卦 · %s", (unsigned)changed->number, changed->title);
            label_wrap(card2, chg_title, ui->font_small, COLOR_VERMILION, 12, 8, 188, LV_TEXT_ALIGN_LEFT);
            label_wrap(card2, changed->summary, ui->font_body, COLOR_INK, 12, 30, 188, LV_TEXT_ALIGN_LEFT);
        } else {
            label_wrap(card2, "【大象箴言】", ui->font_small, COLOR_VERMILION, 12, 8, 188, LV_TEXT_ALIGN_LEFT);
            label_wrap(card2, hex->image, ui->font_body, COLOR_INK, 12, 30, 188, LV_TEXT_ALIGN_LEFT);
        }
        add_footer(ui, screen, "OK:看破局签  双击:切换本卦/之卦");

    } else {
        // 子页 4：关联的第一性原理破局提问（大字一屏一事）
        lv_obj_t *tag_box = block(screen, 14, 62, 212, 32, COLOR_PAPER_DEEP, 6);
        char tag_buf[64];
        snprintf(tag_buf, sizeof(tag_buf), "破局第 %03u 签 · %s", (unsigned)prompt->id, prompt->category);
        label_wrap(tag_box, tag_buf, ui->font_small, COLOR_VERMILION, 10, 7, 192, LV_TEXT_ALIGN_LEFT);

        lv_obj_t *q_card = block(screen, 14, 100, 212, 184, COLOR_WHITE, 8);
        lv_obj_set_style_border_width(q_card, 2, 0);
        lv_obj_set_style_border_color(q_card, lv_color_hex(COLOR_VERMILION), 0);
        label_wrap(q_card, prompt->question, ui->font_title, COLOR_INK, 14, 16, 184, LV_TEXT_ALIGN_LEFT);

        add_footer(ui, screen, "上下:换一签  OK:回本卦  长按:首页");
    }
}

// 4. 100 条第一性原理破局签独立抽签页（一屏只放一句 20px 大字反问）
static void render_oracle_screen(iching_ui_t *ui, lv_obj_t *screen, const iching_model_t *model)
{
    const iching_prompt_t *prompt = iching_get_prompt(model->oracle_index);
    char eyebrow[48];
    snprintf(eyebrow, sizeof(eyebrow), "百签第 %03u 签 · 第一性原理", (unsigned)prompt->id);
    add_header(ui, screen, eyebrow, prompt->category);

    lv_obj_t *card = block(screen, 14, 62, 212, 220, COLOR_WHITE, 10);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_VERMILION), 0);
    block(card, 14, 16, 36, 3, COLOR_VERMILION, 1);

    label_wrap(card, prompt->question, ui->font_title, COLOR_INK, 14, 28, 184, LV_TEXT_ALIGN_LEFT);

    add_footer(ui, screen, "OK:随机再抽一签  上下:逐签翻阅");
}

// 5. 六十四卦全书目录页
static void render_book_screen(iching_ui_t *ui, lv_obj_t *screen, const iching_model_t *model)
{
    uint8_t cur = model->book_cursor % ICHING_HEXAGRAM_COUNT;
    char eyebrow[48];
    snprintf(eyebrow, sizeof(eyebrow), "周易全书 · 第 %u / 64 卦", (unsigned)(cur + 1U));
    add_header(ui, screen, eyebrow, "六十四卦目录");

    uint8_t page_start = (uint8_t)((cur / 4U) * 4U);
    for (uint8_t i = 0; i < 4U; i++) {
        uint8_t idx = (uint8_t)(page_start + i);
        if (idx >= ICHING_HEXAGRAM_COUNT) break;
        const iching_hexagram_t *h = iching_get_hexagram(idx);
        bool sel = (idx == cur);
        int y = 60 + (int)i * 55;

        lv_obj_t *row = block(
            screen, 14, y, 212, 49, sel ? COLOR_WHITE : COLOR_PAPER_DEEP, 6
        );
        if (sel) {
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(COLOR_VERMILION), 0);
        }

        // 左侧迷你六爻符号
        draw_hexagram_bars(row, h->mask, 0, -1, 10, 7, 26, 3, 3);

        char title_buf[48];
        snprintf(title_buf, sizeof(title_buf), "%02u · %s", (unsigned)h->number, h->title);
        label_wrap(
            row,
            title_buf,
            ui->font_body,
            sel ? COLOR_VERMILION : COLOR_INK,
            46,
            4,
            158,
            LV_TEXT_ALIGN_LEFT
        );
        label_wrap(
            row,
            h->judgment,
            ui->font_small,
            COLOR_MUTED,
            46,
            26,
            158,
            LV_TEXT_ALIGN_LEFT
        );
    }

    add_footer(ui, screen, "上下:选卦  OK:精读  双击:跳八卦");
}

void iching_ui_render(iching_ui_t *ui, const iching_model_t *model)
{
    if (!ui || !model) return;

    lv_obj_t *screen = base_screen();
    ui->battery_fill = NULL;
    ui->battery_label = NULL;

    switch ((iching_screen_t)model->screen) {
    case ICHING_SCREEN_HOME:
        render_home_screen(ui, screen, model);
        break;
    case ICHING_SCREEN_CAST:
        render_cast_screen(ui, screen, model);
        break;
    case ICHING_SCREEN_READING:
        render_reading_screen(ui, screen, model);
        break;
    case ICHING_SCREEN_ORACLE:
        render_oracle_screen(ui, screen, model);
        break;
    case ICHING_SCREEN_BOOK:
    default:
        render_book_screen(ui, screen, model);
        break;
    }

    if (ui->screen) {
        lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    } else {
        lv_screen_load(screen);
    }
    ui->screen = screen;
}

void iching_ui_show_sleeping(iching_ui_t *ui)
{
    if (!ui) return;
    lv_obj_t *screen = base_screen();
    ui->battery_fill = NULL;
    ui->battery_label = NULL;
    add_header(ui, screen, "知几 · 随身易", "敛神入静");

    lv_obj_t *card = block(screen, 14, 96, 212, 110, COLOR_WHITE, 8);
    label_wrap(
        card,
        "君子以向晦入宴息。\n\n按任意键即可唤醒。",
        ui->font_body,
        COLOR_INK,
        16,
        18,
        180,
        LV_TEXT_ALIGN_CENTER
    );
    add_footer(ui, screen, "深睡省电中");

    if (ui->screen) {
        lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    } else {
        lv_screen_load(screen);
    }
    ui->screen = screen;
}

void iching_ui_init(
    iching_ui_t *ui,
    const iching_model_t *model,
    const lv_font_t *font_small,
    const lv_font_t *font_body,
    const lv_font_t *font_title
)
{
    if (!ui || !model) return;
    memset(ui, 0, sizeof(*ui));
    ui->font_small = font_small;
    ui->font_body = font_body;
    ui->font_title = font_title;
    iching_ui_render(ui, model);
    ui->battery_timer = lv_timer_create(on_battery_timer, 15000, ui);
}
