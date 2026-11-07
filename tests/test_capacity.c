/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file test_capacity.c @brief 最大容量与逻辑 ID 边界的独立实例验证。 */
#include "mod_button.h"
#define CHECK(x) do { if (!(x)) { return __LINE__; } } while (0)
/** @brief 静态容量表，由测试初始化后交给模块复制。 */
static MOD_ButtonConfig_t keys[MOD_BUTTON_MAX_COUNT];
/** @brief 最后一个逻辑 ID，输入次数与事件次数，只在测试上下文访问。 */
static uint8_t lastId;
static unsigned reads, events;
/** @brief 只有最后一个按键处于按下物理电平。 */
static uint8_t read_level(uint8_t id) { ++reads; return id == lastId ? 0U : 1U; }
/** @brief 只累计最后一个按键的按下事件。 */
static void receive(uint8_t id, MOD_ButtonEvent_t event)
{
    if (id == lastId && event == MOD_BUTTON_EVENT_DOWN) { ++events; }
}
/** @brief 填满编译容量，执行真实扫描并验证全部对象被访问且没有截断。 */
__declspec(dllexport) int test_configured_capacity(void)
{
    unsigned i;
    for (i = 0U; i < MOD_BUTTON_MAX_COUNT; ++i) {
        keys[i].id = (uint8_t)i; keys[i].activeLevel = 0U;
    }
    lastId = (uint8_t)(MOD_BUTTON_MAX_COUNT - 1U);
    CHECK(MOD_Button_InitState(keys, MOD_BUTTON_MAX_COUNT, read_level, receive) == MOD_BUTTON_STATUS_OK);
    CHECK(MOD_Button_Start() == MOD_BUTTON_STATUS_OK);
    for (i = 0U; i < 3U; ++i) { CHECK(MOD_Button_ProcessTick() == MOD_BUTTON_STATUS_OK); }
    CHECK(reads == 3U * MOD_BUTTON_MAX_COUNT && events == 1U);
    CHECK(MOD_Button_Stop() == MOD_BUTTON_STATUS_OK);
    return 0;
}
