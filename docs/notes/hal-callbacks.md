---
title: 【拓展】HAL回调的注册方式
description: 用四字节接收实验区分弱回调与句柄注册，定位注册状态、IRQ链路和回调执行上下文。
sourceKey: 【拓展】HAL回调的注册方式
---

# HAL 回调的注册方式

串口已经能收到数据，接收完成回调里的计数器却一直为零。你照着别的工程写了一个函数，又把“注册回调”的宏改成 1，结果编译通过，现象没变。这里至少有三个不同的问题：函数有没有接到句柄上，这一轮接收有没有启动，硬件事件有没有沿中断入口走到 HAL。**注册成功只回答第一个问题。**

下面沿用 [2.4 程序是怎么跑起来的](/lessons/2-4) 的工程结构，以 STM32F407、Cortex-M4 和 STM32F4 HAL 1.8.5 为例。不同系列、不同外设的注册规则不能仅凭函数名类推。

## 先让四个字节触发一次回调

准备一个已能正常运行的 F407 CubeMX 工程：USART2 异步模式，115200、8N1；配置实际使用的 GPIO 复用和 USART2 全局中断。USB 转串口使用匹配的 3.3 V 逻辑电平，共地，将发送端 TX 接板上 USART2 RX，不要把 RS-232 电平直接接过来。串口工具关闭自动附加换行。

完整板级片段在 [hal_callback_probe.c](https://github.com/cafezn/from-c-to-mcu/blob/HEAD/examples/notes/serial/hal-flow/hal_callback_probe.c) 和 [头文件](https://github.com/cafezn/from-c-to-mcu/blob/HEAD/examples/notes/serial/hal-flow/hal_callback_probe.h)。它依赖生成的 USART2 句柄、HAL 头文件和中断入口，不是独立固件。把两者加入用户源文件目录与构建清单；若项目已经定义接收完成回调，应合并分发逻辑，不能再定义同名强符号。注册方式的核心如下：


```c
static uint8_t probe_rx[4];
volatile uint32_t callback_probe_hits;

static void probe_done(UART_HandleTypeDef *uart)
{
    if (uart == &huart2) ++callback_probe_hits;
}

HAL_StatusTypeDef callback_probe_start(void)
{
    HAL_StatusTypeDef status = HAL_UART_RegisterCallback(
        &huart2, HAL_UART_RX_COMPLETE_CB_ID, probe_done);
    if (status != HAL_OK) return status;
    return HAL_UART_Receive_IT(&huart2, probe_rx, sizeof probe_rx);
}
```

在工程的 HAL 配置中把 `USE_HAL_UART_REGISTER_CALLBACKS` 设为 1，确保应用、HAL 驱动和所有依赖该句柄定义的编译单元使用相同配置，修改后完整重编译。然后在 `MX_USART2_UART_Init()` 之后、首次收发之前调用 `callback_probe_start()`，检查返回值是否为 `HAL_OK`。示例头文件要放在调用处可见的位置。

1. 复位后发送 `AB`，预期计数仍为 0：这次请求还差两个字节。
2. 再发送 `CD`，预期缓冲区为四个字节，计数变成 1。
3. 在上一轮完成后再调用一次启动函数，发送 `EFGH`，预期计数变成 2。不重新启动时，完成回调不会自行循环；继续灌数据还可能产生硬件溢出。

这组预期依赖接线和 IRQ 正常，本文没有真机实测。不要在连续流量下长时间停在断点里，把暂停导致的溢出误判成注册错误。

## 两条路径，最后调用的是不同入口

宏为 0 时，HAL 内部直接调用 `HAL_UART_RxCpltCallback(huart)`。库提供一个弱定义，你在应用里给出同签名的强定义，链接后用后者。它是全工程共享的入口，通常按 `huart` 或实例分派不同串口；拼错函数名只会多出一个无人调用的普通函数。

宏为 1 时，句柄增加函数指针成员，接收完成处调用 `huart->RxCpltCallback(huart)`。初始化会把这些成员接到默认回调上，因此注册模式并不是“彻底取消 weak”，而是在默认入口之上允许每个句柄换接函数。调用注册 API 比直接写结构体成员更合适，因为 API 会检查支持的回调 ID、指针和状态。

本例函数的签名必须是 `void f(UART_HandleTypeDef *)`。不要用强制类型转换掩盖参数不同：函数指针转换不把不兼容的调用变成合法调用。注销 API 恢复默认的弱/已覆盖入口，并不是停止接收、关闭 IRQ，也不是将函数指针置空。

## 注册时机要看源码里的状态

所读版本的 `HAL_UART_RegisterCallback` 检查 `gState`，规则如下。

| 当时的状态或参数 | 这个版本的结果 |
| --- | --- |
| `HAL_UART_STATE_RESET`，注册普通收发完成回调 | `HAL_ERROR`，设置无效回调错误位 |
| `HAL_UART_STATE_RESET`，注册 MSP 初始化/反初始化回调 | 允许，便于初始化时使用 |
| `HAL_UART_STATE_READY`，合法 ID、非空函数指针 | `HAL_OK` |
| 其他状态、空函数指针或不支持的 ID | `HAL_ERROR` |

句柄本身必须是有效对象，不能用空句柄调用这项 API。这里的忙状态注册失败是 `HAL_ERROR`，不要擅自写成 `HAL_BUSY`；而重复启动尚未完成的 `HAL_UART_Receive_IT` 则会按 `RxState` 返回 `HAL_BUSY`。同为 HAL API，状态机并不相同。

尤其要注意：UART 的 `gState` 管全局管理和发送，接收另有 `RxState`。只有接收忙时，前者仍可能是 READY，于是注册 API 仍能返回成功。**这不构成并发换接回调的安全保证。** 教学工程最稳妥的顺序是初始化完成、尚未启动收发时注册；运行中确需替换，应先停止相关事务并处理待决事件，按项目中断或任务同步方案切换。

从 RESET 路径执行初始化会重设普通完成回调，MSP 回调则有保留已注册值的特殊处理。不要在初始化前偷偷给接收函数指针赋值后期待它保留。接收至空闲事件还有专用 `HAL_UART_RegisterRxEventCallback`，其签名带长度/位置参数，不能拿普通接收完成 ID 代替。

## 接好函数以后，还差一条事件链

普通 IT 接收经过下面的链路：


```text
USART 接收事件 -> NVIC -> USART2_IRQHandler
    -> HAL_UART_IRQHandler -> 处理计数与状态 -> 用户回调
```

检查 USART2 的 NVIC 使能、优先级与屏蔽状态，并确认生成的中断函数确实调用 `HAL_UART_IRQHandler(&huart2)`。不要另写第二个同名中断函数。轮询接收返回成功也不证明这条 IT 链路已接通。

DMA 同样不意味着“不需要中断”。DMA 流的 IRQ 要进入对应 `HAL_DMA_IRQHandler`，句柄与父外设要正确关联。此版 UART 的普通 DMA 发送完成先关闭 DMA 请求、打开 USART TC 中断，等最后一帧真正发送完，USART IRQ 才恢复状态并调用发送完成回调。只开 DMA IRQ，可能看到数据发出却一直等不到通知。循环 DMA 的路径不同，不应混为一谈。

普通 IT 接收完成前，HAL 先关闭该轮相关接收中断并把 RxState 恢复为 READY，再调用用户函数，所以可在短回调内按协议安排下一轮接收。但普通接收的完成回调不等于所有接收模式的统一入口，错误、半传输与空闲事件应分别阅读实现。

## 两个容易越过的边界

**回调不是任务调度器。** 这里的 IT/DMA 完成通知通常在 IRQ 上下文执行，适合记状态、搬有限数据、发通知，不适合 `HAL_Delay`、阻塞日志或忙等另一个中断完成。若完成条件依赖同一个被占住的 IRQ，等待会永远不结束。MSP 初始化回调则由初始化 API 直接调用，通常发生在主线程；不能把所有带 Callback 名字的函数都说成 ISR。

**队列满不等于 UART ORE。** 中断及时读取硬件数据寄存器后，即使应用环缓已满，也可能只是软件丢数据而没有硬件溢出。此版错误处理对 ORE 和 DMA 接收错误会终止接收，对某些非阻塞错误可能继续；恢复代码必须检查错误、接收状态和重新启动结果，不能无条件重启后丢弃返回值。调试用的 volatile 计数器也不替多字段队列提供同步。

## 没有板子，先验证顺序

[callback_model.c](https://github.com/cafezn/from-c-to-mcu/blob/HEAD/examples/notes/serial/hal-flow/callback_model.c) 是单线程 C99 事件模型。它检查 RESET/READY 注册条件、空指针与非法 ID、接收忙返回值、未接 IRQ 时计数不变、每句柄分派，以及普通 DMA 完成仍需 UART TC 事件。它没有包含 HAL，不模拟寄存器、电气或真实并发。

在仓库根目录可复现：

```sh
mkdir -p artifacts/course-completion/serial/hal-flow
gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic examples/notes/serial/hal-flow/callback_model.c -o artifacts/course-completion/serial/hal-flow/callback_model.exe
./artifacts/course-completion/serial/hal-flow/callback_model.exe
```

实际运行以 `callback model: PASS` 结尾。模型把源码中选定的约束变成可检查的预测，不代表真实 HAL 已链接测试，更不代表板子已经通过。

## 练习：为什么返回成功还会接错函数

初始化后正在 IT 接收，gState 为 READY、RxState 为 BUSY_RX。此时主循环把接收回调从甲换成乙，注册返回成功。能否据此断言“正在进行的这一轮一定交给甲，下一轮才交给乙”？又该怎样组织安全切换？

<details>
<summary>展开答案</summary>

不能。此版注册没有检查 RxState，完成处理读取的是句柄里当时的函数指针，没有为本轮固定一份副本。模型里可以安排事件先后，真实工程还存在抢占时序。应在停止相关接收、确认终止/完成并处理待决事件后，按同步策略换接，再显式启动下一轮；最简单的是启动阶段一次注册、不在流量中热切换。

</details>

## 核对依据与适用范围

- [ST STM32F4 HAL UART 源码](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_uart.c)：本次实际读取提交 `1f6451c3`；重点是初始化、Register/UnRegister、IRQHandler、UART_Receive_IT、UART_DMATransmitCplt 与 UART_EndTransmit_IT。
- [HAL 配置模板](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Inc/stm32f4xx_hal_conf_template.h#L189) 与 [UART 头文件](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Inc/stm32f4xx_hal_uart.h#L188)：核对默认宏值、两种状态、函数指针签名；同提交版本源文件标为 HAL 1.8.5。
- [RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：实际读取 Rev 22 缓存的 §30.3.14 和 USART_CR3，辅助区分硬件接收器与软件状态。其他 STM32 系列请以各自手册和驱动为准。

回到 [2.4 程序是怎么跑起来的](/lessons/2-4)，接着做 [6.1 串口命令行](/lessons/6-1)；相关分支：[串口流控与 USB CDC](/notes/flow-control-usb-cdc)、[寄存器与内存映射](/notes/memory-mapped-io)。
