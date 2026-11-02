# "Command Tower" 3-B dual-focus pixel workstation radar application.
# Baseline demo sources remain in the repository for hardware reference.
idf_component_register(
    SRCS "main.c"
         "tower_model.c"
         "tower_store.c"
         "tower_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp lvgl driver esp_event esp_http_client esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/tower_cjk_12.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/tower_cjk_14.c"
)
