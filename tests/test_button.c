/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file test_button.c @brief 真实 MultiButton 后端的边界及生命周期测试。 */
#include "mod_button.h"
#include "button_example.h"
/** @brief 仅测试故障注入接缝及直接检查后端撤销效果。 */
extern unsigned test_start_fail_after;
extern void button_ticks(void);

/** @brief 断言失败返回源码行，供 Python runner 定位。 */
#define CHECK(x) do { if (!(x)) { return __LINE__; } } while (0)
/** @brief Windows DLL 测试入口。 */
#define TEST __declspec(dllexport) int
/** @brief 按逻辑 ID 索引的模拟物理输入，仅测试读写。 */
static uint8_t levels[256];
/** @brief 接收器记录的按键 ID、事件及扫描序号。 */
static struct { uint8_t id; MOD_ButtonEvent_t event; unsigned tick; } events[2048];
/** @brief 当前测试的事件数量及扫描序号。 */
static unsigned used, tick;
/** @brief 输入读取次数，用于验证停止、回退和漏扫。 */
static unsigned reads;
/** @brief 是否在回调中尝试反入 API；仅测试使用。 */
static int reenter, reenterFailed;
/** @brief 两个按键分别采用低有效和高有效。 */
static const MOD_ButtonConfig_t config[] = {{7U, 0U}, {200U, 1U}};

/** @brief 提供受测试控制的输入电平。 */
static uint8_t read_level(uint8_t id) { ++reads; return levels[id]; }
/** @brief 记录真实事件，并在指定测试中验证全部反入 API 被拒绝。 */
static void receive(uint8_t id, MOD_ButtonEvent_t event)
{
    if (used < 2048U) {
        events[used].id = id; events[used].event = event; events[used].tick = tick; ++used;
    }
    if (reenter) {
        if (MOD_Button_Stop() != MOD_BUTTON_STATUS_BUSY ||
            MOD_Button_Start() != MOD_BUTTON_STATUS_BUSY ||
            MOD_Button_ProcessTick() != MOD_BUTTON_STATUS_BUSY ||
            MOD_Button_InitState(config, 2U, read_level, receive) != MOD_BUTTON_STATUS_BUSY) {
            reenterFailed = 1;
        }
    }
}
/** @brief 重置测试输入并启动两个真实后端对象。 */
static int setup(void)
{
    (void)MOD_Button_Stop(); used = 0U; tick = 0U; reenter = 0; reenterFailed = 0;
    levels[7] = 1U; levels[200] = 0U;
    CHECK(MOD_Button_InitState(config, 2U, read_level, receive) == MOD_BUTTON_STATUS_OK);
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK);
    return 0;
}
/** @brief 推进固定数量的真实扫描，不伪造后端事件。 */
static int scan(unsigned n)
{
    unsigned i;
    for (i = 0U; i < n; ++i) { ++tick; CHECK(MOD_Button_ProcessTick() == MOD_BUTTON_STATUS_OK); }
    return 0;
}
/** @brief 拒绝漏初始化使用；每个 DLL 仅最先执行此测试。 */
TEST test_not_initialized(void)
{
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_NOT_INIT);
    CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_NOT_INIT);
    CHECK(MOD_Button_ProcessTick() == MOD_BUTTON_STATUS_NOT_INIT);
    return 0;
}
/** @brief 消抖不足不产生事件，第三次稳定采样报告按下。 */
TEST test_debounce(void)
{
    CHECK(setup() == 0); levels[7] = 0U; CHECK(scan(2U) == 0); CHECK(used == 0U);
    levels[7] = 1U; CHECK(scan(1U) == 0); levels[7] = 0U;
    CHECK(scan(2U) == 0); CHECK(used == 0U); CHECK(scan(1U) == 0);
    CHECK(used == 1U && events[0].id == 7U && events[0].event == MOD_BUTTON_EVENT_DOWN);
    return 0;
}
/** @brief 单击必须等候 61 个释放后 tick，输出 ID 和类型正确。 */
TEST test_single(void)
{
    CHECK(setup() == 0); levels[7] = 0U; CHECK(scan(3U) == 0);
    levels[7] = 1U; CHECK(scan(3U) == 0); CHECK(used == 2U);
    CHECK(events[1].event == MOD_BUTTON_EVENT_UP);
    CHECK(scan(60U) == 0); CHECK(used == 2U); CHECK(scan(1U) == 0);
    CHECK(used == 3U && events[2].event == MOD_BUTTON_EVENT_SINGLE_CLICK && events[2].id == 7U);
    CHECK(events[2].tick == 67U); return 0;
}
/** @brief 连击重复事件不能被错误转为 PRESS_DOWN。 */
TEST test_double_repeat(void)
{
    CHECK(setup() == 0); levels[7] = 0U; CHECK(scan(3U) == 0);
    levels[7] = 1U; CHECK(scan(3U) == 0); levels[7] = 0U; CHECK(scan(3U) == 0);
    CHECK(used == 4U && events[2].event == MOD_BUTTON_EVENT_DOWN && events[3].event == MOD_BUTTON_EVENT_REPEAT);
    levels[7] = 1U; CHECK(scan(3U) == 0); CHECK(scan(61U) == 0);
    CHECK(used == 6U && events[5].event == MOD_BUTTON_EVENT_DOUBLE_CLICK); return 0;
}
/** @brief 长按阈值使用 >，开始仅一次，保持每 tick，释放无单击。 */
TEST test_long(void)
{
    CHECK(setup() == 0); levels[200] = 1U; CHECK(scan(3U) == 0);
    CHECK(scan(199U) == 0); CHECK(used == 1U);
    CHECK(scan(1U) == 0); CHECK(used == 1U);
    CHECK(scan(1U) == 0); CHECK(used == 2U && events[1].event == MOD_BUTTON_EVENT_LONG_START);
    CHECK(events[1].id == 200U && events[1].tick == 204U);
    CHECK(scan(2U) == 0); CHECK(used == 4U && events[3].event == MOD_BUTTON_EVENT_LONG_HOLD);
    levels[200] = 0U; CHECK(scan(3U) == 0);
    CHECK(events[used - 1U].event == MOD_BUTTON_EVENT_UP);
    { unsigned before = used; CHECK(scan(70U) == 0); CHECK(used == before); }
    return 0;
}
/** @brief 幂等启动不重复注册；停止不再采样；重启清除旧手势。 */
TEST test_lifecycle(void)
{
    CHECK(setup() == 0); CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK);
    CHECK(MOD_Button_InitState(config, 2U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_STATE);
    levels[7] = 0U; CHECK(scan(150U) == 0); CHECK(used == 1U);
    CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK); CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK);
    CHECK(MOD_Button_ProcessTick() == MOD_BUTTON_STATUS_INVALID_STATE); CHECK(used == 1U);
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK); used = 0U;
    CHECK(scan(3U) == 0); CHECK(used == 1U && events[0].event == MOD_BUTTON_EVENT_DOWN);
    CHECK(scan(100U) == 0); CHECK(used == 1U); return 0;
}
/** @brief 参数失败不能销毁旧配置，所有 ID、极性和容量先检查。 */
TEST test_parameters(void)
{
    MOD_ButtonConfig_t duplicate[] = {{7U, 0U}, {7U, 1U}};
    MOD_ButtonConfig_t bad = {1U, 2U};
    CHECK(setup() == 0); CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK);
    CHECK(MOD_Button_InitState(0, 2U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(config, 0U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(config, (size_t)MOD_BUTTON_MAX_COUNT + 1U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(config, 2U, 0, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(config, 2U, read_level, 0) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(duplicate, 2U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_InitState(&bad, 1U, read_level, receive) == MOD_BUTTON_STATUS_INVALID_PARAM);
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK); levels[200] = 1U;
    CHECK(scan(3U) == 0); CHECK(used == 1U && events[0].id == 200U); return 0;
}
/** @brief 同一扫描轮中多个按键互相独立，反入被拒后扫描仍可继续。 */
TEST test_reentry_and_isolation(void)
{
    unsigned i; int first = 0, second = 0;
    CHECK(setup() == 0); reenter = 1; levels[7] = 0U; levels[200] = 1U;
    CHECK(scan(3U) == 0); CHECK(used == 2U && reenterFailed == 0);
    for (i = 0U; i < used; ++i) {
        CHECK(events[i].event == MOD_BUTTON_EVENT_DOWN);
        if (events[i].id == 7U) { first = 1; } if (events[i].id == 200U) { second = 1; }
    }
    CHECK(first && second); reenter = 0; CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK); return 0;
}
/** @brief 第二个注册失败必须移除第一个，且失败后可重新启动。 */
TEST test_start_rollback(void)
{
    CHECK(setup() == 0); CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK);
    test_start_fail_after = 2U; reads = 0U;
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_BACKEND_ERROR);
    CHECK(MOD_Button_ProcessTick() == MOD_BUTTON_STATUS_INVALID_STATE);
    button_ticks(); CHECK(reads == 0U);
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK); levels[7] = 0U;
    CHECK(scan(3U) == 0); CHECK(used == 1U && events[0].id == 7U); return 0;
}
/** @brief 实际 App 示例漏掉三个周期时只读取一次，并保持截止时间相位。 */
TEST test_example_schedule(void)
{
    CHECK(setup() == 0); CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK); reads = 0U;
    CHECK(APP_ButtonExample_Init(0U, read_level, receive) == MOD_BUTTON_STATUS_OK);
    CHECK(APP_ButtonExample_Poll(4U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 0U);
    CHECK(APP_ButtonExample_Poll(5U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 2U);
    CHECK(APP_ButtonExample_Poll(26U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 4U);
    CHECK(APP_ButtonExample_GetMissedSlots() == 3U);
    CHECK(APP_ButtonExample_Poll(29U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 4U);
    CHECK(APP_ButtonExample_Poll(30U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 6U);
    return 0;
}
/** @brief uint32 毫秒时间回绕不能提前触发或丢失首个周期。 */
TEST test_example_wrap(void)
{
    CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK); reads = 0U;
    CHECK(APP_ButtonExample_Init(UINT32_MAX - 2U, read_level, receive) == MOD_BUTTON_STATUS_OK);
    CHECK(APP_ButtonExample_Poll(1U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 0U);
    CHECK(APP_ButtonExample_Poll(2U) == MOD_BUTTON_STATUS_OK); CHECK(reads == 2U);
    CHECK(APP_ButtonExample_GetMissedSlots() == 0U); return 0;
}
