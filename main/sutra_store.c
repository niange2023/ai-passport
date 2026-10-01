#include "sutra_store.h"

#include <stddef.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define STORE_MAGIC 0x56414A52U
#define STORE_VERSION 2
#define STORE_VERSION_LEGACY 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t current_section;
    uint8_t current_page;
    uint8_t daily_section;
    uint8_t reserved;
    uint32_t completed_lessons;
    uint32_t crc32;
} store_blob_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t current_section;
    uint8_t daily_section;
    uint16_t reserved;
    uint32_t completed_lessons;
    uint32_t crc32;
} store_blob_v1_t;

_Static_assert(sizeof(store_blob_t) == sizeof(store_blob_v1_t),
               "progress migration assumes an unchanged blob size");

static const char *TAG = "sutra_store";
static QueueHandle_t s_queue;
static nvs_handle_t s_nvs;
static bool s_ready;
static volatile bool s_error;

static uint32_t crc32_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

static store_blob_t blob_from_model(const sutra_model_t *model)
{
    store_blob_t blob = {
        .magic = STORE_MAGIC,
        .version = STORE_VERSION,
        .size = sizeof(store_blob_t),
        .current_section = model->section,
        .current_page = model->page,
        .daily_section = model->daily_section,
        .completed_lessons = model->completed_lessons,
    };
    blob.crc32 = crc32_bytes(&blob, offsetof(store_blob_t, crc32));
    return blob;
}

static bool restore_blob(const store_blob_t *blob, sutra_model_t *model)
{
    if (blob->magic != STORE_MAGIC || blob->size != sizeof(*blob) ||
        blob->crc32 != crc32_bytes(blob, offsetof(store_blob_t, crc32))) {
        return false;
    }

    uint8_t section;
    uint8_t page;
    uint8_t daily_section;
    if (blob->version == STORE_VERSION) {
        section = blob->current_section;
        page = blob->current_page;
        daily_section = blob->daily_section;
    } else if (blob->version == STORE_VERSION_LEGACY) {
        const store_blob_v1_t *legacy = (const store_blob_v1_t *)blob;
        section = legacy->current_section;
        page = 0;
        daily_section = legacy->daily_section;
    } else {
        return false;
    }
    if (section >= SUTRA_SECTION_COUNT ||
        page >= SUTRA_SECTIONS[section].page_count ||
        daily_section >= SUTRA_SECTION_COUNT) {
        return false;
    }

    sutra_model_defaults(model);
    model->section = section;
    model->page = page;
    model->daily_section = daily_section;
    model->completed_lessons = blob->completed_lessons;
    return true;
}

static void save_task(void *arg)
{
    (void)arg;
    store_blob_t blob;
    for (;;) {
        if (xQueueReceive(s_queue, &blob, portMAX_DELAY) != pdTRUE) continue;
        esp_err_t error = nvs_set_blob(s_nvs, "progress", &blob, sizeof(blob));
        if (error == ESP_OK) error = nvs_commit(s_nvs);
        if (error != ESP_OK) {
            s_error = true;
            ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(error));
        }
    }
}

bool sutra_store_init(sutra_model_t *model)
{
    if (!model) return false;
    if (s_ready) return true;

    sutra_model_defaults(model);
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "NVS unavailable; progress stays in memory: %s",
                 esp_err_to_name(error));
        s_error = true;
        return false;
    }

    error = nvs_open("vajra", NVS_READWRITE, &s_nvs);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace unavailable: %s", esp_err_to_name(error));
        s_error = true;
        return false;
    }

    store_blob_t blob;
    size_t size = sizeof(blob);
    error = nvs_get_blob(s_nvs, "progress", &blob, &size);
    if (error == ESP_OK && size == sizeof(blob)) {
        if (!restore_blob(&blob, model)) {
            ESP_LOGW(TAG, "Stored progress is invalid; using defaults");
        }
    } else if (error != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS read failed: %s", esp_err_to_name(error));
    }

    s_queue = xQueueCreate(1, sizeof(store_blob_t));
    if (!s_queue ||
        xTaskCreate(save_task, "sutra_save", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start progress worker");
        s_error = true;
        return false;
    }
    s_ready = true;
    return true;
}

void sutra_store_request_save(const sutra_model_t *model)
{
    if (!s_ready || !model) return;
    const store_blob_t blob = blob_from_model(model);
    if (xQueueOverwrite(s_queue, &blob) != pdTRUE) s_error = true;
}

bool sutra_store_has_error(void)
{
    return s_error;
}
