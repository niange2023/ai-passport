// AI Passport "Zhaojian": an offline Diamond Sutra reading and practice app.
//
// Button callbacks only enqueue events. The input task owns the application
// model and acquires the LVGL lock before rebuilding UI objects.
#include "sutra_model.h"
#include "sutra_store.h"
#include "sutra_ui.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

LV_FONT_DECLARE(sutra_cjk_14);
LV_FONT_DECLARE(sutra_cjk_16);
LV_FONT_DECLARE(sutra_cjk_20);

#define INPUT_QUEUE_DEPTH 8
#define INPUT_POLL_MS 250
#define IDLE_DIM_MS 45000U
#define IDLE_SCREEN_OFF_MS 150000U
#define IDLE_SLEEP_MS 420000U
#define BACKLIGHT_FULL 100
#define BACKLIGHT_DIM 24

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} input_event_t;

static const char *TAG = "sutra";
static sutra_model_t s_model;
static sutra_ui_t s_ui;
static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static uint32_t s_idle_ms;
static uint8_t s_backlight = BACKLIGHT_FULL;

static void set_backlight(uint8_t percent)
{
    if (s_backlight == percent) return;
    s_backlight = percent;
    bsp_display_backlight(percent);
}

static sutra_input_t map_input(const input_event_t *input)
{
    if (input->button == BSP_BTN_OK && input->event == BSP_BTN_LONG) {
        return SUTRA_INPUT_BACK;
    }
    if (input->event != BSP_BTN_CLICK) return (sutra_input_t)-1;
    if (input->button == BSP_BTN_UP) return SUTRA_INPUT_UP;
    if (input->button == BSP_BTN_DOWN) return SUTRA_INPUT_DOWN;
    if (input->button == BSP_BTN_OK) return SUTRA_INPUT_OK;
    return (sutra_input_t)-1;
}

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .button = button, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Idle for %u ms; entering deep sleep", (unsigned)s_idle_ms);
    esp_err_t error =
        esp_deep_sleep_enable_gpio_wakeup(1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure key wake: %s", esp_err_to_name(error));
        s_idle_ms = IDLE_SCREEN_OFF_MS;
        return;
    }

    if (bsp_lvgl_lock(500)) {
        sutra_ui_show_sleeping(&s_ui);
        bsp_lvgl_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(250));
    (void)bsp_battery_sleep();
    (void)bsp_audio_prepare_deep_sleep();

    int key_level = 0;
    if (bsp_button_prepare_deep_sleep(&key_level) != ESP_OK || key_level == 0) {
        ESP_LOGW(TAG, "Wake key is not ready; restarting instead of sleeping");
        esp_restart();
    }
    (void)bsp_i2c_prepare_deep_sleep();

    if (!bsp_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Could not stop display safely; restarting");
        esp_restart();
    }
    (void)bsp_display_prepare_deep_sleep();
    esp_deep_sleep_start();
    esp_restart();
}

static void process_input(const input_event_t *input)
{
    const sutra_input_t mapped = map_input(input);
    if ((int)mapped < 0) return;

    // When the display is blank, the first key only wakes it. This prevents an
    // unseen navigation action from changing reading progress.
    if (s_backlight == 0) {
        s_idle_ms = 0;
        set_backlight(BACKLIGHT_FULL);
        return;
    }

    s_idle_ms = 0;
    set_backlight(BACKLIGHT_FULL);
    const sutra_model_result_t result = sutra_model_handle(&s_model, mapped);
    if (result.redraw && bsp_lvgl_lock(500)) {
        sutra_ui_render(&s_ui, &s_model);
        bsp_lvgl_unlock();
    }
    if (result.save) sutra_store_request_save(&s_model);
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    int64_t previous_us = esp_timer_get_time();
    for (;;) {
        const bool received =
            xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(INPUT_POLL_MS)) == pdTRUE;
        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_ms = (uint32_t)((now_us - previous_us) / 1000);
        previous_us = now_us;

        if (received) process_input(&input);
        else s_idle_ms += elapsed_ms;

        if (s_idle_ms >= IDLE_SLEEP_MS) {
            enter_deep_sleep();
        } else if (s_idle_ms >= IDLE_SCREEN_OFF_MS) {
            set_backlight(0);
        } else if (s_idle_ms >= IDLE_DIM_MS) {
            set_backlight(BACKLIGHT_DIM);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Zhaojian Diamond Sutra app");
    const esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "Woke from a key press");
    }

    if (!sutra_store_init(&s_model)) {
        ESP_LOGW(TAG, "Progress persistence unavailable; continuing in memory");
    }
    if (bsp_i2c_init() != ESP_OK) ESP_LOGW(TAG, "I2C unavailable");
    if (bsp_battery_init() != ESP_OK) ESP_LOGW(TAG, "Battery indicator unavailable");

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed (MOSI=%d SCLK=%d CS=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS);
        return;
    }
    bsp_display_backlight(BACKLIGHT_FULL);

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Could not acquire LVGL lock");
        return;
    }
    sutra_ui_init(&s_ui, &s_model, &sutra_cjk_14, &sutra_cjk_16, &sutra_cjk_20);
    bsp_lvgl_unlock();

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "Could not create input queue");
        return;
    }
    if (xTaskCreate(input_task, "sutra_input", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create input task");
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed");
        return;
    }
    s_input_ready = true;

    ESP_LOGI(TAG, "Ready: 32 sections, daily practice, recitation, reflection");
    ESP_LOGI(TAG, "Free heap %u bytes; largest internal block %u bytes",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(
                 MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
}
