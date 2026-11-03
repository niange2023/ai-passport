// main/iching_store.c
// 「知几 · 随身《周易》与破局签」NVS 持久化实现。
#include "iching_store.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "iching_store";
static const char *NVS_NAMESPACE = "iching_state";
static const char *NVS_KEY_MODEL = "model_v1";
static bool s_nvs_ready = false;

bool iching_store_init(iching_model_t *model)
{
    if (!model) return false;
    iching_model_defaults(model);

    if (!s_nvs_ready) {
        esp_err_t err = nvs_flash_init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "NVS init failed (%s); continuing in memory without erasing",
                     esp_err_to_name(err));
            return false;
        }
        s_nvs_ready = true;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return true;
    }

    iching_model_t saved;
    size_t len = sizeof(saved);
    esp_err_t err = nvs_get_blob(handle, NVS_KEY_MODEL, &saved, &len);
    nvs_close(handle);

    if (err == ESP_OK && len == sizeof(saved)) {
        *model = saved;
        iching_model_restore(model);
    }
    return true;
}

void iching_store_save(const iching_model_t *model)
{
    if (!model || !s_nvs_ready) return;

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(handle, NVS_KEY_MODEL, model, sizeof(*model)) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}
