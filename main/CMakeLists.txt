# "Zhaojian" Diamond Sutra application. Baseline demo sources remain in the
# repository for hardware reference but are intentionally not compiled here.
idf_component_register(
    SRCS "main.c"
         "sutra_content.c"
         "sutra_model.c"
         "sutra_store.c"
         "sutra_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl esp_timer nvs_flash esp_hw_support
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sutra_cjk_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sutra_cjk_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sutra_cjk_20.c"
)
