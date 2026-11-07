/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file start_fault.c @brief 仅测试链接接缝：第二次注册可注入失败。 */
#include "multi_button.h"
/** @brief 测试 runner 将上游 button_start 重命名，源码保持不变。 */
int button_start_real(struct Button *handle);
/** @brief 注册失败倒计数，测试控制，产品工程不编译本文件。 */
unsigned test_start_fail_after;
/** @brief 保留真实后端，指定次数时模拟注册失败。 */
int button_start(struct Button *handle)
{
    if (test_start_fail_after != 0U) {
        --test_start_fail_after;
        if (test_start_fail_after == 0U) { return -1; }
    }
    return button_start_real(handle);
}
