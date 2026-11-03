// main/tower_model.c
// 「AI 工位看板 (6 工位大字人话版)」纯逻辑状态机与协议解析实现。
#include "tower_model.h"

#include <stdio.h>
#include <string.h>

// 默认展示的 6 个常用 AI 与工作工位（未连上电脑同步前全部保持安静的「休息中」，不伪造假报警）
static const tower_slot_t k_default_slots[TOWER_SLOT_COUNT] = {
    { "TRAE1", TOWER_ROLE_WRITE,  TOWER_STATE_IDLE, 1 },
    { "CURS1", TOWER_ROLE_CODE,   TOWER_STATE_IDLE, 1 },
    { "AIME",  TOWER_ROLE_SEARCH, TOWER_STATE_IDLE, 1 },
    { "MULT",  TOWER_ROLE_TASK,   TOWER_STATE_IDLE, 1 },
    { "CHRM1", TOWER_ROLE_DOC,    TOWER_STATE_IDLE, 1 },
    { "FEIS",  TOWER_ROLE_CHAT,   TOWER_STATE_IDLE, 1 },
};

// 根据内部代号返回直白工具名
static const char *tool_display_name(const char *code)
{
    if (!code) return "工具";
    if (strncmp(code, "TRAE", 4) == 0) return "Trae";
    if (strncmp(code, "CURS", 4) == 0) return "Cursor";
    if (strncmp(code, "AIME", 4) == 0) return "Aime";
    if (strncmp(code, "MULT", 4) == 0) return "Multica";
    if (strncmp(code, "CHRM", 4) == 0) return "浏览器";
    if (strncmp(code, "FEIS", 4) == 0) return "飞书";
    if (strncmp(code, "GROK", 4) == 0) return "Grok";
    if (strncmp(code, "TERM", 4) == 0) return "终端";
    if (strncmp(code, "SHIY", 4) == 0) return "拾页";
    return code;
}

// 根据动作枚举返回模糊动作类型词（不露任何具体文件名）
static const char *role_display_name(uint8_t role)
{
    switch ((tower_role_t)role) {
    case TOWER_ROLE_CODE:
        return "写代码";
    case TOWER_ROLE_WRITE:
        return "写文章";
    case TOWER_ROLE_DOC:
        return "看文档";
    case TOWER_ROLE_TASK:
        return "跑任务";
    case TOWER_ROLE_SEARCH:
        return "查资料";
    case TOWER_ROLE_DESIGN:
        return "做设计";
    case TOWER_ROLE_CHAT:
        return "聊协作";
    case TOWER_ROLE_TERM:
    default:
        return "跑脚本";
    }
}

void tower_model_format_title(
    const tower_slot_t *slot,
    char *out_buf,
    size_t out_size
)
{
    if (!slot || !out_buf || out_size == 0) return;
    snprintf(
        out_buf,
        out_size,
        "%s·%s",
        tool_display_name(slot->code),
        role_display_name(slot->role)
    );
}

const char *tower_model_state_text(uint8_t state)
{
    switch ((tower_state_t)state) {
    case TOWER_STATE_WAIT:
        return "等你确认";
    case TOWER_STATE_DONE:
        return "跑完了!";
    case TOWER_STATE_ACTIVE:
        return "正在用";
    case TOWER_STATE_RUN:
        return "后台跑着";
    case TOWER_STATE_IDLE:
    default:
        return "休息中";
    }
}

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
                "正在从电脑刷新状态..."
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
        char title[24];
        tower_model_format_title(&model->slots[sel], title, sizeof(title));
        snprintf(
            model->toast_text,
            sizeof(model->toast_text),
            "电脑切到：%s",
            title
        );
        model->toast_ticks = 8;
        result.redraw = true;
        result.action = TOWER_ACTION_FOCUS;
        result.target_slot = sel;
        return result;
    }

    if (input == TOWER_INPUT_OK_DOUBLE) {
        tower_slot_t *slot = &model->slots[sel];
        char title[24];
        tower_model_format_title(slot, title, sizeof(title));
        if (slot->state == TOWER_STATE_WAIT || slot->state == TOWER_STATE_DONE) {
            slot->state = TOWER_STATE_RUN;
            slot->minutes = 1;
        }
        snprintf(
            model->toast_text,
            sizeof(model->toast_text),
            "已看完：%s",
            title
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

static bool is_safe_code_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           (ch == '-') || (ch == '_');
}

// 根据代码自动推断默认动作类型（当协议未显式传第 4 段 role 时兜底）
static uint8_t default_role_for_code(const char *code)
{
    if (!code) return TOWER_ROLE_CODE;
    if (strncmp(code, "TRAE", 4) == 0) return TOWER_ROLE_WRITE;
    if (strncmp(code, "CURS", 4) == 0) return TOWER_ROLE_CODE;
    if (strncmp(code, "AIME", 4) == 0) return TOWER_ROLE_SEARCH;
    if (strncmp(code, "MULT", 4) == 0) return TOWER_ROLE_TASK;
    if (strncmp(code, "CHRM", 4) == 0) return TOWER_ROLE_DOC;
    if (strncmp(code, "FEIS", 4) == 0) return TOWER_ROLE_CHAT;
    if (strncmp(code, "TERM", 4) == 0) return TOWER_ROLE_TERM;
    return TOWER_ROLE_TASK;
}

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
        cursor++;

        uint8_t st = TOWER_STATE_IDLE;
        if (!parse_state_char(*cursor, &st)) return false;
        parsed[idx].state = st;
        cursor++;
        if (*cursor != ':') return false;
        cursor++;

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

        // 可选的第 4 字段 :ROLE (0..7)
        parsed[idx].role = default_role_for_code(parsed[idx].code);
        if (*cursor == ':') {
            cursor++;
            if (*cursor >= '0' && *cursor <= '7') {
                parsed[idx].role = (uint8_t)(*cursor - '0');
                cursor++;
            } else {
                return false;
            }
        }

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

    uint8_t new_top = model->stage_slots[0];
    uint8_t new_top_state = model->slots[new_top].state;
    if ((new_top_state == TOWER_STATE_WAIT || new_top_state == TOWER_STATE_DONE) &&
        (new_top != prev_top_urgent || new_top_state != prev_top_state)) {
        model->selected_slot = new_top;
    }

    return true;
}

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
