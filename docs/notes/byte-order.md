---
title: 【拓展】字节序之争
description: 把同一个 32 位数编码成大小端字节序列，分清主机表示、协议约定与逐位传输顺序。
sourceKey: 【拓展】字节序之争
---

# 字节序之争

发送端说自己发了 `0x12345678`，接收端却读出 `0x78563412`。逐字节抓包又发现没有丢字节：线上确实依次出现 `12 34 56 78`。故障可能不在串口波特率，而在双方对“这四个字节拼成哪个数”的约定不同。

先记住一个区分：**整数的值不分大小端，多字节对象的存放顺序和协议的字节顺序才分。** `0x12345678` 是同一个数学值；把它保存为四个字节时，需要说明哪个字节排在前面。

## 在纸上拆开四个字节

这个数由四个字节权重组成：

```text
0x12 × 2^24 + 0x34 × 2^16 + 0x56 × 2^8 + 0x78
```

若按地址从低到高观察：

| 位置 | 大端表示 | 小端表示 |
|---|---|---|
| 第 0 字节 | 12 | 78 |
| 第 1 字节 | 34 | 56 |
| 第 2 字节 | 56 | 34 |
| 第 3 字节 | 78 | 12 |

**大端**把高权重字节放在低地址或序列前面；**小端**把低权重字节放在低地址或序列前面。两列都没有倒转单个字节内部的位。CPU 按它的规则读取自己的对象时，得到的整数值仍然相同，错误发生在未经转换就跨过了不同的约定边界。

## 一个不猜主机端序的编解码实验

将下面代码作为独立 C99 程序，按 [1.0 的构建方法](/lessons/1-0) 运行。固定输入是四个协议字节 `12 34 56 78`，约定它们是大端无符号 32 位值。程序也测试零、最高位为 1 和全部位为 1 的情况。

要求字节为 8 位、实现提供 `uint32_t`；这是此协议的目标条件，不是宣称所有 C 平台都满足。保留断言以执行测试。

```c
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CHAR_BIT != 8
#error This protocol uses 8-bit bytes.
#endif

static uint32_t read_be32(const unsigned char p[4])
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t read_le32(const unsigned char p[4])
{
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

static void write_be32(unsigned char p[4], uint32_t value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static void write_le32(unsigned char p[4], uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void show(const char *label, const unsigned char p[4])
{
    printf("%s: %02X %02X %02X %02X\n", label,
           (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
}

int main(void)
{
    const unsigned char wire[4] = {0x12, 0x34, 0x56, 0x78};
    uint32_t value = read_be32(wire);
    unsigned char be[4], le[4], host[4];
    assert(value == UINT32_C(0x12345678));
    write_be32(be, value);
    write_le32(le, value);
    memcpy(host, &value, sizeof value);
    assert(memcmp(be, wire, sizeof be) == 0);
    printf("value=%08" PRIX32 "\n", value);
    show("BE", be);
    show("LE", le);
    show("host", host);

    const uint32_t cases[] = {0, 1, UINT32_C(0x12345678),
                             UINT32_C(0x80000000), UINT32_MAX};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        write_be32(be, cases[i]);
        write_le32(le, cases[i]);
        assert(read_be32(be) == cases[i]);
        assert(read_le32(le) == cases[i]);
    }
    puts("roundtrip=5 cases ok");
    return 0;
}
```

以下四行不依赖当前主机大小端：

```text
value=12345678
BE: 12 34 56 78
LE: 78 56 34 12
roundtrip=5 cases ok
```

其中实际输出还会在末行之前插入 `host` 行。本次 PC 上是 `host: 78 56 34 12`。主机为大端时，host 行会与 BE 行相同；编码后的 BE、LE 两行依然不变。代码没有根据 host 行决定如何编解码，所以不会在换平台时自动反着来。

## 看懂读取函数：先建立数值，再交给主机

`read_be32` 把第一个输入字节放到结果的 bit 31:24，把第二个放到 bit 23:16，逐项合成整数。完成后，返回的是 `uint32_t` 数值，不是某个需要再“调一次端序”的半成品。

接收方随后用这个值进行比较、加法或打印，都按照普通整数语义工作。它在寄存器或本机内存里如何表示，由编译器和 CPU 处理。只有再次写出协议时，才调用约定的编码函数。

`write_be32` 用右移选出不同权重的字节，再转换成 `unsigned char`。在本例的 8 位字节前提下，转换保留低 8 位，输出位置由我们显式指定。小端函数调换位置，不调换业务含义。

函数参数写成 `p[4]` 只是便于表达预期形状，仍会按数组形参规则调整为指针。它没有自动检查四字节长度。实际解析时必须先验证输入至少还剩四字节，再调用；不要相信报文自己声明的长度，更不要先读完才检查总长。

## 为什么转换必须放在移位之前

一个常见写法是 `p[0] << 24`。尽管 `p[0]` 是无符号字符，整数提升之后常成为 32 位有符号 `int`。当输入首字节为 `80` 时，左移结果超出正 int 的可表示范围，C99 下可能成为未定义行为。

安全写法是先转成 `uint32_t`，再左移：`((uint32_t)p[0] << 24)`。本实验把 `0x80000000` 和 `UINT32_MAX` 放进往返测试，正是为了覆盖最高位，而不是只用 `00 00 00 01` 这种容易掩盖错误的输入。

同样，不要用符号性可能不同的普通 `char` 去承载待移位字节。负值的符号扩展、负数移位等问题会把端序问题和整数规则混到一起。本例选 `unsigned char` 作为原始字节，移位次数固定为 0、8、16、24，全部小于结果宽度。

## 三种看上去方便的捷径

**直接强转指针读取。** 把接收缓冲区强转成 `uint32_t *` 再解引用，不仅会采用主机端序，还可能不满足对齐和有效类型访问规则。即便 F407 支持某些不对齐访问，C 语义和区域限制仍不能跳过。

**只用 memcpy。** `memcpy` 适合安全复制对象表示。实验用它把已存在的整数复制到字符数组观察，避免非法别名访问。但它只搬字节，不懂“线上是大端”。把协议字节 memcpy 到整数里再读，仍按主机表示解释；这与逐字节解码不是同一件事。

**收到就无条件交换。** 字节交换函数只做字节重排。如果协议与主机本来同序，再交换反而错。像“网络序转换”这类接口表达的是从协议约定到主机值的转换，在某些平台上可能什么也不做；不要把它们的意义简化成“总是交换四个字节”。本实验直接用显式编解码避免分支判断。

## 别把其他顺序也叫成端序

抓包工具的一格通常表示一个字节，工具可以按抓到的先后显示，也可能把多个字节组合成一个数显示。排查时先切换到原始字节视图，确认显示格式，再判断数据是否反了。

调试器内存窗口也一样：若按 32 位字显示，看到 `12345678` 不代表低地址字节是 `12`；要切成逐字节显示才能验证主机对象表示。程序打印整数的十六进制文本通常总是高位写在左边，这也不能证明 CPU 是大端。

逐位传输的先后是另一层约定。一条链路可以在每个字节内先发送低位，又按高字节在先的顺序发送一个多字节字段；两件事并不矛盾。驱动读到了正确的字节以后，CPU 的小端并不会自动替你决定下一个要发送哪个字节。

字符串 `"1234"` 是四个字符，并不等于二进制整数 1234 的四字节表示；字符串格式、字符编码、浮点格式以及多寄存器设备的寄存器顺序也都需要各自约定。只写一句“小端协议”还不足以描述一份完整数据格式。

## F407 的具体位置

已核对的 ST PM0214 说明本课程 STM32 Cortex-M4 目标按小端访问内存：最低有效字节存到最低地址。因此直接观察 F407 上的 `uint32_t` 对象时，应按小端理解。但这不强迫串口协议、SPI 器件或文件格式也使用小端；接口两端按什么顺序交换字节，仍然由协议决定。

本轮只有 PC 程序执行和 Cortex-M4 编译检查，没有发送真实串口数据，也没有访问板上内存窗口。PC 上 host 行的小端现象与手册规则一致，不等于做过板级实验。

## 练习：只改解码约定，不改线上字节

线上依次收到 `01 02`。如果字段是大端无符号 16 位整数，值是多少？若字段是小端呢？现在把线上改成 `80 00 00 00`，应该用本例哪个函数得到 `0x80000000`？是否还要再交换一次？

<details>
<summary>展开答案</summary>

大端 16 位是 `0x0102`，即 258；小端 16 位是 `0x0201`，即 513。四字节输入按大端约定应调用 `read_be32`，得到数值 `0x80000000`，无需再交换；再交换会把正确结果改为 128。需要发送这个值时，依据发送协议调用对应写函数，不把主机对象原样假定成线上的格式。

</details>

## 适用范围与依据

本篇处理 8 位字节组成的无符号 32 位整数，不规定浮点数或有符号字段的传输格式，也不替协议完成长度校验。已运行五组值的大端、小端往返断言，未执行未对齐强转反例。

- [ST PM0214 Rev10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：第 2.2.6 节 Memory endianness、第 3.3.5 节 Address alignment；本次读取缓存原文。
- [WG14 N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：第 6.2.6.1 节对象表示，第 6.5 第 7 段字符类型访问，第 6.5.7 节移位，第 7.20.1.1 节精确宽度类型。以 C11 草案核对 C99 已有规则；官方 PDF 请求超时，本次读取 [文本转录](https://port70.net/~nsz/c/c11/n1570.html#6.5.7)。

回到 [1.6 结构体、枚举与 typedef](/lessons/1-6) 或 [6.3 设计自己的协议](/lessons/6-3)。相关分支：[结构体对齐与填充](/notes/struct-alignment)、[packed、位域与 union](/notes/packed-bitfields-unions)、[缓冲区溢出与边界检查](/notes/buffer-bounds)。
