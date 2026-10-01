#include "sutra_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "sutra_content.h"

#define COLOR_PAPER      0xF4F0E8
#define COLOR_PAPER_DEEP 0xE5DED1
#define COLOR_WHITE      0xFFFDF8
#define COLOR_INK        0x171716
#define COLOR_MUTED      0x77736B
#define COLOR_VERMILION  0xB83A2E
#define COLOR_JADE       0x356A5A
#define COLOR_GOLD       0xB98B36

#define READ_BODY_X          18
#define READ_BODY_Y          91
#define READ_BODY_WIDTH      204
#define READ_BODY_LINE_SPACE 8

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int width, int height,
                       uint32_t color, int radius)
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

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color, int x, int y, int width,
                       lv_text_align_t align)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(object, align, 0);
    lv_obj_set_style_text_letter_space(object, 0, 0);
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

static void battery_refresh_now(sutra_ui_t *ui)
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
    lv_obj_set_style_bg_color(ui->battery_fill,
        lv_color_hex(soc < 20 ? COLOR_VERMILION : COLOR_JADE), 0);
    lv_label_set_text_fmt(ui->battery_label, "%d%%", soc);
}

static void battery_refresh(lv_timer_t *timer)
{
    battery_refresh_now(lv_timer_get_user_data(timer));
}

static void add_battery(sutra_ui_t *ui, lv_obj_t *screen)
{
    lv_obj_t *body = block(screen, 183, 14, 26, 12, COLOR_PAPER, 2);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(COLOR_INK), 0);
    block(screen, 210, 17, 3, 6, COLOR_INK, 0);
    ui->battery_fill = block(body, 2, 2, 0, 6, COLOR_JADE, 1);
    ui->battery_label = label(screen, "--", ui->font_small, COLOR_MUTED,
                              174, 29, 48, LV_TEXT_ALIGN_CENTER);
    battery_refresh_now(ui);
}

static void add_header(sutra_ui_t *ui, lv_obj_t *screen, const char *eyebrow,
                       const char *title)
{
    block(screen, 14, 15, 4, 31, COLOR_VERMILION, 0);
    label(screen, eyebrow, ui->font_small, COLOR_MUTED, 27, 11, 145,
          LV_TEXT_ALIGN_LEFT);
    label(screen, title, ui->font_title, COLOR_INK, 27, 28, 150,
          LV_TEXT_ALIGN_LEFT);
    block(screen, 14, 57, 212, 1, COLOR_PAPER_DEEP, 0);
    add_battery(ui, screen);
}

static void add_footer(sutra_ui_t *ui, lv_obj_t *screen, const char *text)
{
    block(screen, 0, 291, 240, 29, COLOR_INK, 0);
    label(screen, text, ui->font_small, COLOR_WHITE, 10, 297, 220,
          LV_TEXT_ALIGN_CENTER);
}

static void add_progress(lv_obj_t *screen, uint32_t current, uint32_t total,
                         int x, int y, int width)
{
    block(screen, x, y, width, 3, COLOR_PAPER_DEEP, 0);
    int filled = total ? (int)((current * (uint32_t)width) / total) : 0;
    if (filled < 3 && current > 0) filled = 3;
    block(screen, x, y, filled, 3, COLOR_VERMILION, 0);
}

static lv_obj_t *home_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    static const char *const items[] = {
        "今日功课", "通读全经", "背诵练习", "理解观照",
    };
    static const char *const subtitles[] = {
        "读 · 背 · 观", "三十二分", "渐隐提词", "要义与练习",
    };

    lv_obj_t *screen = base_screen();
    block(screen, 17, 18, 5, 68, COLOR_VERMILION, 0);
    label(screen, "金刚经", ui->font_title, COLOR_INK, 34, 19, 132,
          LV_TEXT_ALIGN_LEFT);
    label(screen, "金刚般若波罗蜜经", ui->font_body, COLOR_INK, 34, 46, 144,
          LV_TEXT_ALIGN_LEFT);
    label(screen, "后秦 鸠摩罗什译", ui->font_small, COLOR_MUTED, 34, 69, 135,
          LV_TEXT_ALIGN_LEFT);
    add_battery(ui, screen);

    label(screen, "修习进度", ui->font_small, COLOR_MUTED, 17, 101, 70,
          LV_TEXT_ALIGN_LEFT);
    char progress[48];
    snprintf(progress, sizeof(progress), "%lu 次 · 下一课 %02u",
             (unsigned long)model->completed_lessons,
             (unsigned)(model->daily_section + 1));
    label(screen, progress, ui->font_small, COLOR_INK, 88, 101, 134,
          LV_TEXT_ALIGN_RIGHT);
    add_progress(screen, model->daily_section + 1, SUTRA_SECTION_COUNT,
                 17, 123, 206);

    for (int i = 0; i < SUTRA_HOME_ITEM_COUNT; i++) {
        const int y = 139 + i * 35;
        if (i == model->home_selection) {
            block(screen, 17, y + 3, 4, 25, COLOR_VERMILION, 0);
            block(screen, 27, y + 31, 196, 1, COLOR_GOLD, 0);
        }
        label(screen, items[i], ui->font_body,
              i == model->home_selection ? COLOR_INK : COLOR_MUTED,
              31, y, 104, LV_TEXT_ALIGN_LEFT);
        label(screen, subtitles[i], ui->font_small, COLOR_MUTED,
              135, y + 3, 88, LV_TEXT_ALIGN_RIGHT);
    }
    add_footer(ui, screen, "上/下选择  ·  确定进入  ·  长按关于");
    return screen;
}

static lv_obj_t *library_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    lv_obj_t *screen = base_screen();
    add_header(ui, screen, "全经目录", "三十二分");
    for (int row = -3; row <= 3; row++) {
        int index = (int)model->section + row;
        while (index < 0) index += SUTRA_SECTION_COUNT;
        index %= SUTRA_SECTION_COUNT;
        const int y = 66 + (row + 3) * 30;
        if (row == 0) {
            block(screen, 14, y - 2, 212, 27, COLOR_WHITE, 4);
            block(screen, 14, y - 2, 4, 27, COLOR_VERMILION, 0);
        }
        char number[8];
        snprintf(number, sizeof(number), "%02u", SUTRA_SECTIONS[index].number);
        label(screen, number, ui->font_small,
              row == 0 ? COLOR_VERMILION : COLOR_MUTED,
              24, y + 2, 28, LV_TEXT_ALIGN_LEFT);
        label(screen, SUTRA_SECTIONS[index].title, ui->font_body,
              row == 0 ? COLOR_INK : COLOR_MUTED,
              54, y, 158, LV_TEXT_ALIGN_LEFT);
    }
    add_progress(screen, model->section + 1, SUTRA_SECTION_COUNT, 14, 280, 212);
    add_footer(ui, screen, "上/下选分  ·  确定阅读  ·  长按返回");
    return screen;
}

static lv_obj_t *read_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    const sutra_section_t *section = &SUTRA_SECTIONS[model->section];
    lv_obj_t *screen = base_screen();

    if (model->reading_plain) {
        char heading[48];
        snprintf(heading, sizeof(heading), "第%02u分 · %s",
                 (unsigned)section->number, section->title);
        label(screen, heading, ui->font_body, COLOR_INK,
              18, 11, 158, LV_TEXT_ALIGN_LEFT);
        label(screen, "对照", ui->font_small, COLOR_JADE,
              178, 14, 44, LV_TEXT_ALIGN_RIGHT);
        block(screen, 16, 39, 208, 1, COLOR_GOLD, 0);

        char source_label[24];
        snprintf(source_label, sizeof(source_label), "原文 · %u/%u",
                 (unsigned)(model->page + 1),
                 (unsigned)section->page_count);
        label(screen, source_label, ui->font_small, COLOR_VERMILION,
              18, 47, 100, LV_TEXT_ALIGN_LEFT);
        const char *source = sutra_compare_source_part(
            model->section, model->page, model->plain_part);
        lv_obj_t *source_body = label(screen, source, ui->font_body, COLOR_INK,
                                      18, 66, 204, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_text_line_space(source_body, 2, 0);

        block(screen, 18, 161, 204, 1, COLOR_PAPER_DEEP, 0);
        label(screen, "白话", ui->font_small, COLOR_JADE,
              18, 169, 60, LV_TEXT_ALIGN_LEFT);
        const char *plain = sutra_compare_plain_part(
            model->section, model->page, model->plain_part);
        lv_obj_t *plain_body = label(screen, plain, ui->font_body, COLOR_INK,
                                     18, 188, 204, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_text_line_space(plain_body, 2, 0);

        const size_t part_count =
            sutra_compare_part_count(model->section, model->page);
        char part[24];
        snprintf(part, sizeof(part), "%u / %u",
                 (unsigned)(model->plain_part + 1), (unsigned)part_count);
        add_progress(screen, model->plain_part + 1, part_count,
                     18, 279, 204);
        label(screen, part, ui->font_small, COLOR_MUTED,
              18, 288, 58, LV_TEXT_ALIGN_LEFT);
        label(screen, "上下对照 · 确定返回", ui->font_small, COLOR_MUTED,
              80, 288, 142, LV_TEXT_ALIGN_RIGHT);
        return screen;
    }

    label(screen, "金刚般若波罗蜜经", ui->font_small, COLOR_MUTED,
          18, 11, 204, LV_TEXT_ALIGN_CENTER);
    block(screen, 16, 37, 208, 1, COLOR_GOLD, 0);

    char section_number[8];
    snprintf(section_number, sizeof(section_number), "%02u", section->number);
    lv_obj_t *seal = block(screen, 17, 48, 28, 28, COLOR_VERMILION, 2);
    label(seal, section_number, ui->font_small, COLOR_WHITE,
          0, 5, 28, LV_TEXT_ALIGN_CENTER);
    label(screen, section->title, ui->font_title, COLOR_INK,
          53, 48, 125, LV_TEXT_ALIGN_LEFT);
    label(screen, "原文", ui->font_small, COLOR_VERMILION,
          178, 53, 44, LV_TEXT_ALIGN_RIGHT);

    const char *page_text = sutra_text_page(model->section, model->page);
    char rich_text[384];
    const char *key = strstr(page_text, section->highlight);
    if (key) {
        const size_t prefix = (size_t)(key - page_text);
        snprintf(rich_text, sizeof(rich_text), "%.*s#b83a2e %s#%s",
                 (int)prefix, page_text, section->highlight,
                 key + strlen(section->highlight));
    } else {
        snprintf(rich_text, sizeof(rich_text), "%s", page_text);
    }
    lv_obj_t *body = label(screen, rich_text, ui->font_body, COLOR_INK,
                           READ_BODY_X, READ_BODY_Y, READ_BODY_WIDTH,
                           LV_TEXT_ALIGN_LEFT);
    lv_label_set_recolor(body, true);
    lv_obj_set_style_text_line_space(body, READ_BODY_LINE_SPACE, 0);

    char page[24];
    snprintf(page, sizeof(page), "%u / %u",
             (unsigned)(model->page + 1), (unsigned)section->page_count);
    add_progress(screen, model->page + 1, section->page_count, 18, 279, 204);
    label(screen, page, ui->font_small, COLOR_MUTED, 18, 288, 58,
          LV_TEXT_ALIGN_LEFT);
    label(screen, "上下翻页 · 确定对照",
          ui->font_small, COLOR_MUTED, 80, 288, 142, LV_TEXT_ALIGN_RIGHT);
    return screen;
}

static const char *recite_text(const sutra_section_t *section, uint8_t level)
{
    if (level == 0) return section->key;
    if (level == 1) return section->cloze_easy;
    if (level == 2) return section->cloze_hard;
    return "合上提示，完整背诵这一句。";
}

static void add_recite_dots(lv_obj_t *screen, uint8_t level)
{
    for (int i = 0; i < 4; i++) {
        block(screen, 91 + i * 17, 261, 8, 8,
              i == level ? COLOR_VERMILION : COLOR_PAPER_DEEP, 4);
    }
}

static lv_obj_t *memorize_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    const sutra_section_t *section = &SUTRA_SECTIONS[model->section];
    char eyebrow[32];
    snprintf(eyebrow, sizeof(eyebrow), "第 %02u 分 · 背诵", section->number);
    lv_obj_t *screen = base_screen();
    add_header(ui, screen, eyebrow, section->title);
    label(screen, model->recite_level == 0 ? "原句" :
                  model->recite_level == 1 ? "轻度遮字" :
                  model->recite_level == 2 ? "首字提示" : "脱离提示",
          ui->font_small, COLOR_VERMILION, 17, 78, 206, LV_TEXT_ALIGN_CENTER);
    block(screen, 18, 106, 204, 2, COLOR_GOLD, 0);
    lv_obj_t *verse = label(screen, recite_text(section, model->recite_level),
                            ui->font_title, COLOR_INK, 24, 126, 192,
                            LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_line_space(verse, 8, 0);
    add_recite_dots(screen, model->recite_level);
    add_footer(ui, screen, "上/下换句  ·  确定加难  ·  长按返回");
    return screen;
}

static lv_obj_t *insight_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    const sutra_section_t *section = &SUTRA_SECTIONS[model->section];
    char eyebrow[32];
    snprintf(eyebrow, sizeof(eyebrow), "第 %02u 分 · %s", section->number,
             model->insight_tab ? "观照" : "要义");
    lv_obj_t *screen = base_screen();
    add_header(ui, screen, eyebrow, section->title);

    label(screen, section->key, ui->font_body, COLOR_VERMILION,
          18, 72, 204, LV_TEXT_ALIGN_CENTER);
    block(screen, 18, 119, 204, 1, COLOR_GOLD, 0);
    label(screen, model->insight_tab ? "当下练习" : "学习提示",
          ui->font_small, COLOR_MUTED, 18, 133, 204, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *body = label(screen,
                           model->insight_tab ? section->practice : section->insight,
                           ui->font_body, COLOR_INK, 18, 158, 204,
                           LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_line_space(body, 5, 0);
    label(screen, model->insight_tab ? "做一次，不求做对。" : "不把提示当成定论。",
          ui->font_small, COLOR_JADE, 18, 265, 204, LV_TEXT_ALIGN_CENTER);
    add_footer(ui, screen, "上/下换分  ·  确定切换  ·  长按返回");
    return screen;
}

static const char *daily_step_name(int index)
{
    static const char *const names[] = {"读", "背", "观"};
    if (index < 0 || index > 2) return "";
    return names[index];
}

static lv_obj_t *daily_screen(sutra_ui_t *ui, const sutra_model_t *model)
{
    const sutra_section_t *section = &SUTRA_SECTIONS[model->section];
    char eyebrow[32];
    snprintf(eyebrow, sizeof(eyebrow), "今日功课 · 第 %02u 分", section->number);
    lv_obj_t *screen = base_screen();
    add_header(ui, screen, eyebrow, section->title);

    if (model->daily_step == SUTRA_DAILY_DONE) {
        label(screen, "功课完成", ui->font_title, COLOR_VERMILION,
              18, 101, 204, LV_TEXT_ALIGN_CENTER);
        label(screen, "读过，背过，也让它照进今天。",
              ui->font_body, COLOR_INK, 27, 150, 186, LV_TEXT_ALIGN_CENTER);
        char total[48];
        snprintf(total, sizeof(total), "已完成 %lu 次 · 下一课第 %02u 分",
                 (unsigned long)model->completed_lessons,
                 (unsigned)(model->daily_section + 1));
        label(screen, total, ui->font_small, COLOR_JADE,
              18, 217, 204, LV_TEXT_ALIGN_CENTER);
        add_footer(ui, screen, "确定回到首页");
        return screen;
    }

    for (int i = 0; i < 3; i++) {
        const bool active = i == model->daily_step;
        block(screen, 54 + i * 49, 68, 32, 25,
              active ? COLOR_VERMILION : COLOR_PAPER_DEEP, 4);
        label(screen, daily_step_name(i), ui->font_small,
              active ? COLOR_WHITE : COLOR_MUTED, 54 + i * 49, 73, 32,
              LV_TEXT_ALIGN_CENTER);
    }

    if (model->daily_step == SUTRA_DAILY_READ) {
        label(screen, sutra_text_page(model->section, 0), ui->font_body,
              COLOR_INK, 17, 110, 206, LV_TEXT_ALIGN_LEFT);
        add_footer(ui, screen, "确定进入背诵  ·  长按退出");
    } else if (model->daily_step == SUTRA_DAILY_RECITE) {
        label(screen, recite_text(section, model->recite_level), ui->font_title,
              COLOR_INK, 24, 124, 192, LV_TEXT_ALIGN_CENTER);
        add_recite_dots(screen, model->recite_level);
        add_footer(ui, screen, "上/下调难度  ·  确定进入观照");
    } else {
        label(screen, section->key, ui->font_body, COLOR_VERMILION,
              19, 109, 202, LV_TEXT_ALIGN_CENTER);
        block(screen, 18, 153, 204, 1, COLOR_GOLD, 0);
        label(screen, section->practice, ui->font_body, COLOR_INK,
              22, 174, 196, LV_TEXT_ALIGN_LEFT);
        add_footer(ui, screen, "确定完成今日功课  ·  长按退出");
    }
    return screen;
}

static lv_obj_t *about_screen(sutra_ui_t *ui)
{
    lv_obj_t *screen = base_screen();
    add_header(ui, screen, "关于", "照见");
    label(screen, "一部放进口袋里的《金刚经》修习工具。",
          ui->font_body, COLOR_INK, 18, 76, 204, LV_TEXT_ALIGN_LEFT);
    block(screen, 18, 123, 204, 1, COLOR_GOLD, 0);
    label(screen, "全文采用后秦鸠摩罗什译本。三十二分题名为通行分章。解读与练习是学习提示，不替代经典注疏。",
          ui->font_body, COLOR_MUTED, 18, 142, 204, LV_TEXT_ALIGN_LEFT);
    label(screen, "离线 · 无账号 · 进度仅存本机",
          ui->font_small, COLOR_JADE, 18, 261, 204, LV_TEXT_ALIGN_CENTER);
    add_footer(ui, screen, "确定或长按返回");
    return screen;
}

void sutra_ui_render(sutra_ui_t *ui, const sutra_model_t *model)
{
    if (!ui || !model) return;
    ui->battery_fill = NULL;
    ui->battery_label = NULL;
    lv_obj_t *next;
    switch ((sutra_screen_t)model->screen) {
    case SUTRA_SCREEN_HOME:
        next = home_screen(ui, model);
        break;
    case SUTRA_SCREEN_DAILY:
        next = daily_screen(ui, model);
        break;
    case SUTRA_SCREEN_LIBRARY:
        next = library_screen(ui, model);
        break;
    case SUTRA_SCREEN_READ:
        next = read_screen(ui, model);
        break;
    case SUTRA_SCREEN_MEMORIZE:
        next = memorize_screen(ui, model);
        break;
    case SUTRA_SCREEN_INSIGHT:
        next = insight_screen(ui, model);
        break;
    default:
        next = about_screen(ui);
        break;
    }

    lv_obj_t *previous = ui->screen;
    ui->screen = next;
    lv_screen_load(next);
    if (previous) lv_obj_delete(previous);
}

void sutra_ui_init(sutra_ui_t *ui, const sutra_model_t *model,
                   const lv_font_t *font_small, const lv_font_t *font_body,
                   const lv_font_t *font_title)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->font_small = font_small;
    ui->font_body = font_body;
    ui->font_title = font_title;
    sutra_ui_render(ui, model);
    ui->battery_timer = lv_timer_create(battery_refresh, 10000, ui);
}

void sutra_ui_show_sleeping(sutra_ui_t *ui)
{
    if (!ui || !ui->screen) return;
    lv_obj_t *screen = base_screen();
    label(screen, "合卷", ui->font_title, COLOR_INK,
          18, 125, 204, LV_TEXT_ALIGN_CENTER);
    label(screen, "按任意键继续", ui->font_small, COLOR_MUTED,
          18, 169, 204, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *previous = ui->screen;
    ui->screen = screen;
    lv_screen_load(screen);
    lv_obj_delete(previous);
}
