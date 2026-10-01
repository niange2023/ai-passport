#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sutra_content.h"

#define SUTRA_MODEL_VERSION 2
#define SUTRA_HOME_ITEM_COUNT 4

typedef enum {
    SUTRA_SCREEN_HOME = 0,
    SUTRA_SCREEN_DAILY,
    SUTRA_SCREEN_LIBRARY,
    SUTRA_SCREEN_READ,
    SUTRA_SCREEN_MEMORIZE,
    SUTRA_SCREEN_INSIGHT,
    SUTRA_SCREEN_ABOUT,
} sutra_screen_t;

typedef enum {
    SUTRA_INPUT_UP = 0,
    SUTRA_INPUT_DOWN,
    SUTRA_INPUT_OK,
    SUTRA_INPUT_BACK,
} sutra_input_t;

typedef enum {
    SUTRA_DAILY_READ = 0,
    SUTRA_DAILY_RECITE,
    SUTRA_DAILY_REFLECT,
    SUTRA_DAILY_DONE,
} sutra_daily_step_t;

typedef struct {
    uint8_t version;
    uint8_t screen;
    uint8_t home_selection;
    uint8_t section;
    uint8_t page;
    uint8_t reading_plain;
    uint8_t plain_part;
    uint8_t daily_section;
    uint8_t daily_step;
    uint8_t recite_level;
    uint8_t insight_tab;
    uint32_t completed_lessons;
} sutra_model_t;

typedef struct {
    bool redraw;
    bool save;
} sutra_model_result_t;

void sutra_model_defaults(sutra_model_t *model);
void sutra_model_restore(sutra_model_t *model);
sutra_model_result_t sutra_model_handle(sutra_model_t *model, sutra_input_t input);
