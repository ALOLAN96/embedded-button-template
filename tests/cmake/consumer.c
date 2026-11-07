/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file consumer.c @brief CMake 公共接口及最终链接验证。 */
#include "mod_button.h"
/** @brief 提供未按下的模拟物理电平。 */
static uint8_t read_level(uint8_t id) { (void)id; return 1U; }
/** @brief 本用例只验证无按下事件的路径。 */
static void receive(uint8_t id, MOD_ButtonEvent_t event) { (void)id; (void)event; }
/** @brief 通过公共接口初始化、扫描及停止。 */
int main(void)
{
    const MOD_ButtonConfig_t config[] = {{255U, 0U}};
    if (MOD_Button_InitState(config, 1U, read_level, receive) != MOD_BUTTON_STATUS_OK) { return 1; }
    if (MOD_Button_Start() != MOD_BUTTON_STATUS_OK) { return 2; }
    if (MOD_Button_ProcessTick() != MOD_BUTTON_STATUS_OK) { return 3; }
    return MOD_Button_Stop() == MOD_BUTTON_STATUS_OK ? 0 : 4;
}
