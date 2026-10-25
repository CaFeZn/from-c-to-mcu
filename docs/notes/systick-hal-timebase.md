---
title: 【原理】SysTick与HAL时基
description: 顺着 SysTick 中断到 HAL 毫秒计数的路径排查延时卡死、时钟修改与丢节拍。
sourceKey: 【原理】SysTick与HAL时基
---

# SysTick 与 HAL 时基

把 `HAL_Delay(100)` 放进按键中断，程序突然不回主循环；把同一行搬到 `HAL_Init()` 前面，又停在开机第一步。灯不闪不是延时函数“不够聪明”，而是它等的计数从未再前进。

[2.4 程序是怎么跑起来的](/lessons/2-4) 给出了先初始化 HAL、再配置系统时钟的骨架。本篇沿着那个骨架往下追：**谁负责产生一拍，谁把一拍变成毫秒，谁只是拿毫秒做比较？**

## 先算一个可以观察的配置

在 STM32F407 裸机 HAL 默认时基、HCLK=168 MHz 的前提下，SysTick 选择处理器时钟，每毫秒需要 168000 个时钟周期。它是 24 位向下计数器，所以连续周期的重装值是 `168000-1=167999`。不是 PSC=167、ARR=999；那是外设 TIM 的另一套结构。

| 时基配置 | 每拍输入周期数 | LOAD |
|---|---|---|
| HCLK=16 MHz，1 ms | 16000 | 15999 |
| HCLK=168 MHz，1 ms | 168000 | 167999 |
| HCLK=168 MHz，10 ms | 1680000 | 1679999 |

先预测：保留 LOAD=15999，却把它的输入从 16 MHz 改为 168 MHz，一拍还是 1 ms 吗？不是，变成约 95.238 µs，软件“毫秒”会跑快 10.5 倍。正常 HAL 时钟配置路径会更新 `SystemCoreClock` 并重新调用 `HAL_InitTick`；自己只改 RCC 而不重配时基，就可能制造这个故障。

可跟做的观察方法是在已正常初始化的主循环中保存开始的 `HAL_GetTick()`，每差值达到 1000 再翻转一个空闲 GPIO。独立仪器预期量到相邻翻转约 1 s，即完整方波约 0.5 Hz。串口可用于粗看，但 USB 缓冲、日志传输和主循环延迟都会影响到达电脑的时间，不要把串口时间戳当精密频率基准。

## 三层不要混成一层

1. **硬件 SysTick**：CNT 从 LOAD 向下数，数到 0 可提出异常请求。CTRL 的 ENABLE 控制计数，TICKINT 控制异常请求，CLKSOURCE 选择时钟；F407 此处是 HCLK 或 HCLK/8。
2. **中断入口**：默认工程的 `SysTick_Handler` 必须实际调用 `HAL_IncTick()`。计数器运转、请求挂起，并不等于处理函数已经运行。
3. **HAL 软件计数**：本篇核对的 HAL v1.8.5 中，`HAL_IncTick` 执行 `uwTick += uwTickFreq`，`HAL_GetTick` 返回这个变量。默认每 1 ms 加一。

这里的 `uwTickFreq` 名字容易误导：它的枚举取值参与“每次推进多少毫秒”。默认 1 kHz tick 时增量为 1；使用库支持的 100 Hz tick 时，约每 10 ms 加 10，不能只把硬件中断降到 100 Hz，却仍然每次加一。后者会让以毫秒定义的 HAL 超时一起变慢十倍。

SysTick 的 LOAD 只有 24 位，连续周期最大输入计数为 2²⁴。168 MHz 时最长约 99.864 ms，不能直接拿它装一个 1 s 的周期。长时间应由软件累计；LOAD=0 也不是正常的一拍分频配置，因为该硬件的异常与 COUNTFLAG 依赖从 1 数到 0 的过程。

## HAL_Delay 等待的不是一个硬件完成位

核对 v1.8.5 的实现，其核心先取 `tickstart`，再设置 `wait=Delay`；对小于 `HAL_MAX_DELAY` 的值再加一个 tick 增量，最后忙等：

```c
/* 解释性片段；使用工程 HAL 的真实实现，不另定义同名函数。 */
while ((HAL_GetTick() - tickstart) < wait) {
    /* 中断能运行时，uwTick 才可能继续前进 */
}
```

以默认 1 ms tick、请求 Delay=2 为例，wait 是 3。假定起始 tick=100，人工输入 100、101、102、103，判断结果依次是“继续、继续、继续、结束”。增加这一拍是为了覆盖调用时刚好落在 tick 末尾的情况。它保证的是正常时基条件下的最短等待意图，不是精确产生 2.000 ms 的脉冲；抢占、调度和调用开销还能让它更长。

同样，Delay=0 在这个默认实现里也可能等到下一拍，不能把它当作“肯定什么也不做”。不要把上述推导推广到用户重写的弱函数或任意极大延时参数；长期定时使用明确的时间戳算法。

## 卡死时沿着链检查

先看计数有没有变化，再看为什么没有变化，避免一上来改优先级碰运气：

- 尚未初始化：默认 `HAL_Init` 还没建立时基，先调用正延时会一直等。把应用延时放到初始化完成之后。
- SysTick 能计数但 IRQ 不执行：检查全局屏蔽、BASEPRI、向量、Handler 和 `TICKINT`。也检查是否改成了 TIM 时基，却忘记启动或转发它的中断。
- 当前 ISR 等待 tick：只有时基异常拥有**更高的抢占优先级**，才能打断正在等待的 ISR。Cortex-M 优先级数值越小，逻辑优先级越高；相同抢占级别不能互相抢占。子优先级不能弥补这一点。

课程建议仍是“ISR 中不调用阻塞延时”。即使提升 SysTick 抢占级别使计数能走，当前 ISR 还是占着执行路径数毫秒，会破坏其他外设的实时性。用 ISR 记录事件，主循环状态机根据到期时间推进。

长时间屏蔽中断还会造成**丢节拍**。SysTick 挂起状态和 COUNTFLAG 不是一个能记录“漏了几百次”的队列；恢复中断后不能保证把每个错过的毫秒逐一补回。CTRL 的 COUNTFLAG 表示自上次读取以来是否到过 0，读取会影响它，调试器连续观察寄存器也可能扰动以该标志做轮询的实验。

## 时基的拥有者必须只有一个

SysTick 是内核资源，而 HAL 时基是一份软件约定。HAL 的相关弱函数允许改用外设 TIM；RTOS 也可能占用 SysTick，此时要按对应 RTOS/Cube 集成方案分工，不能另外加一份 `SysTick_Handler`，或让 SysTick 和 TIM 都去增加同一个 `uwTick`。

主线 [5.2 定时器与节拍](/lessons/5-2) 用 TIM6 维护独立 `g_ms`，默认 HAL 仍可用 SysTick。两只计数不是自动同步的；如果把 Cube 的 HAL Timebase Source 又选成 TIM6，就必须重新安排资源和回调，而不是照抄两份初始化。

停止也要分层看：默认 `HAL_SuspendTick` 禁止 tick 中断，并不是给时间做自动补偿；处理器停钟的深睡眠以及调试暂停都有各自行为。跨掉电的日历时间应该交给 RTC，不用“多加几次 uwTick”猜回来。

## 练习：先算，再找责任层

HCLK=84 MHz，默认 1 ms 时基，LOAD 应是多少？如果 CNT 在变化，`HAL_GetTick()` 却一直为 7，至少列出两个应检查的位置。

<details><summary>展开答案</summary>

LOAD=83999。检查 TICKINT 和全局/优先级屏蔽是否允许异常执行；再检查正确的向量是否进入当前工程的 Handler、Handler 是否调用 HAL_IncTick。若使用替代 HAL 时基，还应追查实际 TIM 中断链。单看 SysTick CNT 变化，不能证明 HAL 软件计数在走。

</details>

## 适用范围与依据

本篇按 F407/Cortex-M4、默认裸机 HAL v1.8.5 路径解释，已核对源码和输入表，未声称在板上测过 GPIO 时间。硬件计数、忙等和软件定时算法的验证不是 RTOS 集成测试。

- [ST PM0214](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)，Rev 10，§4.5：24 位 SysTick、时钟选择、LOAD/VAL/COUNTFLAG 与停钟。
- [STM32F4 HAL v1.8.5](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/v1.8.5/Src/stm32f4xx_hal.c)，`HAL_InitTick`、`HAL_IncTick`、`HAL_Delay`、`HAL_SuspendTick`。
- [ST HAL RCC 源码](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_rcc.c)，`HAL_RCC_ClockConfig` 末尾重新配置时基；[CMSIS SysTick_Config](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/core_cm4.h#L2022-L2035) 中实际写入 `ticks-1`。

回到 [2.4 程序是怎么跑起来的](/lessons/2-4)。相关分支：[软件定时器与简易调度器](/notes/software-timers)、[时钟树](/notes/clock-tree)、[RTC 与长期计时](/notes/rtc-timekeeping)。
