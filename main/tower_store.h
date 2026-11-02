// main/tower_store.h
// 「3-B 双焦像素总控台」NVS 配网与远端地址持久化接口。
//
// 保存通过串口 TOWER_WIFI 指令配置的 Wi-Fi 凭据与 Mac 端同步 URL。
// 遵循安全基线：初始化 NVS 失败时绝不自动擦除整个 NVS 分区。
#pragma once

#include <stdbool.h>

typedef struct {
    bool configured;
    char ssid[33];
    char password[65];
    char url[128];
} tower_wifi_cfg_t;

// 初始化 NVS（不执行破坏性擦除）。成功返回 true。
bool tower_store_init(void);

// 从 NVS 读取已保存的 Wi-Fi 与 Mac 同步地址配置。
bool tower_store_load_wifi(tower_wifi_cfg_t *cfg);

// 将新的 Wi-Fi 凭据与 Mac 同步地址写入 NVS。
bool tower_store_save_wifi(
    const char *ssid,
    const char *password,
    const char *url
);
