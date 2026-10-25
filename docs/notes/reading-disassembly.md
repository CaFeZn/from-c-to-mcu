---
title: 【拓展】看反汇编
description: 对比同一份 C 代码的 O0 与 O2 目标文件，识别外设访问、优化、常量池和重定位。
sourceKey: 【拓展】看反汇编
---

# 看反汇编

C 代码明明写了两次赋值，调试器里却只看到一次 `str`；另一个函数写着乘以三，机器码里又找不到乘法指令。此时先不要判断编译器有错。反汇编展示的是机器实际执行的操作，不是把每行 C 翻译成一段固定格式的字幕。读懂它的第一步，是带着一个具体问题寻找证据，而不是从文件第一行逐条翻译。

本篇的问题是：对普通对象和 `volatile` 对象连续写入 1、2，优化后各保留几次写入？顺便检查 F407 的 GPIO 写入到底落在什么地址。所有结果来自同一组小函数的实际 ARM 交叉编译，没有执行它们的裸地址访问，也没有宣称在开发板上观察到波形。

## 固定实验条件再比较

[配套示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/platform)中的两个函数只有指针所指对象的限定符不同：

```c
#include <stdint.h>

void write_twice(uint32_t *p)
{
    *p = UINT32_C(1);
    *p = UINT32_C(2);
}

void write_twice_volatile(volatile uint32_t *p)
{
    *p = UINT32_C(1);
    *p = UINT32_C(2);
}
```

假设指针指向有效、适当对齐且可写的对象，函数调用之后两者的最终值都是 2。但第二个函数还要求编译器按目标实现的 volatile 规则处理每次访问，不能只看最终值。若用第一个函数访问外设，把中间写 1 当成硬件脉冲，就是程序没有正确表达访问要求。

在已配置工具链的终端中，从仓库根目录分别生成两个目标文件：

```sh
arm-none-eabi-gcc -std=c99 -mcpu=cortex-m4 -mthumb \
  -g -O0 -c examples/notes/platform/probes.c \
  -o artifacts/course-completion/platform/probes-O0.o
arm-none-eabi-gcc -std=c99 -mcpu=cortex-m4 -mthumb \
  -g -O2 -c examples/notes/platform/probes.c \
  -o artifacts/course-completion/platform/probes-O2.o
arm-none-eabi-objdump -drS \
  artifacts/course-completion/platform/probes-O2.o
```

`-g` 产生调试信息，不等于关闭优化；`-O0` 与 `-O2` 才是这次比较改变的条件。`-d` 反汇编代码段，`-r` 显示重定位，`-S` 尝试混排对应源代码。源文件不在原位置时，混排可能缺失，但目标指令仍然存在。记录编译器版本、目标 CPU 和全部选项，否则两次输出不同可能不是优化级别造成的。

## 找到真正写对象的指令

本组 ARM GCC 12.3.1 的 O2 输出，去掉地址、机器码和对齐填充后如下：

```asm
write_twice:
    movs r3, #2
    str  r3, [r0, #0]
    bx   lr

write_twice_volatile:
    movs r2, #1
    movs r3, #2
    str  r2, [r0, #0]
    str  r3, [r0, #0]
    bx   lr
```

R0 接收指针参数。普通版本只把 2 写入 R0 指向的位置，因为在这个函数里第一次普通写入的结果立即被覆盖；volatile 版本保留两次目标写入，顺序是 1、2。提前准备 R3 中的常量 2 并不等于提前向目标写 2，要跟踪的是 `str` 的目标地址和数据来源。

O0 版本存在更多栈访问：保存参数到栈，再从栈取回参数，然后写目标对象。不能数文件里一共有几个 `str` 就宣称“外设写了几次”。写 `[r7, #4]` 的指令可能只是保存局部指针，写 `[r3, #0]` 才真正通过它访问对象。先追踪寄存器中的地址，比只认助记符可靠。

这个实验也不能推出 volatile 是线程同步工具。它不自动保证复合读改写的原子性，不替代中断临界区，不为普通内存访问提供通用内存屏障，更不能修复 C 线程数据竞争。观察到两条 `str`，最多回答了这个构建中两次访问是否保留。

## 从基址和偏移确认 GPIO

同一文件的 `pa5_high` 使用教学 GPIO 布局，在 BSRR 中写入 `0x20`。本次 O2 输出如下：

```asm
0:  ldr  r3, [pc, #4]
2:  movs r2, #32
4:  str  r2, [r3, #24]
6:  bx   lr
8:  .word 0x40020000
```

地址 8 附近存放的是常量，第一条指令把该常量载入 R3；第三条才访问 `0x40020000 + 24 = 0x40020018`。结合 RM0090，才能认出这是 GPIOA 的 BSRR。不要把 `.word 0x40020000` 当成程序跳去执行的指令，也不要因为看见 `ldr` 就以为读取了一次 GPIO。

同理，`pa5_input` 的访问落在基址加 16，即 IDR。本次 O2 用 `ubfx r0, r0, #5, #1` 提取 PA5，O0 则用右移再与一相与。二者回答的是同一个位提取问题。精确的寄存器分配、常量池位置与指令选型可能随编译器变化，地址和数据语义才是本篇要核对的不变量。

## 乘法消失，调用地址为零，都不一定异常

`triple_u32` 返回一个 `uint32_t` 输入的三倍，本次优化为：

```asm
add.w r0, r0, r0, lsl #1
bx lr
```

右侧的原始值加上其左移一位的值，得到三倍，结果保留 32 位。给定输入 7，输出是 21；输入 `0xFFFFFFFF`，无符号模运算结果是 `0xFFFFFFFD`。这不是把有符号溢出合理化：若换成 `int` 并超过范围，就不能用观察到的机器指令为 C 的未定义行为背书。

再看调用外部函数的片段：

```asm
bl 0 <external_helper>
    R_ARM_THM_CALL external_helper
```

这个文件只经过 `-c`，外部函数还没链接进来。第二行记录的是“链接时修补这次调用”的重定位请求，零并非最终跳转地址。要确认实际固件调用谁，必须对与下载镜像匹配的最终 ELF 再运行反汇编，必要时配合 map 文件查看符号归属。直接读 BIN 又没有装载地址、符号和调试信息，问题会更难定位。

## 让反汇编回答有限而明确的问题

适合的问题包括：访问宽度是不是预期的字访问？写地址是否正确？某个外部调用是否仍然存在？两次 volatile 写是否保留？不适合只凭它回答的，是实际电平、DMA 可达性、栈最坏占用以及实时延迟。指令条数不是运行周期，Flash 等待、分支、总线竞争和中断都会影响耗时。

优化还会内联函数、合并源代码位置和消去变量。单步时行号跳动或变量显示不可用，不足以证明程序漏跑；先确认当前 ELF 的构建时间、源码、优化和下载镜像一致。需要理解行为时可做 O0/O2 对照，但不能把关闭优化当成并发错误的最终修复。带 LTO 的项目尤其要看最终链接后的代码，而非只看中间目标文件。

## 练习：检查一条看似奇怪的读

有人看到 `ldr r3, [pc, #4]` 就认定 GPIO 已经被读取，同时看到 `bl 0 <external_helper>` 就认定程序一定跳到地址零。分别还需要哪条证据才能下结论？

<details><summary>展开判断依据与答案</summary>

第一处要看 PC 相对访问的目标及其内容。本例读取代码附近的地址常量，后续通过 R3 加偏移才访问 GPIO；不能把加载地址当成加载外设值。第二处要看当前产物是否已链接，以及是否跟着重定位记录。本例是可重定位目标文件，`R_ARM_THM_CALL` 明确要求链接器解析外部符号。应查看最终 ELF 中的调用目标与符号，而不是给占位数值直接赋予运行时含义。

</details>

## 适用范围与依据

本文使用 C99、Cortex-M4 和 GNU Arm 工具链，实际核验了目标文件，不包含硬件执行结果。

- [GNU objdump 文档](https://sourceware.org/binutils/docs/binutils/objdump.html)：`-d`、`-r`、`-S` 的输出含义。
- [GCC Volatiles](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)：volatile 访问与普通对象排序的边界。
- [GCC ARM Options](https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html)：目标 CPU、Thumb 与浮点选项。
- [PM0214](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)，Rev 10，第 3 章：装载存储、移位、位域提取和分支指令。

回到主线：[1.8 CMake 与多文件](/lessons/1-8)、[3.2 调试与排错](/lessons/3-2)。相关分支：[汇编如何读写寄存器](/notes/assembly-register-access)、[Cortex-M 系列差异](/notes/cortex-m-families)。
