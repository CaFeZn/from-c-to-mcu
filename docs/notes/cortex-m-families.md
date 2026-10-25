---
title: 【拓展】Cortex-M系列差异
description: 用同一个浮点加法的交叉编译结果，分清内核架构、可选功能、芯片实现与编译选项。
sourceKey: 【拓展】Cortex-M系列差异
---

# Cortex-M 系列差异

你把 F407 工程换到一颗 Cortex-M0+ 芯片，普通整数控制逻辑还能改一改继续用，浮点加法却多出一个库函数调用；若直接沿用原来的机器码，还可能遇到不支持的指令。反过来，换到主频更高的芯片，DMA 偶尔读到旧数据，也不能只靠“都是 Cortex-M”解释。内核名字提供重要线索，却不是完整芯片规格。

先做一个不接板的实验，再建立选型和移植时的核对顺序。本篇以 F407 的 Cortex-M4 为主，比较 M0+ 的浮点编译结果；其他系列只用来说明差异维度，不给出“数字越大就一定更适合”的排名。

## 同一行加法，两种目标代码

[配套示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/platform)中有一个使用参数而非编译期常量的函数：

```c
float add_float(float a, float b)
{
    return a + b;
}
```

给定 `a = 1.25f`、`b = 2.5f`，数学结果为 `3.75f`，这三个数都可以用二进制浮点精确表示。问题不是谁能算出结果，而是完成运算需要哪些指令、库和调用约定。下面从仓库根目录分别交叉编译：

```sh
arm-none-eabi-gcc -O2 -mcpu=cortex-m4 -mthumb \
  -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -c examples/notes/platform/float_probe.c \
  -o artifacts/course-completion/platform/float-m4.o
arm-none-eabi-gcc -O2 -mcpu=cortex-m0plus -mthumb \
  -mfloat-abi=soft \
  -c examples/notes/platform/float_probe.c \
  -o artifacts/course-completion/platform/float-m0plus.o
arm-none-eabi-objdump -dr \
  artifacts/course-completion/platform/float-m4.o
arm-none-eabi-objdump -dr \
  artifacts/course-completion/platform/float-m0plus.o
```

本次 ARM GCC 12.3.1 的 M4 结果含 `vadd.f32 s0, s0, s1`，M0+ 结果含带重定位的 `bl __aeabi_fadd`。前者使用硬件单精度浮点加法，后者需要链接运行库提供的软件实现。只执行了编译和反汇编，没有运行这两个目标文件，因此 `3.75f` 是给定输入的预期结果，不是实测日志；也不能从两段文本推算精确的运行周期。

M0+ 并不是不能写 `float`，而是没有这里使用的硬件 FPU。软件运算的代码体积、耗时、栈使用都需要放进预算。M4 目标文件能够生成浮点指令，也不证明目标芯片一定实现了 FPU，更不证明启动代码已经启用它。对 F407，数据手册明确有单精度 FPU，固件仍需检查启动配置与工程选项一致。

## 先把三个层次分开

第一层是架构与指令集：它决定哪些指令、异常规则和系统寄存器存在。第二层是具体 Cortex-M 内核及其可选配置：同一类内核的某些单元可以按芯片设计需求选配。第三层是芯片厂商集成的 Flash、SRAM、总线、DMA、时钟、引脚和外设。课程里的 `GPIOA->BSRR` 属于 STM32F407 的外设接口，不是所有 Cortex-M 的通用地址。

下面这张表用于查资料，不是速度排行榜：

| 常见内核 | 应先注意的变化 | 移植时要问的问题 |
|---|---|---|
| M0、M0+ | Armv6-M，指令集较精简 | 是否用了目标不支持的指令或内核寄存器？ |
| M3 | Armv7-M，异常与指令能力不同于 M0 | 现有低层代码依赖了哪些功能？ |
| M4 | 增加 DSP 相关能力，FPU 需核对具体实现 | 本芯片有无 FPU，使用何种浮点 ABI？ |
| M7 | 更重视计算性能，存储系统需重新审查 | 是否存在并启用了数据缓存，DMA 如何共享内存？ |
| M23、M33 | Armv8-M，安全扩展等能力依实现而定 | 是否启用安全域，启动和中断归属如何配置？ |

M4、M7 常用更具体的 Armv7E-M 名称描述带 DSP 扩展的架构能力。较新的 M52、M55、M85 属于 Armv8.1-M 范围，Helium/MVE 又引入向量相关能力，但不要把这些名称套到 F407 上。学习新内核时应查它自己的用户指南，再回到具体器件手册确认实际实现。

## 三个编译选项分别回答什么

`-mcpu` 告诉编译器为哪个处理器选择和调度指令；`-mfpu` 指定允许使用的浮点单元类型；`-mfloat-abi` 还涉及函数之间如何传递浮点参数。最后一个选项不是简单的“开关 FPU”。GCC 明确区分：

- `soft`：浮点运算走软件库调用。
- `softfp`：允许硬件浮点指令，但采用软浮点的调用约定。
- `hard`：允许硬件浮点指令，并采用对应的浮点参数传递约定。

因此“库和应用都用了 M4”不足以保证能链接合作。若应用按 hard 约定传参，库按不兼容的 soft 约定取参，双方对参数位置的认识就不同。应让应用、启动配套配置和预编译库的目标约定一致，不用强行忽略链接器的 ABI 报错。

F407 的 FPU 是单精度，不能据此承诺所有 `double` 运算都由硬件直接完成。表达式中不带 `f` 后缀的浮点常量通常是 `double`，可能把原本的单精度运算提升到双精度路径。先明确所需精度，再看反汇编和性能测量；不能为了看到一条漂亮指令而悄悄改变数值要求。

## 比算术更容易漏掉的是存储和中断

迁移 DMA 代码时，要重新回答“哪个总线主设备可以访问这块内存”。F407 的 64 KiB CCM 数据 RAM 不在 DMA 可访问的普通 SRAM 路径上。CPU 可以读写，不代表 DMA 也可以。它没有 Cortex-M7 式的通用数据缓存，不能为了排查 F407 的 DMA 错误就照搬另一款芯片的缓存清理函数。

与此同时，F407 的 Flash ART 加速相关描述里确实会出现 instruction/data cache 字样。这是 Flash 接口局部加速机制，不等于存在一个能缓存普通 SRAM、需要为 SRAM DMA 做一致性维护的通用 D-cache。判断问题必须看地址所在存储区与总线路径，而不是只搜索“cache”这个单词。

中断方面也不能把某个工程里的优先级位数写死后带到所有系列。CMSIS Device 头文件中的 `__NVIC_PRIO_BITS` 反映芯片实现；异常类型、屏蔽机制以及调试计数器是否可用，都要按目标核对。能共用一套 CMSIS 函数名，不代表所有能力和配置范围完全相同。

最后，实时性不是“主频越高越实时”。中断最坏响应时间还与屏蔽区间、总线阻塞、存储等待、缓存状态和软件调度有关。评估一个固定周期任务时，应测量完整路径的最坏时间并留出余量，而不是只比较芯片宣传页上的 MHz。

## 练习：代码能编译，为什么仍不能直接迁移

某项目在 F407 上用硬浮点库，并把采样缓冲区放在 CCM。现在要移到 M0+，工程人员只改了型号宏，保留原编译选项和链接脚本。请给出至少三项必须重查的内容。

<details><summary>展开迁移检查答案</summary>

首先，目标 CPU 与浮点选项必须匹配 M0+，不能继续生成 M4/FPU 指令，相关库也要换成兼容调用约定和指令集的版本。其次，链接脚本必须按新芯片实际存储区重写，不能继续假设有相同地址和容量的 CCM；即使某存储区对 CPU 可见，也要查 DMA 是否能访问。再次，启动文件、向量表、IRQ 编号和优先级位数应换成目标器件对应版本。然后才是重新预算软件浮点耗时和缓冲区大小。只让 C 预处理宏变了，并没有让机器指令与芯片硬件一起变对。

</details>

## 适用范围与依据

- [Arm CMSIS-Core Overview](https://arm-software.github.io/CMSIS_6/latest/Core/index.html)，Processor Support 与 Armv8-M/Armv8.1-M 小节：架构分组及可选扩展。
- [GCC ARM Options](https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html)：`-mcpu`、`-mfpu`、`-mfloat-abi` 及 ABI 兼容限制。
- [PM0214](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)，Rev 10，1.3.3、1.3.4、4.6：STM32 Cortex-M4 单精度 FPU 与内核外设。
- [RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)，Rev 22，2.1、2.3、第 3 章：F407 总线、CCM 与 Flash 接口。

回到主线：[2.1 单片机里有什么](/lessons/2-1)、[2.4 程序是怎么跑起来的](/lessons/2-4)。相关分支：[MCU、MPU 与 SoC](/notes/mcu-mpu-soc)、[看反汇编](/notes/reading-disassembly)。
