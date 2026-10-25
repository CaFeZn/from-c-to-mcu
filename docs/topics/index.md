---
title: 原理与拓展
outline: [2, 2]
---
# 原理与拓展

主线先解决眼前的问题；需要更多解释时，再从这里继续。这里有 **100 篇原理与拓展**，以及 **2 篇工程实践**，不需要按目录从头读完。每篇都给出具体例子、验证方法、适用边界和练习答案。

刚开始学习？先回 [1.0 跑通 hello](/lessons/1-0)。原理选读不会替代动手；板级示例的文档与编译检查也不等于真实硬件验收。

## 已有原理入口

- <span id="topic-92a4be8ed3"></span>[【原理】交叉编译与工具链](/notes/cross-compilation)
- <span id="topic-30b3aa2d2d"></span>[【原理】从源码到可执行文件](/notes/source-to-program)
- <span id="topic-a842ebf1eb"></span>[【原理】单片机内部结构](/notes/mcu-architecture)
- <span id="topic-5566ad0f25"></span>[【原理】寄存器与内存映射](/notes/memory-mapped-io)
- <span id="topic-a5b48ea0d5"></span>[让 VS Code 看懂 CMake 工程](/notes/editor-setup)

## 先把工程保存清楚

从已跑通的 hello 开始，保存可用版本、记录复现步骤。

- [【实践】用Git保存每一次可用的进展](/notes/git-workflow)
- [【实践】用Markdown写清楚工程](/notes/markdown-notes)

## C 语言：对象、内存与布局

从数组和结构体的实际访问，理解地址、边界与对象寿命。

- <span id="topic-fd4e5e1d61"></span>[【原理】变量与内存](/notes/variables-memory)
- <span id="topic-8283207c9a"></span>[【原理】数组退化与指针算术](/notes/array-pointer-arithmetic)
- <span id="topic-90a89924dd"></span>[【原理】结构体对齐与填充](/notes/struct-alignment)
- <span id="topic-1980ba1093"></span>[【原理】缓冲区溢出与边界检查](/notes/buffer-bounds)
- <span id="topic-9229a999ab"></span>[【拓展】野指针与悬空指针](/notes/pointer-lifetime)
- <span id="topic-b7683f6b52"></span>[【拓展】packed位域与union](/notes/packed-bitfields-unions)
- <span id="topic-3206379458"></span>[【拓展】字节序之争](/notes/byte-order)

## C 语言：数值与表示

把转换、溢出和精度拆成可计算的例子。

- <span id="topic-d6e42da962"></span>[【原理】补码与整型转换](/notes/integer-conversions)
- <span id="topic-00fa1830e6"></span>[【拓展】C为什么不检查溢出](/notes/integer-overflow)
- <span id="topic-1808db85e6"></span>[【拓展】浮点与定点](/notes/float-fixed-point)
- <span id="topic-c0918976ce"></span>[【拓展】补码为什么成了标准](/notes/twos-complement)
- <span id="topic-6160c94390"></span>[【拓展】饱和运算](/notes/saturating-arithmetic)
- <span id="topic-55688240ed"></span>[【拓展】随机数与熵](/notes/randomness-entropy)

## C 语言：调用与组织

从函数调用和对象状态，走到接口、回调与设计取舍。

- <span id="topic-047843ecf5"></span>[【原理】函数指针与间接调用](/notes/function-pointer-calls)
- <span id="topic-044ac95db9"></span>[【原理】栈与函数调用](/notes/stack-function-calls)
- <span id="topic-87cb01baee"></span>[【原理】C语言里的面向对象](/notes/objects-in-c)
- <span id="topic-c26b3ee1aa"></span>[【拓展】函数指针与虚表](/notes/function-tables)
- <span id="topic-23597877a4"></span>[【拓展】设计模式是什么](/notes/design-patterns)
- <span id="topic-2b6b2a6a0c"></span>[【拓展】C++入门与嵌入式C++](/notes/embedded-cpp)
- <span id="topic-9f298902d6"></span>[【拓展】调用约定与尾调用优化](/notes/calling-conventions)

## C 语言：编译器与语言边界

区分“这次跑出来了”和语言、编译器真正保证了什么。

- <span id="topic-3ac2c59cbb"></span>[【原理】volatile与编译器优化](/notes/volatile-optimization)
- <span id="topic-b31e331ea1"></span>[【拓展】const与restrict](/notes/const-restrict)
- <span id="topic-c8dcede207"></span>[【拓展】严格别名与未定义行为](/notes/strict-aliasing)
- <span id="topic-57cada87f4"></span>[【拓展】参数求值顺序与未定义行为](/notes/evaluation-order)
- <span id="topic-0b785ef4e2"></span>[【拓展】动态内存与碎片](/notes/dynamic-memory)
- <span id="topic-656bb04fc8"></span>[【拓展】strtok为什么不可重入](/notes/strtok-reentrancy)
- <span id="topic-69f7cfbd35"></span>[【拓展】宏的高级用法](/notes/advanced-macros)

## 构建、链接与启动

需要拆开一次构建时再来，不是第一章的前置。

- <span id="topic-9861de3d70"></span>[【原理】CMake如何工作](/notes/cmake-how-it-works)
- <span id="topic-e40f309844"></span>[【原理】预处理器与链接](/notes/preprocessor-linkage)
- <span id="topic-13b2a37518"></span>[【原理】可执行文件格式](/notes/executable-formats)
- <span id="topic-9f4e84f763"></span>[【原理】启动流程与链接脚本](/notes/startup-linker-script)
- <span id="topic-05899f1fa5"></span>[【原理】弱符号如何让回调生效](/notes/weak-symbols)
- <span id="topic-e6f9e6ecda"></span>[【拓展】CMake与增量编译](/notes/incremental-builds)
- <span id="topic-58c3a88045"></span>[【拓展】在Windows上安装现代gcc](/notes/windows-gcc)

## 芯片、体系结构与工具

把平台相关假设标清楚，再去看手册和生成的代码。

- <span id="topic-4130e9bab0"></span>[【拓展】CMSIS如何用结构体描述外设](/notes/cmsis-peripheral-structs)
- <span id="topic-b1a475d36a"></span>[【拓展】Cortex-M系列差异](/notes/cortex-m-families)
- <span id="topic-1c5a160b70"></span>[【拓展】MCU MPU与SoC的区别](/notes/mcu-mpu-soc)
- <span id="topic-6115f4998a"></span>[【拓展】STM32Cube工具生态](/notes/stm32cube-tools)
- <span id="topic-8b20d4d7d1"></span>[【拓展】STM32型号命名与勘误表](/notes/stm32-part-errata)
- <span id="topic-845b7233ad"></span>[【原理】汇编如何读写寄存器](/notes/assembly-register-access)
- <span id="topic-533ccbd35b"></span>[【拓展】看反汇编](/notes/reading-disassembly)

## GPIO、电路与测量

先检查电气边界，再看逻辑值、引脚功能和观测结果。

- <span id="topic-364815701a"></span>[【原理】GPIO内部电路](/notes/gpio-circuits)
- <span id="topic-251e35b8fb"></span>[【原理】引脚复用机制](/notes/alternate-functions)
- <span id="topic-0283ec4a59"></span>[【原理】按键抖动与消抖](/notes/button-debounce)
- <span id="topic-8c1b375621"></span>[【拓展】5V容忍引脚与电平转换](/notes/voltage-tolerance)
- <span id="topic-8ae96519e7"></span>[【拓展】GPIO电气参数与保护](/notes/gpio-electrical)
- <span id="topic-6d80fe0201"></span>[【拓展】有源蜂鸣器与无源蜂鸣器](/notes/buzzers)
- <span id="topic-5a4ffad028"></span>[【拓展】示波器与逻辑分析仪怎么用](/notes/scope-logic-analyzer)

## 时钟、定时与调度

区分计数、事件、实际时间和调度延迟。

- <span id="topic-f886218325"></span>[【原理】PWM与预装载](/notes/pwm-preload)
- <span id="topic-5cf3f0b207"></span>[【原理】SysTick与HAL时基](/notes/systick-hal-timebase)
- <span id="topic-27ed29f86a"></span>[【原理】定时器工作原理](/notes/timer-principles)
- <span id="topic-54f0a74960"></span>[【原理】时钟树](/notes/clock-tree)
- <span id="topic-b5e8e80d53"></span>[【拓展】RTC与32.768kHz长期计时](/notes/rtc-timekeeping)
- <span id="topic-351e4f47d7"></span>[【拓展】晶振精度与时钟抖动](/notes/clock-accuracy-jitter)
- <span id="topic-a5b92c7bf4"></span>[【拓展】测频法与测周法](/notes/frequency-period-measurement)
- <span id="topic-6950aa3e75"></span>[【拓展】编码器模式与死区](/notes/encoder-deadtime)
- <span id="topic-4bebe7f78e"></span>[【拓展】软件定时器与简易调度器](/notes/software-timers)

## ADC 与 DMA 数据流

从采样窗口到缓冲交接，明确数据何时有效、谁能访问。

- <span id="topic-d44142b319"></span>[【原理】ADC工作原理与采样保持](/notes/adc-sample-hold)
- <span id="topic-42f46c1bb1"></span>[【原理】DMA工作原理](/notes/dma-principles)
- <span id="topic-b671ac7708"></span>[【拓展】DMA半传输与流式处理](/notes/dma-streaming)
- <span id="topic-b520d73a1e"></span>[【拓展】定时器触发ADC采样](/notes/timer-triggered-adc)
- <span id="topic-31e7df08f6"></span>[【拓展】用内部参考电压校准ADC](/notes/adc-internal-reference)

## 串口、协议与状态机

从字节流开始，逐步处理边界、吞吐、校验和业务状态。

- <span id="topic-e5ea18a823"></span>[【原理】printf为什么慢又贵](/notes/printf-cost)
- <span id="topic-bd589f9b90"></span>[【原理】UART帧格式与波特率](/notes/uart-framing)
- <span id="topic-58cae8dcd3"></span>[【原理】环形缓冲区](/notes/ring-buffer)
- <span id="topic-43c4f5d395"></span>[【原理】CRC校验](/notes/crc-principles)
- <span id="topic-b81efe4e7a"></span>[【拓展】CRC32与查表实现](/notes/crc32-table)
- <span id="topic-09dfe9a1f4"></span>[【拓展】HAL回调的注册方式](/notes/hal-callbacks)
- <span id="topic-c3cde9e953"></span>[【拓展】串口流控与USB CDC](/notes/flow-control-usb-cdc)
- <span id="topic-3b0197c059"></span>[【拓展】文本协议与二进制协议](/notes/text-binary-protocols)
- <span id="topic-3289bd0b6d"></span>[【拓展】状态机设计](/notes/state-machines)

## CAN：从一帧到一条总线

以 STM32F407 bxCAN / Classical CAN 为具体例子，其他协议明确区分。

- <span id="topic-b68aac71f7"></span>[【原理】CAN位定时与采样点](/notes/can-bit-timing)
- <span id="topic-eedae309b7"></span>[【原理】CAN总线电气与仲裁](/notes/can-electrical-arbitration)
- <span id="topic-361a827ec8"></span>[【原理】CAN错误状态机](/notes/can-error-states)
- <span id="topic-23c58ad819"></span>[【拓展】bxCAN过滤器组详解](/notes/bxcan-filters)
- <span id="topic-626c58cc2e"></span>[【拓展】CAN FD与CANopen简介](/notes/can-fd-canopen)
- <span id="topic-fdcc037cc5"></span>[【拓展】CAN总线拓扑与长度](/notes/can-topology)
- <span id="topic-7482b35af2"></span>[【拓展】CAN扩展帧与远程帧](/notes/can-extended-remote)
- <span id="topic-4995242c18"></span>[【拓展】总线负载与优先级设计](/notes/can-load-priority)

## 异常、中断与并发

先画出可能的交错，再选择临界区、原子操作和同步方式。

- <span id="topic-c6aafc617b"></span>[【原理】读改写与原子性](/notes/read-modify-write)
- <span id="topic-41411edc75"></span>[【原理】中断与NVIC](/notes/interrupts-nvic)
- <span id="topic-883e343bf4"></span>[【拓展】中断向量表的由来](/notes/vector-table)
- <span id="topic-d957cbcc46"></span>[【拓展】中断风暴与忘清标志](/notes/interrupt-storms)
- <span id="topic-e8cd0fb02e"></span>[【拓展】LDREX与STREX原子指令](/notes/exclusive-access)
- <span id="topic-391285efe2"></span>[【拓展】内存屏障与指令重排](/notes/memory-barriers)
- <span id="topic-6f60bb27f1"></span>[【拓展】序号锁与双缓冲](/notes/sequence-double-buffer)
- <span id="topic-b4c25cc00a"></span>[【拓展】从协作式到抢占式调度](/notes/cooperative-preemptive)

## 调试、故障与量产日志

保留现场、缩小范围；调试工具和持久化方案都有边界。

- <span id="topic-eade08a93a"></span>[【原理】HardFault现场分析](/notes/hardfault-analysis)
- <span id="topic-b17fbd069b"></span>[【原理】SWD调试原理](/notes/swd-debugging)
- <span id="topic-ab9004b540"></span>[【拓展】HardFault现场dump与复位后回放](/notes/fault-persistence)
- <span id="topic-788d1abfc0"></span>[【拓展】SEGGER_RTT与SWO替代串口日志](/notes/rtt-swo)
- <span id="topic-a67f307230"></span>[【拓展】SWD口的安全与断点数量限制](/notes/swd-security-breakpoints)
- <span id="topic-81a1612b77"></span>[【拓展】SWD引脚被占用后的救援](/notes/swd-recovery)
- <span id="topic-79e51de039"></span>[【拓展】为什么能跑和能调试是两件事](/notes/run-vs-debug)
- <span id="topic-762d8175bd"></span>[【拓展】量产固件的日志策略与日志泄露](/notes/production-logging)
