/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file button_example.c @brief App 调度示例，readLevel 由宿主 BSP 适配实现。 */
#include "button_example.h"
/** @brief 示例逻辑 ID 与有效极性；宿主替换为真实板级配置。 */
static const MOD_ButtonConfig_t exampleConfig[] = {{7U, 0U}, {200U, 1U}};
/** @brief 最后处理的调度槽，单位 ms，使用无符号差处理回绕。 */
static uint32_t lastSlotMs;
/** @brief 丢失的调度槽数量，饱和于 UINT32_MAX。 */
static uint32_t missedSlots;
/** @brief 示例已初始化标记；只在主循环/任务访问。 */
static uint8_t ready;
/**
 * @brief 注入 BSP 读电平和 App 消费者，启动前建立扫描时间基线。
 * @param nowMs 宿主单调毫秒时钟，后续 Poll 必须采用同一来源。
 * @param readLevel BSP 适配读取函数，按示例 ID 返回物理 0/1。
 * @param eventSink App 事件消费者，同步、非阻塞。
 * @return 模块初始化或启动的状态码。
 * @note 第一次调用前无需清零；再次初始化须先成功停止 Module。
 */
MOD_ButtonStatus_t APP_ButtonExample_Init(uint32_t nowMs, MOD_ButtonReadLevel_t readLevel,
                                        MOD_ButtonEventSink_t eventSink)
{
    MOD_ButtonStatus_t status = MOD_Button_InitState(exampleConfig,
        sizeof(exampleConfig) / sizeof(exampleConfig[0]), readLevel, eventSink);
    if (status != MOD_BUTTON_STATUS_OK) { return status; }
    ready = 0U;
    status = MOD_Button_Start();
    if (status == MOD_BUTTON_STATUS_OK) {
        lastSlotMs = nowMs; missedSlots = 0U; ready = 1U;
    }
    return status;
}
/**
 * @brief 主循环每轮调用，到期时只采样一次，跳过失去的周期并统计。
 * @param nowMs 单调毫秒时钟，同一两次调用间隔必须小于 uint32 完整周期。
 * @return 未初始化 NOT_INIT；其他情况返回扫描结果或尚未到期的 OK。
 * @note 调用者须串行调度，不在回调中反入本示例。不支持时钟倒退。
 *       漏扫会使后端手势时间变长，统计不能补偿或证明手势计时精度。
 */
MOD_ButtonStatus_t APP_ButtonExample_Poll(uint32_t nowMs)
{
    uint32_t elapsed, slots, lost;
    if (ready == 0U) { return MOD_BUTTON_STATUS_NOT_INIT; }
    elapsed = nowMs - lastSlotMs;
    if (elapsed < MOD_BUTTON_TICK_INTERVAL_MS) { return MOD_BUTTON_STATUS_OK; }
    slots = elapsed / MOD_BUTTON_TICK_INTERVAL_MS;
    lost = slots - 1U;
    lastSlotMs += slots * MOD_BUTTON_TICK_INTERVAL_MS;
    missedSlots = lost > UINT32_MAX - missedSlots ? UINT32_MAX : missedSlots + lost;
    return MOD_Button_ProcessTick();
}
/** @brief 读取漏扫调度槽统计。 */
uint32_t APP_ButtonExample_GetMissedSlots(void) { return missedSlots; }
