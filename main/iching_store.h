// main/iching_store.h
// 「知几 · 随身《周易》与破局签」NVS 持久化存储接口。
//
// 保存最近一次起卦结果、抽中的破局签号与累计起卦次数。
// 遵循安全基线：初始化 NVS 失败时绝不自动擦除整个分区。
#pragma once

#include <stdbool.h>
#include "iching_model.h"

bool iching_store_init(iching_model_t *model);
void iching_store_save(const iching_model_t *model);
