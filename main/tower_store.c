// main/tower_store.c
// 「3-B 双焦像素总控台」NVS 配网与远端地址持久化实现。
#include "tower_store.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "tower_store";
static const char *NVS_NAMESPACE = "tower_cfg";
static bool s_nvs_ready = false;

bool tower_store_init(void)
{
    if (s_nvs_ready) return true;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed (%s); preserving partition without erase",
                 esp_err_to_name(err));
        return false;
    }
    s_nvs_ready = true;
    return true;
}

bool tower_store_load_wifi(tower_wifi_cfg_t *cfg)
{
    if (!cfg) return false;
    memset(cfg, 0, sizeof(*cfg));
    if (!tower_store_init()) return false;

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t ssid_len = sizeof(cfg->ssid);
    size_t pass_len = sizeof(cfg->password);
    size_t url_len = sizeof(cfg->url);

    esp_err_t e1 = nvs_get_str(handle, "ssid", cfg->ssid, &ssid_len);
    esp_err_t e2 = nvs_get_str(handle, "pass", cfg->password, &pass_len);
    esp_err_t e3 = nvs_get_str(handle, "url", cfg->url, &url_len);
    nvs_close(handle);

    if (e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK &&
        cfg->ssid[0] != '\0' && cfg->url[0] != '\0') {
        cfg->configured = true;
        return true;
    }
    memset(cfg, 0, sizeof(*cfg));
    return false;
}

bool tower_store_save_wifi(
    const char *ssid,
    const char *password,
    const char *url
)
{
    if (!ssid || !password || !url || ssid[0] == '\0' || url[0] == '\0') {
        return false;
    }
    if (!tower_store_init()) return false;

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_set_str(handle, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, "pass", password);
    if (err == ESP_OK) err = nvs_set_str(handle, "url", url);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save Wi-Fi config: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
