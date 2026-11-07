# embedded-button-template

基于固定版本 MultiButton 的 Module 层按键封装，供嵌入式工程复制使用。
C99 实现，采用静态内存，提供中文接口说明和真实后端主机测试。
公共头文件不依赖 HAL、RTOS 或第三方类型。默认最多 16 个按键，只有一个管理器。

[公共接口](User/Module/mod_button.h) · [实现](User/Module/mod_button.c) · [可编译示例](examples/button_example.c) · [设计](docs/design.md) · [验证记录](docs/validation.md)

## 功能与分层

支持消抖、按下与释放、单击、双击、连击、长按开始和长按保持。
封装复制初始化配置，管理按键实例及启停，向应用同步交付按键 ID 和事件。

| 位置 | 职责 |
|---|---|
| `User/Module/mod_button.c/h` | 公共接口、参数校验、事件转换及生命周期 |
| `User/ThirdParty/MultiButton/` | 固定版本后端、许可证及 SHA-256 来源记录 |
| `examples/button_example.c/h` | 注入输入与消费者，按单调毫秒时间安排周期扫描 |
| `tests/` | 真实后端行为测试、注册失败注入和 Windows 主机测试入口 |
| `docs/` | 架构说明与验证记录 |
| `CMakeLists.txt` | 可供宿主链接的 `mod_button` 静态库目标 |

GPIO 读取由宿主 BSP 提供，产品动作由 App 决定，Module 通过函数指针连接两者。
适用于 STM32 CubeMX/HAL 工程及提供相同输入契约的 C99 工程。

## 获取源码

```sh
git clone https://github.com/ALOLAN96/embedded-button-template.git
cd embedded-button-template
```

MultiButton 所需源码随仓库保存，固定到提交 `397a30ef45027da5a603e16358a651a3c193c64a`。
更新依赖时同步更新来源记录并重新执行行为测试。

## 集成

1. 复制 `User/Module`、`User/ThirdParty/MultiButton`，保留许可证及 provenance.json。
2. 将 `mod_button.c`、`multi_button.c` 加入宿主工程源码列表。
3. 添加 `User/Module`、`User/ThirdParty/MultiButton` 到实现的 include 路径。应用只包含 `mod_button.h`。
4. 使用 C99 或兼容模式。需要扩容时对组件统一设置 `MOD_BUTTON_MAX_COUNT`，范围 1..256；默认 16。
5. 通过 CubeMX 及原有 BSP 完成 GPIO 静态配置，再初始化 Module，最后启动扫描。
6. 在同一任务或主循环中每 **5 ms** 调用一次 `MOD_Button_ProcessTick()`。禁止 ISR 调用、并发调用和直接调用底层 API。

CMake 可 `add_subdirectory` 后链接 `mod_button`。测试专用的 `tests/stubs`、runtime.c 和 start_fault.c 不得进入产品工程。
模板没有指定板卡、.ioc、RTOS 或固件镜像；EIDE/Keil 工程配置和目标验证由宿主完成。

```c
#include "mod_button.h"

/* Board_ReadButtonLevel 和 App_OnButtonEvent 由宿主提供。 */
extern uint8_t Board_ReadButtonLevel(uint8_t id);
extern void App_OnButtonEvent(uint8_t id, MOD_ButtonEvent_t event);

MOD_ButtonStatus_t App_ButtonInit(void)
{
    const MOD_ButtonConfig_t keys[] = {{0U, 0U}, {1U, 1U}};
    MOD_ButtonStatus_t status = MOD_Button_InitState(keys, 2U,
        Board_ReadButtonLevel, App_OnButtonEvent);
    if (status != MOD_BUTTON_STATUS_OK) { return status; }
    return MOD_Button_Start();
}
/* 主循环/任务的每个 5 ms 调度点调用 MOD_Button_ProcessTick() 并检查结果。 */
```

`Board_ReadButtonLevel(id)` 在 BSP/适配边界内映射 GPIO，返回 **物理 0/1**，不转换为“按下为 1”；有效极性由 activeLevel 负责。
`App_OnButtonEvent` 根据按键 ID 与事件决定页面切换等产品动作，必须非阻塞；较重的工作延后处理。

## 接口与生命周期

| API | 契约 |
|---|---|
| InitState | 检查并复制配置，不读取硬件；原数组只需在调用期间有效 |
| Start | 初始化后注册全部按键；已运行时成功且不重置手势；失败撤销本次注册 |
| Stop | 已初始化后幂等成功；解除注册并保留配置；停止后不再产生事件 |
| ProcessTick | 扫描一次并同步回调；未初始化 NOT_INIT，停止时 INVALID_STATE |

- 首次使用无需提供或清零对象；管理器和 Button 数组由封装独占。
- ID 是 uint8_t，0..255，表内不得重复；activeLevel 只允许 0 或 1；数量与两个回调均须有效。
- 参数失败不改变旧配置。运行中 InitState 返回 INVALID_STATE；输入回调、事件回调中的所有 Module API 调用返回 BUSY，不修改状态。
- BUSY 是同步反入防护，不是锁。跨任务/ISR 调用违反使用契约，不能依赖 BUSY 解决数据竞争。
- 重新配置必须先 Stop，再 InitState、Start。Stop 后 Start 清除旧手势；仍按住的按键消抖后作为新按下处理。
- 禁止宿主直接使用同一份 MultiButton 的注册、停止、tick 等 API：其全局链表不支持独立管理器隔离。其他组件也需要该库时须统一所有者。
- 所有回调和输入源必须在运行期间有效。事件参数按值传递，没有第三方指针生命周期负担。

## 事件与时间

DOWN、UP、REPEAT、SINGLE_CLICK、DOUBLE_CLICK、LONG_START、LONG_HOLD 分别表示按下、释放、连击增加、单击确认、双击确认、长按开始和长按保持。
REPEAT 并非长按重复；LONG_HOLD 在长按期间每次扫描都可能报告。跨按键回调顺序不作为公共契约。

固定后端参数位于 `multi_button.h`：扫描 5 ms，消抖 3 次，连击相关阈值 300 ms，长按阈值 1000 ms。
后端采用计数和严格大于比较：在理想 5 ms 扫描下，按下确认之后第 201 个 tick 才报告长按开始；不能把阈值宏解释为精确毫秒截止时间。
单击需要等待连击窗口结束，不在松开时立刻确认。首版不提供运行时修改阈值接口，不能在外部重复定义一套未生效参数。

[App 示例](examples/button_example.c) 接受宿主单调毫秒时间：保持 5 ms 调度相位，迟到多个周期时只扫描一次，记录漏掉的槽数并饱和计数。
示例使用 ID 7 低有效、ID 200 高有效，复制时按板卡修改。Poll 可在每轮主循环调用；调用时间不能倒退，且两次调用不得跨越完整 uint32 毫秒周期（约 49.7 天）。
示例所有入口也必须串行，禁止在其消费者回调中重入示例 Poll/Init；统计读取同样由所有者上下文执行。

**漏扫会拉长真实手势识别时间。** 不允许对当前电平连续调用扫描来假冒过去的采样。要求准确时序时应保证调度延迟，实测最大抖动及全部按键扫描耗时。
长时间阻塞、STOP 睡眠前应停止模块，恢复后重新建立调度基线并启动；本模板不承诺睡眠期间识别手势。

## 测试与许可

在仓库根目录执行以下命令，替换为本机 Clang 路径，同目录需提供 LLD。

```sh
python tests/run.py --clang "C:/path/to/LLVM/bin/clang.exe"
```

测试支持 Windows x86_64 与 Python 3，已验证 Clang 16.0.6。
2026-11-07 复验结果为 O0/O2 各 11 组行为测试通过，32 位编译检查通过。
提供 O0/O2 行为执行与 32 位编译检查，后者不代表执行或 STM32 构建通过。完整边界见验证记录。

自有封装采用 MIT，Copyright (c) 2026 ALOLAN。MultiButton 保留 Zibin Zheng 的 MIT 许可证和原始源码声明，见 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md)。
