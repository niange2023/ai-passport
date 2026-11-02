// main/main.c
// AI Passport 「3-B 双焦像素总控台 (Command Tower)」主应用入口。
//
// 架构与运行时约束：
// 1. 按键回调仅将轻量事件入队 s_input_queue，绝不阻塞。
// 2. 应用主循环与后台同步任务访问 LVGL 控件时均严格持有 bsp_lvgl_lock()。
// 3. 支持双模实时联动：
//    - USB-C 串口直连（读取主机发送的 TOWER_SYNC v1 帧，按键时输出 TOWER_CMD 帧）；
//    - Wi-Fi 局域网轮询（若 NVS 已配置 Wi-Fi 凭据与 Mac 地址，后台自动连接并拉取最新状态）。
// 4. 全程零敏感信息：仅传输与展示脱敏应用代号、五态枚举与持续分钟数。
#include "tower_model.h"
#include "tower_store.h"
#include "tower_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

LV_FONT_DECLARE(tower_cjk_12);
LV_FONT_DECLARE(tower_cjk_14);

#define INPUT_QUEUE_DEPTH     8
#define INPUT_POLL_MS         250
#define IDLE_DIM_MS           60000U
#define IDLE_SCREEN_OFF_MS    300000U
#define BACKLIGHT_FULL        100
#define BACKLIGHT_DIM         25

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} input_event_t;

static const char *TAG = "command_tower";

static tower_model_t s_model;
static tower_wifi_cfg_t s_wifi_cfg;
static tower_ui_t s_ui;
static QueueHandle_t s_input_queue;
static SemaphoreHandle_t s_model_mutex;
static volatile bool s_input_ready = false;
static volatile bool s_wifi_connected = false;
static volatile bool s_wifi_sync_requested = false;
static uint32_t s_idle_ms = 0;
static uint8_t s_backlight = BACKLIGHT_FULL;
static bool s_usb_driver_ready = false;

static void set_backlight(uint8_t percent)
{
    if (s_backlight == percent) return;
    s_backlight = percent;
    bsp_display_backlight(percent);
}

// 将 BSP 按钮事件映射为总控台纯逻辑输入枚举
static bool map_input(const input_event_t *ev, tower_input_t *out)
{
    if (ev->event == BSP_BTN_CLICK) {
        if (ev->button == BSP_BTN_UP) *out = TOWER_INPUT_UP;
        else if (ev->button == BSP_BTN_DOWN) *out = TOWER_INPUT_DOWN;
        else if (ev->button == BSP_BTN_OK) *out = TOWER_INPUT_OK;
        else return false;
        return true;
    }
    if (ev->event == BSP_BTN_DOUBLE && ev->button == BSP_BTN_OK) {
        *out = TOWER_INPUT_OK_DOUBLE;
        return true;
    }
    if (ev->event == BSP_BTN_LONG && ev->button == BSP_BTN_OK) {
        *out = TOWER_INPUT_OK_LONG;
        return true;
    }
    return false;
}

// 按键中断/轮询回调：仅做非阻塞入队
static void on_button_event(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t item = { .button = button, .event = event };
    (void)xQueueSend(s_input_queue, &item, 0);
}

// 通过 USB-Serial-JTAG 向 Mac 发送单行控制指令（如 TOWER_CMD FOCUS 0 AIME）
static void send_serial_command(const char *cmd_line)
{
    if (!cmd_line) return;
    size_t len = strlen(cmd_line);
    if (s_usb_driver_ready && usb_serial_jtag_is_driver_installed()) {
        (void)usb_serial_jtag_write_bytes(cmd_line, len, pdMS_TO_TICKS(100));
    } else {
        fputs(cmd_line, stdout);
        fflush(stdout);
    }
}

// 通过 HTTP POST 将切窗/收菜指令同步给局域网 Mac（当 Wi-Fi 已连接时）
static void send_http_action(tower_action_t action, uint8_t slot_index, const char *code)
{
    if (!s_wifi_connected || !s_wifi_cfg.configured) return;

    char action_url[160];
    const char *wire_pos = strstr(s_wifi_cfg.url, "/wire");
    if (wire_pos) {
        size_t base_len = (size_t)(wire_pos - s_wifi_cfg.url);
        if (base_len + 8U >= sizeof(action_url)) return;
        memcpy(action_url, s_wifi_cfg.url, base_len);
        strcpy(action_url + base_len, "/action");
    } else {
        snprintf(action_url, sizeof(action_url), "%s/action", s_wifi_cfg.url);
    }

    char body[96];
    const char *cmd_name = (action == TOWER_ACTION_FOCUS) ? "focus" : "ack";
    int body_len = snprintf(
        body,
        sizeof(body),
        "{\"command\":\"%s\",\"slot\":%u,\"code\":\"%s\"}",
        cmd_name,
        (unsigned)slot_index,
        code
    );
    if (body_len <= 0 || (size_t)body_len >= sizeof(body)) return;

    esp_http_client_config_t config = {
        .url = action_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 1500,
        .buffer_size = 512,
        .buffer_size_tx = 256,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, body_len);
    (void)esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// 处理接收到的一行外部命令（来自 USB 串口或 HTTP 响应）
static void handle_incoming_line(const char *line, tower_link_mode_t source_mode)
{
    if (!line || line[0] == '\0') return;

    // 1. 检查是否为串口下发的 Wi-Fi 配网指令：TOWER_WIFI <ssid> <pass> <url>
    char ssid[33];
    char pass[65];
    char url[128];
    if (tower_model_parse_wifi_cmd(line, ssid, sizeof(ssid), pass, sizeof(pass), url, sizeof(url))) {
        if (tower_store_save_wifi(ssid, pass, url)) {
            (void)tower_store_load_wifi(&s_wifi_cfg);
            s_wifi_sync_requested = true;
            if (xSemaphoreTake(s_model_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                snprintf(s_model.toast_text, sizeof(s_model.toast_text), "已保存 Wi-Fi: %.16s", ssid);
                s_model.toast_ticks = 8;
                if (bsp_lvgl_lock(200)) {
                    tower_ui_render(&s_ui, &s_model);
                    bsp_lvgl_unlock();
                }
                xSemaphoreGive(s_model_mutex);
            }
        }
        return;
    }

    // 2. 检查是否为 12 窗口脱敏状态同步帧：TOWER_SYNC v1 ...
    if (xSemaphoreTake(s_model_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool ok = tower_model_parse_sync(&s_model, line, source_mode);
        if (ok) {
            s_idle_ms = 0;
            set_backlight(BACKLIGHT_FULL);
            if (bsp_lvgl_lock(200)) {
                tower_ui_render(&s_ui, &s_model);
                bsp_lvgl_unlock();
            }
        }
        xSemaphoreGive(s_model_mutex);
    }
}

// USB-Serial-JTAG 后台读取任务：遵循 serial-screenshot-protocol 经验，
// 先安装驱动并压低任务优先级至 3，每次空读或错误均带 vTaskDelay 退避，绝不忙等。
static void usb_serial_rx_task(void *arg)
{
    (void)arg;
    char line_buf[TOWER_WIRE_MAX_LEN];
    size_t line_len = 0;
    uint8_t rx_chunk[64];

    for (;;) {
        if (!s_usb_driver_ready || !usb_serial_jtag_is_driver_installed()) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        int n = usb_serial_jtag_read_bytes(rx_chunk, sizeof(rx_chunk), pdMS_TO_TICKS(100));
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        for (int i = 0; i < n; i++) {
            char ch = (char)rx_chunk[i];
            if (ch == '\r' || ch == '\n') {
                if (line_len > 0) {
                    line_buf[line_len] = '\0';
                    handle_incoming_line(line_buf, TOWER_LINK_USB);
                    line_len = 0;
                }
            } else if (line_len + 1U < sizeof(line_buf)) {
                line_buf[line_len++] = ch;
            } else {
                // 超长行直接丢弃并重置缓冲，防止异常输入污染
                line_len = 0;
            }
        }
    }
}

// Wi-Fi 事件回调：跟踪 STA 连接与 IP 获取状态
static void on_wifi_event(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_connected = true;
        ESP_LOGI(TAG, "Wi-Fi STA connected and got IP");
    }
}

// 通过 HTTP GET 拉取一次 Mac 端的 /api/passport-tower/wire 单行状态帧
static void poll_http_tower_wire(void)
{
    if (!s_wifi_connected || !s_wifi_cfg.configured) return;

    esp_http_client_config_t config = {
        .url = s_wifi_cfg.url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 2000,
        .buffer_size = 512,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return;

    if (esp_http_client_open(client, 0) == ESP_OK) {
        int content_len = esp_http_client_fetch_headers(client);
        (void)content_len;
        char buf[TOWER_WIRE_MAX_LEN];
        int read_len = esp_http_client_read(client, buf, sizeof(buf) - 1);
        if (read_len > 0) {
            buf[read_len] = '\0';
            handle_incoming_line(buf, TOWER_LINK_WIFI);
        }
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
}

// Wi-Fi 后台轮询任务：仅在 NVS 已配置 Wi-Fi 时按需初始化 STA 并每 3 秒轮询一次
static void wifi_poll_task(void *arg)
{
    (void)arg;
    bool wifi_initialized = false;

    for (;;) {
        if (!s_wifi_cfg.configured) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!wifi_initialized) {
            if (esp_netif_init() == ESP_OK &&
                esp_event_loop_create_default() == ESP_OK) {
                esp_netif_create_default_wifi_sta();
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                if (esp_wifi_init(&cfg) == ESP_OK) {
                    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL);
                    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL);
                    wifi_config_t sta_cfg = { 0 };
                    strncpy((char *)sta_cfg.sta.ssid, s_wifi_cfg.ssid, sizeof(sta_cfg.sta.ssid) - 1);
                    strncpy((char *)sta_cfg.sta.password, s_wifi_cfg.password, sizeof(sta_cfg.sta.password) - 1);
                    esp_wifi_set_mode(WIFI_MODE_STA);
                    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
                    esp_wifi_start();
                    wifi_initialized = true;
                }
            }
        } else if (s_wifi_sync_requested) {
            s_wifi_sync_requested = false;
            wifi_config_t sta_cfg = { 0 };
            strncpy((char *)sta_cfg.sta.ssid, s_wifi_cfg.ssid, sizeof(sta_cfg.sta.ssid) - 1);
            strncpy((char *)sta_cfg.sta.password, s_wifi_cfg.password, sizeof(sta_cfg.sta.password) - 1);
            esp_wifi_disconnect();
            esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
            esp_wifi_connect();
        }

        if (s_wifi_connected) {
            poll_http_tower_wire();
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// 处理一次物理按键事件
static void process_button_input(const input_event_t *ev)
{
    tower_input_t mapped;
    if (!map_input(ev, &mapped)) return;

    // 息屏或暗屏状态下首次按键先恢复全亮背光
    if (s_backlight == 0) {
        s_idle_ms = 0;
        set_backlight(BACKLIGHT_FULL);
        return;
    }
    s_idle_ms = 0;
    set_backlight(BACKLIGHT_FULL);

    tower_model_result_t result = { 0 };
    char cmd_line[64] = { 0 };
    char target_code[TOWER_CODE_MAX_LEN] = { 0 };

    if (xSemaphoreTake(s_model_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        result = tower_model_handle_input(&s_model, mapped);
        if (result.action == TOWER_ACTION_FOCUS || result.action == TOWER_ACTION_ACK) {
            (void)tower_model_format_cmd(
                &s_model, result.action, result.target_slot, cmd_line, sizeof(cmd_line)
            );
            strncpy(
                target_code,
                s_model.slots[result.target_slot].code,
                sizeof(target_code) - 1
            );
        }
        if (result.redraw && bsp_lvgl_lock(200)) {
            tower_ui_render(&s_ui, &s_model);
            bsp_lvgl_unlock();
        }
        xSemaphoreGive(s_model_mutex);
    }

    if (cmd_line[0] != '\0') {
        send_serial_command(cmd_line);
        send_http_action(result.action, result.target_slot, target_code);
    } else if (result.action == TOWER_ACTION_SYNC_NOW) {
        send_serial_command("TOWER_CMD SYNC\n");
        poll_http_tower_wire();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting 3-B Dual-Focus Command Tower");

    s_model_mutex = xSemaphoreCreateMutex();
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));

    // 安装 USB-Serial-JTAG 驱动以支持安全的双向串口帧收发
    usb_serial_jtag_driver_config_t usb_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 256,
    };
    if (usb_serial_jtag_driver_install(&usb_cfg) == ESP_OK) {
        usb_serial_jtag_vfs_use_driver();
        s_usb_driver_ready = true;
    }

    (void)tower_store_init();
    (void)tower_store_load_wifi(&s_wifi_cfg);
    tower_model_init(&s_model);

    if (bsp_i2c_init() == ESP_OK) {
        (void)bsp_battery_init();
    }

    ESP_ERROR_CHECK(bsp_display_init());
    ESP_ERROR_CHECK(bsp_button_init(on_button_event, NULL));

    if (bsp_lvgl_lock(1000)) {
        tower_ui_init(&s_ui, &s_model, &s_wifi_cfg, &tower_cjk_12, &tower_cjk_14);
        bsp_lvgl_unlock();
    }

    s_input_ready = true;

    // 启动低优先级 (3 < LVGL 4) 串口监听与 Wi-Fi 轮询任务
    xTaskCreate(usb_serial_rx_task, "tower_usb_rx", 4096, NULL, 3, NULL);
    xTaskCreate(wifi_poll_task, "tower_wifi", 5120, NULL, 3, NULL);

    ESP_LOGI(
        TAG,
        "Command Tower ready. Free internal heap=%u largest=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
    );

    uint32_t anim_accum_ms = 0;
    for (;;) {
        input_event_t ev;
        if (xQueueReceive(s_input_queue, &ev, pdMS_TO_TICKS(INPUT_POLL_MS)) == pdTRUE) {
            process_button_input(&ev);
            continue;
        }

        s_idle_ms += INPUT_POLL_MS;
        anim_accum_ms += INPUT_POLL_MS;

        if (s_idle_ms >= IDLE_SCREEN_OFF_MS) {
            set_backlight(0);
        } else if (s_idle_ms >= IDLE_DIM_MS) {
            set_backlight(BACKLIGHT_DIM);
        }

        // 每 1000ms 更新一次上屏像素小人动画帧（敲键盘/挥手）与底部 Toast 倒计时
        if (anim_accum_ms >= 1000U && s_backlight > 0) {
            anim_accum_ms = 0;
            if (xSemaphoreTake(s_model_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                s_model.anim_tick++;
                if (s_model.toast_ticks > 0) {
                    s_model.toast_ticks--;
                }
                if (bsp_lvgl_lock(100)) {
                    tower_ui_render(&s_ui, &s_model);
                    bsp_lvgl_unlock();
                }
                xSemaphoreGive(s_model_mutex);
            }
        }
    }
}
