---
title: 【原理】volatile与编译器优化
description: 用同一段代码的输出和汇编区分可省略的普通访问、volatile访问，以及仍需单独处理的原子性与同步。
sourceKey: 【原理】volatile与编译器优化
---

# volatile 与编译器优化

你在调试版里等一个按键标志，一按就继续；换成优化版，程序却一直转圈。在循环里加一句日志又好了。先别得出“优化器不可靠”的结论：日志可能只是改变了编译器能证明的事情，并没有修复代码的访问约定。

本篇接着 [5.1 中断入门](/lessons/5-1) 和 [3.2 调试](/lessons/3-2)，先在电脑上看一个不会卡死、也不制造数据竞争的实验，再回到硬件。我们要分清三件事：**是否保留访问、访问是否不可分割、多个访问如何同步**。它们不是同一个问题。

## 先让两份程序输出一样，再看它们做了几次读取

下面两个函数都把指针指向的数加四次。先预测：对象一直是 7 时，两个结果各是多少？编译器一定要读取四次吗？

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint32_t sum_plain(const uint32_t *p)
{
    uint32_t total = 0;
    for (unsigned i = 0; i < 4; ++i) {
        total += *p;
    }
    return total;
}

uint32_t sum_volatile(const volatile uint32_t *p)
{
    uint32_t total = 0;
    for (unsigned i = 0; i < 4; ++i) {
        total += *p;
    }
    return total;
}

void write_plain(uint32_t *p)
{
    *p = 1;
    *p = 2;
}

void write_volatile(volatile uint32_t *p)
{
    *p = 1;
    *p = 2;
}

int main(void)
{
    uint32_t ordinary = 7;
    volatile uint32_t observed = 7;
    assert(sum_plain(&ordinary) == 28);
    assert(sum_volatile(&observed) == 28);
    write_plain(&ordinary);
    write_volatile(&observed);
    assert(ordinary == 2 && observed == 2);

    /* 用顺序语句模拟读改写被插入一次更新，不是真正的中断。 */
    volatile uint32_t counter = 0;
    uint32_t saved = counter;
    counter = counter + 1;
    counter = saved + 1;
    assert(counter == 1);
    puts("sums=28,28 writes=2,2 interleaved=1");
    return 0;
}
```

将上面程序保存后，用 C99、警告和断言运行；仓库配套源可以直接这样编译。在仓库根目录执行，Windows 上运行相应的 可执行文件：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic-errors -O2 examples/notes/c-semantics/volatile-optimization.c -o volatile-demo.exe
./volatile-demo.exe
```

这里使用可提供 `uint32_t` 的实现，PC 和本课程的 ARM 工具链都满足。预期固定输出是：

```text
sums=28,28 writes=2,2 interleaved=1
```

两个求和都是 28，两个最终值都是 2，但这不意味着中间的机器访问一样。再生成汇编，不必运行硬件：
```sh
gcc -std=c99 -O2 -S examples/notes/c-semantics/volatile-optimization.c -o volatile-demo.s
```

寻找 `sum_plain`、`sum_volatile`、`write_plain` 和 `write_volatile` 函数。别只看 `main`：优化器可能把普通函数调用内联、直接算出断言结果。

本例使用 MinGW GCC 6.3.0 实际编译运行了 `-O0` 和 `-O2`，两次输出相同；另用 ARM GCC 12.3.1 为 Cortex-M4 生成汇编。在所检查的 `-O2` 结果中，普通求和只需一次加载再乘四，普通写入只保留写 2；带 `volatile` 的求和保留四次读取，写入保留先写 1、再写 2。**具体寄存器分配、指令条数不是 C 标准保证，访问语义才是。**

## 编译器为什么敢把“先写 1”删掉

C 语言描述一台抽象机器，不要求每一行源代码恰好变成一条指令。在普通对象上，只要程序可观察到的行为不变，编译器可以复用已读取的值、折叠计算、去掉不会被观察到的中间写入。对本例的普通 `p`，函数执行期间没有合法的外部修改需要被当成硬件副作用处理，因此最终写 2 就够了。

但外设状态和中断标志不是“只有当前这段普通 C 代码会改”的对象。`volatile` 限定要求实现按照它规定的 volatile 访问规则处理相关访问，不能把有意义的访问当成普通冗余操作消除。C99 的 6.7.3 同时明确：**什么构成一次访问，由实现定义。** 因此“每次用都读内存”是便于入门的缩写，不是针对所有表达式、所有 CPU 的逐字保证。

例如 `sizeof observed` 不会为了求对象大小去读取这个非 VLA 对象；GCC 对 `x = (observed = 2);` 也不要求赋值后再读回 `observed`。若确实要读寄存器反馈，应在下一条完整表达式里显式读取。多个 volatile 操作在一个表达式内的求值顺序，还要遵守 [参数求值顺序](/notes/evaluation-order)，不能靠限定符凭空确定。

## 回到中断和寄存器：只补它负责的那一块

在本课程 Cortex-M4、相应编译器的 ISR 约定下，单个中断标志常声明为 `volatile`，由中断修改，主循环重新检查。MMIO 则使用芯片设备头文件提供的寄存器声明，不要自己通过非 volatile 指针绕开它。至于地址、访问宽度、时钟、读清零或写一清零规则，仍需按 [寄存器与内存映射](/notes/memory-mapped-io) 核对。

这不是让你在 PC 上拿线程偷偷改一个普通变量，期待本例模拟中断。ISO C99 没有 C11 那套线程内存模型；C11 下两个线程无同步地冲突访问同一非原子对象，至少一个写入时可能构成数据竞争，`volatile` 不会消除这种未定义行为。线程应使用语言原子操作或同步库；裸机 ISR 则要核对工具链和目标平台的保证。

尤其记住下面三个边界。

### 边界一：读改写仍然可以丢更新

实验最后三行已经给了反例模型：主循环先记住 0，中途“中断”将计数写成 1，主循环再用自己的旧快照写回 1。两次加一的意图只留下了 1。所有读取和写入都可以真的发生，问题仍然存在。

真实 `counter++` 通常包含读、计算、写，不因为一行代码就不可分割。必要时使用目标平台正确的临界区或合适的原子操作；一次对齐的单字访问也不能推导出一个结构体快照、一个读清零事务同样安全。具体方案回到 [5.4 共享数据](/lessons/5-4)。

### 边界二：volatile 标志不是发布普通缓冲区的屏障

下面是**同步设计反例片段**，不是可直接移植的线程通信方案：
```c
/* 假定 payload 是普通对象，ready 才是 volatile。 */
payload = value;
ready = 1;
```

GCC 官方说明，普通对象访问并不因相邻的 volatile 访问就获得所需的排序保证。接收方看见 `ready` 并不自动建立“普通 payload 已正确发布”的跨上下文协议。只把标志加上 `volatile`、不讨论所有权、编译器排序和目标内存顺序，证明是不完整的。

编译器屏障和 CPU 内存屏障还不是同一种东西；“加一条屏障”也不能修复 C11 线程数据竞争。这里不提供脱离平台的万能屏障宏。应使用已论证的队列、临界区或原子发布方案。

### 边界三：它不是缓存刷新，也不是定时器

`volatile` 不保证读到“整个系统绝对最新”的值，不会替你刷新缓存，也不提供精确延时。空循环即使保留下来，时钟、指令和总线等待都会影响耗时；需要时间基准就用定时器。

本篇也没有用 `volatile` 解决 DMA 缓存一致性问题。移到带 D-cache 的目标，缓存维护还需按芯片和 DMA 方案单独处理，不能把某块板上“碰巧能工作”的经验直接推广过去。也不要给整个程序所有变量加 `volatile`：那会限制有效优化，却仍可能保留越界、非法别名或生命周期错误。

## 练习：把“足够”拆成三个判断

现有 `volatile uint32_t events`，ISR 执行 `events++`，主循环先读出它，再写零。请回答：加 `volatile` 后是否还可能丢事件？换成两个相邻 volatile 字段是否天然得到一致快照？循环里加日志能否作为正确性修复？

<details><summary>展开答案</summary>

都不能作这样的保证。读计数和清零之间若有 ISR 加一，新事件可能被清掉；两个字段分两次读取时，中断可以落在两次之间；日志只改变时序和编译器可见性，甚至掩盖问题。先明确是否允许事件合并，再选择单写者累计计数、受保护的取走操作等方案。`volatile` 仅处理应保留的访问，不替这些协议负责。

</details>

## 适用范围与技术依据

本文可执行部分是 C99、顺序程序，没有运行未定义行为。PC 断言和 ARM 编译/汇编检查都不等于上板验证；文中的中断插入是手工顺序模型，未声称真机时序测试。

- [WG14 N1256（C99 合并技术勘误稿）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：5.1.2.3 的抽象机与副作用，6.7.3 第 5–6 段与脚注 115–116 的 volatile 访问规则。
- [GCC：When is a Volatile Object Accessed?](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)：volatile 的访问边界、赋值不隐含读回、不能替代普通内存排序屏障。本文实际编译器版本已在实验中列明，在线手册是滚动版本。
- [WG14 N1570（C11 委员会草案）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：5.1.2.4 第 25 段的数据竞争规则；这是 C11 补充说明，不把线程原子 API 当成 C99 功能。

继续阅读：[const 与 restrict](/notes/const-restrict)。回到主线时，先把访问限定符用对，再去证明共享数据协议，而不是看到优化版异常就只补一个关键词。
