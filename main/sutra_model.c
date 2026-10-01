#include "sutra_model.h"

#include <string.h>

static uint8_t wrap_section(int section)
{
    if (section < 0) return SUTRA_SECTION_COUNT - 1;
    if (section >= SUTRA_SECTION_COUNT) return 0;
    return (uint8_t)section;
}

static sutra_model_result_t result(bool save)
{
    return (sutra_model_result_t){ .redraw = true, .save = save };
}

void sutra_model_defaults(sutra_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->version = SUTRA_MODEL_VERSION;
    model->screen = SUTRA_SCREEN_READ;
}

void sutra_model_restore(sutra_model_t *model)
{
    if (!model) return;
    if (model->version != SUTRA_MODEL_VERSION ||
        model->screen > SUTRA_SCREEN_ABOUT ||
        model->home_selection >= SUTRA_HOME_ITEM_COUNT ||
        model->section >= SUTRA_SECTION_COUNT ||
        model->page >= SUTRA_SECTIONS[model->section].page_count ||
        model->daily_section >= SUTRA_SECTION_COUNT) {
        sutra_model_defaults(model);
        return;
    }

    model->screen = SUTRA_SCREEN_READ;
    model->reading_plain = 0;
    model->plain_part = 0;
    model->daily_step = SUTRA_DAILY_READ;
    model->recite_level = 0;
    model->insight_tab = 0;
}

static sutra_model_result_t handle_home(sutra_model_t *model, sutra_input_t input)
{
    if (input == SUTRA_INPUT_UP) {
        model->home_selection =
            (model->home_selection + SUTRA_HOME_ITEM_COUNT - 1) % SUTRA_HOME_ITEM_COUNT;
    } else if (input == SUTRA_INPUT_DOWN) {
        model->home_selection = (model->home_selection + 1) % SUTRA_HOME_ITEM_COUNT;
    } else if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_ABOUT;
    } else if (input == SUTRA_INPUT_OK) {
        switch (model->home_selection) {
        case 0:
            model->screen = SUTRA_SCREEN_DAILY;
            model->section = model->daily_section;
            model->daily_step = SUTRA_DAILY_READ;
            break;
        case 1:
            model->screen = SUTRA_SCREEN_LIBRARY;
            break;
        case 2:
            model->screen = SUTRA_SCREEN_MEMORIZE;
            model->recite_level = 0;
            break;
        default:
            model->screen = SUTRA_SCREEN_INSIGHT;
            model->insight_tab = 0;
            break;
        }
    }
    return result(false);
}

static sutra_model_result_t handle_library(sutra_model_t *model, sutra_input_t input)
{
    if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_HOME;
        return result(false);
    }
    if (input == SUTRA_INPUT_UP) {
        model->section = wrap_section((int)model->section - 1);
        model->page = 0;
        return result(true);
    }
    if (input == SUTRA_INPUT_DOWN) {
        model->section = wrap_section((int)model->section + 1);
        model->page = 0;
        return result(true);
    }
    if (input == SUTRA_INPUT_OK) {
        model->screen = SUTRA_SCREEN_READ;
        model->page = 0;
    }
    return result(false);
}

static sutra_model_result_t handle_read(sutra_model_t *model, sutra_input_t input)
{
    const sutra_section_t *section = &SUTRA_SECTIONS[model->section];
    if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_LIBRARY;
        model->reading_plain = 0;
        model->plain_part = 0;
        return result(false);
    }
    if (input == SUTRA_INPUT_UP) {
        if (model->reading_plain && model->plain_part > 0) {
            model->plain_part--;
            return result(false);
        }
        if (model->page > 0) {
            model->page--;
        } else {
            model->section = wrap_section((int)model->section - 1);
            model->page = SUTRA_SECTIONS[model->section].page_count - 1;
        }
        if (model->reading_plain) {
            model->plain_part =
                sutra_compare_part_count(model->section, model->page) - 1;
        }
        return result(true);
    }
    if (input == SUTRA_INPUT_DOWN) {
        if (model->reading_plain &&
            model->plain_part + 1 <
                sutra_compare_part_count(model->section, model->page)) {
            model->plain_part++;
            return result(false);
        }
        if (model->page + 1 < section->page_count) {
            model->page++;
        } else {
            model->section = wrap_section((int)model->section + 1);
            model->page = 0;
        }
        model->plain_part = 0;
        return result(true);
    }
    if (input == SUTRA_INPUT_OK) {
        model->reading_plain ^= 1;
        model->plain_part = 0;
    }
    return result(false);
}

static sutra_model_result_t handle_memorize(sutra_model_t *model, sutra_input_t input)
{
    if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_HOME;
        model->recite_level = 0;
        return result(false);
    }
    if (input == SUTRA_INPUT_UP || input == SUTRA_INPUT_DOWN) {
        int delta = input == SUTRA_INPUT_UP ? -1 : 1;
        model->section = wrap_section((int)model->section + delta);
        model->recite_level = 0;
        return result(true);
    }
    if (input == SUTRA_INPUT_OK) {
        model->recite_level = (model->recite_level + 1) % 4;
    }
    return result(false);
}

static sutra_model_result_t handle_insight(sutra_model_t *model, sutra_input_t input)
{
    if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_HOME;
        model->insight_tab = 0;
        return result(false);
    }
    if (input == SUTRA_INPUT_UP || input == SUTRA_INPUT_DOWN) {
        int delta = input == SUTRA_INPUT_UP ? -1 : 1;
        model->section = wrap_section((int)model->section + delta);
        model->insight_tab = 0;
        return result(true);
    }
    if (input == SUTRA_INPUT_OK) {
        model->insight_tab ^= 1;
    }
    return result(false);
}

static sutra_model_result_t handle_daily(sutra_model_t *model, sutra_input_t input)
{
    if (input == SUTRA_INPUT_BACK) {
        model->screen = SUTRA_SCREEN_HOME;
        return result(false);
    }
    if (model->daily_step == SUTRA_DAILY_DONE) {
        if (input == SUTRA_INPUT_OK) model->screen = SUTRA_SCREEN_HOME;
        return result(false);
    }

    if (model->daily_step == SUTRA_DAILY_RECITE &&
        (input == SUTRA_INPUT_UP || input == SUTRA_INPUT_DOWN)) {
        int level = model->recite_level + (input == SUTRA_INPUT_UP ? -1 : 1);
        if (level < 0) level = 3;
        if (level > 3) level = 0;
        model->recite_level = (uint8_t)level;
        return result(false);
    }

    if (input != SUTRA_INPUT_OK) return result(false);
    if (model->daily_step == SUTRA_DAILY_READ) {
        model->daily_step = SUTRA_DAILY_RECITE;
        model->recite_level = 1;
    } else if (model->daily_step == SUTRA_DAILY_RECITE) {
        model->daily_step = SUTRA_DAILY_REFLECT;
    } else {
        model->daily_step = SUTRA_DAILY_DONE;
        model->completed_lessons++;
        model->daily_section = wrap_section((int)model->daily_section + 1);
        return result(true);
    }
    return result(false);
}

sutra_model_result_t sutra_model_handle(sutra_model_t *model, sutra_input_t input)
{
    if (!model) return (sutra_model_result_t){0};
    switch ((sutra_screen_t)model->screen) {
    case SUTRA_SCREEN_HOME:
        return handle_home(model, input);
    case SUTRA_SCREEN_DAILY:
        return handle_daily(model, input);
    case SUTRA_SCREEN_LIBRARY:
        return handle_library(model, input);
    case SUTRA_SCREEN_READ:
        return handle_read(model, input);
    case SUTRA_SCREEN_MEMORIZE:
        return handle_memorize(model, input);
    case SUTRA_SCREEN_INSIGHT:
        return handle_insight(model, input);
    case SUTRA_SCREEN_ABOUT:
        if (input == SUTRA_INPUT_BACK || input == SUTRA_INPUT_OK) {
            model->screen = SUTRA_SCREEN_HOME;
        }
        return result(false);
    default:
        sutra_model_defaults(model);
        return result(false);
    }
}
