// tests/test_tower_model.c
// 「AI 工位看板 (6 工位大字人话版)」主机端纯逻辑单元测试。
#include "tower_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_init_and_human_readable_titles(void)
{
    tower_model_t model;
    tower_model_init(&model);

    assert(model.wait_count == 1);
    assert(model.done_count == 1);
    assert(model.run_count == 3);
    assert(model.idle_count == 1);

    // 1 号 AIME (等你确认) 最优先，0 号 TRAE1 (跑完了!) 次之
    assert(model.stage_slots[0] == 1);
    assert(model.stage_slots[1] == 0);
    assert(model.selected_slot == 1);

    char title[32];
    tower_model_format_title(&model.slots[0], title, sizeof(title));
    assert(strcmp(title, "Trae·写文章") == 0);
    tower_model_format_title(&model.slots[1], title, sizeof(title));
    assert(strcmp(title, "Aime·查资料") == 0);
    tower_model_format_title(&model.slots[2], title, sizeof(title));
    assert(strcmp(title, "Cursor·写代码") == 0);

    assert(strcmp(tower_model_state_text(TOWER_STATE_WAIT), "等你确认") == 0);
    assert(strcmp(tower_model_state_text(TOWER_STATE_DONE), "跑完了!") == 0);
    assert(strcmp(tower_model_state_text(TOWER_STATE_ACTIVE), "正在用") == 0);
    assert(strcmp(tower_model_state_text(TOWER_STATE_RUN), "后台跑着") == 0);
    assert(strcmp(tower_model_state_text(TOWER_STATE_IDLE), "休息中") == 0);
}

static void test_navigation_and_focus_command(void)
{
    tower_model_t model;
    tower_model_init(&model);

    // 初始在 1 号，按上键移回 0 号 (TRAE1)
    tower_model_result_t res = tower_model_handle_input(&model, TOWER_INPUT_UP);
    assert(res.redraw);
    assert(model.selected_slot == 0);

    res = tower_model_handle_input(&model, TOWER_INPUT_OK);
    assert(res.redraw);
    assert(res.action == TOWER_ACTION_FOCUS);
    assert(res.target_slot == 0);
    assert(strstr(model.toast_text, "Trae·写文章") != NULL);

    char cmd[64];
    assert(tower_model_format_cmd(&model, res.action, res.target_slot, cmd, sizeof(cmd)));
    assert(strcmp(cmd, "TOWER_CMD FOCUS 0 TRAE1\n") == 0);
}

static void test_double_ok_acknowledges_urgent_slot(void)
{
    tower_model_t model;
    tower_model_init(&model);

    // 初始选中 1 (AIME, 等你确认)，双击 OK 标记已看
    tower_model_result_t res = tower_model_handle_input(&model, TOWER_INPUT_OK_DOUBLE);
    assert(res.redraw);
    assert(res.action == TOWER_ACTION_ACK);
    assert(res.target_slot == 1);
    assert(model.slots[1].state == TOWER_STATE_RUN);
    assert(model.wait_count == 0);

    // 光标自动跳到下一个待处理工位 0 号 (TRAE1, 跑完了!)
    assert(model.stage_slots[0] == 0);
    assert(model.selected_slot == 0);
}

static void test_parse_wire_sync_with_roles_and_reject_unsafe(void)
{
    tower_model_t model;
    tower_model_init(&model);

    const char *wire =
        "TOWER_SYNC v1 "
        "CURS1:A:1:0,TRAE1:D:3:1,AIME:W:8:4,"
        "MULT:R:5:3,CHRM1:R:6:2,FEIS:I:25:6\r\n";

    assert(tower_model_parse_sync(&model, wire, TOWER_LINK_USB));
    assert(model.link_mode == TOWER_LINK_USB);
    assert(model.sync_seq == 1);
    assert(model.wait_count == 1);
    assert(model.done_count == 1);
    assert(model.stage_slots[0] == 2); // AIME (W)
    assert(model.selected_slot == 2);

    char title[32];
    tower_model_format_title(&model.slots[4], title, sizeof(title));
    assert(strcmp(title, "浏览器·看文档") == 0);

    const char *unsafe = "TOWER_SYNC v1 机密任务:W:5:0,TRAE1:D:2:1\n";
    assert(!tower_model_parse_sync(&model, unsafe, TOWER_LINK_USB));
}

int main(void)
{
    test_init_and_human_readable_titles();
    test_navigation_and_focus_command();
    test_double_ok_acknowledges_urgent_slot();
    test_parse_wire_sync_with_roles_and_reject_unsafe();
    printf("Tower model tests: PASS\n");
    return 0;
}
