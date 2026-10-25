---
title: 【拓展】中断风暴与忘清标志
description: 通过 EXTI 写一清零的状态模型，定位反复进中断、误清其他事件和回调重复清标志的问题。
sourceKey: 【拓展】中断风暴与忘清标志
---

# 中断风暴与忘清标志

按钮只按了一下，主循环的 LED 却像停住了。暂停 CPU，几乎每次都落在同一个中断函数里；继续运行，下一次暂停还是那里。先不要怀疑主循环被优化掉：可能是中断处理函数一直在返回，却每次刚返回就又有同一个请求等待处理。

这个现象叫中断风暴。本篇围绕 STM32F407 的 EXTI 展开，区分三个原因：源标志没清、真实事件不断到来、处理速度赶不上事件。它们看起来相似，修法却完全不同。

## 把“请求”拆成两张便条

在[5.1 中断入门](/lessons/5-1)的按键链路中，EXTI 的 PR 位记录所选边沿已出现；NVIC 记录某个 IRQ 等待处理。前者属于外设，后者属于中断控制器。

CPU 接受异常时，NVIC 的状态会从 pending 转为 active，但这不等于外设已完成应答。若源请求仍然有效，IRQ 可以再次挂起。只反复调用 `NVIC_ClearPendingIRQ`，无法替 EXTI、UART、定时器等外设完成各自的清除流程。

这就像门口的人仍按着门铃，你只擦掉屋里的“有人来过”便条；门铃马上又把便条写满。应当根据外设手册处理源头，而不是每次退出前清空所有 NVIC 标志。

## 一个不会误触真实寄存器的实验

RM0090 将 EXTI_PR 标为 `rc_w1`：读到 1 表示有请求，写入 1 清除相应标志，写 0 不改变它。这与普通 RAM 的赋值语义相反。

假设 bit 0、bit 1 都已挂起，你只处理了 bit 0，正确结果应该还留着 bit 1。用 C99 顺序模型演算：


```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t write_one_to_clear(uint32_t state, uint32_t command)
{
    return state & ~command;
}

int main(void)
{
    uint32_t before = 3u;
    uint32_t good = write_one_to_clear(before, 1u);
    uint32_t bad_or = write_one_to_clear(before, before | 1u);
    uint32_t bad_and = write_one_to_clear(before, before & ~1u);
    assert(good == 2u);
    assert(bad_or == 0u);
    assert(bad_and == 1u);
    printf("direct=%lu, or=%lu, and=%lu\n",
           (unsigned long)good, (unsigned long)bad_or,
           (unsigned long)bad_and);
    return 0;
}
```

预期输出 `direct=2, or=0, and=1`。注意这里的函数是在模拟“寄存器接到写命令后的状态”，不是让 PC 访问 MCU 地址。

对应到 F407：


```c
EXTI->PR = 1u << 0;     /* correct for clearing EXTI line 0 */
/* EXTI->PR |= 1u << 0;    WRONG: may clear other pending bits */
/* EXTI->PR &= ~(1u << 0); WRONG: may leave bit 0 pending */
```

`|=` 先读回全部 1，再把它们一起写回；硬件将这些 1 全当成清除命令。另一种“把该位清零”的习惯写法，则写了 0 给想清的位，却写了 1 给别的位，恰好做反。关中断也修不好这个语义错误。

## 共享入口：只应答你确实处理的线

F407 的 EXTI5–9 共用 EXTI9_5_IRQHandler。仅清 line 5 而忽略同入口的 line 7，line 7 的请求仍可能让处理函数再次进入。反过来，为省事直接写全 1，又会丢掉别的模块拥有的事件。

以下是**不用 HAL EXTI 分发时**的板级示例。仅在独立实验中由同一模块拥有 line 5、7，二者已完成 GPIO、SYSCFG、触发沿、IMR 和 NVIC 初始化；用它替代原有的同名入口，而不是再定义第二份。


```c
#include "stm32f407xx.h"

static volatile uint32_t line5_count;
static volatile uint32_t line7_count;

void EXTI9_5_IRQHandler(void)
{
    const uint32_t owned = (1u << 5) | (1u << 7);
    uint32_t seen = EXTI->PR & EXTI->IMR & owned;
    EXTI->PR = seen;          /* acknowledge the observed sources */
    if ((seen & (1u << 5)) != 0u) { ++line5_count; }
    if ((seen & (1u << 7)) != 0u) { ++line7_count; }
}
```

设输入快照为 line 5 和 line 7 都挂起，执行一次后两个计数各加一。计数在 ISR 中只由这一个入口写；主循环若需要读两个计数的一致快照，仍要使用临界区等方法。只读各自独立的累计数，与要求二者属于同一时刻，是不同需求。

这里先应答，再做极短的处理，缩小了旧请求保留的时间。但它不是无损事件队列：同一 PR 位已经为 1 时再次到来的同类边沿可能合并；若新边沿恰在读快照与清除之间到来，也不能凭一个位区分“旧的一次”和“新的一次”。精确计数应考虑硬件计数器或其他事件采集机制。

## 使用 HAL 时，别在回调里再清一次

课程主线的调用链是入口调用 HAL_GPIO_EXTI_IRQHandler，HAL 检查 PR、清除对应位，然后调用 HAL_GPIO_EXTI_Callback。源代码里清除发生在回调之前。

如果你在回调末尾又执行一次清除，在回调期间刚到来的新事件就可能被第二次清除吃掉。因此选一种所有权方式：

- 使用 HAL：入口正确分发给 HAL，业务回调只处理被传入的 GPIO_Pin，不重复应答。
- 自己写寄存器入口：自己负责源判断、清除和共享 IRQ 分发，不再让 HAL 对同一事件清一遍。

不要因为换成了回调就认为自己离开了中断上下文。回调仍占着 ISR 的执行时间，日志格式化、等待串口和 HAL_Delay 仍不应放进去。

## 用证据区分三种风暴

第一次定位时，不要在 ISR 每次打印一整行。115200、8N1 下 20 字节仅线上的发送时间就约 1.74 ms，足够把 1 ms 周期的实验本身变成过载。

更轻的观察办法是让 ISR 只增加入口计数，主循环周期性打印；或用一个已确认安全的 GPIO 在入口置位、出口复位，拿逻辑分析仪观察占空比。当前没有真机，这些是实验步骤而不是已测波形。

1. **源一直为 1**：核对状态寄存器访问语义、清除顺序，以及是否漏了共享入口中的一个源。
2. **源能清掉但快速重来**：查看物理输入是否抖动或悬空，触发沿是否选错；软件清标志不能替代输入整形和消抖。
3. **请求确实周期性到来，但处理更慢**：缩短 ISR，把工作移到主循环/任务，用队列明确满时策略。把优先级一律调高只会让其他任务更饿。

调试器也会干扰观察：CPU 停住时外设是否冻结取决于调试配置；某些外设寄存器的读取本身有副作用。暂停后看到的一个状态，不等于暂停前的完整时序。

## 练习：哪个“修复”会丢事件

EXTI0 的 HAL 回调开始时旧事件已清掉，回调期间又来一个下降沿。A 方案在回调尾部写 PR0=1；B 方案只记录软件事件，交给之后的正常中断入口处理新请求。哪个可能吃掉第二次边沿？若 line 0 在清前已经发生三次边沿，能从 PR0=1 恢复出次数吗？

<details><summary>展开答案</summary>

A 的第二次清除可能把回调期间新置位的 PR0 清掉。B 不在业务层重复应答，但仍要承认硬件 pending 位会合并事件。PR0=1 只能说明至少出现过触发请求，不能恢复三次的精确次数；软件计数统计的是被成功服务的次数，而不必然等于物理边沿总数。

</details>

## 适用范围与依据

配套模型收入 `examples/notes/core/test.c`；裸寄存器 ISR 经过 F407 头文件的交叉编译，不是完整固件，没有上板验证。其他外设可能写 0 清除、读数据后清除或要求特定读写序列，不能把 EXTI 的 W1C 规则推广给所有标志。

- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：§12.3.6 EXTI_PR；第12章共享中断与边沿请求。
- [ST HAL GPIO 源码](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_gpio.c#L492-L500)：HAL_GPIO_EXTI_IRQHandler 先清标志再调用用户回调。
- [PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.3.2、§2.3.7，active/pending 状态与异常尾链。

回到[5.1 中断入门](/lessons/5-1)；继续看[中断与 NVIC](/notes/interrupts-nvic)、[读改写与原子性](/notes/read-modify-write)、[按键消抖](/notes/button-debounce)。
