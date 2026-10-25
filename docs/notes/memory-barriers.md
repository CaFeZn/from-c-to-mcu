---
title: 【拓展】内存屏障与指令重排
description: 从先填数据再公布就绪的协议出发，区分编译器约束、DMB、DSB、ISB 与缓存一致性。
sourceKey: 【拓展】内存屏障与指令重排
---

# 内存屏障与指令重排

主循环先填好两个数据字，最后把 `ready` 设为 1。中断看到 ready=1 就使用数据。源码顺序看起来无懈可击，但“C 源码在前面”与“接收方已经能观察到”并不是一句话。

先不要在每一行之间插 DSB。我们要回答三个不同的问题：编译器可以如何安排指令？处理器与总线何时让访问生效？数据被读完之前，生产者是否还拥有再次改写它的权利？屏障主要约束前两项，第三项必须由协议解决。

## 用一次性交接把顺序讲清楚

设 F407 主循环只发布一份固定数据，唯一消费者是一个 ISR。对象在普通 SRAM，所有状态在开放中断前初始化；消费者不会抢先读 payload，生产者发布后不再改它。此处不涉及 DMA、缓存或其他 CPU。

期望的时间关系是：


```text
producer: write word[0]=10 -> write word[1]=20 -> publish ready=1
consumer: observe ready=1 -> read word[0], word[1] -> obtain (10,20)
```

如果把“公布 ready”挪到两次数据写入之前，中断就可能获得 `(0,0)` 或半份数据。这不是计算错误，而是公告早于货物。即使你没有手工挪动，仍需要用工具链和架构都认可的方式表达顺序要求。

下面是一个受限的板级教学实现，依赖 STM32F407 设备头文件和 CMSIS。它展示的是一次性发布，不是可以无限复用的队列：


```c
#include "stm32f407xx.h"

typedef struct {
    uint32_t word[2];
    volatile uint32_t ready;
} publication_t;

void publish_once(publication_t *p)
{
    p->word[0] = 10u;
    p->word[1] = 20u;
    __DMB();
    p->ready = 1u;
}

int receive_once(const publication_t *p, uint32_t out[2])
{
    if (p->ready == 0u) { return 0; }
    __DMB();
    out[0] = p->word[0];
    out[1] = p->word[1];
    return 1;
}
```

初始化时令整个对象为零，先开放消费者中断，再在主循环调用一次发布函数。消费者要么返回 0，不使用输出；要么返回 1，复制出 `(10,20)`。输入中断可发生在任意步骤，但只要还没公布 ready，消费者不会读普通数据。发布完成后数据不再变化，读取不需要与第二次写入竞争。

这里依靠 GCC/CMSIS 对嵌入式中断及屏障的实现约定，不是“普通字段加 volatile 标志即可移植到 C11 线程”的证明。PC 线程应使用 release/acquire 原子或互斥量等语言同步设施，不能把此片段中的 MCU 宏移植过去就宣称消除了数据竞争。

## 先检查编译器，再检查 CPU

GCC 文档明确指出，普通对象的访问不因旁边有 volatile 对象就自动获得顺序。volatile 约束目标对象的可观察访问，但既不是所有普通内存的编译器屏障，也不是复合读改写的原子保证。

CMSIS 的 GCC `__COMPILER_BARRIER()` 展开为带 `memory` clobber 的空内联汇编。它不一定生成机器指令，而是告诉编译器：这里不能把相关内存操作随意跨过去。它没有向总线发送“全部完成”的命令。

核对过的 CMSIS `__DMB()`、`__DSB()`、`__ISB()` 实现不仅发出目标指令，也含 memory clobber，因此本例同时表达编译器层与处理器层的边界。若自己写一条没有适当约束的内联汇编，汇编文字看起来相同，编译器可做的移动却可能不同。

可跟做的编译实验是：对配套 `board_snippets.c` 使用 `-mcpu=cortex-m4 -mthumb -O2 -S`，找到 publish_once，检查 payload 写入、dmb 与 ready 写入的位置。再看仅含 `__COMPILER_BARRIER()` 的函数，通常看不到额外 CPU 指令。

这里本轮实际做的是交叉编译与汇编核对，没有测量“去掉屏障后必然读错”。去掉屏障仍未观察到错误，不能证明协议在所有优化与目标上成立；不同处理器、访问属性和等待状态会改变可观察行为。

## DMB、DSB、ISB 各解决哪一步

**DMB：约束内存访问的先后观察顺序。** 对屏障两侧的相关显式内存访问建立顺序，常用于数据与公告、描述符与启动动作之间。不要把它描述为“CPU 在这里等所有事情结束再执行任何指令”；后续非内存指令不属于这种一概等待的保证。

**DSB：等待此前显式访问完成，再继续后续指令。** 当接下来的动作确实要求先前操作完成时，用它承担更强的同步要求。它也不是等待外设内部整个业务结束：把启动命令写到寄存器完成，不代表 ADC 已转换完、UART 已发完。那些要读状态或等待完成事件。

**ISB：让后续指令按更新后的执行环境取指和执行。** 修改影响执行上下文的系统配置时，可能需要重新同步指令流。它不是“所有变量刷新一下”，不能替代数据发布中的 DMB。

例如更换向量表涉及先把表内容准备好，再公布表基址，并同步后续异常环境；修改正在 pending/active 的异常优先级也有架构规定的同步要求。应遵从对应手册流程，不从这几个缩写推导出一条适用于所有寄存器的固定口诀。

F407 虽然不是桌面乱序执行 CPU，内存系统仍有不同存储属性、多个接口及可能缓冲的访问。不能由“单核、流水线简单”推出“源码顺序就是所有观察者看到的完成顺序”。

## 屏障不能补上的三块拼图

第一块是**互斥**。两个执行者同时做 n++，中间加 DMB 仍可能丢更新。屏障没有阻止另一个执行者插队，也没有把三步操作合成一次原子事务。

第二块是**缓存可见性**。STM32F407 没有 Cortex-M7 那样的 D-cache，不能把别的系列的 cache clean/invalidate 代码当成 F407 必需模板。反过来，移植到带数据缓存的芯片，仅有 DMB 也不代表脏缓存行已经写回、DMA 写入已经被 CPU 重新读取；要按该平台做缓存维护和地址范围、行对齐处理。

第三块是**地址可达性与所有权**。F407 DMA 不能访问 CCM。把缓冲放进 CCM，再加十次 DSB，也不会增加一条 DMA 总线路径。DMA 正在使用的缓冲被 CPU 改写，同样不是屏障能修好的问题；要等完成信号、交还所有权，再复用。

同理，上面的 publish_once 不能简单放进循环。如果消费者还在复制，生产者就写第二份，两次屏障也挡不住半新半旧。要增加消费完成确认、序号验证或双缓冲协议，见[序号锁与双缓冲](/notes/sequence-double-buffer)。

## 练习：先把边界说清楚

生产者写好一个 DMA 描述数据，执行 DMB，再写启动寄存器。之后立即修改原缓冲区，理由是“屏障已经保证同步”。判断这句话的错误。把 DMB 换成 DSB 是否自动解决？

<details><summary>展开答案</summary>

屏障只能处理规定范围内的访问顺序或完成条件，不代表 DMA 已读完缓冲区。启动寄存器写完与传输业务完成是两件事。换成 DSB 也不能替代 DMA 完成事件和所有权回收；还要确认 DMA 能访问缓冲区、长度地址正确，带缓存平台还要处理缓存维护。本篇一次性示例之所以简单，正是因为发布后没有第二个写者再改数据。

</details>

## 适用范围与技术参考

配套主机测试只检查合法/非法发布时间线的逻辑；ARM 编译检查核对屏障指令与布局，不是总线时序仿真。本篇没有真机、DMA 或跨核一致性实测，不能据此给出性能成本数字。

- [C11 委员会草案 N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：§5.1.2.4 第25段 的线程数据竞争规则；此处用于说明不能把 MCU 中断协议直接当成标准线程同步。
- [PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.2.2–2.2.4，内存属性与软件顺序；§3.11 的 DMB、DSB、ISB 指令条目，正文页183–185。
- [GCC volatile 说明](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)：普通访问与 volatile 之间不自动有序。
- [CMSIS GCC 实现](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/cmsis_gcc.h)：编译器屏障与三种 CPU 屏障。
- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：§2.1 总线结构和 §2.3.1 SRAM/CCM 的访问范围。

回到[5.4 共享数据](/lessons/5-4)；继续看[读改写与原子性](/notes/read-modify-write)、[序号锁与双缓冲](/notes/sequence-double-buffer)、[DMA 工作原理](/notes/dma-principles)。
