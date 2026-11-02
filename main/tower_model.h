// main/tower_model.h
// 「3-B 双焦像素总控台 (Command Tower)」纯逻辑状态机与零泄密协议解析头文件。
//
// 设计原则：
// 1. 与 ESP-IDF / LVGL 完全解耦，支持在主机环境通过 C11 单元测试完整验证。
// 2. 零敏感信息：仅保存固定脱敏工位代号（如 AIME、TRAE1）、五态枚举和分钟数，
//    绝不接收、保存或显示任何具体任务标题、文档文件名或屏幕 OCR 原文。
// 3. 零动态内存分配：所有结构体与解析缓冲均为固定大小，避免无 PSRAM 下的堆碎片。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 固定 12 个工作窗口槽位（对应下屏 4x3 雷达矩阵）
#define TOWER_SLOT_COUNT      12U
// 上屏放大展示的紧急像素工位数量（左/右双焦工位）
#define TOWER_STAGE_COUNT     2U
// 工位代号最大字节数（含结尾 '\0'，例如 "TRAE1"）
#define TOWER_CODE_MAX_LEN    8U
// 串口/网络单行同步帧最大长度
#define TOWER_WIRE_MAX_LEN    256U

// 窗口工位的五种脱敏运行状态
typedef enum {
    TOWER_STATE_IDLE = 0,    // 暗灰：后台闲置 (>15m)，像素小人趴桌休息
    TOWER_STATE_WAIT = 1,    // 橙红：卡在审批/确认等你拍板，像素小人举手呼救
    TOWER_STATE_DONE = 2,    // 翠绿：后台任务刚跑完待收菜，像素小人举手报喜
    TOWER_STATE_ACTIVE = 3,  // 亮黄：当前 Mac 前台正在操作的焦点窗口
    TOWER_STATE_RUN = 4,     // 深蓝：后台持续运行/生成中，像素小人敲键盘
} tower_state_t;

// 界面层级枚举
typedef enum {
    TOWER_SCREEN_MAIN = 0,   // 3-B 双焦像素总控台主页（上2大工位 + 下12窗矩阵）
    TOWER_SCREEN_LINK = 1,   // 连接状态与配网说明页（长按 OK 切换）
} tower_screen_t;

// 数据链路来源状态
typedef enum {
    TOWER_LINK_OFFLINE = 0,  // 内置离线演示快照兜底
    TOWER_LINK_USB = 1,      // USB-C 串口实时直连同步
    TOWER_LINK_WIFI = 2,     // Wi-Fi 局域网轮询 cognitive_api / passport_tower
} tower_link_mode_t;

// 物理三键输入映射
typedef enum {
    TOWER_INPUT_UP = 0,         // 上键单击：移动选中光标到上一个窗口槽位
    TOWER_INPUT_DOWN = 1,       // 下键单击：移动选中光标到下一个窗口槽位
    TOWER_INPUT_OK = 2,         // 确定键单击：让 Mac 立即将选中窗口切到最前台
    TOWER_INPUT_OK_DOUBLE = 3,  // 确定键双击：标记选中窗口「已收菜/已阅」
    TOWER_INPUT_OK_LONG = 4,    // 确定键长按：在总控台主页与连接信息页之间切换
} tower_input_t;

// 按键处理后请求上层执行的外部动作
typedef enum {
    TOWER_ACTION_NONE = 0,
    TOWER_ACTION_FOCUS = 1,     // 发送 TOWER_CMD FOCUS <slot> <code>
    TOWER_ACTION_ACK = 2,       // 发送 TOWER_CMD ACK <slot> <code>
    TOWER_ACTION_SYNC_NOW = 3,  // 在连接页按 OK 立即请求一次同步
} tower_action_t;

// 单个窗口槽位的脱敏状态描述
typedef struct {
    char code[TOWER_CODE_MAX_LEN];  // 脱敏应用+窗口代号，如 "AIME"、"TRAE1"
    uint8_t state;                  // 取值见 tower_state_t
    uint16_t minutes;               // 当前状态持续分钟数 (1..999)
} tower_slot_t;

// 3-B 双焦像素总控台完整状态模型
typedef struct {
    tower_slot_t slots[TOWER_SLOT_COUNT];    // 12 个工作窗口槽位
    uint8_t stage_slots[TOWER_STAGE_COUNT];  // 最紧急的前 2 个槽位下标 (0..11)
    uint8_t selected_slot;                   // 当前光标选中的槽位下标 (0..11)
    uint8_t screen;                          // 当前界面，取值见 tower_screen_t
    uint8_t link_mode;                       // 当前连接方式，取值见 tower_link_mode_t
    uint8_t wait_count;                      // 处于 TOWER_STATE_WAIT 的窗口总数
    uint8_t done_count;                      // 处于 TOWER_STATE_DONE 的窗口总数
    uint8_t run_count;                       // 处于 RUN 或 ACTIVE 的窗口总数
    uint8_t idle_count;                      // 处于 IDLE 的窗口总数
    uint8_t anim_tick;                       // 像素小人动画帧计数 (0..255)
    uint8_t toast_ticks;                     // 底部操作反馈提示剩余显示拍数
    char toast_text[48];                     // 底部操作反馈提示文案
    uint32_t sync_seq;                       // 成功同步的帧计数
} tower_model_t;

// 状态机单步输入处理返回值
typedef struct {
    bool redraw;            // 是否需要刷新 LVGL 界面
    tower_action_t action;  // 需要由串口/Wi-Fi 发送给 Mac 的控制动作
    uint8_t target_slot;    // 动作针对的槽位下标 (0..11)
} tower_model_result_t;

// 初始化总控台模型并加载默认 12 窗口脱敏演示快照。
void tower_model_init(tower_model_t *model);

// 根据当前 12 个槽位的状态与持续时间，重新计算统计计数与 Top 2 紧急工位。
void tower_model_recompute_stage(tower_model_t *model);

// 获取上屏左右两个大像素工位应当展示的槽位下标：
// 左工位固定为最紧急 1 号 (stage_slots[0])；
// 右工位在用户选中其他槽位时跟随显示 selected_slot，否则显示紧急 2 号 (stage_slots[1])。
void tower_model_get_stage_pair(
    const tower_model_t *model,
    uint8_t *left_slot,
    uint8_t *right_slot
);

// 处理三键事件，更新选中槽位、界面或确认状态，并返回外部动作指令。
tower_model_result_t tower_model_handle_input(
    tower_model_t *model,
    tower_input_t input
);

// 解析一行或多行文本中的 "TOWER_SYNC v1 <CODE>:<STATE>:<MIN>,..." 同步帧。
// 成功时更新 model->slots、重算紧急工位、自动将光标跳至新增紧急工位并返回 true。
bool tower_model_parse_sync(
    tower_model_t *model,
    const char *line,
    tower_link_mode_t source_mode
);

// 将设备发往 Mac 的动作格式化为单行 ASCII 指令（如 "TOWER_CMD FOCUS 0 AIME\n"）。
bool tower_model_format_cmd(
    const tower_model_t *model,
    tower_action_t action,
    uint8_t slot_index,
    char *out_buf,
    size_t out_size
);

// 解析串口下发的 Wi-Fi 配网指令："TOWER_WIFI <ssid> <password> <url>"
bool tower_model_parse_wifi_cmd(
    const char *line,
    char *ssid_out,
    size_t ssid_size,
    char *pass_out,
    size_t pass_size,
    char *url_out,
    size_t url_size
);
