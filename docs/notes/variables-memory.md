---
title: 【原理】变量与内存
description: 用复制、别名和三次函数调用区分对象的值、地址、作用域与存储期。
sourceKey: 【原理】变量与内存
---

# 变量与内存

把温度读数复制到另一个变量后，修改原读数，副本没有变；但换成指针再修改，原读数跟着变了。两个写法都用了赋值号，究竟复制了什么？再看一个更容易混淆的现象：函数每次执行都经过同一条声明，有的计数器每次从头开始，有的却能记住上次的值。

本篇从这两件事拆开“变量是一只格子”的比喻。先不要背栈、堆、全局区的位置；先确认**谁是对象、表达式取到了什么、对象何时还活着**。这些问题确定以后，才有资格问它被编译器安排到了哪里。

## 先跑一次复制与别名实验

沿用 [1.0 的练习工程](/lessons/1-0)，建立一个独立的 C99 可执行程序。下面是完整代码，无需键盘输入。先预测前三个整数和三次调用的计数，再运行。保留断言，不要定义 `NDEBUG`。

```c
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static unsigned file_count;

static void visit(void)
{
    unsigned automatic = 10;
    static unsigned saved;
    ++automatic;
    ++saved;
    ++file_count;
    assert(automatic == 11 && saved == file_count);
    printf("automatic=%u saved=%u file=%u\n", automatic, saved, file_count);
}

int main(void)
{
    int original = 7;
    int copy = original;
    int *alias = &original;
    *alias = 9;
    assert(original == 9 && copy == 7 && alias == &original);
    printf("original=%d copy=%d alias_value=%d\n", original, copy, *alias);
    assert(file_count == 0);
    visit();
    visit();
    visit();
    printf("char_bits=%d int_bytes=%zu pointer_bytes=%zu\n",
           CHAR_BIT, sizeof original, sizeof alias);
    return 0;
}
```

前四行应当固定为：

```text
original=9 copy=7 alias_value=9
automatic=11 saved=1 file=1
automatic=11 saved=2 file=2
automatic=11 saved=3 file=3
```

最后一行是环境探针，不是普适答案。本次 32 位 MinGW 环境中是 `char_bits=8 int_bytes=4 pointer_bytes=4`。64 位 PC 的指针常见为 8 字节；不能因为 PC 装着 64 位操作系统，就推断当前编译器一定生成 64 位程序。

## 同一个赋值号，复制的值不同

`int copy = original;` 建立了另一个 `int` 对象，把当时的整数值 7 放进去。之后修改 `original`，不会自动同步 `copy`。C 不是电子表格，初始化表达式并不建立永久的联动公式。

`int *alias = &original;` 也建立了一个独立对象，但这个对象的类型是“指向 `int` 的指针”。它保存的是可用于指向 `original` 的指针值。执行 `*alias = 9`，写的是指针所指的对象，不是重新给 `alias` 赋一个整数。

这里有三个要分开的量：

- `original` 的值是整数；`&original` 给出其地址。
- `alias` 的值指向原对象；`&alias` 才是指针变量自身的地址。
- `sizeof original` 量整数对象；`sizeof alias` 量指针对象。两者在某个平台碰巧一样大，不表示它们的含义一样。

如果要打印普通对象地址，使用 `printf("%p", (void *)&original);`。地址的文本形式由实现决定，不能把一次运行的数字抄回下次程序当有效地址，也不要拿两次打印的相同数字证明两个对象有相同寿命。

## “名字看得见”与“对象还活着”是两个问题

函数中的 `automatic` 每次进入都会重新初始化为 10，所以加一后每次都是 11。`saved` 具有静态存储期，只初始化一次，函数返回并不结束它的寿命。因此三次调用看到 1、2、3。

这次我们给 `saved` 和文件作用域的 `file_count` 都没有写显式初值。对于这种静态存储期的算术对象，C 保证初始值为零。没有初始化的自动局部变量没有这项保证。把它直接当整数读取不是一个“随机数发生器”；像未初始化的自动 `int` 用于 `printf` 这样的反例可能触发未定义行为，不能靠多跑几次收集它的范围。

**作用域**回答“在源码的哪里可以使用这个名字”；**存储期**规定对象可以存在多久。`saved` 的名字只在函数内可见，不意味着其他函数永远不能访问那个对象：如果接口返回它的地址，其他代码仍可能通过指针访问它。反过来，曾经保存过一个地址，也不意味着目标对象会因此延长寿命。

在 C99 的本篇范围中，常见的三类存储期是自动、静态、分配存储期。最后一种由 `malloc` 等分配函数开始、由释放操作结束，不由指针变量离开作用域自动结束。C11 增加的线程存储期不是本实验的前提。

## 语言中的对象，最后怎样落到机器上

“对象”可以理解为执行环境中用于表示值的一段存储。这个模型不要求每个源码变量都永久占据一块可在调试器内存窗口中找到的 RAM。只要可观察行为不变，编译器可以把值留在寄存器、合并计算，甚至消除不需要的存储。

常见 MCU 工程里，会看到这些实现安排：

1. 有非零初始值的可写静态数据常在 `.data` 一类段中；初始镜像在 Flash，启动代码把初值复制到 RAM。
2. 零初始化的静态数据常在 `.bss` 一类段中；启动代码负责清零，并不是上电后的 SRAM 天然全零。
3. 自动对象常由调用栈或寄存器承载；分配对象通常由运行库管理的堆承载。

这些是编译器、链接脚本和运行库的合作方式，不是 C 标准规定所有平台必须采用的地址分区。已核对的 ST F407 GCC 启动代码确实在进入 `main` 前复制数据段、清零 BSS；换成引导程序跳转、定制启动代码或保留 RAM 段时，还得检查具体工程。

对调试更有效的问题因此变成：“这个对象在当前程序点是否有效？初始化是否完成？这条写入究竟更新了对象还是其副本？”而不是凭截图认定“局部变量一定在某个地址范围”。

## 三个容易越界的推论

**一个字节永远是 8 位？** C 保证 `sizeof(char) == 1`，但一个 C 字节包含多少位要看 `CHAR_BIT`。本课程常用 PC 和 STM32F407 都按 8 位字节工作，不能把这个目标条件偷换成所有 C 实现的保证。精确宽度类型也要由实现实际提供。

**加了 `const` 就一定放 Flash？** 它首先约束经相关类型进行修改的方式。存放位置由目标实现、链接安排和对象用法决定。局部 `const` 并不自动具有静态存储期；对真正定义为 const 的对象强行去掉限定并修改，不会因此变合法。

**加了 `volatile` 就能保住一切？** 它不是寿命延长器，也不是原子性保证，更不能修复越界或失效指针。MMIO 还有独立的硬件访问契约，不能把所有“长得像地址”的数都当普通对象地址。

## 练习：名字消失以后还能读吗

把 `visit` 想象成返回 `saved` 地址的函数，再想象它返回 `automatic` 地址。调用者在函数返回后用这两个地址读值，为什么结论不同？如果把 `saved` 换成 `const unsigned saved = 0;`，结论会自动变好吗？

<details>
<summary>展开分析</summary>

静态对象 `saved` 的寿命覆盖程序执行期，返回地址后仍可访问；它的名字只在函数内可见不是障碍。不过共享同一个对象意味着不同调用会互相影响，还需要另外考虑并发。

普通自动对象 `automatic` 的寿命在函数返回时结束，返回其地址不能让它续命，之后不能解引用。改成不带 `static` 的局部 `const` 仍是自动对象，也同样失效。要返回一个独立结果，优先按值返回，或让调用者提供仍然存活的目标对象。

</details>

## 适用范围与核对依据

本篇示例是 C99、单线程普通内存实验，未访问真实外设。地址、大小以及段的位置属于当前构建的观测，不能由一次 PC 运行推断任意 MCU 的布局。

- [WG14 N1570（C11 委员会草案）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：第 6.2.1 节作用域、第 6.2.4 节存储期、第 6.3.2.1 节未初始化对象访问、第 6.7.9 节初始化；这里使用的是 C99 已有规则，不使用线程存储或原子类型。本次官方 PDF 请求超时，逐条读取了 [N1570 文本转录](https://port70.net/~nsz/c/c11/n1570.html#6.2.4)，不把转录网站当标准发布者。
- [ST F407 GCC 启动代码](https://github.com/STMicroelectronics/cmsis-device-f4/blob/9192c7b9df75a142f2027ab266601fe061fc00b3/Source/Templates/gcc/startup_stm32f407xx.s#L60-L100)：数据段复制、BSS 清零及进入 main 的具体实现。

回到 [1.2 变量、运算与控制流](/lessons/1-2) 或 [1.4 函数与作用域](/lessons/1-4)。继续读 [数组退化与指针算术](/notes/array-pointer-arithmetic)、[野指针与悬空指针](/notes/pointer-lifetime)、[结构体对齐与填充](/notes/struct-alignment)。
