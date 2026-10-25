---
title: 【拓展】packed位域与union
description: 对照 packed、位域与带标签联合体，区分省空间的内部布局和稳定的外部编码。
sourceKey: 【拓展】packed位域与union
---

# packed、位域与 union

“结构体多出几个字节，那就加 packed；标志只占几位，那就用位域；同一段数据有两种解释，那就用 union。”这三条建议看上去都在节省空间，但如果顺手把对象整块发给另一台设备，问题并没有自动解决。

本篇做三个连在一起的实验：量一次压紧布局、把两个位域值显式编码成一个字节、用标签决定联合体当前应读取哪个成员。目标不是记三个语法，而是回答：**这项技巧约束了什么，又没有承诺什么？**

## 先观察，不先规定必须几字节

下面程序以 C99 为基线；`__attribute__((packed))` 是 GCC 兼容扩展，不是 ISO C99 关键字。要求 8 位字节以及实现提供相应精确宽度整数类型。作为独立例子按 [1.0 的方法](/lessons/1-0) 编译，不要拿它去修改工程的全局打包选项。

```c
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8 || !defined(__GNUC__)
#error This example requires 8-bit bytes and GCC-compatible packed attributes.
#endif

struct __attribute__((packed)) Packet {
    uint8_t tag;
    uint32_t value;
};

struct Flags {
    unsigned int ready : 1;
    unsigned int mode : 3;
};

enum Kind { COUNT, RATIO };
struct Value {
    enum Kind kind;
    union {
        uint32_t count;
        float ratio;
    } data;
};

int main(void)
{
    struct Packet p = {3, UINT32_C(0x12345678)};
    uint32_t copy;
    memcpy(&copy, (const unsigned char *)&p + offsetof(struct Packet, value),
           sizeof copy);
    assert(offsetof(struct Packet, tag) == 0);
    assert(sizeof p >= offsetof(struct Packet, value) + sizeof p.value);
    assert(copy == UINT32_C(0x12345678));
    printf("packed size=%zu value_offset=%zu\n", sizeof p,
           offsetof(struct Packet, value));

    struct Flags flags = {1, 5};
    unsigned char wire = (unsigned char)((flags.mode << 1) | flags.ready);
    assert(wire == 0x0B && ((wire >> 1) & 7u) == 5);
    printf("ready=%u mode=%u wire=%02X\n",
           (unsigned)flags.ready, (unsigned)flags.mode, (unsigned)wire);

    struct Value v = {.kind = COUNT, .data = {.count = 27}};
    assert(v.kind == COUNT && v.data.count == 27);
    printf("tagged count=%lu\n", (unsigned long)v.data.count);
    return 0;
}
```

后两行应当是：

```text
ready=1 mode=5 wire=0B
tagged count=27
```

第一行是刻意保留的实现探针。本次 MinGW GCC 6.3.0 默认目标设置下得到 `packed size=8 value_offset=4`；在同一编译器上仅对这个独立程序增加 `-mno-ms-bitfields`，得到 `packed size=5 value_offset=1`。Arm GCC 12.3.1 的 Cortex-M4 编译输出也把大小、偏移分别生成为常数 5、1，但这里没有上板运行。

这个差别不是理由去对整个 Windows 工程随意加选项。GCC 的 x86 文档明确提醒：Microsoft 兼容布局会影响 packed 和位域结构。**属性、编译器版本、目标 ABI 与选项必须一起说明**，不能从网上看到“packed 后必定等于成员之和”就直接当协议定义。

## packed 省掉的间隙，会从别处付账

在 GCC 的常规压紧布局下，`Packet.value` 从偏移 1 开始。若对象基址适当对齐，成员仍可能没有满足普通 `uint32_t` 所需的自然对齐。编译器看到 `p.value` 这种直接成员访问时，知道它属于 packed 对象，可以按目标限制生成适当访问。

危险在于提取 `&p.value`，当作普通 `uint32_t *` 传给一个完全不知道压紧布局的函数。这个指针可能不满足该类型的对齐要求。转换不能补上物理间隙，关闭某条告警也不能让对齐自然成立。

实验没有生成这种普通成员指针，而是从对象的字符表示加上成员偏移，把已存在的 `uint32_t` 表示经 `memcpy` 复制到正常对齐的 `copy`。这里复制的正是本机成员的完整表示，不是把外部协议字节直接当整数；要解释外部数据仍然需要确认字节序。

压紧还可能增加指令数、降低访问效率。对于 F407 的 Cortex-M4，不对齐支持受指令、内存区域和 `UNALIGN_TRP` 影响；`LDM/STM/LDRD/STRD` 仍有必须对齐的限制。不要把“支持部分不对齐”说成“任何 packed 成员地址都能安全交给任意指针接口”。

另外，外层结构体用了 packed，不会自动重新安排嵌套成员类型的内部布局。若它包含一个原本有填充的子结构体，子结构体内部填充仍可能存在。外层少空白，并不等于内层字段也都紧挨。

## 位域保存值，不为通信协议承诺位号

`unsigned int ready : 1` 能表达 0、1；三位无符号的 `mode` 能表达 0 到 7。给它们赋值 1 和 5，再按成员名读取，会得到这些数值。

但实现可以决定一个分配单元里的位域从低位向高位还是从高位向低位排列，是否跨分配单元也有实现相关规则。即使 CPU 都是小端，也不能只凭端序推断位域顺序。普通 `int` 位域的有符号性也由实现规定，所以本例明确写 `unsigned int`。

因此，实验没有把 `struct Flags` 的第一个字节直接发送，而是按我们自己的协议约定：bit 0 存 ready，bit 3:1 存 mode，其余位清零。`5 << 1 | 1` 得到十六进制 `0B`。这是一项**数值编码规则**，与位域对象内部怎么放无关；接收端按掩码、移位解码，也无需复制同一份结构体定义。

位域还有两个接口限制：不能对单个位域取地址，也不能对它单独使用 `sizeof`。读改一个位域可能访问所在的存储单元；给它加 `volatile` 不会保证一次任意宽度的原子位修改。尤其别擅自用位域覆盖具有写 1 清除或特殊访问宽度要求的硬件寄存器，官方寄存器定义与访问方法优先。

## union 节省的是互斥数据的存储

`struct Value` 的计数与比例共享联合体空间。它不是同时保存两份有效业务值；标签 `kind` 记录本次保存的是哪个。示例先设置 `COUNT`，然后只读取 `data.count`，于是输出 27。

联合体大小要足够容纳最大成员，还可能有尾部填充；不能普遍把大小写成成员大小简单求和，也不能只说永远恰好等于最大成员。标签位于联合体之外，是程序自己维持的不变式，C 不会在你读取错误成员时自动检查标签。

更值得问的是“写 count 后读 ratio 会怎样”。在 C 中，经联合体成员读取另一个成员有按新类型重解释相应对象表示的规则，不能照搬 C++ 的说法把所有这种读取都一律叫未定义行为。但得到的数依赖表示，可能形成陷阱表示；并不是数值转换，更不保证得到你想要的浮点数。

C 还对满足条件的联合体结构成员共同初始序列提供特殊读取保证，那不是允许任意字段混读。课程里的协议与业务代码优先采用带标签联合体，读取最后存储且与标签一致的成员，避免让表示技巧泄漏成接口约定。

如果真正想把整数 27 转成浮点数，应做数值转换。如果想观察一个 float 的字节表示，可以经字符类型或 `memcpy` 复制表示，但必须另行确认浮点格式。把 `float *` 强转成 `uint32_t *` 后解引用，不会因为 union 存在这种规则就自动合法；还涉及有效类型、别名和对齐要求。

## 为什么三个技巧叠加也造不出稳定协议

packed 主要影响空间与对齐，位域给字段提供窄值宽度，union 让互斥内容共享空间。三者都没有完整定义跨平台的序列化格式。一个可靠的数据帧仍需单独规定：

- 每个字段的字节偏移、字节数和数值范围；
- 多字节值的大小端顺序；
- 标志位号、保留位取值和非法标签处理；
- 总长度与版本变化的兼容策略。

内部保存数据时可选择适合本目标的类型；出入口用显式编码函数连接两种世界。这样以后为节省 RAM 改成员顺序，也不会悄悄改变线上格式。

## 练习：一字节指令

协议规定 bit 0 为 enable，bit 3:1 为 mode，bit 7:4 保留为零。输入 enable=1、mode=6，编码结果是什么？收到 `9D` 时，为什么不能仅把它复制进某个 packed 位域结构再使用？

<details>
<summary>展开答案</summary>

合法输入的编码是 `(6u << 1) | 1u = 0x0D`。收到 `0x9D`，用掩码可解出 enable=1、mode=6，但高四位非零；若当前协议要求保留位必须为零，应拒绝或按明确的版本策略处理，而不是忽略异常。packed 不能规定本机位域顺序，也不会替你检查保留位。先校验输入范围和保留位，再按数值编码、解码。

</details>

## 核对依据与实验边界

PC 实测了默认布局和单例 `-mno-ms-bitfields` 布局；Arm 仅做 C99 交叉编译与生成常数检查，未访问板上内存、未测试故障行为。联合体没有执行跨成员读取的反例。

- [GCC Common Type Attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html)：packed 的适用范围、嵌套布局；[GCC x86 Options](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html)：`-mms-bitfields` 与 Microsoft 兼容布局。在线文档用于核对规则，本次实际编译版本另列于实验。
- [WG14 N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：第 6.7.2.1 节位域与联合体布局，第 6.5.2.3 节及脚注 95 的联合体表示重解释，第 6.5 第 7 段别名访问规则。C11 草案中这些规则用于核对 C99 基线；官方 PDF 请求超时，本次读取 [文本转录](https://port70.net/~nsz/c/c11/n1570.html#6.5.2.3)。
- [ST PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：第 3.3.5 节及 CCR 的 UNALIGN_TRP，读取已缓存原文。

回到 [1.6 结构体、枚举与 typedef](/lessons/1-6)。配合 [结构体对齐与填充](/notes/struct-alignment)、[字节序之争](/notes/byte-order)、[寄存器与内存映射](/notes/memory-mapped-io) 阅读。
