---
title: 【原理】读改写与原子性
description: 用一次确定发生的丢计数和 GPIO 位操作，区分单次访问、复合操作与临界区的保护范围。
sourceKey: 【原理】读改写与原子性
---

# 读改写与原子性

主循环给计数器加了一次，中断又加了一次，最后却只增加一。变量已经加了 `volatile`，关闭优化也能遇到。先别换编译选项：问题可能不是“没去读内存”，而是两个人分别读到了同一份旧值。

沿用[5.4 中断与主循环共享数据](/lessons/5-4)的场景，本篇把抢占位置固定下来，再把同一问题移到 GPIO。这样不用靠按键时机碰运气，也能解释为什么一条 C 语句不一定是一件不可打断的事。

## 先让丢失的一次加法必然发生

下面是可在 PC 上运行的 C99 顺序模型，不创建线程，不访问外设，因此没有把 C 数据竞争拿来当实验。每条语句代表主循环或 ISR 的一个动作。运行前预测最后是 11 还是 12。


```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t shared = 10u;
    uint32_t local = shared;  /* main: load */
    shared = shared + 1u;     /* ISR: complete increment */
    local = local + 1u;       /* main: calculate */
    shared = local;           /* main: store old result */
    assert(shared == 11u);
    printf("final=%lu, expected=12\n", (unsigned long)shared);
    return 0;
}
```

输出是 `final=11, expected=12`。ISR 的写入不是没发生；它已把内存改成 11，随后主循环又把基于旧值 10 算出的 11 写了回去。加多少次 `volatile` 都不会改变这条时间线。

把全过程记成三步：读出旧值、在寄存器里修改、写回。这叫**读改写**，常见表达式有 `n++`、`flags |= mask`、`flags &= ~mask`。编译器可能用不同指令实现，但 C 源码只有一行并不能证明它只产生一次内存事务。

## “原子”必须带上观察者和操作范围

在这里说一次操作原子，意思是：对我们考虑的并发执行者而言，看不到它的中间状态。这个承诺必须同时说明目标、访问宽度、对齐、编译器和并发来源。

对 STM32F407 的普通 SRAM，一个自然对齐的 32 位对象，若被编译成单条字加载或存储，可以作为主循环与中断之间不撕裂的一次访问讨论。它不等于下面三项也原子：

- 先读后写的 `n++`；中断可以插在两条访存指令之间。
- 一次复制两个 32 位字段；两个字段可能来自不同版本。
- 64 位值或未对齐对象；不能把字访问的结论直接套过去。

这里还没有证明任意外设总线、DMA 或其他 CPU 的行为，也没有把硬件 ISR 模型转换成 ISO C 的线程内存模型。PC 多线程应使用语言原子或互斥量；不要把两个线程同时修改普通变量再输出“正确概率”。

## 把整个读改写放进短临界区

在 F407 的特权代码里，若共享计数器的写者只有主循环和可屏蔽中断，可以临时阻止这些中断进入。以下板级片段依赖工程的 `stm32f407xx.h` 及其 CMSIS 头文件，放进自己的模块，不需要改启动文件：


```c
#include "stm32f407xx.h"

void count_add(volatile uint32_t *count)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    *count = *count + 1u;
    __set_PRIMASK(saved);
}
```

读旧值必须在关闭中断之后，恢复中断必须在写回之后。若中断恰好在读取 PRIMASK 与关闭中断之间到来，也没有问题：此时尚未读取计数器；中断返回后再完整执行受保护的更新。

退出时恢复 `saved`，而不是无条件 `__enable_irq()`。外层调用者可能本来就在临界区内：内层进入时保存的是 1，退出仍然恢复 1，外层的保护不会被提前解除。本篇使用的 GCC CMSIS 实现还给这些修改屏蔽寄存器的内联汇编附带了编译器内存约束；不要用一个没有约束的自制汇编宏随意替代它。

这个办法保护的是一段执行，不是冻结整块芯片。PRIMASK 不屏蔽 NMI 和 HardFault，也不停止 DMA；若这些路径也修改对象，假设便已失效。临界区里不等串口、不延时、不做大块复制，否则会把“数据正确”换成“中断迟到”。先在区外计算，在区内只更新必须一起生效的状态。

## GPIO 为什么有专门的 BSRR

假设主循环管理 PA5，ISR 管理 PA0，初始 ODR 为零。主循环读出 ODR=0，准备置 PA5；ISR 先置 PA0；主循环再写回自己的旧副本 `0x20`。PA0 被无意清掉了。即使两边操作不同的位，只要都做整字读改写，仍会相互覆盖。

F407 的 GPIO BSRR 把“置哪些位、清哪些位”编码为一次写命令：


```c
/* Assumes GPIOA clock, modes and wiring are already configured. */
GPIOA->BSRR = 1u << 5;           /* set PA5 */
GPIOA->BSRR = 1u << (5u + 16u);  /* reset PA5 */
```

低 16 位写 1 请求置位，高 16 位写 1 请求复位，写 0 对该位无影响。这不是先读 ODR 再覆盖整个 ODR，所以不同位的命令不会携带其他位的旧副本。

试着在纸上模拟：初始 ODR=`0x01`，向 BSRR 写 `0x00000020`，得到 `0x21`；再写 `0x00200000`，得到 `0x01`。芯片手册把它描述为单次原子 AHB1 写访问。若同一次 BSRR 写入同时要求设置和清除同一位，F407 规定设置优先；工程里通常应避免这种相互矛盾的命令。

仍有三个边界：BSRR 不锁住输出，别处之后仍可覆盖 ODR；两边都改变同一引脚时，最终状态仍取决于先后顺序；“先读 ODR 再决定翻转”依然含一个决策窗口，BSRR 没有把任意 toggle 算法自动变原子。

## 别把所有寄存器当成普通 RAM

对配置寄存器，读改写可能是保留其他字段所必需的。对写 1 清除的状态寄存器，`|=` 却会把刚读到的其他标志一起清掉。对只写命令寄存器，回读值也未必是上次写入值。

因此先问“这个写入的语义是什么”，再问“如何使它原子”。BSRR 的直接赋值不能机械推广成“全部寄存器都用赋值”；临界区也不能修复错误的清标志协议。具体反例见[中断风暴与忘清标志](/notes/interrupt-storms)。

## 练习：保护的究竟是哪一步

初始 `flags=0`，主循环要设置 bit 1，中断要设置 bit 0。主循环已读出 0，中断随后完成设置。请写出主循环最后的错误值。若在主循环读出旧值之后才关闭中断，能否修好？GPIO 上两个独立输出位又该如何处理？

<details><summary>展开答案</summary>

错误值为 2，而预期是 3。读出 0 后再关中断已经太晚：ISR 的 bit 0 已经存在于内存，但主循环还持有过期副本。应把整个读改写包含在保护内，或重新设计成单写者。对 F407 GPIO 的独立置位请求，两边直接写各自的 BSRR 位即可；仍不得有第三处使用旧的 ODR 整字副本覆盖它们。

</details>

## 适用范围与核对依据

本篇的主机例子是确定性交错模型，已纳入 `examples/notes/core/test.c`；板级函数只做 ARM 编译验证，不代表真板测得丢失次数或中断延迟。所有硬件结论以 STM32F407、Cortex-M4 和对应工具链为边界。

- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：§8.3.5、§8.4.7，GPIO 位操作与 BSRR。
- [PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.1.3，PRIMASK 与异常屏蔽；§2.2.7，同步原语。
- [GCC 的 volatile 说明](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)：普通内存访问不因旁边存在 volatile 就获得同步保证。

回到[5.4 共享数据](/lessons/5-4)；继续阅读[LDREX 与 STREX](/notes/exclusive-access)和[序号锁与双缓冲](/notes/sequence-double-buffer)。
