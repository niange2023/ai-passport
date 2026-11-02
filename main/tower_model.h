// main/tower_model.h
// 「AI 工位看板 (6 工位大字人话版)」纯逻辑状态机与零泄密协议解析头文件。
//
// 设计原则：
// 1. 一屏 6 个大像素工位（2 列 × 3 行），去掉一切晦涩缩写代码与重复小格子。
// 2. 每个工位直接显示「工具名 + 模糊动作类型」（如「Trae·写文章」「Cursor·写代码」）
//    以及大白话状态（「等你确认」「跑完了!」「正在用」「后台跑着」「休息中」）。
// 3. 零泄密保证：线上协议仅传输固定工具代号、动作枚举 (0..7) 与状态枚举，绝不传输任何窗口标题。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 一屏固定 6 个大像素工位（2 列 × 3 行）
#define TOWER_SLOT_COUNT      6U
#define TOWER_STAGE_COUNT     2U
// 工具代号最大字节数（如 "TRAE1"、"CURS1"）
#define TOWER_CODE_MAX_LEN    8U
// 串口/网络单行同步帧最大长度
#define TOWER_WIRE_MAX_LEN    256U

// 工位的五种运行状态
typedef enum {
    TOWER_STATE_IDLE = 0,    // 休息中：有一阵没动了，像素小人趴桌睡觉
    TOWER_STATE_WAIT = 1,    // 等你确认：卡在选项/审批等你拍板，像素小人举手冒 "?"
    TOWER_STATE_DONE = 2,    // 跑完了!：后台刚跑完还没看，像素小人举手冒 "✓"
    TOWER_STATE_ACTIVE = 3,  // 正在用：当前 Mac 前台正在操作的窗口
    TOWER_STATE_RUN = 4,     // 后台跑着：后台正在执行或刚切走不久
} tower_state_t;

// 隐私安全的模糊动作类型枚举（避免泄露具体文件名/课题名，同时又能分清不同窗口在干嘛）
typedef enum {
    TOWER_ROLE_CODE = 0,     // 写代码
    TOWER_ROLE_WRITE = 1,    // 写文章
    TOWER_ROLE_DOC = 2,      // 看文档
    TOWER_ROLE_TASK = 3,     // 跑任务
    TOWER_ROLE_SEARCH = 4,   // 查资料
    TOWER_ROLE_DESIGN = 5,   // 做设计
    TOWER_ROLE_CHAT = 6,     // 聊协作
    TOWER_ROLE_TERM = 7,     // 跑终端
} tower_role_t;

// 界面层级枚举
typedef enum {
    TOWER_SCREEN_MAIN = 0,   // 6 工位像素看板主页
    TOWER_SCREEN_LINK = 1,   // 连接说明页（长按 OK 切换）
} tower_screen_t;

// 数据链路来源状态
typedef enum {
    TOWER_LINK_OFFLINE = 0,  // 内置演示数据
    TOWER_LINK_USB = 1,      // USB-C 线直连电脑
    TOWER_LINK_WIFI = 2,     // Wi-Fi 无线连接电脑
} tower_link_mode_t;

// 物理三键输入映射
typedef enum {
    TOWER_INPUT_UP = 0,         // 上键单击：选上一个工位
    TOWER_INPUT_DOWN = 1,       // 下键单击：选下一个工位
    TOWER_INPUT_OK = 2,         // 确定键单击：让电脑切到这个窗口
    TOWER_INPUT_OK_DOUBLE = 3,  // 确定键双击：标记这个工位「已看」
    TOWER_INPUT_OK_LONG = 4,    // 确定键长按：查看连接状态页
} tower_input_t;

// 按键处理后请求上层执行的外部动作
typedef enum {
    TOWER_ACTION_NONE = 0,
    TOWER_ACTION_FOCUS = 1,     // 发送 TOWER_CMD FOCUS <slot> <code>
    TOWER_ACTION_ACK = 2,       // 发送 TOWER_CMD ACK <slot> <code>
    TOWER_ACTION_SYNC_NOW = 3,  // 立即请求一次同步
} tower_action_t;

// 单个像素工位的状态描述
typedef struct {
    char code[TOWER_CODE_MAX_LEN];  // 内部切换代号，如 "TRAE1"、"CURS1"
    uint8_t role;                   // 模糊动作类型，取值见 tower_role_t
    uint8_t state;                  // 运行状态，取值见 tower_state_t
    uint16_t minutes;               // 持续分钟数 (1..999)
} tower_slot_t;

// 6 工位像素看板完整状态模型
typedef struct {
    tower_slot_t slots[TOWER_SLOT_COUNT];    // 6 个像素工位
    uint8_t stage_slots[TOWER_STAGE_COUNT];  // 最优先的 2 个工位下标 (0..5)
    uint8_t selected_slot;                   // 当前金色光标选中的工位下标 (0..5)
    uint8_t screen;                          // 当前界面，取值见 tower_screen_t
    uint8_t link_mode;                       // 当前连接方式，取值见 tower_link_mode_t
    uint8_t wait_count;                      // 「等你确认」工位数
    uint8_t done_count;                      // 「跑完了!」工位数
    uint8_t run_count;                       // 「正在用/后台跑着」工位数
    uint8_t idle_count;                      // 「休息中」工位数
    uint8_t anim_tick;                       // 像素小人动画帧计数
    uint8_t toast_ticks;                     // 底部反馈提示剩余显示拍数
    char toast_text[48];                     // 底部反馈提示文案
    uint32_t sync_seq;                       // 成功同步的帧计数
} tower_model_t;

typedef struct {
    bool redraw;
    tower_action_t action;
    uint8_t target_slot;
} tower_model_result_t;

void tower_model_init(tower_model_t *model);
void tower_model_recompute_stage(tower_model_t *model);
void tower_model_get_stage_pair(
    const tower_model_t *model,
    uint8_t *left_slot,
    uint8_t *right_slot
);

// 将内部工位代号与动作枚举格式化为直白中文标题（如 "Trae·写文章"、"Cursor·写代码"）
void tower_model_format_title(
    const tower_slot_t *slot,
    char *out_buf,
    size_t out_size
);

// 将工位状态格式化为直白中文短句（如 "等你确认"、"跑完了!"、"正在用"、"后台跑着"、"休息中"）
const char *tower_model_state_text(uint8_t state);

tower_model_result_t tower_model_handle_input(
    tower_model_t *model,
    tower_input_t input
);

// 解析同步帧：支持 "TOWER_SYNC v1 CODE:STATE:MIN:ROLE,..." 或兼容三段式 "CODE:STATE:MIN"
bool tower_model_parse_sync(
    tower_model_t *model,
    const char *line,
    tower_link_mode_t source_mode
);

bool tower_model_format_cmd(
    const tower_model_t *model,
    tower_action_t action,
    uint8_t slot_index,
    char *out_buf,
    size_t out_size
);

bool tower_model_parse_wifi_cmd(
    const char *line,
    char *ssid_out,
    size_t ssid_size,
    char *pass_out,
    size_t pass_size,
    char *url_out,
    size_t url_size
);
