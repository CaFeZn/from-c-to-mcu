---
title: 【拓展】严格别名与未定义行为
description: 用字节表示往返和协议字段解码，学会替代不合法的指针强转，并区分别名、对齐、越界与端序问题。
sourceKey: 【拓展】严格别名与未定义行为
---

# 严格别名与未定义行为

你想查看一个 `float` 的位模式，顺手把它的地址转成 `unsigned int *` 再解引用。调试版看似输出了“正确十六进制”，优化后却不一样。另一段代码把串口字节缓冲区直接转成结构体指针，电脑上能跑，换到板子偶尔异常。

这两件事不能都用“编译器优化过头”解释。地址值看起来合理、内存长度似乎足够，仍不代表你能用任意类型读取它。接着 [1.5 数组与指针](/lessons/1-5) 的指针，我们要检查的不只有“指向哪里”，还有“那里是什么对象，允许怎样访问”。

## 先把诱人的写法隔离出来

下面是**未定义行为反例，不要依赖任何输出，也不要放进正常测试里**：

```c
float sample = 1.0f;
unsigned int bits = *(unsigned int *)&sample;
```

即使当前平台两种类型宽度相同、地址也满足对齐，`sample` 仍然是 `float` 对象。强制转换改变指针表达式的类型，没有把这个对象改造成 `unsigned int`。随后通过不允许的类型访问对象存储值，违反了 C 的访问规则。

这里也不是“正确结果在两种数字之间随机选”。**未定义行为**表示标准不再要求这种程序具有某个指定行为；某次输出看似稳定，甚至多年测试没有失败，都不能补上缺失的保证。调试版不做相关优化，也不等于这种访问获得了许可。

## 动手：既不强转解引用，也能拿到对象的字节

把原对象复制到 `unsigned char` 数组，再原样复制回同类型对象。与此同时，从一个偏移一字节的位置解码四个网络顺序字节，看看是否必须先把它转成四字节整数指针。

先预测：输入 `12 34 56 78` 按高字节在前的规则解释，应该得到哪个整数？`float` 往返后是否应仍等于 `1.0f`？

```c
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8
#error This wire-format example requires 8-bit bytes.
#endif

static uint32_t read_be32(const unsigned char b[4])
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

int main(void)
{
    float original = 1.0f;
    unsigned char bytes[sizeof original];
    float restored = 0.0f;
    memcpy(bytes, &original, sizeof bytes);
    memcpy(&restored, bytes, sizeof restored);
    assert(restored == original);

    const unsigned char *view = (const unsigned char *)&original;
    for (size_t i = 0; i < sizeof original; ++i) {
        assert(view[i] == bytes[i]);
    }

    const unsigned char packet[5] = {0xff, 0x12, 0x34, 0x56, 0x78};
    uint32_t number = read_be32(packet + 1);
    assert(number == UINT32_C(0x12345678));
    printf("roundtrip=ok decoded=%08lx\n", (unsigned long)number);
    return 0;
}
```

在仓库根目录编译配套源并运行：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic-errors -O2 examples/notes/c-semantics/strict-aliasing.c -o alias-demo.exe
./alias-demo.exe
```

固定输出：

```text
roundtrip=ok decoded=12345678
```

这段代码做了两种不同的工作：

1. `memcpy` 复制的是对象表示的字节，没有通过整数指针读取 `float`。把一个已有效初始化的 `float` 的表示复制回另一个 `float`，不需要假设它恰好是 IEEE 754 binary32。
2. `read_be32` 按协议规则逐字节组装值。`packet + 1` 可能不满足 `uint32_t` 的对齐，但代码只用字符指针读取，没有制造四字节整数指针。每个字节先转为 `uint32_t` 再左移，避免依赖默认 int 提升后的有符号移位范围。

示例明确要求八位字节且提供 `uint32_t` 的实现；这适用于本课程 PC/STM32 工具链，不声称适用所有抽象 C 实现。字节序是由移位表达式定义的，所以不依赖宿主是大端还是小端。

## “严格别名”管的究竟是什么

给同一个对象保存多根指针，叫有别名，本身完全正常。`int x; int *a = &x; int *b = &x;` 并不违规。常说的“严格别名规则”，主要指 C99 6.5 第 6–7 段的有效类型与允许访问类型规则。

先把两层分开：

- 对一个有声明类型的对象，通常由声明决定它的有效类型。`float sample` 不会因指针强转而改变身份。
- 对 `malloc` 取得的、没有声明类型的存储，写入路径与复制来源会影响有效类型；这不是“堆内存可以任意换指针读”。通过非字符类型写入、或用 `memcpy` 从另一对象复制，各有标准条件。

允许访问的类型不仅限于“拼写完全相同”。标准列出的类别包括兼容类型及其限定版本、对应的有符号/无符号类型、包含相应成员的聚合或联合类型，以及**字符类型**。因此不能简化成“不同类型指针一律不许相同地址”，也不能反过来认为“大小相同就能互读”。

字符类型是观察对象表示的重要例外；日常字节查看优先用 `unsigned char *`，避免普通 `char` 符号性干扰。这个例外不是双向授权：能把整数当字节查看，不等于一个声明为字符数组的缓冲区可以任意当整数对象解引用。把字节输入读进对齐正确的、真正声明为整数的局部变量，或逐字节解码，通常更容易证明。

## 为什么优化等级会暴露问题

假定函数接收一个 `int *` 和一个 `float *`。在有效程序的相应访问中，编译器可以利用类型规则排除某些别名关系，从而保留寄存器里的旧计算结果，而不是每次都去重新加载。

如果调用者用非法强转让这两条路径指向同一对象，就破坏了编译器依赖的前提。GCC 的 `-fstrict-aliasing` 在常见的 `-O2`、`-O3`、`-Os` 等级启用，因此错误常在优化后露出，但规则不是优化开关才发明的。`-fno-strict-aliasing` 可用于某些编译器特定的排查或兼容策略，**不是 ISO C 合法化开关**；它也不会修复越界、失效指针或未对齐访问。

同样，`-Wstrict-aliasing` 没报警不是证明，运行时检查器也未必能检测所有非法别名。正确测试应先设计合法访问，再验证值和边界，而不是把某种 UB 输出写进断言。

## 再过三道关：别名安全还不够

### 一、memcpy 不能代替协议定义

`memcpy(&number, packet, sizeof number)` 可以避免从字符缓冲区经整数指针解引用，但得到的是**本机对象表示对应的值**，不会自动做网络字节序转换。并且源缓冲区仍须足够长，源目的不能重叠；需要重叠复制时用 `memmove`。

也别把任意四字节包直接复制进 `float` 就宣称可移植：目标实现的浮点格式、特殊值和陷阱表示还需约定。本文的往返实验来自一个已经有效的浮点对象，不是对任意外部字节做这种承诺。

### 二、硬件支持非对齐，不等于 C 强转有效

一个处理器可能支持某些非对齐加载，仍不能据此绕过 C 对齐规则。不同指令、内存区域和配置也可能有不同硬件约束。本例没有用“Cortex-M4 某条指令碰巧能读”作为语言层依据，而是直接不产生不满足要求的类型指针。

### 三、union 不是一张任意通行证

C 中经真正的 union 对象读取不同成员有对象表示重解释的规则，可能得到陷阱表示；不能把 C++ 的规则不加区分搬过来，也不能把任意普通对象地址强转成某个 union 指针就当作已有 union 对象。GCC 手册还明确区分经 union 类型访问与拿出成员指针绕开的情形。

若只是为传输或调试复制位表示，`memcpy` 加显式格式假设通常更直白。若只是想把数值 1.0 转成整数 1，应该写数值转换 `(unsigned int)sample`，那是另一项工作，并非位模式重解释；同样要保证浮点值落在可转换范围内。

## 练习：把“能读”改成“能证明地读”

收到五个字节 `00 89 AB CD EF`，有人写 `*(uint32_t *)(buf + 1)` 取最后四个字节。请指出至少三类待检查的问题，并用本篇函数给出高字节在前的确定结果。

<details><summary>展开答案</summary>

要检查剩余长度是否至少为四、转换后的地址是否满足对齐、通过整数类型访问字符缓冲区是否符合有效类型规则，以及协议字节序与宿主表示是否一致。即使长度正确，其他条件也不会自动成立。

调用 `read_be32(buf + 1)`，四次字符读取再移位，结果应为 `0x89abcdef`。这里每次移位前的无符号宽度转换很重要；先写 `buf[1] << 24` 再转换，不能回头修复已经发生的有符号移位问题。

</details>

## 适用范围与技术依据

安全示例已在 MinGW GCC 6.3.0 的 `-O0`、`-O2` 下真实编译运行，Clang 20.1.8 严格语法检查通过，并用 ARM GCC 12.3.1 编译 Cortex-M4 目标文件。没有把 UB 反例编入测试；没有声称上板、性能或任意协议格式已验证。

- [WG14 N1256（C99 合并技术勘误稿）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：6.5 第 6–7 段（有效类型与允许的访问类型），6.2.6.1（对象表示、字符数组复制、陷阱表示），6.3.2.3 第 7 段（指针对齐），6.5.2.3 脚注 82（union 重解释），7.21.2.1（memcpy）。
- [GCC：Optimization Options 的 -fstrict-aliasing](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fstrict-aliasing)：类型别名优化、union 访问示例及启用等级。此链接为滚动手册，实际验证版本见上。

回到 [1.5 数组与指针](/lessons/1-5) 和 [1.6 结构体](/lessons/1-6) 时，把“地址、长度、生命周期、对齐、访问类型、数据格式”一起检查。相关分支：[const 与 restrict](/notes/const-restrict)、[动态内存与碎片](/notes/dynamic-memory)、[参数求值顺序与未定义行为](/notes/evaluation-order)。
