# "Zhi Ji" Pocket I-Ching (64 hexagrams, 384 lines) & 100 Oblique Oracle prompts.
# Baseline demo sources remain in the repository for hardware reference.
idf_component_register(
    SRCS "main.c"
         "iching_content.c"
         "iching_model.c"
         "iching_store.c"
         "iching_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl esp_hw_support esp_timer nvs_flash
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/iching_cjk_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/iching_cjk_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/iching_cjk_20.c"
)
