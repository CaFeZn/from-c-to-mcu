---
title: 【拓展】CMSIS如何用结构体描述外设
description: 用可运行的偏移实验把 GPIO 成员、保留字段和真实寄存器地址逐项对齐。
sourceKey: 【拓展】CMSIS如何用结构体描述外设
---

# CMSIS 如何用结构体描述外设

你在程序里写下 `GPIOA->BSRR = 0x20u;`，灯的输出锁存位就能改变；但把同样的结构体定义复制到 PC，声明一个局部变量，再给它的 `BSRR` 赋值，什么灯也不会亮。两段代码的语法几乎相同，差别究竟藏在哪里？

先不接板。本篇先让编译器报告成员的真实位置，再把这些位置与 F407 的手册对齐。你会发现，结构体只是一份访问布局，不是外设仿真器，也不是创建硬件的命令。

## 先量一量，而不是凭成员数量猜地址

下面程序使用普通 RAM，不包含任何单片机裸地址，可以放进第 1 章的 PC 工程。它按 ST 的 F407 设备头文件摘出 GPIO 的布局；为了避免和正式工程重名，类型另取为 `gpio_layout_t`。

```c
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} gpio_layout_t;

int main(void)
{
    gpio_layout_t ram = {0};
    assert(sizeof(uint32_t) == 4);
    assert(offsetof(gpio_layout_t, BSRR) == 0x18);
    assert(offsetof(gpio_layout_t, AFR[1]) == 0x24);
    ram.BSRR = UINT32_C(0x20);
    assert(ram.ODR == 0);
    printf("BSRR offset=%lu, ODR=%lu\n",
           (unsigned long)offsetof(gpio_layout_t, BSRR),
           (unsigned long)ram.ODR);
    return 0;
}
```

先预测两个结果：BSRR 在结构体开头之后多少字节？给它写 32，会不会让同一个 RAM 对象的 ODR 也变成 32？在本篇验证的 8 位字节、32 位 `uint32_t` 环境中，输出是：

```text
BSRR offset=24, ODR=0
```

断言失败时应检查目标 ABI、类型大小或自己是否改错结构体，不要删掉断言继续使用。这里的 `offsetof` 来自标准头文件，结果是字节偏移；不用把成员地址转成整数再相减，更不用构造空指针来模拟它。

RAM 中的 `BSRR` 和 `ODR` 是独立成员，C 语言没有定义“写前者改变后者”。真实 GPIO 里，BSRR 后面接着置位/复位逻辑，因此硬件会解释这次写入。用 RAM 做寄存器测试时，必须额外实现状态模型，不能把赋值本身当成硬件验证。

## 把同一份布局放到芯片规定的地址

F407 的 GPIOA 基址是 `0x40020000`，BSRR 偏移是 `0x18`。ST 的设备头文件最终形成这样的关系，下面只是等价展开，不要加入已经包含正式头文件的工程：

```c
#define GPIOA_BASE 0x40020000UL
#define GPIOA ((GPIO_TypeDef *) GPIOA_BASE)
```

因此 `GPIOA->BSRR` 的访问地址是 `0x40020018`。编译器知道成员位于 24 字节处，生成基址加偏移的访问，不需要在运行时逐一搜索成员名。这个宏不会分配 RAM，也不会运行构造或初始化操作。整数地址转指针依赖目标编译器和芯片的 MMIO 约定，不是任何 PC 进程都能合法使用这些地址。

真实固件应使用与芯片一致的 ST CMSIS Device 头文件，而不是长期维护本篇教学副本。CMSIS-Core 给出内核访问约定与定义，芯片厂商的 Device 部分补上 IRQ 编号、外设地址、寄存器结构体和位掩码。HAL 位于更高一层，通过这些定义访问硬件；CMSIS 不是另一个 HAL 点灯函数库。

## 空出来的位置，也属于地址契约

GPIO 的这一小段正好连续，RCC 却不是。官方 `RCC_TypeDef` 在 AHB3RSTR 后保留一个 32 位位置，在 APB2RSTR 后又保留两个：

| 成员或空位 | 偏移 | 占用 |
|---|---|---|
| AHB3RSTR | `0x18` | 4 字节 |
| RESERVED0 | `0x1C` | 4 字节 |
| APB1RSTR、APB2RSTR | `0x20`、`0x24` | 各 4 字节 |
| RESERVED1[2] | `0x28`、`0x2C` | 共 8 字节 |
| AHB1ENR | `0x30` | 4 字节 |

这些 `RESERVED` 成员让后续字段继续对齐手册。删去它们，代码仍可能编译，但写 AHB1ENR 实际落到别的地址。不能靠 `packed` 修补漏字段；它解决的不是同一类问题，还可能引入不合适的访问对齐。

仓库配套例子同时断言 GPIO 的 BSRR、AFR[1] 和 RCC 的 AHB1ENR 偏移，可独立编译运行：

```sh
gcc -std=c99 -Wall -Wextra -pedantic examples/notes/platform/test.c -o platform_test
./platform_test
```

Windows 的原生 GCC 通常生成 `platform_test.exe`，对应执行该文件。程序预期报告 `BSRR=24 AFR1=36 RCC_AHB1ENR=48`，最后显示 `platform tests: PASS`。配套程序里的 BSRR 状态演算是另写的模型；它还验证同一位同时请求置位与复位时，F407 按手册以置位为优先。

## 类型限定符能提醒什么，不能保证什么

CMSIS 中常见的 `__I`、`__O`、`__IO`，以及结构体成员版 `__IM`、`__OM`、`__IOM`，不是 C 语言新增关键字。以所核对的 C 头文件为例，只读成员用 `volatile const`，可写成员使用 `volatile`。

这里有两条很重要的限制。第一，C 没有“禁止读取”的通用类型限定符，`__OM` 也只是 `volatile`，编译器未必阻止你读写只允许写入的寄存器。第二，设备头文件的访问标记未必精确表达全部硬件语义：所核对的 F407 头文件连 IDR、BSRR 都声明成 `__IO`，但 RM0090 对它们的读写行为有更严格定义。**访问是否合法，以手册为准，不能以代码能编译为准。**

`volatile` 让编译器按其实现约定保留相关访问，不会自动开启 RCC 时钟、设置 GPIO 模式、处理互斥或缓存一致性。`GPIOA->ODR |= mask` 仍是读、改、写过程；其他执行者在中间更新 ODR，仍可能丢失改动。需要单独置位时，对 BSRR 写入请求比把 BSRR 当普通变量做 `|=` 更明确。

还要避免整块复制外设结构体。把 GPIO 当普通对象赋值、`memcpy` 或清零，会依次访问保留位、状态寄存器和带副作用的入口。HAL 的初始化参数结构体保存在 RAM，可以作为普通配置对象处理；它与映射外设地址的 `GPIO_TypeDef` 不是同一种用途。

## 练习：字段删掉后，错在哪里

有人把 RCC 中的 `RESERVED0` 和 `RESERVED1[2]` 一起删除，其他成员不变。请预测自己这个结构体中的 AHB1ENR 偏移，并说明为什么“调试器看到我给 AHB1ENR 写了 1”不能证明 GPIO 时钟已开启。

<details><summary>展开答案</summary>

共少了 12 字节，因此教学结构体的偏移从 `0x30` 变成 `0x24`。若仍把它放到 RCC 基址 `0x40023800`，写入落到 `0x40023824`，不是 AHB1ENR 的 `0x40023830`。调试器显示的是你的类型解释；类型写错，字段名就会误导你。应恢复匹配芯片的官方定义，并检查 `offsetof`、实际地址以及手册中的寄存器名称，不能对错误目标反复写入试运气。

</details>

## 适用范围与依据

布局例子限定 STM32F407，C99 为基线。PC 测试验证布局及教学状态模型，ARM 交叉编译验证访问指令；均不表示已验证真实引脚、时钟或电气条件。

- [ST F407 设备头文件](https://github.com/STMicroelectronics/cmsis-device-f4/blob/9192c7b9df75a142f2027ab266601fe061fc00b3/Include/stm32f407xx.h)：GPIO、RCC 类型及 GPIOA/RCC 基址，固定提交核对。
- [CMSIS-Core CM4 头文件](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/core_cm4.h)：IO Type Qualifiers。
- [RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)，Rev 22，8.4.7：BSRR 的写请求与置位优先规则。
- [GCC Volatiles](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)：访问与排序的实现边界。

回到主线：[1.6 结构体、枚举与 typedef](/lessons/1-6)、[2.3 怎么读手册](/lessons/2-3)。继续阅读：[寄存器与内存映射](/notes/memory-mapped-io)、[汇编如何读写寄存器](/notes/assembly-register-access)、[看反汇编](/notes/reading-disassembly)。
