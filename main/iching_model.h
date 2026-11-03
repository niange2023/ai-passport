// main/iching_model.h
// 「知几 · 随身《周易》与破局签 (Pocket I-Ching & Oblique Oracle)」纯逻辑状态机头文件。
//
// 设计原则：
// 1. 与 ESP-IDF / LVGL 完全解耦，支持在主机端通过 C11 单元测试完整验证。
// 2. 正统三枚铜钱六爻起卦法（纳甲噬法）：
//    - 3 枚铜钱：字(阳)=3，背(阴)=2；
//    - 三背 = 6（老阴 ⚋×，动爻，阴变阳）；
//    - 二背一字 = 7（少阳 ⚊，静爻）；
//    - 二字一背 = 8（少阴 ⚋，静爻）；
//    - 三字 = 9（老阳 ⚊○，动爻，阳变阴）。
// 3. 自动推演本卦 (primary_hex)、之卦/变卦 (changed_hex) 与本卦主爻（动爻优先），
//    并配一枚对应的第一性原理破局签 (0..99)。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "iching_content.h"

#define ICHING_MODEL_VERSION  1U
#define ICHING_HOME_ITEMS     4U

// 六爻铜钱数值枚举
typedef enum {
    ICHING_VAL_OLD_YIN    = 6,  // 老阴 (三背 2+2+2)，动爻，本阴变阳
    ICHING_VAL_YOUNG_YANG = 7,  // 少阳 (二背一字)，静爻，阳爻
    ICHING_VAL_YOUNG_YIN  = 8,  // 少阴 (二字一背)，静爻，阴爻
    ICHING_VAL_OLD_YANG   = 9,  // 老阳 (三字 3+3+3)，动爻，本阳变阴
} iching_line_val_t;

// 界面层级枚举
typedef enum {
    ICHING_SCREEN_HOME    = 0,  // 首页四入口（六爻起卦 / 盲抽破局签 / 六十四卦典藏 / 上次卦签）
    ICHING_SCREEN_CAST    = 1,  // 三枚铜钱六爻起卦仪式页
    ICHING_SCREEN_READING = 2,  // 卦象与爻辞品读页（4 子页：本卦大象 / 爻辞与小象 / 义理与之卦 / 破局签）
    ICHING_SCREEN_ORACLE  = 3,  // 100 条第一性原理破局签独立抽签页
    ICHING_SCREEN_BOOK    = 4,  // 六十四卦全书目录页
} iching_screen_t;

// 物理三键输入映射
typedef enum {
    ICHING_INPUT_UP        = 0,  // 上键单击
    ICHING_INPUT_DOWN      = 1,  // 下键单击
    ICHING_INPUT_OK        = 2,  // 确定键单击
    ICHING_INPUT_OK_DOUBLE = 3,  // 确定键双击
    ICHING_INPUT_BACK      = 4,  // 确定键长按（返回上一级 / 首页）
} iching_input_t;

// 触发 ES8311 扬声器播放的声学反馈类型
typedef enum {
    ICHING_SOUND_NONE     = 0,
    ICHING_SOUND_COIN     = 1,  // 清脆铜钱落盘声（每次抛掷一爻）
    ICHING_SOUND_COMPLETE = 2,  // 六爻成卦双音颂钵和弦
    ICHING_SOUND_SLIP     = 3,  // 竹签清响（抽取破局签）
} iching_sound_t;

typedef struct {
    uint8_t version;
    uint8_t screen;                             // 当前页面，见 iching_screen_t
    uint8_t home_selection;                     // 首页选中项 (0..3)
    uint8_t cast_step;                          // 已抛掷爻数 (0..6)
    uint8_t cast_coins[3];                      // 最近一次抛出的三枚铜钱正反 (0=背2, 1=字3)
    uint8_t cast_lines[ICHING_LINE_COUNT];      // 六爻铜钱数值 (6/7/8/9)，下标 0=初爻 .. 5=上爻
    uint8_t primary_hex;                        // 本卦下标 (0..63)
    uint8_t changed_hex;                        // 之卦(变卦)下标 (0..63)
    uint8_t moving_mask;                        // 6-bit 动爻掩码 (bit i=1 表示第 i+1 爻为老阴6或老阳9)
    uint8_t focus_line;                         // 当前聚焦品读的爻位 (0..5，对应初爻..上爻)
    uint8_t reading_tab;                        // 品读页子页 (0=卦辞大象, 1=三百八十四爻辞, 2=义理与变卦, 3=关联破局签)
    uint8_t oracle_index;                       // 当前抽中的破局签下标 (0..99)
    uint8_t book_cursor;                        // 六十四卦全书目录选中卦下标 (0..63)
    uint8_t has_saved_cast;                     // 是否有已完成的起卦记录
    uint32_t cast_count;                        // 累计起卦次数
} iching_model_t;

typedef struct {
    bool redraw;
    bool save;
    iching_sound_t sound;
} iching_model_result_t;

// 初始化默认状态
void iching_model_defaults(iching_model_t *model);

// 从 NVS 恢复后校验边界合法性
void iching_model_restore(iching_model_t *model);

// 根据随机熵源计算单次三枚铜钱结果 (返回 6/7/8/9)，并填充 coins_out[3] (0=背, 1=字)
uint8_t iching_toss_three_coins(uint32_t entropy, uint8_t coins_out[3]);

// 根据六爻数值 (6/7/8/9) 计算本卦、之卦、动爻掩码与默认聚焦爻位
void iching_compute_cast(iching_model_t *model);

// 处理按键输入（entropy 由调用方传入硬件随机数或高精度计时器值）
iching_model_result_t iching_model_handle(
    iching_model_t *model,
    iching_input_t input,
    uint32_t entropy
);
