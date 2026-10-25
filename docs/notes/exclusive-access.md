---
title: 【拓展】LDREX与STREX原子指令
description: 手工制造一次独占写失败，理解 Cortex-M4 如何重试读改写，以及它为何不是 DMA 锁或万能自旋锁。
sourceKey: 【拓展】LDREX与STREX原子指令
---

# LDREX 与 STREX 原子指令

在[读改写与原子性](/notes/read-modify-write)里，主循环读取 10，中断把计数改为 11，主循环仍把自己的 11 写回，丢了一次更新。临界区可以不让中断插队。有没有另一种办法：允许插队，但让主循环知道“刚才的读取已经不能作为这次写回的依据”？

Cortex-M4 的独占访问指令提供了这种尝试机制。重点不是名字里的“独占”，而是**写回可能失败，失败时必须重新读**。

## 先用一张表理解成功与失败

设计数器在自然对齐的普通 SRAM 中，初值为 10，只有同一 CPU 上的主循环与中断访问。主循环使用独占读写；中断在中间执行一次普通自增。

| 步骤 | 执行者 | 动作 | 计数器 |
|---|---|---|---|
| 1 | 主循环 | LDREX 读出 10，建立本地独占标记 | 10 |
| 2 | CPU | 接受异常，清除本地独占标记 | 10 |
| 3 | ISR | 完成一次自增并返回 | 11 |
| 4 | 主循环 | STREX 尝试写 11，返回失败，不写入 | 11 |
| 5 | 主循环 | 重新 LDREX，读出 11 | 11 |
| 6 | 主循环 | STREX 写入 12，返回成功 | 12 |

它与直接写回的差别发生在第 4 步：没有资格的旧计算结果被拒绝，没有覆盖内存。若第 5 步只重复 STREX，而不重新 LDREX 和计算，就没有重新建立本次读改写所需的条件。

CMSIS 的 `__STREXW` 返回 0 表示成功，返回 1 表示失败。这里不要套用“非零为真，所以成功”的习惯。STREX 返回的是状态，不是写入的数，也不是内存里的旧值。

## 让失败路径可以在 PC 上稳定验证

真正指令只能在支持它的目标上执行。PC 例子使用显式状态模型，让每个动作都按 C99 顺序发生，不用数据竞争来赌失败时机：


```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { uint32_t value; int tagged; } model_t;

static uint32_t load_exclusive(model_t *m)
{
    m->tagged = 1;
    return m->value;
}

static unsigned store_exclusive(model_t *m, uint32_t next)
{
    unsigned failed = m->tagged ? 0u : 1u;
    m->tagged = 0;
    if (failed == 0u) { m->value = next; }
    return failed;
}

int main(void)
{
    model_t m = {10u, 0};
    uint32_t old = load_exclusive(&m);
    m.tagged = 0;              /* model exception entry */
    ++m.value;                /* model ISR update */
    assert(store_exclusive(&m, old + 1u) == 1u);
    assert(m.value == 11u);
    old = load_exclusive(&m);
    assert(store_exclusive(&m, old + 1u) == 0u);
    assert(m.value == 12u);
    puts("first=fail, retry=success, value=12");
    return 0;
}
```

先注释掉异常清标记和 ISR 自增两行，再预测：首次尝试就会成功，值变为 11。模型只复现我们指定的本地标记协议，没有模拟总线监视范围、缓存或多核互连；它不能作为某芯片支持跨主设备原子访问的证据。

## 在 F407 上写一个最小计数器操作

以下板级函数依赖设备头文件与 CMSIS，适用于特定 Cortex-M4 编译目标。对象必须自然对齐、位于适当的普通 SRAM，生命周期有效；它不是 ISO C99 自带的语言原子函数。


```c
#include "stm32f407xx.h"

uint32_t fetch_inc_m4(volatile uint32_t *p)
{
    uint32_t old;
    do {
        old = __LDREXW(p);
    } while (__STREXW(old + 1u, p) != 0u);
    return old;
}
```

输入 `*p=10`，若无其他更新，函数返回 10，最终保存 11。若中间发生一次上表中的 ISR 更新，重试成功后返回 11，最终保存 12。无符号计数到最大值后会按模回绕；如果你的业务要求饱和或溢出报警，需要另写协议，不能把位宽问题交给独占指令。

函数没有声称“发布其他数据”。它只对这个计数对象做读改写，不自动建立缓冲区内容与就绪标志之间的顺序。若计数还承担释放/获取数据的职责，要进一步设计屏障和所有权，或者在支持 C11 的工程中使用适当语言原子操作。

配套的 `examples/notes/core/board_snippets.c` 包含此函数。用 ARM GCC 针对 Cortex-M4 生成汇编后，可以看到 LDREX、加法、STREX、判断结果并重试的序列；汇编中出现这些指令证明选择了对应原语，不证明所有并发路径都遵守协议。

## 独占标记并不把地址锁起来

LDREX 不让 CPU 一直占着总线，也不阻止其他执行者访问地址。它记录的是本次尝试的资格。PM0214 列出的本地标记清除条件包括：执行 CLREX、执行 STREX（无论成功失败）、发生异常。

因此失败不一定说明计数器真的改变了：即便一个 ISR 什么也不写，只要异常打断这对指令，后续尝试也可能需要重来。程序不能把失败次数直接解释为“冲突写入次数”。

配对必须使用相同地址与数据宽度。读一个字后用半字独占写，或者把读 A、写 B 拼成一对，不是合法的替代算法。如果在 LDREX 后决定放弃本次尝试，使用 CLREX 放弃本地标记；别把它留给完全不同的后续流程猜测。

读与写之间应只保留必要的短计算，不调用耗时函数，不记录日志，不把复杂处理塞进重试窗口。这不仅减少失败，也让你能清楚审核被重复执行的动作。窗口里若发送过一次命令，重试就可能重复发送；要重试的是无外部副作用的计算，而不是整段业务。

## 不能拿它修好的三个问题

**DMA 不是遵守你这把锁的线程。** F407 的 DMA 是独立总线主设备；不要假设它的写入会按你的软件协议清理 CPU 本地独占标记。更不能让 DMA 一边写缓冲区、CPU 一边独占访问索引，就认为整个数据块一致。要靠完成状态、所有权移交和设备协议。

**外设寄存器不是普通锁变量。** 对有读写副作用、W1C 或命令语义的 MMIO 地址，别套用 SRAM 计数算法。GPIO 位控制用 BSRR；状态标志按外设手册应答。

**无锁计数不等于可以在 ISR 自旋等锁。** 假设主循环已持有一个软件锁，ISR 抢占后在锁上忙等；主循环不可能在 ISR 返回前解锁，ISR 又不肯返回，程序就死锁了。WFE 或多加几条屏障也不会创造这个被打断的解锁机会。

即使只是这里的重试计数器，也没有固定的最大重试次数保证。高频抢占可以让某次调用很晚才成功。硬实时路径若必须给出最坏执行时间，应考虑短临界区、单写者、有限尝试后延后处理等设计，而不是只看无冲突时有几条指令。

## 练习：失败后还能用原来的 old 吗

初值 100。主循环独占读出 100，中断把计数器加到 103，随后主循环 STREX 失败。有人重新 LDREX 但丢弃新读数，仍写 101。这个版本即使第二次 STREX 成功，结果正确吗？

<details><summary>展开答案</summary>

不正确。新 LDREX 只建立了新一次尝试的资格，没有让旧的计算结果自动更新。必须使用新读出的 103 重新计算，成功写回 104。正确协议同时要求“重新获取资格”和“基于最新读数重算”，不能只盯着返回码。

</details>

## 参考与验证边界

本篇以 STM32F407/Cortex-M4 为例，使用 C99 加 CMSIS 编译器扩展。主机模型与 ARM 编译是两种不同证据；本轮没有在真板上注入异常并测量重试次数，也没有验证多核或 DMA 独占互操作。

- [PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.2.7–2.2.8 同步原语和标记清除；§3.4.8–3.4.9 LDREX/STREX、CLREX。
- [CMSIS GCC 内联实现](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/cmsis_gcc.h)：__LDREXW、__STREXW、__CLREX 的操作数与状态返回。

回到[5.4 共享数据](/lessons/5-4)；相关分支：[读改写与原子性](/notes/read-modify-write)、[内存屏障](/notes/memory-barriers)、[序号锁与双缓冲](/notes/sequence-double-buffer)。
