#pragma once

#include <stddef.h>
#include <stdint.h>

#define SUTRA_SECTION_COUNT 32

typedef struct {
    uint8_t number;
    const char *title;
    const char *key;
    const char *highlight;
    const char *cloze_easy;
    const char *cloze_hard;
    const char *insight;
    const char *practice;
    uint16_t page_start;
    uint8_t page_count;
} sutra_section_t;

extern const sutra_section_t SUTRA_SECTIONS[SUTRA_SECTION_COUNT];

const char *sutra_text_page(size_t section_index, size_t page_index);
size_t sutra_compare_part_count(size_t section_index, size_t page_index);
const char *sutra_compare_source_part(size_t section_index, size_t page_index,
                                      size_t part_index);
const char *sutra_compare_plain_part(size_t section_index, size_t page_index,
                                     size_t part_index);
