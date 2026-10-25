---
title: 【拓展】为什么能跑和能调试是两件事
description: 用相同程序的不同构建证明运行、符号、断点和时序各有条件，建立优化固件与脱机运行的验证矩阵。
sourceKey: 【拓展】为什么能跑和能调试是两件事
---

# 为什么能跑和能调试是两件事

板子正常闪灯，断点却显示灰色；换成 Debug 后断点能停，原来的偶发超时又不见了。更怪的是，连接探头时程序一直正常，拔掉探头重新上电就卡在第一次打印。

不要把这三种现象合并成“Release 不适合调试”。它们分别可能涉及源码映射、优化改变时序、运行时依赖调试器。我们先做一个没有硬件的不变量实验，再把四个层次逐个拆开。

## 同一份源码，三个构建

配套程序只计算 1 到 10 的和。完整核心如下，边界是已知的小输入，使用 C99 无符号整数，不用越界或有符号溢出来制造“优化器怪事”。

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t sum_to(uint32_t n)
{
    uint32_t sum = 0u;
    uint32_t i;
    assert(n <= 1000u);
    for (i = 1u; i <= n; ++i) sum += i;
    return sum;
}

int main(void)
{
    uint32_t result = sum_to(10u);
    assert(result == 55u);
    printf("sum=%lu\n", (unsigned long)result);
    return 0;
}
```

先预测：只去掉调试信息，结果会变吗？启用优化但保留调试信息，程序还能不能运行？然后在仓库根目录，用本机 GCC 做这三组构建：

```sh
gcc -std=c99 -O0 -g examples/notes/debug/optimization.c -o artifacts/course-completion/debug/plain.exe
gcc -std=c99 -O0 -g0 examples/notes/debug/optimization.c -o artifacts/course-completion/debug/no-debug.exe
gcc -std=c99 -O2 -g examples/notes/debug/optimization.c -o artifacts/course-completion/debug/optimized.exe
```

第一次使用时先建立对应输出目录，或先运行本组 CMake 示例。逐个运行，预期都是 `sum=55`。本次使用 MinGW GCC 6.3.0 的独立构建验证了结果一致。Windows 可执行文件不是给 F407 下载的；这里验证的是编译器选项关系。

再生成优化后的汇编：

```sh
gcc -std=c99 -O2 -g -S examples/notes/debug/optimization.c -o artifacts/course-completion/debug/optimized.s
```

本次产物中，主函数把常量 55 直接交给输出函数，求和循环不再需要运行。循环变量没有某个“正在 RAM 里变化的值”，不是调试器读坏了内存。其他编译器版本可能采用不同指令序列，但仍须保留定义良好的可观察行为。

## 调试信息和优化是两个开关

`-g` 让编译器生成调试信息；`-O0`、`-Og`、`-O2`、`-Os` 调整优化策略。GCC 明确允许同时用 `-g` 与优化选项。Debug、Release 是构建系统的配置名称，不是 C 语言定义的两种程序。

主线工程目前把 Debug 配成 `-O0 -g3`、Release 配成 `-Os -g0`，这是那份工程的选择，不是无法改变的硬规则。定位只在发布优化下发生的问题时，应保留实际优化与相关链接配置，同时生成并归档相匹配的符号。否则你调试的是另一个程序布局和时序。

`-O0` 通常让源码跟随更直观，但也不保证“每行源码一条机器指令”。一行有多个表达式、宏、函数调用和隐式类型转换，都会破坏这种一一对应想象。`-Og` 是 GCC 面向调试体验的优化选择，也需要按工具链版本实际验证。

调试信息通常不属于目标需要执行的可装载机器码。带符号 ELF 在磁盘上大，不表示全部大小都要写进 MCU Flash；应看可装载段、映射文件和存储区域使用情况。与此同时，剥掉符号也不会自动消除固件中的日志字符串与秘密。

## 不要用“文件里有 debug 段”作为唯一证明

可用 `objdump -h` 检查调试段，但本机实验还发现一个细节：即使当前源码用 `-g0` 编译，最终可执行文件仍可能带上链接库自己的调试段。因此更精确的对照是先把当前源码编译成目标文件，再比较这个目标文件：

```sh
gcc -std=c99 -O0 -g0 -c examples/notes/debug/optimization.c -o artifacts/course-completion/debug/no-debug.o
gcc -std=c99 -O2 -g -c examples/notes/debug/optimization.c -o artifacts/course-completion/debug/optimized.o
objdump -h artifacts/course-completion/debug/no-debug.o
objdump -h artifacts/course-completion/debug/optimized.o
```

本次后者含 `.debug_info`、`.debug_line` 等，前者没有这些当前翻译单元的段。这个现象提醒我们：验证应对准自己的函数和源码行，不能只搜索整个文件里有没有某个单词。

## 能跑、能连、能定位、能复现，分别查

| 问题 | 必需证据 |
|---|---|
| 能运行吗 | 正确机器码、向量/入口、初始化、供电、时钟和外设条件 |
| 能连探头吗 | 调试电气链路、访问权限、复位与低功耗状态 |
| 能定位源码吗 | 匹配的 ELF/调试信息、可访问源码、对应的代码地址 |
| 能复现故障吗 | 正确输入、时序、优化配置、探头影响的对照 |

下载校验只覆盖其中一部分。源文件后来改过、板子仍跑旧固件，而调试器装载新 ELF，会出现“断在不可能执行的行”。固件发布时应把构建标识、精确 ELF、映射文件和编译选项一并保存。重新编译一份相同版本号，不保证地址仍相同。

断点也依赖机器指令真正存在。一行被合并、内联或删除，行断点可能移动或无法安放；变量可能在寄存器中、分散在多个位置、某段范围内不可恢复。`optimized out` 是信息不可恢复的声明，不该用随便加 `volatile` 当通用补丁。那会改变程序访问语义，可能掩盖原来的时序问题。

## 为什么接上探头反而正常

有三类依赖值得分别排除：

1. **semihosting**。某些 C 库输出请求由调试代理在主机完成，目标可能为此暂停。没有代理时，不受控的断点请求可能进入故障等路径。产品应改用明确的 UART/其他后端，或采用经过设计的降级方式，不盲目跳过所有 HardFault。
2. **调试冻结**。F407 的 DBGMCU 配置可让某些定时器、IWDG/WWDG 在内核停止时冻结。若你在断点里“等了十秒也没复位”，可能只是冻结设置在起作用；不能据此证明脱机看门狗正常。
3. **观察改变时序**。暂停让竞争窗口消失，也可能让外设超时；自动读寄存器可能取走数据。对偶发故障要考虑低侵入计数、事件快照、跟踪，而不是无限增加打印和单步。

还应确认调试启动脚本是否帮助完成了 RAM 装载、时钟或外部存储初始化。独立冷启动必须由固件自己完成需要的初始化；IDE 的便利不能成为隐藏的启动步骤。

## 一张小矩阵比反复点 F5 更可靠

对同一份已归档固件，至少比较以下场景：

- 连接探头但不暂停，使用实际优化与符号。
- 断开探头独立上电，验证日志出口无人接收时仍有界运行。
- 主动触发受控超时或复位，检查复位原因与重新初始化。
- 在允许暂停的非危险实验环境中停住内核，记录哪些外设继续、哪些被冻结。

每一项记录固件标识和观察手段。调试优化版不是为了获得完美源码动画，而是为了在真实程序上收集足够可靠的证据。

### 练习：给 Release 加上 -g 就变成 Debug 了吗

发布故障只在 `-Os` 时出现。你给该配置增加 `-g`，还能称它是优化构建吗？一个局部变量仍显示被优化掉，是否证明符号没生成？

<details><summary>答案</summary>

仍是优化构建，调试信息和优化独立。应继续核对实际编译命令中的有效选项、链接优化和其他配置，不只看 preset 名称。局部变量可能没有独立运行时存储，即使带有符号也不能恢复不存在的值。可以结合反汇编、寄存器、上下文与输入推断，但不能让调试器凭空造出变量。

</details>

## 适用范围与依据

主机三类构建及目标文件检查已经执行，未做 GDB 交互会话、板级 semihosting、看门狗或脱机上电试验。生成汇编的具体形式仅代表这次工具链，不作为所有编译器的指令保证。

- [GCC：Options for Debugging Your Program](https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html)：`-g` 与优化可共存，`-Og` 建议及调试级别。
- [GDB：Debugging Optimized Code](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Optimized-Code.html)：优化后的执行路径、变量消失与源码映射限制。
- [SEGGER：Semihosting](https://kb.segger.com/Semihosting)：调试代理参与 I/O 与脱离调试器后的运行边界。
- [ST RM0090 Rev 22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第 38 章 DBGMCU，DBG_IWDG_STOP、DBG_WWDG_STOP 与相关冻结控制。

回到 [3.2 调试与排错](/lessons/3-2)；继续看 [看反汇编](/notes/reading-disassembly)、[volatile 与优化](/notes/volatile-optimization)、[SWD 调试原理](/notes/swd-debugging)。
