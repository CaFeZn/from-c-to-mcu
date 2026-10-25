---
title: 【原理】结构体对齐与填充
description: 用成员偏移和数组步长解释结构体为何变大，并区分 ABI 布局、成员值与协议编码。
sourceKey: 【原理】结构体对齐与填充
---

# 结构体对齐与填充

一个传感器记录包含 1 字节编号、4 字节计数和 2 字节状态码。按加法算是 7 字节，编译器却报告 12；只是把字段顺序调一下，又变成 8。假如设备有一千条记录，这个差别会直接影响 RAM 预算。

这不是编译器偷偷增加了业务字段。它在满足访问位置要求的同时，也要保证**结构体放进数组之后，每一项里的成员仍能正确访问**。本篇先量出差别，再解释为什么末尾也有空白，最后讨论为什么这些空白不能进入协议契约。

## 用三个测量回答一个疑问

下面是可独立运行的 C99 程序，可按 [1.0 的方法](/lessons/1-0) 构建。运行前先画出两种记录的成员位置，注意不要只算成员大小之和。

```c
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Sample {
    uint8_t tag;
    uint32_t count;
    uint16_t code;
};

struct Compact {
    uint32_t count;
    uint16_t code;
    uint8_t tag;
};

#ifdef VERIFY_ARM_LAYOUT
typedef char sample_size[(sizeof(struct Sample) == 12) ? 1 : -1];
typedef char sample_count[(offsetof(struct Sample, count) == 4) ? 1 : -1];
typedef char sample_code[(offsetof(struct Sample, code) == 8) ? 1 : -1];
typedef char compact_size[(sizeof(struct Compact) == 8) ? 1 : -1];
#endif

int main(void)
{
    struct Sample pair[2] = {{1, 100, 2}, {3, 200, 4}};
    assert(offsetof(struct Sample, tag) == 0);
    assert(offsetof(struct Sample, count) >= sizeof(uint8_t));
    assert(offsetof(struct Sample, code) >=
           offsetof(struct Sample, count) + sizeof(uint32_t));
    assert(sizeof(struct Sample) >=
           offsetof(struct Sample, code) + sizeof(uint16_t));
    assert(sizeof pair == 2 * sizeof pair[0]);
    assert(pair[1].count == 200);
    printf("Sample: size=%zu tag=%zu count=%zu code=%zu\n",
           sizeof(struct Sample), offsetof(struct Sample, tag),
           offsetof(struct Sample, count), offsetof(struct Sample, code));
    printf("Compact: size=%zu count=%zu code=%zu tag=%zu\n",
           sizeof(struct Compact), offsetof(struct Compact, count),
           offsetof(struct Compact, code), offsetof(struct Compact, tag));
    printf("two_samples=%zu\n", sizeof pair);
    return 0;
}
```

在本次 32 位 MinGW 默认布局下，输出为：

```text
Sample: size=12 tag=0 count=4 code=8
Compact: size=8 count=0 code=4 tag=6
two_samples=24
```

这里没有断言“所有 C 实现都必须是 12 和 8”。普通断言只检查成员次序、末端覆盖和数组长度关系。`VERIFY_ARM_LAYOUT` 是为采用本课程 Arm 目标 ABI 的交叉编译验证保留的开关；不要在任意平台开启后把报错当成该平台有问题。

`offsetof` 来自 `<stddef.h>`，返回成员相对结构体起点的字节偏移。它适合这种普通成员，不应用于位域。`sizeof` 包含结构体内部和末尾的填充，数组大小也会把每一项的末尾填充算进去。

## 按目标的对齐要求走一遍

先明确本次手算的条件：字节为 8 位，`uint8_t`、`uint16_t`、`uint32_t` 的大小分别为 1、2、4，目标 ABI 对它们的自然对齐要求也分别为 1、2、4。**大小与对齐是两个概念，只是这几个类型在这个目标上数值相等。**

对 `Sample`，按声明顺序从偏移 0 开始：

1. `tag` 放在偏移 0，占一个字节，下一个空位置是 1。
2. `count` 要求 4 字节对齐，向后找到偏移 4；中间偏移 1、2、3 成为内部填充。
3. `count` 使用偏移 4 到 7；`code` 在偏移 8，满足 2 字节对齐，占偏移 8、9。
4. 结构体最严格成员要求 4 字节对齐。按这里的 ABI，整体大小向上补到 4 的倍数，即 12，所以偏移 10、11 是尾部填充。

对 `Compact`，先放 4 字节计数，再放 2 字节状态码，最后放编号，已用到偏移 6。整体向上补到 8，只剩一个尾部填充字节。相同成员并没有消失，消失的是布局中不必要的间隔。

这是一份**有条件的布局推演**，不是 C99 对所有目标的算式。成员的对齐要求由实现决定，某些目标的 8 字节类型只要求 4 字节对齐，某些扩展又可以提高要求。不要把“成员必须位于自己大小的倍数”当通用规则。

## 最后一个字段后面，为何还要补

设想 `Sample` 只占 10 字节，而第一项从满足 4 字节对齐的地址开始。数组第二项就会从第一项起点加 10 的位置开始，第二项里的 `count` 将落在整体起点加 14 的位置，无法保持所需对齐。

C 数组相邻元素没有另外的隐形间隙，下一项的步长就是 `sizeof(struct Sample)`。因此尾部填充是对象表示的一部分，帮助每个数组元素保持相同的内部布局。实验里两个 `Sample` 占 24 字节，而不是只给第一项补空白、总计 22。

C 标准保证普通成员的地址按声明顺序增加，结构体开头没有前置填充，但允许内部和末尾填充。Arm AAPCS32 进一步规定常规聚合的对齐取最严格成员的对齐，大小为能容纳布局的最小对齐倍数。前者是语言规则，后者是具体 ABI 的实现约定，不能把层次混在一起。

## 填充不是一个能依赖的隐藏字段

给所有成员赋同样的值，并不意味着两份结构体的每个字节都一样。填充字节不是业务值；向结构体或成员存值时，标准允许相关填充字节取未指定值。

于是以下两种做法都有问题：

- 使用 `memcmp(&a, &b, sizeof a) == 0` 作为结构体业务相等的唯一判断。业务字段相同仍可能因填充不同而判不等，应逐成员比较或比较规范编码。
- 把整个结构体的原始字节直接发出、做持久化格式或计算跨设备校验。接收方可能有不同填充、字节序、枚举宽度和浮点表示，填充还可能带出不该暴露的旧内容。

“先 `memset` 全零再赋字段”并没有建立永久的填充值保证；后续成员存储仍允许影响填充。对含指针、浮点或其他表示约束的类型，也不能把全零字节与语言意义上的零初始化一概等同。需要默认值时优先使用语言初始化，需要外部数据时则显式逐字段编码。

调试器中看到几个 `AA` 填充字节保留不变，只说明那次构建的那条路径如此，不是优化后必须维持的契约。

## 对齐不是纯粹的浪费

硬件对不同宽度的访问有不同支持。适当对齐常让访存更直接；不对齐可能需要额外操作，甚至触发故障。但“所有不对齐都崩溃”和“Cortex-M4 支持不对齐所以可以随意转换指针”都不准确。

STM32F407 使用的 Cortex-M4 只对部分指令支持某些不对齐访问，还受内存区域与 `UNALIGN_TRP` 设置影响。`LDM/STM/LDRD/STRD` 的不对齐访问并不会因为关闭该陷阱位就全部合法。更重要的是，硬件能完成某次访问，不等于 C 中通过不合适的类型或对齐访问就被允许。

为了节省 RAM，可先测量并合理调整**内部数据结构**的成员顺序。但不要重排 CMSIS 外设寄存器布局，那里每个偏移都由芯片手册定义；也不要悄悄改动已发布的库 ABI 或共享存储布局。编译器选项、packing pragma、枚举选项发生变化时，同一份头文件也可能产生不同布局，跨模块必须保持一致。

## 练习：找出尾部填充

在大小和对齐分别为 1、2、4 的上述目标上，考虑 `struct T { uint16_t a; uint8_t b; uint32_t c; };`。求三个偏移、结构体大小、五项数组大小。若删去 `c`，大小是否就是 3？

<details>
<summary>展开推演</summary>

`a` 在 0，占 0、1；`b` 在 2；偏移 3 填充；`c` 从 4 到 7。整体对齐为 4，大小 8，五项数组共 40 字节。删去 `c` 后最大对齐变为 2，前三个字节装成员，尾部仍需补一个字节，所以大小为 4，不是 3。上述数字依赖题目给出的目标规则。

</details>

## 适用范围与依据

本篇使用 C99 的 `sizeof`、`offsetof`，不要求 C11 的 `_Alignof`。PC 运行验证了当前 ABI 的输出；Arm 目标可用编译期数组长度检查布局，不能把编译成功称为上板实测。

- [WG14 N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：第 6.7.2.1 第 14–17 段规定成员对齐、顺序及填充，第 6.2.6.1 第 6 段规定填充字节。以 C11 草案核对 C99 沿用机制；官方 PDF 请求超时，本次读取 [文本转录](https://port70.net/~nsz/c/c11/n1570.html#6.7.2.1)。
- [Arm AAPCS32，2025Q1](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs32/aapcs32.rst)：Data Types and Alignment 中 Fundamental Data Types、Composite Types 的聚合、联合体与数组布局规则。
- [ST PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：第 3.3.5 节 Address alignment，及第 4.4 节 CCR 的 UNALIGN_TRP 描述；本次核对本地缓存原文。

回到 [1.6 结构体、枚举与 typedef](/lessons/1-6)。继续看 [packed、位域与 union](/notes/packed-bitfields-unions)、[字节序之争](/notes/byte-order)、[寄存器与内存映射](/notes/memory-mapped-io)。
