// main/iching_model.c
// 「知几 · 随身《周易》与破局签」纯逻辑状态机实现。
#include "iching_model.h"

#include <string.h>

// xorshift32 混淆器：确保即使传入单调递增的微秒计数也能产生均匀分布的三枚铜钱与签号
static uint32_t mix_entropy(uint32_t seed, uint32_t salt)
{
    uint32_t x = seed ^ (salt * 0x9E3779B9U) ^ 0xA5A55A5AU;
    if (x == 0) x = 0x6D2B79F5U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

uint8_t iching_toss_three_coins(uint32_t entropy, uint8_t coins_out[3])
{
    uint32_t mixed = mix_entropy(entropy, 1U);
    uint8_t sum = 0;
    for (int i = 0; i < 3; i++) {
        uint8_t is_heads = (uint8_t)((mixed >> (i * 3)) & 1U);
        if (coins_out) {
            coins_out[i] = is_heads;
        }
        sum = (uint8_t)(sum + (is_heads ? 3U : 2U));
    }
    return sum;
}

void iching_compute_cast(iching_model_t *model)
{
    if (!model) return;

    uint8_t primary_mask = 0;
    uint8_t changed_mask = 0;
    uint8_t moving_mask = 0;
    uint8_t first_moving = 0xFFU;

    for (uint8_t i = 0; i < ICHING_LINE_COUNT; i++) {
        uint8_t v = model->cast_lines[i];
        bool is_yang = (v == ICHING_VAL_YOUNG_YANG || v == ICHING_VAL_OLD_YANG);
        bool is_moving = (v == ICHING_VAL_OLD_YIN || v == ICHING_VAL_OLD_YANG);
        bool changed_yang = is_moving ? (!is_yang) : is_yang;

        if (is_yang) {
            primary_mask |= (uint8_t)(1U << i);
        }
        if (changed_yang) {
            changed_mask |= (uint8_t)(1U << i);
        }
        if (is_moving) {
            moving_mask |= (uint8_t)(1U << i);
            if (first_moving == 0xFFU) {
                first_moving = i;
            }
        }
    }

    model->primary_hex = iching_index_by_mask(primary_mask);
    model->changed_hex = iching_index_by_mask(changed_mask);
    model->moving_mask = moving_mask;
    // 若有动爻，默认聚焦第一个动爻；若六爻皆静，默认聚焦初爻
    model->focus_line = (first_moving != 0xFFU) ? first_moving : 0U;
}

void iching_model_defaults(iching_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->version = ICHING_MODEL_VERSION;
    model->screen = ICHING_SCREEN_HOME;
    model->home_selection = 0;

    // 默认预设「水山蹇」九三动爻为初始演示卦象
    model->cast_lines[0] = ICHING_VAL_YOUNG_YIN;   // 初六
    model->cast_lines[1] = ICHING_VAL_YOUNG_YIN;   // 六二
    model->cast_lines[2] = ICHING_VAL_OLD_YANG;    // 九三 (动)
    model->cast_lines[3] = ICHING_VAL_YOUNG_YIN;   // 六四
    model->cast_lines[4] = ICHING_VAL_YOUNG_YANG;  // 九五
    model->cast_lines[5] = ICHING_VAL_YOUNG_YIN;   // 上六
    model->cast_step = ICHING_LINE_COUNT;
    iching_compute_cast(model);
    model->oracle_index = 10U; // 追本·底线思维
    model->book_cursor = model->primary_hex;
    model->has_saved_cast = 1U;
}

void iching_model_restore(iching_model_t *model)
{
    if (!model || model->version != ICHING_MODEL_VERSION) {
        iching_model_defaults(model);
        return;
    }
    model->screen = ICHING_SCREEN_HOME;
    if (model->home_selection >= ICHING_HOME_ITEMS) {
        model->home_selection = 0;
    }
    if (model->primary_hex >= ICHING_HEXAGRAM_COUNT) {
        model->primary_hex = 0;
    }
    if (model->changed_hex >= ICHING_HEXAGRAM_COUNT) {
        model->changed_hex = model->primary_hex;
    }
    if (model->focus_line >= ICHING_LINE_COUNT) {
        model->focus_line = 0;
    }
    if (model->reading_tab > 3U) {
        model->reading_tab = 0;
    }
    if (model->oracle_index >= ICHING_PROMPT_COUNT) {
        model->oracle_index = 0;
    }
    if (model->book_cursor >= ICHING_HEXAGRAM_COUNT) {
        model->book_cursor = 0;
    }
    for (uint8_t i = 0; i < ICHING_LINE_COUNT; i++) {
        uint8_t v = model->cast_lines[i];
        if (v < ICHING_VAL_OLD_YIN || v > ICHING_VAL_OLD_YANG) {
            model->cast_lines[i] = ICHING_VAL_YOUNG_YANG;
        }
    }
}

static void start_new_cast_session(iching_model_t *model)
{
    model->screen = ICHING_SCREEN_CAST;
    model->cast_step = 0;
    memset(model->cast_coins, 0, sizeof(model->cast_coins));
    for (uint8_t i = 0; i < ICHING_LINE_COUNT; i++) {
        model->cast_lines[i] = ICHING_VAL_YOUNG_YANG;
    }
}

static void draw_random_oracle(iching_model_t *model, uint32_t entropy)
{
    uint32_t mixed = mix_entropy(entropy, model->cast_count + model->oracle_index + 17U);
    uint8_t next = (uint8_t)(mixed % ICHING_PROMPT_COUNT);
    if (next == model->oracle_index) {
        next = (uint8_t)((next + 1U) % ICHING_PROMPT_COUNT);
    }
    model->oracle_index = next;
}

iching_model_result_t iching_model_handle(
    iching_model_t *model,
    iching_input_t input,
    uint32_t entropy
)
{
    iching_model_result_t res = {
        .redraw = false,
        .save = false,
        .sound = ICHING_SOUND_NONE,
    };
    if (!model) return res;

    // 全局返回键（长按 OK）
    if (input == ICHING_INPUT_BACK) {
        if (model->screen != ICHING_SCREEN_HOME) {
            model->screen = ICHING_SCREEN_HOME;
            res.redraw = true;
        }
        return res;
    }

    switch ((iching_screen_t)model->screen) {
    case ICHING_SCREEN_HOME:
        if (input == ICHING_INPUT_UP) {
            model->home_selection = (uint8_t)((model->home_selection + ICHING_HOME_ITEMS - 1U) % ICHING_HOME_ITEMS);
            res.redraw = true;
        } else if (input == ICHING_INPUT_DOWN) {
            model->home_selection = (uint8_t)((model->home_selection + 1U) % ICHING_HOME_ITEMS);
            res.redraw = true;
        } else if (input == ICHING_INPUT_OK) {
            if (model->home_selection == 0) {
                start_new_cast_session(model);
            } else if (model->home_selection == 1) {
                draw_random_oracle(model, entropy);
                model->screen = ICHING_SCREEN_ORACLE;
                res.sound = ICHING_SOUND_SLIP;
                res.save = true;
            } else if (model->home_selection == 2) {
                model->screen = ICHING_SCREEN_BOOK;
            } else {
                model->reading_tab = 0;
                model->screen = ICHING_SCREEN_READING;
            }
            res.redraw = true;
        } else if (input == ICHING_INPUT_OK_DOUBLE) {
            // 首页任意位置双击 OK 直接盲抽一支破局签
            draw_random_oracle(model, entropy);
            model->screen = ICHING_SCREEN_ORACLE;
            res.redraw = true;
            res.save = true;
            res.sound = ICHING_SOUND_SLIP;
        }
        break;

    case ICHING_SCREEN_CAST:
        if (input == ICHING_INPUT_OK) {
            if (model->cast_step < ICHING_LINE_COUNT) {
                uint8_t idx = model->cast_step;
                uint32_t step_entropy = mix_entropy(entropy, (uint32_t)idx + model->cast_count * 7U);
                model->cast_lines[idx] = iching_toss_three_coins(step_entropy, model->cast_coins);
                model->cast_step++;
                res.redraw = true;
                if (model->cast_step == ICHING_LINE_COUNT) {
                    iching_compute_cast(model);
                    draw_random_oracle(model, step_entropy);
                    model->book_cursor = model->primary_hex;
                    model->has_saved_cast = 1U;
                    model->cast_count++;
                    res.sound = ICHING_SOUND_COMPLETE;
                    res.save = true;
                } else {
                    res.sound = ICHING_SOUND_COIN;
                }
            } else {
                // 六爻已成，再按 OK 进入卦象品读页
                model->reading_tab = 0;
                model->screen = ICHING_SCREEN_READING;
                res.redraw = true;
            }
        } else if (input == ICHING_INPUT_OK_DOUBLE) {
            // 双击 OK 重新起卦
            start_new_cast_session(model);
            res.redraw = true;
        }
        break;

    case ICHING_SCREEN_READING:
        if (input == ICHING_INPUT_OK) {
            // 单击 OK 循环切换 4 个品读子页：0卦辞大象 -> 1六爻爻辞 -> 2义理与变卦 -> 3破局签
            model->reading_tab = (uint8_t)((model->reading_tab + 1U) % 4U);
            res.redraw = true;
        } else if (input == ICHING_INPUT_OK_DOUBLE) {
            // 若有动爻且之卦不同，双击 OK 在「本卦」与「之卦」之间切换审阅
            if (model->moving_mask != 0 && model->changed_hex != model->primary_hex) {
                uint8_t tmp = model->primary_hex;
                model->primary_hex = model->changed_hex;
                model->changed_hex = tmp;
                res.redraw = true;
            } else {
                draw_random_oracle(model, entropy);
                model->reading_tab = 3U;
                res.redraw = true;
                res.sound = ICHING_SOUND_SLIP;
            }
        } else if (input == ICHING_INPUT_UP) {
            if (model->reading_tab == 1U) {
                model->focus_line = (uint8_t)((model->focus_line + ICHING_LINE_COUNT - 1U) % ICHING_LINE_COUNT);
            } else if (model->reading_tab == 3U) {
                model->oracle_index = (uint8_t)((model->oracle_index + ICHING_PROMPT_COUNT - 1U) % ICHING_PROMPT_COUNT);
            } else {
                model->primary_hex = (uint8_t)((model->primary_hex + ICHING_HEXAGRAM_COUNT - 1U) % ICHING_HEXAGRAM_COUNT);
                model->book_cursor = model->primary_hex;
            }
            res.redraw = true;
        } else if (input == ICHING_INPUT_DOWN) {
            if (model->reading_tab == 1U) {
                model->focus_line = (uint8_t)((model->focus_line + 1U) % ICHING_LINE_COUNT);
            } else if (model->reading_tab == 3U) {
                model->oracle_index = (uint8_t)((model->oracle_index + 1U) % ICHING_PROMPT_COUNT);
            } else {
                model->primary_hex = (uint8_t)((model->primary_hex + 1U) % ICHING_HEXAGRAM_COUNT);
                model->book_cursor = model->primary_hex;
            }
            res.redraw = true;
        }
        break;

    case ICHING_SCREEN_ORACLE:
        if (input == ICHING_INPUT_OK || input == ICHING_INPUT_OK_DOUBLE) {
            draw_random_oracle(model, entropy);
            res.redraw = true;
            res.save = true;
            res.sound = ICHING_SOUND_SLIP;
        } else if (input == ICHING_INPUT_UP) {
            model->oracle_index = (uint8_t)((model->oracle_index + ICHING_PROMPT_COUNT - 1U) % ICHING_PROMPT_COUNT);
            res.redraw = true;
        } else if (input == ICHING_INPUT_DOWN) {
            model->oracle_index = (uint8_t)((model->oracle_index + 1U) % ICHING_PROMPT_COUNT);
            res.redraw = true;
        }
        break;

    case ICHING_SCREEN_BOOK:
        if (input == ICHING_INPUT_UP) {
            model->book_cursor = (uint8_t)((model->book_cursor + ICHING_HEXAGRAM_COUNT - 1U) % ICHING_HEXAGRAM_COUNT);
            res.redraw = true;
        } else if (input == ICHING_INPUT_DOWN) {
            model->book_cursor = (uint8_t)((model->book_cursor + 1U) % ICHING_HEXAGRAM_COUNT);
            res.redraw = true;
        } else if (input == ICHING_INPUT_OK_DOUBLE) {
            // 双击一次快跳 8 卦（一宫）
            model->book_cursor = (uint8_t)((model->book_cursor + 8U) % ICHING_HEXAGRAM_COUNT);
            res.redraw = true;
        } else if (input == ICHING_INPUT_OK) {
            const iching_hexagram_t *hex = iching_get_hexagram(model->book_cursor);
            for (uint8_t i = 0; i < ICHING_LINE_COUNT; i++) {
                bool is_yang = ((hex->mask >> i) & 1U) != 0;
                model->cast_lines[i] = is_yang ? ICHING_VAL_YOUNG_YANG : ICHING_VAL_YOUNG_YIN;
            }
            model->primary_hex = model->book_cursor;
            model->changed_hex = model->book_cursor;
            model->moving_mask = 0;
            model->focus_line = 0;
            model->reading_tab = 0;
            model->screen = ICHING_SCREEN_READING;
            res.redraw = true;
        }
        break;
    }

    return res;
}
