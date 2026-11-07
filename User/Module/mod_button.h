/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/**
 * @file mod_button.h
 * @brief Module 层按键识别；公共类型不依赖第三方、HAL 或 RTOS。
 * @note 单一管理器，全部操作须在同一主循环/任务串行执行，禁止 ISR 调用。
 *       readLevel 必须返回物理电平 0/1。回调同步执行，不得阻塞。
 */
#ifndef MOD_BUTTON_H
#define MOD_BUTTON_H

#include <stddef.h>
#include <stdint.h>

/** @brief 静态按键容量；仅影响实现，集成时统一配置，范围 1..256。 */
#ifndef MOD_BUTTON_MAX_COUNT
#define MOD_BUTTON_MAX_COUNT 16U
#endif

/** @brief 固定后端所要求的扫描间隔，单位 ms。 */
#define MOD_BUTTON_TICK_INTERVAL_MS 5U

/** @brief 模板独立状态码；宿主可在边界映射项目统一状态。 */
typedef enum
{
    MOD_BUTTON_STATUS_OK = 0,       /**< 操作成功，重复 Start/Stop 也成功。 */
    MOD_BUTTON_STATUS_INVALID_PARAM,/**< 参数无效，原配置保持不变。 */
    MOD_BUTTON_STATUS_NOT_INIT,     /**< 尚未初始化。 */
    MOD_BUTTON_STATUS_INVALID_STATE,/**< 运行期间不允许重新初始化。 */
    MOD_BUTTON_STATUS_BUSY,         /**< 正在扫描/回调，拒绝反入调用。 */
    MOD_BUTTON_STATUS_BACKEND_ERROR /**< 注册失败，已撤销本次全部注册。 */
} MOD_ButtonStatus_t;

/** @brief 对外按键事件；数值不作为第三方事件索引使用。 */
typedef enum
{
    MOD_BUTTON_EVENT_DOWN = 0,      /**< 消抖后按下。 */
    MOD_BUTTON_EVENT_UP,            /**< 消抖后释放。 */
    MOD_BUTTON_EVENT_REPEAT,        /**< 连击次数增加；在第二次及以后按下时报告。 */
    MOD_BUTTON_EVENT_SINGLE_CLICK,  /**< 单击确认，需等待连击窗口结束。 */
    MOD_BUTTON_EVENT_DOUBLE_CLICK,  /**< 双击确认。 */
    MOD_BUTTON_EVENT_LONG_START,    /**< 跨越长按阈值时报告一次。 */
    MOD_BUTTON_EVENT_LONG_HOLD      /**< 长按保持时每个扫描周期报告。 */
} MOD_ButtonEvent_t;

/** @brief 逻辑 ID 与有效极性；不包含 GPIO/HAL 对象。 */
typedef struct
{
    uint8_t id;                    /**< 0..255，初始化表内唯一。 */
    uint8_t activeLevel;           /**< 物理按下电平，仅 0 或 1。 */
} MOD_ButtonConfig_t;

/** @brief 非阻塞输入契约：按逻辑 ID 返回当前物理电平 0/1。 */
typedef uint8_t (*MOD_ButtonReadLevel_t)(uint8_t id);
/** @brief 同步事件接收器，参数按值交付，不持有第三方对象。 */
typedef void (*MOD_ButtonEventSink_t)(uint8_t id, MOD_ButtonEvent_t event);

/* 初始化与运行 --------------------------------------------------------------*/
MOD_ButtonStatus_t MOD_Button_InitState(const MOD_ButtonConfig_t config[], size_t count,
                                      MOD_ButtonReadLevel_t readLevel,
                                      MOD_ButtonEventSink_t eventSink);
MOD_ButtonStatus_t MOD_Button_Start(void);
MOD_ButtonStatus_t MOD_Button_Stop(void);
MOD_ButtonStatus_t MOD_Button_ProcessTick(void);

#endif /* MOD_BUTTON_H */
