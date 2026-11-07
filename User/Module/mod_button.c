/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file mod_button.c @brief Module 层按键对象、事件桥接和生命周期唯一所有者。 */
#include "mod_button.h"
#include "multi_button.h"

#if MOD_BUTTON_MAX_COUNT < 1 || MOD_BUTTON_MAX_COUNT > 256
#error "MOD_BUTTON_MAX_COUNT must be in 1..256"
#endif
#if TICKS_INTERVAL != MOD_BUTTON_TICK_INTERVAL_MS
#error "MultiButton scan period differs from public timing contract"
#endif

/** @brief 静态后端对象；只在此模块访问，不复制活跃对象。 */
static struct Button buttons[MOD_BUTTON_MAX_COUNT];
/** @brief 私有配置副本，调用者初始化返回后可释放原表。 */
static MOD_ButtonConfig_t settings[MOD_BUTTON_MAX_COUNT];
/** @brief 当前有效配置数量，仅在停止状态的 InitState 修改。 */
static size_t buttonCount;
/** @brief 配置/运行/扫描标志；仅单一上下文使用，不提供线程同步。 */
static uint8_t initialized, running, processing;
/** @brief 输入及事件回调；初始化时绑定，运行中不可替换。 */
static MOD_ButtonReadLevel_t input;
static MOD_ButtonEventSink_t sink;

/** @brief 事件按值交付；后端对象仅由后端传回的有效指针访问。 */
static void emit(void *handle, MOD_ButtonEvent_t event)
{
    const struct Button *button = (const struct Button *)handle;
    sink(button->button_id, event);
}
/** @brief 桥接按下，不依赖后端 event 字段。 */
static void on_down(void *handle) { emit(handle, MOD_BUTTON_EVENT_DOWN); }
/** @brief 桥接释放。 */
static void on_up(void *handle) { emit(handle, MOD_BUTTON_EVENT_UP); }
/** @brief 桥接连击；后端此回调时 event 可能仍是 PRESS_DOWN。 */
static void on_repeat(void *handle) { emit(handle, MOD_BUTTON_EVENT_REPEAT); }
/** @brief 桥接确认单击。 */
static void on_single(void *handle) { emit(handle, MOD_BUTTON_EVENT_SINGLE_CLICK); }
/** @brief 桥接确认双击。 */
static void on_double(void *handle) { emit(handle, MOD_BUTTON_EVENT_DOUBLE_CLICK); }
/** @brief 桥接长按开始。 */
static void on_long_start(void *handle) { emit(handle, MOD_BUTTON_EVENT_LONG_START); }
/** @brief 桥接长按保持，每个扫描周期都可能发生。 */
static void on_long_hold(void *handle) { emit(handle, MOD_BUTTON_EVENT_LONG_HOLD); }

/**
 * @brief 停止时校验并复制配置，不读取硬件、不注册后端。
 * @param config 唯一 ID 与有效电平数组；在本调用期间有效且不可被回调修改。
 * @param count 元素数量，1..MOD_BUTTON_MAX_COUNT。
 * @param readLevel 非阻塞读取函数，必须返回物理电平 0/1。
 * @param eventSink 非阻塞事件消费者。
 * @return OK；参数失败不改变旧配置；运行中 INVALID_STATE，反入 BUSY。
 * @note 无需调用者提供静态 Button；重新初始化仅限停止后执行。
 */
MOD_ButtonStatus_t MOD_Button_InitState(const MOD_ButtonConfig_t config[], size_t count,
                                      MOD_ButtonReadLevel_t readLevel, MOD_ButtonEventSink_t eventSink)
{
    size_t i, j;
    if (processing != 0U) { return MOD_BUTTON_STATUS_BUSY; }
    if (running != 0U) { return MOD_BUTTON_STATUS_INVALID_STATE; }
    if (config == NULL || count == 0U || count > MOD_BUTTON_MAX_COUNT ||
        readLevel == NULL || eventSink == NULL) { return MOD_BUTTON_STATUS_INVALID_PARAM; }
    for (i = 0U; i < count; ++i) {
        if (config[i].activeLevel > 1U) { return MOD_BUTTON_STATUS_INVALID_PARAM; }
        for (j = 0U; j < i; ++j) {
            if (config[i].id == config[j].id) { return MOD_BUTTON_STATUS_INVALID_PARAM; }
        }
    }
    for (i = 0U; i < count; ++i) { settings[i] = config[i]; }
    buttonCount = count; input = readLevel; sink = eventSink; initialized = 1U;
    return MOD_BUTTON_STATUS_OK;
}

/**
 * @brief 清除手势状态并注册全部按键；重复启动成功且不重置现有手势。
 * @return 未初始化 NOT_INIT；扫描期间 BUSY；注册失败 BACKEND_ERROR。
 * @note 注册失败撤销本次已加入对象，保持停止状态。不得直接操作底层全局链表。
 */
MOD_ButtonStatus_t MOD_Button_Start(void)
{
    size_t i;
    if (processing != 0U) { return MOD_BUTTON_STATUS_BUSY; }
    if (initialized == 0U) { return MOD_BUTTON_STATUS_NOT_INIT; }
    if (running != 0U) { return MOD_BUTTON_STATUS_OK; }
    for (i = 0U; i < buttonCount; ++i) {
        button_init(&buttons[i], input, settings[i].activeLevel, settings[i].id);
        button_attach(&buttons[i], PRESS_DOWN, on_down);
        button_attach(&buttons[i], PRESS_UP, on_up);
        button_attach(&buttons[i], PRESS_REPEAT, on_repeat);
        button_attach(&buttons[i], SINGLE_CLICK, on_single);
        button_attach(&buttons[i], DOUBLE_CLICK, on_double);
        button_attach(&buttons[i], LONG_PRESS_START, on_long_start);
        button_attach(&buttons[i], LONG_PRESS_HOLD, on_long_hold);
        if (button_start(&buttons[i]) != 0) {
            /* 包含当前对象，撤销本次触及的所有注册。 */
            size_t j;
            for (j = 0U; j <= i; ++j) { button_stop(&buttons[j]); }
            return MOD_BUTTON_STATUS_BACKEND_ERROR;
        }
    }
    running = 1U;
    return MOD_BUTTON_STATUS_OK;
}

/**
 * @brief 逐个解除注册，保留配置；重复停止成功。
 * @return OK、NOT_INIT 或扫描/回调期间的 BUSY。
 * @note 停止与扫描串行执行，返回后无事件；重启从新手势开始。
 */
MOD_ButtonStatus_t MOD_Button_Stop(void)
{
    size_t i;
    if (processing != 0U) { return MOD_BUTTON_STATUS_BUSY; }
    if (initialized == 0U) { return MOD_BUTTON_STATUS_NOT_INIT; }
    if (running == 0U) { return MOD_BUTTON_STATUS_OK; }
    for (i = 0U; i < buttonCount; ++i) { button_stop(&buttons[i]); }
    running = 0U;
    return MOD_BUTTON_STATUS_OK;
}

/**
 * @brief 处理一次真实电平采样；调用者每 5 ms 调用，漏扫不补造历史采样。
 * @return OK、NOT_INIT、停止时 INVALID_STATE 或反入 BUSY。
 * @note 同步执行 readLevel 和 eventSink；耗时取决于数量及回调，不在 ISR 调用。
 *       processing 仅防同步反入，不是互斥锁，也不能使多线程并发安全。
 */
MOD_ButtonStatus_t MOD_Button_ProcessTick(void)
{
    if (processing != 0U) { return MOD_BUTTON_STATUS_BUSY; }
    if (initialized == 0U) { return MOD_BUTTON_STATUS_NOT_INIT; }
    if (running == 0U) { return MOD_BUTTON_STATUS_INVALID_STATE; }
    processing = 1U;
    button_ticks();
    processing = 0U;
    return MOD_BUTTON_STATUS_OK;
}
