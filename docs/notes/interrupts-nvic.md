---
title: 【原理】中断与NVIC
description: 从按键已置位却不进回调的故障，追踪外设请求、NVIC状态、优先级与异常返回。
sourceKey: 【原理】中断与NVIC
---

# 中断与 NVIC

按下按键，调试器里 EXTI 的 pending 位变成 1，回调计数却一直为零。有人改回调名字，有人把中断优先级填成 0，有人不断点“继续”。这些动作都可能没有碰到真正堵住的环节：外设已经提出请求，不代表 CPU 现在就能接待。

从[5.1 中断入门](/lessons/5-1)的 PA0 下降沿实验出发，把线路分为四站：引脚边沿检测、EXTI 请求、NVIC 仲裁、内核异常入口。不要把它们统称为一个“中断开关”。

## 一次按键要穿过哪些门

在 STM32F407 上，先用手册与生成代码逐项核对，而不是照着一串地址盲写：

1. GPIO 输入模式、上下拉和实际接线是否匹配？按键是否真的有下降沿？
2. SYSCFG 的 EXTI 复用是否把端口 A 的第 0 脚接到 EXTI0？同编号的 PA0、PB0 不能同时占有这条线。
3. EXTI 的下降沿选择和中断掩码是否打开？事件通路 EMR 与中断通路 IMR 不是同一件事。
4. NVIC 是否使能了 EXTI0_IRQn？PRIMASK、BASEPRI 和当前正在执行的异常是否允许它进入？
5. 向量表中的入口是不是正确的 EXTI0_IRQHandler？它是否调用了对应的 HAL 处理函数？

EXTI0 的设备 IRQ 编号为 6，但异常编号是 22。前 16 个表项留给初始栈指针和内核异常，所以不能拿 IRQ 编号直接当完整向量表下标。[向量表分支](/notes/vector-table)会把地址算出来。

NVIC 的全名是 Nested Vectored Interrupt Controller。这里的“嵌套”表示更紧急的异常可以抢占正在执行的处理函数；“向量”表示用表找到入口。它不负责按键消抖，也不知道一串边沿中哪一个代表人的一次按压。

## 用纸笔跑一个可复现的状态实验

设只有 A、B 两个可配置中断，均已使能，没有屏蔽，所有有效优先级位都用于抢占；A 的数值为 5，B 为 2。按下列顺序操作，先预测主循环何时能恢复。

| 时刻 | 输入动作 | 应观察到的状态 |
|---|---|---|
| 0 | 主循环运行 | A、B 均 inactive |
| 1 | A 请求到来 | A pending，随后 active |
| 2 | A 执行中，B 请求到来 | B 抢占 A；两者均 active |
| 3 | B 执行中，A 又请求一次 | A 变为 active and pending |
| 4 | B 返回 | 回到尚未完成的 A |
| 5 | A 完成 | A 的挂起请求仍待处理，可再次进入 A |
| 6 | 没有其他请求 | 返回主循环 |

这里的 inactive、pending、active 是状态，不是三个不同函数。一个处理函数被抢占后仍然 active，因为它还没有完成。一个 IRQ 在 active 期间又收到请求，可以同时 active 和 pending。

把时刻 3 改成“在 CPU 无法处理 A 时连续设置 A pending 三次”。能否保证之后调用 A 三次？不能：pending 位只能表达“至少有一个待处理请求”，不是计数队列。需要准确统计脉冲数，应考虑定时器外部计数、输入捕获、DMA 或足够快的软件计数，而不是希望 NVIC 替你记住所有边沿。

配套的 C99 主机模型 `examples/notes/core/test.c` 会检查优先级选择及 pending 合并。它验证这套状态演算，不模拟真实时钟周期、总线延迟或所有异常竞态。

## 数字越小越紧急，但还要看分组

F407 设备头文件给出 `__NVIC_PRIO_BITS=4`。可配置优先级有 16 个逻辑值，0 最紧急，15 最不紧急。存储优先级的字节只有高 4 位有效，不是可以自由使用 256 个级别。

CMSIS 的 `NVIC_SetPriority` 接受未左移的逻辑值，会根据有效位数完成编码：


```c
/* Configuration fragment: privileged F407 startup, before IRQ use. */
#include "stm32f407xx.h"

void configure_exti_priority(void)
{
    NVIC_SetPriorityGrouping(3u); /* 4 preempt bits, 0 subpriority */
    uint32_t p = NVIC_EncodePriority(3u, 5u, 0u);
    NVIC_SetPriority(EXTI0_IRQn, p);
}
```

在这个明确的分组下，逻辑 5 会进入硬件字节的 `0x50`。不要先手动左移再传给 CMSIS，否则会被再次移动。也不要把 CMSIS 的 AIRCR 分组值 3 与 HAL 的宏名称混淆；宏名中的数字未必就是传入寄存器的值。

分组将有效位分为抢占优先级与子优先级。**只有抢占字段决定能否打断当前处理函数**；子优先级用于同组多个 pending 请求的先后。完全相同的优先级由较小异常编号先服务，但“编号小”不能抢占已经执行的同级 ISR。

分组影响全系统，例子只应在独立实验或工程统一初始化处使用，不能为修一个按钮在运行中的 RTOS 工程里随意修改它。

## CPU 怎样记得回到哪里

普通函数调用靠调用约定保存返回点；异常入口则有额外的硬件过程。在基本栈帧情形，Cortex-M4 保存 R0–R3、R12、LR、PC、xPSR 共八个字，随后取向量表入口，进入 Handler mode。被打断的位置、运行状态和必要参数因此有了恢复依据。

LR 此时保存的是特殊 EXC_RETURN 编码，而不是普通 C 函数的返回地址。处理函数完成后，内核据此恢复相应栈帧与模式。编译器仍需处理函数使用的其他寄存器；开启浮点上下文保存后，栈需求也可能更大。不要据“八个字”就给全部中断只留 32 字节栈。

若返回时另一个合适的请求已经挂起，内核可能直接进入下一个处理函数，省去一次不必要的出栈再入栈，这叫尾链。它减少切换开销，不保证主循环一定获得一次执行机会；持续请求仍然可能把主循环饿住。

## 三个容易弄反的判断

**关中断不等于清标志。** PRIMASK 阻止可配置优先级异常被激活，但外设可以继续产生事件。恢复之后积压请求可能立刻被处理；NMI、HardFault 不受 PRIMASK 屏蔽。

**更高优先级不是更大数字。** 若当前 ISR 抢占优先级数值为 5，SysTick 为 15，SysTick 无法打断它；当前 ISR 中的 HAL_Delay 可能永远等不到时基前进。同抢占级也不能打断。即使把 SysTick 改成足够紧急使延时能结束，在 ISR 里忙等仍会扩大响应时间，本课程依然不这样写。

**清 NVIC pending 不等于应答外设。** 外设源仍在请求时，NVIC 还会再次挂起。应按外设协议处理数据和清源标志；使用 HAL 的 EXTI 处理函数时，清标志在 HAL 内部完成，不要在用户回调末尾再清一遍。

## 练习：A 为什么抢不进去

当前 A 的抢占级为 2、子优先级为 1；B 的抢占级为 2、子优先级为 0。A 正执行时 B 到来，B 能否抢占？若 A、B 都在 PRIMASK=1 时挂起，恢复后谁先执行？

<details><summary>展开答案</summary>

不能抢占：两者抢占级相同，B 只能 pending。若恢复时两者都尚未 active，则 B 的子优先级更靠前，先服务 B。若还想让 B 能打断 A，必须调整抢占字段，并评估共享状态、栈深度和其他中断，不能仅改变子优先级。

</details>

## 适用范围与参考

本文针对 STM32F407 的 Cortex-M4；其他 Cortex-M、其他 STM32 实现的优先级位数及中断列表需另查。配置函数只经过交叉编译，没有声称按键实验或嵌套延迟已经上板实测。

- [PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.3 的异常状态、优先级、入口与返回，§4.3 NVIC。
- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第12章 EXTI 与中断事件。
- [ST 官方 F407 设备头文件](https://github.com/STMicroelectronics/cmsis-device-f4/blob/9192c7b9df75a142f2027ab266601fe061fc00b3/Include/stm32f407xx.h)：IRQn 与优先级位数；[CMSIS 内核实现](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/core_cm4.h)中的优先级编码函数。

回到[5.1 中断入门](/lessons/5-1)；相关分支：[中断向量表](/notes/vector-table)、[中断风暴](/notes/interrupt-storms)、[从协作式到抢占式调度](/notes/cooperative-preemptive)。
