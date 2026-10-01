#include <assert.h>
#include <stdio.h>

#include "sutra_content.h"
#include "sutra_model.h"

static void test_home_and_library(void)
{
    sutra_model_t model;
    sutra_model_defaults(&model);
    assert(model.screen == SUTRA_SCREEN_READ);
    model.screen = SUTRA_SCREEN_HOME;
    assert(model.screen == SUTRA_SCREEN_HOME);
    assert(model.home_selection == 0);

    sutra_model_handle(&model, SUTRA_INPUT_UP);
    assert(model.home_selection == SUTRA_HOME_ITEM_COUNT - 1);
    sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(model.home_selection == 0);

    model.home_selection = 1;
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.screen == SUTRA_SCREEN_LIBRARY);
    sutra_model_handle(&model, SUTRA_INPUT_UP);
    assert(model.section == SUTRA_SECTION_COUNT - 1);
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.screen == SUTRA_SCREEN_READ);
}

static void test_read_wraps_pages_and_sections(void)
{
    sutra_model_t model;
    sutra_model_defaults(&model);
    model.screen = SUTRA_SCREEN_READ;
    model.section = 0;
    model.page = 0;

    sutra_model_result_t result = sutra_model_handle(&model, SUTRA_INPUT_UP);
    assert(result.save);
    assert(model.section == SUTRA_SECTION_COUNT - 1);
    assert(model.page == SUTRA_SECTIONS[SUTRA_SECTION_COUNT - 1].page_count - 1);

    model.section = 0;
    model.page = SUTRA_SECTIONS[0].page_count - 1;
    result = sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(result.save);
    assert(model.section == 1 && model.page == 0);

    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.screen == SUTRA_SCREEN_READ);
    assert(model.reading_plain == 1);
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.reading_plain == 0);
}

static void test_comparison_tracks_paired_parts(void)
{
    sutra_model_t model;
    sutra_model_defaults(&model);
    model.section = 0;
    model.page = 0;
    assert(sutra_compare_part_count(0, 0) == 2);

    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.reading_plain == 1);
    sutra_model_result_t result =
        sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(!result.save);
    assert(model.section == 0);
    assert(model.page == 0);
    assert(model.plain_part == 1);
    assert(model.reading_plain == 1);
    assert(sutra_compare_source_part(
               model.section, model.page, model.plain_part) != NULL);
    assert(sutra_compare_plain_part(
               model.section, model.page, model.plain_part) != NULL);

    result = sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(result.save);
    assert(model.section == 0);
    assert(model.page == 1);
    assert(model.plain_part == 0);
    assert(model.reading_plain == 1);

    result = sutra_model_handle(&model, SUTRA_INPUT_UP);
    assert(result.save);
    assert(model.page == 0);
    assert(model.plain_part == 1);

    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.reading_plain == 0);
    assert(model.plain_part == 0);
    sutra_model_handle(&model, SUTRA_INPUT_BACK);
    assert(model.screen == SUTRA_SCREEN_LIBRARY);
}

static void test_comparison_parts_are_paired(void)
{
    for (size_t section = 0; section < SUTRA_SECTION_COUNT; section++) {
        for (size_t page = 0; page < SUTRA_SECTIONS[section].page_count; page++) {
            const char *source = sutra_text_page(section, page);
            assert(source != NULL && source[0] != '\0');
            const size_t parts = sutra_compare_part_count(section, page);
            assert(parts > 0);
            for (size_t part = 0; part < parts; part++) {
                const char *source_part =
                    sutra_compare_source_part(section, page, part);
                const char *plain =
                    sutra_compare_plain_part(section, page, part);
                assert(source_part != NULL && source_part[0] != '\0');
                assert(plain != NULL && plain[0] != '\0');
            }
            assert(sutra_compare_source_part(section, page, parts) == NULL);
            assert(sutra_compare_plain_part(section, page, parts) == NULL);
        }
        assert(sutra_text_page(section, SUTRA_SECTIONS[section].page_count) == NULL);
        assert(sutra_compare_part_count(
                   section, SUTRA_SECTIONS[section].page_count) == 0);
        assert(sutra_compare_source_part(
                   section, SUTRA_SECTIONS[section].page_count, 0) == NULL);
        assert(sutra_compare_plain_part(
                   section, SUTRA_SECTIONS[section].page_count, 0) == NULL);
    }
}

static void test_memorize_and_insight(void)
{
    sutra_model_t model;
    sutra_model_defaults(&model);
    model.screen = SUTRA_SCREEN_MEMORIZE;
    for (int i = 0; i < 4; i++) {
        assert(model.recite_level == i);
        sutra_model_handle(&model, SUTRA_INPUT_OK);
    }
    assert(model.recite_level == 0);

    sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(model.section == 1 && model.recite_level == 0);
    sutra_model_handle(&model, SUTRA_INPUT_BACK);
    assert(model.screen == SUTRA_SCREEN_HOME);

    model.screen = SUTRA_SCREEN_INSIGHT;
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.insight_tab == 1);
    sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(model.section == 2 && model.insight_tab == 0);
}

static void test_daily_completion_persists_next_lesson(void)
{
    sutra_model_t model;
    sutra_model_defaults(&model);
    model.screen = SUTRA_SCREEN_HOME;
    model.home_selection = 0;
    model.daily_section = SUTRA_SECTION_COUNT - 1;
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.screen == SUTRA_SCREEN_DAILY);
    assert(model.section == SUTRA_SECTION_COUNT - 1);

    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.daily_step == SUTRA_DAILY_RECITE);
    assert(model.recite_level == 1);
    sutra_model_handle(&model, SUTRA_INPUT_DOWN);
    assert(model.recite_level == 2);
    sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(model.daily_step == SUTRA_DAILY_REFLECT);
    sutra_model_result_t result = sutra_model_handle(&model, SUTRA_INPUT_OK);
    assert(result.save);
    assert(model.daily_step == SUTRA_DAILY_DONE);
    assert(model.completed_lessons == 1);
    assert(model.daily_section == 0);
}

static void test_restore_sanitizes_transient_state(void)
{
    sutra_model_t model = {
        .version = SUTRA_MODEL_VERSION,
        .screen = SUTRA_SCREEN_READ,
        .home_selection = 2,
        .section = 9,
        .page = 1,
        .daily_section = 11,
        .daily_step = SUTRA_DAILY_DONE,
        .recite_level = 3,
        .insight_tab = 1,
        .completed_lessons = 17,
    };
    sutra_model_restore(&model);
    assert(model.screen == SUTRA_SCREEN_READ);
    assert(model.section == 9);
    assert(model.daily_section == 11);
    assert(model.completed_lessons == 17);
    assert(model.page == 1 && model.daily_step == 0);

    model.version = 99;
    sutra_model_restore(&model);
    assert(model.version == SUTRA_MODEL_VERSION);
    assert(model.completed_lessons == 0);
}

int main(void)
{
    test_home_and_library();
    test_read_wraps_pages_and_sections();
    test_comparison_tracks_paired_parts();
    test_comparison_parts_are_paired();
    test_memorize_and_insight();
    test_daily_completion_persists_next_lesson();
    test_restore_sanitizes_transient_state();
    puts("Diamond Sutra model tests: PASS");
    return 0;
}
