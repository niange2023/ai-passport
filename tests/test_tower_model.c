// tests/test_tower_model.c
// 「3-B 双焦像素总控台 (Command Tower)」主机端纯逻辑单元测试。
#include "tower_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_init_and_urgent_stage_ranking(void)
{
    tower_model_t model;
    tower_model_init(&model);

    assert(model.wait_count == 2);
    assert(model.done_count == 2);
    assert(model.run_count == 4);
    assert(model.idle_count == 4);

    // 0 号 AIME (WAIT 8m) 应当排在第一紧急位，3 号 MULT (WAIT 3m) 排在第二紧急位
    assert(model.stage_slots[0] == 0);
    assert(model.stage_slots[1] == 3);
    assert(model.selected_slot == 0);

    uint8_t left = 0;
    uint8_t right = 0;
    tower_model_get_stage_pair(&model, &left, &right);
    assert(left == 0);
    assert(right == 3);
}

static void test_navigation_and_stage_pair_follow(void)
{
    tower_model_t model;
    tower_model_init(&model);

    // 按下键从 0 移到 1 (TRAE1)，此时左工位仍为最紧急 0 (AIME)，右工位跟随显示 1 (TRAE1)
    tower_model_result_t res = tower_model_handle_input(&model, TOWER_INPUT_DOWN);
    assert(res.redraw);
    assert(res.action == TOWER_ACTION_NONE);
    assert(model.selected_slot == 1);

    uint8_t left = 0;
    uint8_t right = 0;
    tower_model_get_stage_pair(&model, &left, &right);
    assert(left == 0);
    assert(right == 1);

    // 单击 OK 触发切窗指令 TOWER_ACTION_FOCUS
    res = tower_model_handle_input(&model, TOWER_INPUT_OK);
    assert(res.redraw);
    assert(res.action == TOWER_ACTION_FOCUS);
    assert(res.target_slot == 1);

    char cmd[64];
    assert(tower_model_format_cmd(&model, res.action, res.target_slot, cmd, sizeof(cmd)));
    assert(strcmp(cmd, "TOWER_CMD FOCUS 1 TRAE1\n") == 0);
}

static void test_double_ok_acknowledges_urgent_slot(void)
{
    tower_model_t model;
    tower_model_init(&model);

    // 初始选中 0 (AIME, WAIT 8m)，双击 OK 标记已收菜/已处理
    tower_model_result_t res = tower_model_handle_input(&model, TOWER_INPUT_OK_DOUBLE);
    assert(res.redraw);
    assert(res.action == TOWER_ACTION_ACK);
    assert(res.target_slot == 0);
    assert(model.slots[0].state == TOWER_STATE_RUN);
    assert(model.wait_count == 1);

    // 最紧急工位自动变成剩余的 WAIT 窗口 (3 号 MULT)
    assert(model.stage_slots[0] == 3);
    assert(model.selected_slot == 3);
}

static void test_parse_wire_sync_and_reject_unsafe(void)
{
    tower_model_t model;
    tower_model_init(&model);

    const char *wire =
        "I (1204) tower: rx line -> TOWER_SYNC v1 "
        "AIME:I:20,TRAE1:W:14,CURS1:A:1,MULT:R:5,"
        "TRAE2:D:9,CURS2:I:30,CHRM1:R:3,CHRM2:I:40,"
        "GROK:I:50,SHIY:I:60,TERM:R:2,FEIS:I:70\r\n";

    assert(tower_model_parse_sync(&model, wire, TOWER_LINK_USB));
    assert(model.link_mode == TOWER_LINK_USB);
    assert(model.sync_seq == 1);
    assert(model.wait_count == 1);
    assert(model.done_count == 1);
    assert(model.stage_slots[0] == 1); // TRAE1 (WAIT 14m)
    assert(model.stage_slots[1] == 4); // TRAE2 (DONE 9m)
    assert(model.selected_slot == 1);  // 自动跳到新的头号紧急工位

    // 包含非法字符或中文任务名的帧必须拒绝，防止误解析或泄密
    const char *unsafe = "TOWER_SYNC v1 机密任务:W:5,TRAE1:D:2\n";
    assert(!tower_model_parse_sync(&model, unsafe, TOWER_LINK_USB));
}

static void test_parse_wifi_command_and_screen_toggle(void)
{
    tower_model_t model;
    tower_model_init(&model);

    tower_model_result_t res = tower_model_handle_input(&model, TOWER_INPUT_OK_LONG);
    assert(res.redraw);
    assert(model.screen == TOWER_SCREEN_LINK);

    res = tower_model_handle_input(&model, TOWER_INPUT_OK);
    assert(res.action == TOWER_ACTION_SYNC_NOW);

    char ssid[32];
    char pass[32];
    char url[64];
    assert(tower_model_parse_wifi_cmd(
        "TOWER_WIFI MyHomeWiFi secret123 http://192.168.1.10:8766/api/passport-tower/wire\n",
        ssid, sizeof(ssid),
        pass, sizeof(pass),
        url, sizeof(url)
    ));
    assert(strcmp(ssid, "MyHomeWiFi") == 0);
    assert(strcmp(pass, "secret123") == 0);
    assert(strcmp(url, "http://192.168.1.10:8766/api/passport-tower/wire") == 0);
}

int main(void)
{
    test_init_and_urgent_stage_ranking();
    test_navigation_and_stage_pair_follow();
    test_double_ok_acknowledges_urgent_slot();
    test_parse_wire_sync_and_reject_unsafe();
    test_parse_wifi_command_and_screen_toggle();
    printf("Tower model tests: PASS\n");
    return 0;
}
