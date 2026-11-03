// main/main.c
// AI Passport 「知几 · 随身《周易》与破局签 (Pocket I-Ching & Oblique Oracle)」主应用入口。
//
// 器物定位：
// - 0 联网、0 后台服务、刷一次管十年的随身赛博铜钱手把件；
// - 内置完整《周易》六十四卦、三百八十四爻辞/象传與 100 条第一性原理破局提问；
// - 支持三枚铜钱六爻起卦（配 ES8311 清脆铜钱落盘声与成卦泛音和弦）、破局盲抽与全书精读；
// - 闲置自动降亮、息屏，确保随手放包里或桌角零心理负担。
#include "iching_model.h"
#include "iching_store.h"
#include "iching_ui.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

LV_FONT_DECLARE(iching_cjk_14);
LV_FONT_DECLARE(iching_cjk_16);
LV_FONT_DECLARE(iching_cjk_20);

#define INPUT_QUEUE_DEPTH   8
#define SOUND_QUEUE_DEPTH   4
#define INPUT_POLL_MS       250
#define IDLE_DIM_MS         60000U
#define IDLE_SCREEN_OFF_MS  180000U
#define BACKLIGHT_FULL      100
#define BACKLIGHT_DIM       24
#define AUDIO_SAMPLE_RATE   16000U

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} input_event_t;

static const char *TAG = "iching_oracle";

static iching_model_t s_model;
static iching_ui_t s_ui;
static QueueHandle_t s_input_queue;
static QueueHandle_t s_sound_queue;
static volatile bool s_input_ready = false;
static bool s_audio_ready = false;
static uint32_t s_idle_ms = 0;
static uint8_t s_backlight = BACKLIGHT_FULL;

static void set_backlight(uint8_t percent)
{
    if (s_backlight == percent) return;
    s_backlight = percent;
    bsp_display_backlight(percent);
}

static bool map_input(const input_event_t *ev, iching_input_t *out)
{
    if (ev->button == BSP_BTN_OK && ev->event == BSP_BTN_LONG) {
        *out = ICHING_INPUT_BACK;
        return true;
    }
    if (ev->button == BSP_BTN_OK && ev->event == BSP_BTN_DOUBLE) {
        *out = ICHING_INPUT_OK_DOUBLE;
        return true;
    }
    if (ev->event == BSP_BTN_CLICK) {
        if (ev->button == BSP_BTN_UP) *out = ICHING_INPUT_UP;
        else if (ev->button == BSP_BTN_DOWN) *out = ICHING_INPUT_DOWN;
        else if (ev->button == BSP_BTN_OK) *out = ICHING_INPUT_OK;
        else return false;
        return true;
    }
    return false;
}

static void on_button_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t item = { .button = button, .event = event };
    (void)xQueueSend(s_input_queue, &item, 0);
}

// 合成一段带指数衰减包络的双频金属余韵（用于模拟铜钱落盘清响与成卦泛音）
static void play_decaying_bell(uint32_t freq1_hz, uint32_t freq2_hz, uint32_t duration_ms, int16_t peak_amp)
{
    if (!s_audio_ready) return;

    int16_t chunk[256];
    uint32_t total_samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000U;
    uint32_t phase1 = 0;
    uint32_t phase2 = 0;
    uint32_t step1 = (freq1_hz * 65536U) / AUDIO_SAMPLE_RATE;
    uint32_t step2 = (freq2_hz * 65536U) / AUDIO_SAMPLE_RATE;

    uint32_t generated = 0;
    while (generated < total_samples) {
        size_t n = total_samples - generated;
        if (n > 256U) n = 256U;

        for (size_t i = 0; i < n; i++) {
            uint32_t pos = generated + (uint32_t)i;
            // 线性近似指数衰减包络
            int32_t env_num = (int32_t)(total_samples - pos);
            int32_t env = (env_num * env_num) / (int32_t)(total_samples ? total_samples : 1U);

            int32_t w1 = ((phase1 & 0x8000U) ? 1 : -1);
            int32_t w2 = ((phase2 & 0x8000U) ? 1 : -1);
            phase1 += step1;
            phase2 += step2;

            int32_t sample = ((w1 * 3 + w2 * 2) * peak_amp * env) / (5 * (int32_t)(total_samples ? total_samples : 1U));
            chunk[i] = (int16_t)sample;
        }
        (void)bsp_audio_write(chunk, n * sizeof(int16_t));
        generated += (uint32_t)n;
    }
}

// 独立声学反馈工作任务：绝不阻塞按键回调或 LVGL 渲染任务
static void sound_worker_task(void *arg)
{
    (void)arg;
    iching_sound_t snd;
    for (;;) {
        if (xQueueReceive(s_sound_queue, &snd, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!s_audio_ready) continue;

        if (snd == ICHING_SOUND_COIN) {
            // 铜钱落盘两声清脆金属撞击音
            play_decaying_bell(2480U, 3720U, 45U, 2600);
            play_decaying_bell(2960U, 4440U, 65U, 2200);
        } else if (snd == ICHING_SOUND_COMPLETE) {
            // 六爻成卦颂钵双音和弦
            play_decaying_bell(1046U, 1568U, 95U, 2800);
            play_decaying_bell(1318U, 2637U, 140U, 2600);
        } else if (snd == ICHING_SOUND_SLIP) {
            // 竹签轻响
            play_decaying_bell(1760U, 2640U, 55U, 2200);
        }
    }
}

static void process_input_event(const input_event_t *ev)
{
    iching_input_t mapped;
    if (!map_input(ev, &mapped)) return;

    // 息屏状态下首次按键仅点亮屏幕，不误触发摇卦或翻页
    if (s_backlight == 0) {
        s_idle_ms = 0;
        set_backlight(BACKLIGHT_FULL);
        return;
    }

    s_idle_ms = 0;
    set_backlight(BACKLIGHT_FULL);

    uint32_t entropy = esp_random() ^ (uint32_t)esp_timer_get_time();
    iching_model_result_t res = iching_model_handle(&s_model, mapped, entropy);

    if (res.sound != ICHING_SOUND_NONE && s_sound_queue) {
        (void)xQueueSend(s_sound_queue, &res.sound, 0);
    }
    if (res.redraw && bsp_lvgl_lock(500)) {
        iching_ui_render(&s_ui, &s_model);
        bsp_lvgl_unlock();
    }
    if (res.save) {
        iching_store_save(&s_model);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Pocket I-Ching & Oblique Oracle");

    (void)iching_store_init(&s_model);

    if (bsp_i2c_init() == ESP_OK) {
        (void)bsp_battery_init();
        if (bsp_audio_init() == ESP_OK &&
            bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1) == ESP_OK) {
            bsp_audio_set_volume(65);
            s_audio_ready = true;
        }
    }

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL init failed");
        return;
    }
    bsp_display_backlight(BACKLIGHT_FULL);

    if (bsp_lvgl_lock(1000)) {
        iching_ui_init(&s_ui, &s_model, &iching_cjk_14, &iching_cjk_16, &iching_cjk_20);
        bsp_lvgl_unlock();
    }

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    s_sound_queue = xQueueCreate(SOUND_QUEUE_DEPTH, sizeof(iching_sound_t));

    if (s_sound_queue) {
        xTaskCreate(sound_worker_task, "iching_snd", 3072, NULL, 3, NULL);
    }

    if (bsp_button_init(on_button_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Button init failed");
        return;
    }
    s_input_ready = true;

    ESP_LOGI(
        TAG,
        "Pocket I-Ching ready: 64 hexagrams, 384 lines, 100 prompts. Free internal heap=%u largest=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
    );

    for (;;) {
        input_event_t ev;
        if (xQueueReceive(s_input_queue, &ev, pdMS_TO_TICKS(INPUT_POLL_MS)) == pdTRUE) {
            process_input_event(&ev);
            continue;
        }

        s_idle_ms += INPUT_POLL_MS;
        if (s_idle_ms >= IDLE_SCREEN_OFF_MS) {
            set_backlight(0);
        } else if (s_idle_ms >= IDLE_DIM_MS) {
            set_backlight(BACKLIGHT_DIM);
        }
    }
}
