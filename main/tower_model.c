// main/tower_model.c
// 「3-B 双焦像素总控台 (Command Tower)」纯逻辑状态机与零泄密协议解析实现。
//
// 所有函数均不依赖 ESP-IDF 或 LVGL，可在主机测试与设备运行时复用。
#include "tower_model.h"

#include <stdio.h>
#include <string.h>

// 内置 12 窗口脱敏演示快照：当设备未连接 USB 串口或 Wi-Fi 时提供开箱即用的完整体验。
// 仅含固定应用代号、状态枚举和分钟数，不含任何业务或文档名称。
static const tower_slot_t k_default_slots[TOWER_SLOT_COUNT] = {
    { "AIME",  TOWER_STATE_WAIT,   8  },
    { "TRAE1", TOWER_STATE_DONE,   2  },
    { "CURS2", TOWER_STATE_DONE,   5  },
    { "MULT",  TOWER_STATE_WAIT,   3  },
    { "CURS1", TOWER_STATE_ACTIVE, 1  },
    { "TRAE2", TOWER_STATE_RUN,    4  },
    { "CHRM1", TOWER_STATE_RUN,    6  },
    { "CHRM2", TOWER_STATE_RUN,    11 },
    { "GROK",  TOWER_STATE_IDLE,   19 },
    { "SHIY",  TOWER_STATE_IDLE,   24 },
    { "TERM",  TOWER_STATE_IDLE,   32 },
    { "FEIS",  TOWER_STATE_IDLE,   41 },
};

// 计算单个窗口槽位在「上屏紧急工位」排序中的优先级得分（数值越小越优先）。
// 排序规则：
// 1. TOWER_STATE_WAIT（等拍板）最优先，且等待分钟数越久越靠前；
// 2. TOWER_STATE_DONE（待收菜）次之，且完成未查看分钟数越久越靠前；
// 3. TOWER_STATE_ACTIVE（当前焦点窗口）第三；
// 4. TOWER_STATE_RUN（后台运行中）第四，越近活跃越靠前；
// 5. TOWER_STATE_IDLE（闲置）最后。
static int32_t slot_priority_key(const tower_slot_t *slot, uint8_t index)
{
    int32_t rank = 4;
    int32_t time_score = (int32_t)slot->minutes;

    switch ((tower_state_t)slot->state) {
    case TOWER_STATE_WAIT:
        rank = 0;
        time_score = 1000 - (int32_t)slot->minutes;
        break;
    case TOWER_STATE_DONE:
        rank = 1;
        time_score = 1000 - (int32_t)slot->minutes;
        break;
    case TOWER_STATE_ACTIVE:
        rank = 2;
        break;
    case TOWER_STATE_RUN:
        rank = 3;
        break;
    case TOWER_STATE_IDLE:
    default:
        rank = 4;
        break;
    }
    return rank * 100000 + time_score * 32 + (int32_t)index;
}

// 重新统计各状态窗口数量，并选出最紧急的前 2 个槽位放入 stage_slots。
void tower_model_recompute_stage(tower_model_t *model)
{
    if (!model) return;

    uint8_t wait_c = 0;
    uint8_t done_c = 0;
    uint8_t run_c = 0;
    uint8_t idle_c = 0;

    uint8_t order[TOWER_SLOT_COUNT];
    for (uint8_t i = 0; i < TOWER_SLOT_COUNT; i++) {
        order[i] = i;
        switch ((tower_state_t)model->slots[i].state) {
        case TOWER_STATE_WAIT:
            wait_c++;
            break;
        case TOWER_STATE_DONE:
            done_c++;
            break;
        case TOWER_STATE_ACTIVE:
        case TOWER_STATE_RUN:
            run_c++;
            break;
        case TOWER_STATE_IDLE:
        default:
            idle_c++;
            break;
        }
    }

    // 12 个元素使用简单插入排序即可保持稳定、无递归、零额外栈开销。
    for (uint8_t i = 1; i < TOWER_SLOT_COUNT; i++) {
        uint8_t cur = order[i];
        int32_t cur_key = slot_priority_key(&model->slots[cur], cur);
        int j = (int)i - 1;
        while (j >= 0) {
            uint8_t prev = order[j];
            if (slot_priority_key(&model->slots[prev], prev) <= cur_key) {
                break;
            }
            order[j + 1] = prev;
            j--;
        }
        order[j + 1] = cur;
    }

    model->wait_count = wait_c;
    model->done_count = done_c;
    model->run_count = run_c;
    model->idle_count = idle_c;
    model->stage_slots[0] = order[0];
    model->stage_slots[1] = order[1];
}

// 初始化总控台模型：加载默认 12 个脱敏工位，计算 Top 2 紧急工位，并将光标置于最急工位。
void tower_model_init(tower_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    memcpy(model->slots, k_default_slots, sizeof(k_default_slots));
    model->screen = TOWER_SCREEN_MAIN;
    model->link_mode = TOWER_LINK_OFFLINE;
    tower_model_recompute_stage(model);
    model->selected_slot = model->stage_slots[0];
}

// 计算上屏左右两个大像素工位展示的槽位编号：
// 左工位始终锁定全场最紧急的 1 号窗口 (stage_slots[0])；
// 右工位在用户移动光标巡视其他窗口时实时放大展示 selected_slot，
// 若光标就在 1 号紧急窗口上，则右工位展示 2 号紧急窗口 (stage_slots[1])。
void tower_model_get_stage_pair(
    const tower_model_t *model,
    uint8_t *left_slot,
    uint8_t *right_slot
)
{
    if (!model || !left_slot || !right_slot) return;
    uint8_t s0 = model->stage_slots[0] < TOWER_SLOT_COUNT ? model->stage_slots[0] : 0;
    uint8_t s1 = model->stage_slots[1] < TOWER_SLOT_COUNT ? model->stage_slots[1] : 1;
    uint8_t sel = model->selected_slot < TOWER_SLOT_COUNT ? model->selected_slot : 0;

    *left_slot = s0;
    *right_slot = (sel != s0) ? sel : s1;
}

// 处理物理三键输入，更新光标、界面或状态，并生成需要发给 Mac 的控制指令。
tower_model_result_t tower_model_handle_input(
    tower_model_t *model,
    tower_input_t input
)
{
    tower_model_result_t result = {
        .redraw = false,
        .action = TOWER_ACTION_NONE,
        .target_slot = 0,
    };
    if (!model) return result;

    if (input == TOWER_INPUT_OK_LONG) {
        model->screen = (model->screen == TOWER_SCREEN_MAIN)
            ? TOWER_SCREEN_LINK
            : TOWER_SCREEN_MAIN;
        model->toast_ticks = 0;
        model->toast_text[0] = '\0';
        result.redraw = true;
        return result;
    }

    if (model->screen == TOWER_SCREEN_LINK) {
        if (input == TOWER_INPUT_OK) {
            snprintf(
                model->toast_text,
                sizeof(model->toast_text),
                "正在请求同步 12 窗状态..."
            );
            model->toast_ticks = 6;
            result.redraw = true;
            result.action = TOWER_ACTION_SYNC_NOW;
        } else if (input == TOWER_INPUT_UP || input == TOWER_INPUT_DOWN) {
            model->screen = TOWER_SCREEN_MAIN;
            result.redraw = true;
        }
        return result;
    }

    // 主界面 (TOWER_SCREEN_MAIN) 按键逻辑
    uint8_t sel = model->selected_slot < TOWER_SLOT_COUNT ? model->selected_slot : 0;

    if (input == TOWER_INPUT_UP) {
        model->selected_slot = (uint8_t)((sel + TOWER_SLOT_COUNT - 1U) % TOWER_SLOT_COUNT);
        model->toast_ticks = 0;
        result.redraw = true;
        return result;
    }

    if (input == TOWER_INPUT_DOWN) {
        model->selected_slot = (uint8_t)((sel + 1U) % TOWER_SLOT_COUNT);
        model->toast_ticks = 0;
        result.redraw = true;
        return result;
    }

    if (input == TOWER_INPUT_OK) {
        const tower_slot_t *slot = &model->slots[sel];
        snprintf(
            model->toast_text,
            sizeof(model->toast_text),
            "已切至 #%02u %s",
            (unsigned)(sel + 1U),
            slot->code
        );
        model->toast_ticks = 8;
        result.redraw = true;
        result.action = TOWER_ACTION_FOCUS;
        result.target_slot = sel;
        return result;
    }

    if (input == TOWER_INPUT_OK_DOUBLE) {
        tower_slot_t *slot = &model->slots[sel];
        if (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE) {
            slot->state = TOWER_STATE_RUN;
            slot->minutes = 1;
        }
        snprintf(
            model->toast_text,
            sizeof(model->toast_text),
            "已收菜 #%02u %s",
            (unsigned)(sel + 1U),
            slot->code
        );
        model->toast_ticks = 8;
        tower_model_recompute_stage(model);
        model->selected_slot = model->stage_slots[0];
        result.redraw = true;
        result.action = TOWER_ACTION_ACK;
        result.target_slot = sel;
        return result;
    }

    return result;
}

// 将单字符状态码 ('W'/'D'/'A'/'R'/'I') 转换为枚举值。
static bool parse_state_char(char ch, uint8_t *state_out)
{
    switch (ch) {
    case 'W':
    case 'w':
        *state_out = TOWER_STATE_WAIT;
        return true;
    case 'D':
    case 'd':
        *state_out = TOWER_STATE_DONE;
        return true;
    case 'A':
    case 'a':
        *state_out = TOWER_STATE_ACTIVE;
        return true;
    case 'R':
    case 'r':
        *state_out = TOWER_STATE_RUN;
        return true;
    case 'I':
    case 'i':
        *state_out = TOWER_STATE_IDLE;
        return true;
    default:
        return false;
    }
}

// 检查字符是否为允许出现在脱敏代号中的 ASCII 字母、数字或连字符。
static bool is_safe_code_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           (ch == '-') || (ch == '_');
}

// 解析单条 "TOWER_SYNC v1 <CODE>:<STATE>:<MIN>,..." 协议帧。
// 支持在带前缀日志或 HTTP 响应体的字符串中按子串定位 "TOWER_SYNC v1 "。
bool tower_model_parse_sync(
    tower_model_t *model,
    const char *line,
    tower_link_mode_t source_mode
)
{
    if (!model || !line) return false;

    const char *prefix = "TOWER_SYNC v1 ";
    const char *start = strstr(line, prefix);
    if (!start) return false;
    const char *cursor = start + strlen(prefix);

    tower_slot_t parsed[TOWER_SLOT_COUNT];
    memset(parsed, 0, sizeof(parsed));

    for (uint8_t idx = 0; idx < TOWER_SLOT_COUNT; idx++) {
        // 1. 解析 CODE（1..7 个安全 ASCII 字符，直至 ':'）
        size_t code_len = 0;
        while (*cursor && *cursor != ':' && *cursor != ',' &&
               *cursor != '\r' && *cursor != '\n') {
            if (!is_safe_code_char(*cursor)) return false;
            if (code_len + 1U < TOWER_CODE_MAX_LEN) {
                char c = *cursor;
                if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                parsed[idx].code[code_len++] = c;
            }
            cursor++;
        }
        if (code_len == 0 || *cursor != ':') return false;
        parsed[idx].code[code_len] = '\0';
        cursor++; // 跳过第一个 ':'

        // 2. 解析单字符状态码与第二个 ':'
        uint8_t st = TOWER_STATE_IDLE;
        if (!parse_state_char(*cursor, &st)) return false;
        parsed[idx].state = st;
        cursor++;
        if (*cursor != ':') return false;
        cursor++; // 跳过第二个 ':'

        // 3. 解析分钟数（1..999）
        if (*cursor < '0' || *cursor > '9') return false;
        uint32_t mins = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            mins = mins * 10U + (uint32_t)(*cursor - '0');
            if (mins > 9999U) mins = 9999U;
            cursor++;
        }
        if (mins == 0) mins = 1;
        if (mins > 999U) mins = 999U;
        parsed[idx].minutes = (uint16_t)mins;

        if (idx + 1U < TOWER_SLOT_COUNT) {
            if (*cursor != ',') return false;
            cursor++;
        }
    }

    uint8_t prev_top_urgent = model->stage_slots[0];
    uint8_t prev_top_state = model->slots[prev_top_urgent].state;

    memcpy(model->slots, parsed, sizeof(parsed));
    model->link_mode = (uint8_t)source_mode;
    model->sync_seq++;
    tower_model_recompute_stage(model);

    // 若新同步帧出现了新的头号紧急窗口（WAIT 或 DONE），自动把光标对准它，方便一键盲按 OK。
    uint8_t new_top = model->stage_slots[0];
    uint8_t new_top_state = model->slots[new_top].state;
    if ((new_top_state == TOWER_STATE_WAIT || new_top_state == TOWER_STATE_DONE) &&
        (new_top != prev_top_urgent || new_top_state != prev_top_state)) {
        model->selected_slot = new_top;
    }

    return true;
}

// 格式化发往 Mac 的控制指令行（以 '\n' 结尾）。
bool tower_model_format_cmd(
    const tower_model_t *model,
    tower_action_t action,
    uint8_t slot_index,
    char *out_buf,
    size_t out_size
)
{
    if (!model || !out_buf || out_size < 24U || slot_index >= TOWER_SLOT_COUNT) {
        return false;
    }
    const char *verb = NULL;
    if (action == TOWER_ACTION_FOCUS) {
        verb = "FOCUS";
    } else if (action == TOWER_ACTION_ACK) {
        verb = "ACK";
    } else {
        return false;
    }

    int written = snprintf(
        out_buf,
        out_size,
        "TOWER_CMD %s %u %s\n",
        verb,
        (unsigned)slot_index,
        model->slots[slot_index].code
    );
    return written > 0 && (size_t)written < out_size;
}

// 提取以空格分隔的下一个字段，用于解析 TOWER_WIFI 指令。
static const char *next_token(const char *cursor, char *out, size_t out_size)
{
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n') return NULL;
    size_t len = 0;
    while (*cursor && *cursor != ' ' && *cursor != '\t' &&
           *cursor != '\r' && *cursor != '\n') {
        if (len + 1U < out_size) {
            out[len++] = *cursor;
        }
        cursor++;
    }
    out[len] = '\0';
    return len > 0 ? cursor : NULL;
}

// 解析串口配网指令："TOWER_WIFI <ssid> <password> <url>"
bool tower_model_parse_wifi_cmd(
    const char *line,
    char *ssid_out,
    size_t ssid_size,
    char *pass_out,
    size_t pass_size,
    char *url_out,
    size_t url_size
)
{
    if (!line || !ssid_out || !pass_out || !url_out) return false;
    const char *prefix = "TOWER_WIFI ";
    const char *start = strstr(line, prefix);
    if (!start) return false;

    const char *cursor = start + strlen(prefix);
    cursor = next_token(cursor, ssid_out, ssid_size);
    if (!cursor) return false;
    cursor = next_token(cursor, pass_out, pass_size);
    if (!cursor) return false;
    cursor = next_token(cursor, url_out, url_size);
    return cursor != NULL;
}
