// tests/test_iching_model.c
// 「知几 · 随身《周易》与破局签」主机端纯逻辑单元测试。
#include "iching_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_content_completeness(void)
{
    uint8_t seen_masks[64] = { 0 };
    for (uint8_t i = 0; i < ICHING_HEXAGRAM_COUNT; i++) {
        const iching_hexagram_t *h = iching_get_hexagram(i);
        assert(h != NULL);
        assert(h->number == i + 1U);
        assert(h->mask < 64U);
        assert(seen_masks[h->mask] == 0);
        seen_masks[h->mask] = 1U;
        assert(iching_index_by_mask(h->mask) == i);
        assert(strlen(h->name) > 0);
        assert(strlen(h->title) > 0);
        assert(strlen(h->judgment) > 0);
        assert(strlen(h->image) > 0);
        assert(strlen(h->summary) > 0);

        for (uint8_t line_idx = 0; line_idx < ICHING_LINE_COUNT; line_idx++) {
            assert(strlen(h->lines[line_idx].label) > 0);
            assert(strlen(h->lines[line_idx].text) > 0);
            assert(strlen(h->lines[line_idx].xiang) > 0);
        }
    }

    // 乾用九、坤用六校验
    assert(strlen(iching_get_hexagram(0)->extra) > 0);
    assert(strlen(iching_get_hexagram(1)->extra) > 0);

    for (uint8_t i = 0; i < ICHING_PROMPT_COUNT; i++) {
        const iching_prompt_t *p = iching_get_prompt(i);
        assert(p != NULL);
        assert(p->id == i + 1U);
        assert(strlen(p->category) > 0);
        assert(strlen(p->question) > 0);
    }
}

static void test_six_coin_casting_and_changed_hexagram(void)
{
    iching_model_t model;
    iching_model_defaults(&model);

    // 默认预设「水山蹇」(第39卦，下标38)，九三为老阳动爻(9)，变爻后为「水地比」(第8卦，下标7)
    assert(model.primary_hex == 38U);
    assert(model.changed_hex == 7U);
    assert(model.moving_mask == (1U << 2));
    assert(model.focus_line == 2U);

    // 从首页按 OK 进入起卦仪式页
    iching_model_result_t res = iching_model_handle(&model, ICHING_INPUT_OK, 100U);
    assert(res.redraw);
    assert(model.screen == ICHING_SCREEN_CAST);
    assert(model.cast_step == 0U);

    // 连续按 6 次 OK 抛掷六爻铜钱
    for (uint8_t step = 0; step < ICHING_LINE_COUNT; step++) {
        res = iching_model_handle(&model, ICHING_INPUT_OK, 1000U + step * 37U);
        assert(res.redraw);
        assert(model.cast_step == step + 1U);
        uint8_t v = model.cast_lines[step];
        assert(v >= ICHING_VAL_OLD_YIN && v <= ICHING_VAL_OLD_YANG);
        if (step + 1U < ICHING_LINE_COUNT) {
            assert(res.sound == ICHING_SOUND_COIN);
        } else {
            assert(res.sound == ICHING_SOUND_COMPLETE);
            assert(res.save);
        }
    }

    // 第 7 次按 OK 进入品读页
    res = iching_model_handle(&model, ICHING_INPUT_OK, 2000U);
    assert(res.redraw);
    assert(model.screen == ICHING_SCREEN_READING);
    assert(model.reading_tab == 0U);

    // 在品读页按 OK 依次切换 4 个子页
    for (uint8_t tab = 1; tab <= 4; tab++) {
        res = iching_model_handle(&model, ICHING_INPUT_OK, 3000U);
        assert(res.redraw);
        assert(model.reading_tab == (tab % 4U));
    }
}

static void test_oracle_and_book_navigation(void)
{
    iching_model_t model;
    iching_model_defaults(&model);

    // 首页双击 OK 直接盲抽破局签
    uint8_t prev_oracle = model.oracle_index;
    iching_model_result_t res = iching_model_handle(&model, ICHING_INPUT_OK_DOUBLE, 9999U);
    assert(res.redraw);
    assert(res.sound == ICHING_SOUND_SLIP);
    assert(model.screen == ICHING_SCREEN_ORACLE);
    assert(model.oracle_index != prev_oracle);

    // 长按 OK 返回首页，选第 3 项进入六十四卦全书目录
    res = iching_model_handle(&model, ICHING_INPUT_BACK, 0);
    assert(model.screen == ICHING_SCREEN_HOME);
    iching_model_handle(&model, ICHING_INPUT_DOWN, 0);
    iching_model_handle(&model, ICHING_INPUT_DOWN, 0);
    assert(model.home_selection == 2U);
    iching_model_handle(&model, ICHING_INPUT_OK, 0);
    assert(model.screen == ICHING_SCREEN_BOOK);

    uint8_t start_cursor = model.book_cursor;
    iching_model_handle(&model, ICHING_INPUT_OK_DOUBLE, 0);
    assert(model.book_cursor == (uint8_t)((start_cursor + 8U) % ICHING_HEXAGRAM_COUNT));
}

int main(void)
{
    test_content_completeness();
    test_six_coin_casting_and_changed_hexagram();
    test_oracle_and_book_navigation();
    printf("I-Ching model and 384-line tests: PASS\n");
    return 0;
}
