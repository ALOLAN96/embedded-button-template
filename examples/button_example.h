/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file button_example.h @brief App 组合示例；时间来自宿主单调毫秒时钟。 */
#ifndef BUTTON_EXAMPLE_H
#define BUTTON_EXAMPLE_H
#include "mod_button.h"
MOD_ButtonStatus_t APP_ButtonExample_Init(uint32_t nowMs, MOD_ButtonReadLevel_t readLevel,
                                        MOD_ButtonEventSink_t eventSink);
MOD_ButtonStatus_t APP_ButtonExample_Poll(uint32_t nowMs);
uint32_t APP_ButtonExample_GetMissedSlots(void);
#endif
