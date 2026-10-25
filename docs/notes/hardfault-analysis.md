---
title: 【原理】HardFault现场分析
description: 从故障状态寄存器、异常栈帧和匹配的固件符号中，区分故障原因与现场失真。
sourceKey: 【原理】HardFault现场分析
---

# HardFault 现场分析

灯不闪了。按暂停，编辑器停在 `HardFault_Handler` 的死循环。你已经知道处理器出了事，却还不知道是谁、在什么条件下、用哪个地址把它带到这里。此时先重启再改一行代码，等于先清扫现场再找脚印。

本篇从 [3.2 调试与排错](/lessons/3-2) 的“认出 HardFault”继续走：先记录，再解释，最后才修复。对象是 **STM32F407 的 Cortex-M4**，不是所有 Cortex-M 都有同一组故障寄存器。

## 先做一次不会破坏板子的纸上验尸

假设调试器刚进入处理函数，读到以下快照。EXC_RETURN 来自入口 LR，基本帧索引从 0 起。地址是教学输入，不是某份实际固件的符号地址。

| 证据 | 数值 |
|---|---|
| EXC_RETURN | `0xFFFFFFF9` |
| 入口 MSP | `0x20001000` |
| HFSR | `0x40000000` |
| CFSR | `0x00010000` |
| 基本帧[6] | `0x08001234` |
| 基本帧[7] | `0x01000000` |

先预测：这是“在访问数据时地址不合法”，还是“执行了一条未定义指令”？能不能因为调试窗口显示 BFAR 为零，就断言访问了空指针？

<details><summary>展开核对</summary>

HFSR 的 bit 30 是 FORCED，表示一个可配置优先级的故障无法正常处理，升级为 HardFault。CFSR 的 bit 16 是 UNDEFINSTR，因此这里应追查未定义指令。CFSR 的 bit 15 BFARVALID 没有置位，BFAR 当前数值没有有效故障地址的意义。入口 LR 表明这是回到 Thread 模式、使用 MSP 的基本帧；在帧可信的前提下，PC 为 `0x08001234`，xPSR 的 T 位为 1。

</details>

这套推理没有执行非法 C 语句，也不要求你去碰一片未知的地址空间。实际故障必须先冻结证据：固件构建标识、HFSR、CFSR、MMFAR、BFAR、入口 MSP/PSP、入口 LR、可读取的栈字，以及当时的 PC/LR。

## 为什么处理函数里的 LR 不是罪魁祸首

异常进入时，硬件会在被打断上下文使用的栈上保存现场。**非浮点基本帧**按从低地址到高地址排列如下，每项 4 字节：


| 索引 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| 内容 | R0 | R1 | R2 | R3 | R12 | 原 LR | PC | xPSR |

硬件随后把特殊的 EXC_RETURN 值放入处理函数的 LR。它描述“异常返回该用哪个栈、回到哪种模式”，不是普通函数的返回地址。

- `0xFFFFFFF1`：返回 Handler 模式，用 MSP。
- `0xFFFFFFF9`：返回 Thread 模式，用 MSP。
- `0xFFFFFFFD`：返回 Thread 模式，用 PSP。

因此 RTOS 任务在 PSP 上出错时，进入 HardFault 后直接读取当前 MSP，再把它当成任务现场，会找错栈。若已经进入普通 C 函数，编译器还可能在 MSP 上压入自己的寄存器，函数调用也会改写 LR。入口数据应该由最前面的短汇编桥接捕获，或由调试器在入口第一条指令处读取。

板级桥接通常按 EXC_RETURN 的 bit 2 选 MSP/PSP，把入口 SP 和 LR 传给后续处理；它不是“随便加一个 naked 属性就安全”。桥接、ABI、紧急栈和链接位置需要一起验证。本篇不把一段缺少这些前提的处理函数包装成通用固件。

### 先判断能不能读，再谈 PC

按本工程链接区间检查 SP 对齐、完整帧是否落在已知可读的栈区。例子里的普通 SRAM 上界是 `0x20020000`，这是 F407 SRAM1/2 的结尾之后，不应推广到别的芯片，也不能代替某个任务实际分配的栈边界。为防止地址相加回绕，用“SP 不超过上界减帧长”判断，而不是先计算 SP+32。

CFSR 的 MSTKERR、STKERR 表示入栈已经失败；MUNSTKERR、UNSTKERR、MLSPERR、LSPERR 也提示返回或浮点现场不可靠。此时先保留状态和原始 SP，**不要为了打印 PC 再次解引用坏地址**。本组主机实验采用保守策略：遇到这些位或非基本 EXC_RETURN 就拒绝解码。

Cortex-M4F 还存在浮点扩展帧和 lazy stacking。EXC_RETURN bit 4 为 0 时，不得直接套用本篇基本帧模型；需要按实际 FPU 保存状态及手册布局另行解析。xPSR 的栈对齐标记也影响恢复异常前 SP 的计算，不能把“帧后 32 字节”永久当作原 SP。

## 把状态位变成可检验的假设

按下面顺序阅读，而不是只搜索十六进制值：

1. **HFSR**：FORCED 表示继续找下级原因；VECTTBL 表示读取异常向量时出现总线错误，不是普通 C 函数调用失败。
2. **CFSR**：低 8 位是 MemManage 状态，中 8 位是 BusFault 状态，高 16 位是 UsageFault 状态。同一快照可能保留多个标志，不保证只有一个根因。
3. **有效位**：只有 MMARVALID/BFARVALID 置位，才把对应地址作为有效线索。先把状态和地址一起保存，别先清标志。
4. **可信栈帧**：由帧中的 PC、LR 与对应 ELF 的反汇编，确定访问指令和上下文。

PRECISERR 的堆栈 PC 指向造成精确数据总线错误的指令；IMPRECISERR 是异步报告的非精确错误，堆栈 PC 不保证对应最初失败的写入。遇到后者应看之前的存储操作、总线目标和时序，而不是把 PC 所在源码行直接定罪。

故障寄存器中的很多状态位是“写 1 清除”。调试期间应先拍快照，不用 `SCB->CFSR |= mask` 一类读改写来“清某一位”，否则读到的其他置位标志也可能被清掉。清除策略应服从手册和是否返回的设计。

## 再回到一份真正的 ELF

把实际快照的 PC 交给与你下载版本完全对应的工具链。下面命令中的地址只是占位示例，必须换成自己的值：


```sh
arm-none-eabi-addr2line -e firmware.elf -f -C 0x08001234
arm-none-eabi-objdump -d -S firmware.elf
```

地址定位给出函数与源码行只是线索，反汇编才能看清访问宽度、基址来自哪个寄存器、是读还是写。优化、内联、宏展开都会让一行源码对应多段机器码；反过来也可能多行被合并。拿今天重新编译的 ELF 去解释昨天现场，即使函数名字看起来合理，也不构成证据。

若想做板级故障注入，可在**可恢复的实验固件**中用 GCC/Clang 的目标专用汇编执行 `__asm volatile ("udf #0");`，并明确是否启用了 UsageFault：未使能、且无其他限制时会升级成 HardFault；在 Thread 模式中使能、未屏蔽且优先级允许时，先进入 UsageFault。测试前确认处理函数、下载救援与复位路径。不要用“空指针调用必然输出某三行”作为 C 语言保证——那是未定义行为，不能拿它的预期结果当可靠测试规格。

## 可执行的安全边界实验

仓库根目录运行下面的主机模型；Windows 单配置生成器产物带 `.exe` 后缀：

```sh
cmake -S examples/notes/debug -B artifacts/course-completion/debug/build -G Ninja
cmake --build artifacts/course-completion/debug/build
./artifacts/course-completion/debug/build/debug_models.exe
```

第一行预期为：

```text
frame: basic accepted, extended/bad-stack rejected
```

输入覆盖 MSP/PSP 的基本帧、恰好贴住区间上界的帧、越界与非对齐 SP、入栈错误、扩展帧。它只验证整数区间判断，**不解引用数值地址，不模拟异常硬件，更不证明真实 SRAM 仍可访问**。

## 最容易让现场二次死亡的动作

- 在 HardFault 中先调用复杂的 LOG、printf 或动态分配。原来的栈、锁、堆、日志缓冲本身都可能已经坏了；普通中断也无法像主循环里那样推进 DMA 发送流程。优先留下最小记录，恢复启动后再格式化。
- 即使改成 HAL 轮询发送，带一个非零超时参数也不自动安全。默认 HAL 时基靠 SysTick 中断推进；在 HardFault 或关闭中断后，依赖 `HAL_GetTick()` 的超时可能不再前进。应使用故障环境仍能工作的有界机制，而不是只把“100 ms”传进去就认为一定能返回。
- 只改堆栈 PC，试图“跳过出错指令继续跑”。不知道错误是否已经破坏状态、指令多长、异常能否合法返回，这种做法会把确定故障变成延迟损坏。默认应进入受控故障状态或复位，而非盲目恢复。

### 练习：这个地址能相信吗

快照为 HFSR=`0x40000000`、CFSR=`0x00000400`，BFAR=`0xDEADBEEF`，帧没有入栈错误。你能据此断言 PC 所在行写坏了 BFAR 所指地址吗？如果再看到 STKERR 呢？

<details><summary>答案</summary>

不能。这里只有 IMPRECISERR，BFARVALID 未置位，BFAR 不能当有效地址；非精确错误也不能把堆栈 PC 绑定到原始故障写入。若再出现 STKERR，连帧中的 PC 都可能不可信，应把原始状态和 SP 保存下来，先查栈范围、溢出与入栈失败。

</details>

## 适用范围与依据

主机区间模型已按 C99 编译运行；本文未执行板级异常注入、SWD 读寄存器或真机栈回溯。保守拒绝扩展帧是本实验的范围限制，不表示浮点程序无法分析。

- [WG14 N1256（C99 与技术勘误合并草案）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：6.5.2.2 函数调用、6.5.3.2 非法指针间接访问及脚注 87；空函数指针调用不是可移植的硬件故障测试。
- [ST PM0214 Rev 10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：§2.3.7 异常进入与返回、图 12 和表 18；§4.4.10–4.4.16 故障状态与地址寄存器。
- [ST CMSIS Cortex-M4 头文件](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/core_cm4.h)：SCB 的 CFSR/HFSR 字段与状态位定义，与手册交叉核对。

- [ST HAL UART 源码](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_uart.c)：`UART_WaitOnFlagUntilTimeout` 通过 `HAL_GetTick()` 判断超时；默认时基见同版本 [HAL 通用源码](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal.c) 的 `HAL_IncTick` 和 `HAL_GetTick`。

回到 [3.2 调试与排错](/lessons/3-2)；接着看 [故障记录与复位后回放](/notes/fault-persistence)、[栈与函数调用](/notes/stack-function-calls)、[看反汇编](/notes/reading-disassembly)。
