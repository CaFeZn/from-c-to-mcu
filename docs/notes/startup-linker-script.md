---
title: "【原理】启动流程与链接脚本"
description: "沿复位向量、数据搬运和BSS清零追到main，并用链接映像核对Flash初值与RAM地址。"
sourceKey: "【原理】启动流程与链接脚本"
---

# 启动流程与链接脚本

你把 `unsigned mode = 7;` 写在函数外，程序一进 `main` 就能读到七；另一个没有显式初始化的全局数组却全是零。Flash 断电后保存程序，而普通 SRAM 上电内容不能当作可靠初值，这两种结果是谁准备的？如果只把机器指令烧进去，然后直接跳到 `main`，C 代码所依赖的运行环境很可能尚未建立。

## 先追三个对象的地址

使用[本组ARM离线示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build/arm)。`image.c` 定义 `volatile unsigned initialized = 7;`、`volatile unsigned zeros[4];`、`volatile unsigned observed;`。`main` 只执行 `observed = initialized + zeros[0];` 后停在循环里。我们不读串口，而是先证明“构建结果提供了正确初始化所需的信息”。

命令在仓库根目录 PowerShell 执行，ARM工具需已能从当前终端找到。下面是真实离线核验所用选项的简化组织：

```powershell
$S = "examples/notes/build/arm"
$B = "artifacts/course-completion/build/startup"
New-Item -ItemType Directory -Force $B | Out-Null
$F = @("-mcpu=cortex-m4", "-mthumb", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
       "-ffreestanding", "-ffunction-sections", "-fdata-sections",
       "-fno-unwind-tables", "-fno-asynchronous-unwind-tables")
arm-none-eabi-gcc @F -std=c99 -nostdlib -T "$S/layout.ld" "-Wl,--gc-sections" "-Wl,-Map=$B/image.map" "$S/startup.S" "$S/image.c" -o "$B/image.elf"
arm-none-eabi-nm -n "$B/image.elf"
arm-none-eabi-objdump -h -d "$B/image.elf"
```

在 ARM GCC 12.3.1 的结果中，`initialized` 位于 `0x20000000`，`zeros` 位于 `0x20000004`，`observed` 位于 `0x20000014`。`_sidata` 却等于 `0x0800006c`。先别把这个差异当错：一个是运行时访问的 RAM 地址，一个是烧进 Flash 的初值地址。

**实验不是完整固件。** 自制启动文件只放了最初两个向量，没有完整异常/中断表，也没有系统时钟、FPU和C库初始化，不应替换真实项目启动文件或直接烧到板上。已做的是编译、链接、符号和反汇编检查，没有真机或模拟器执行记录。

## 复位后首先读取的不是main

以 STM32F407 从主 Flash 启动为具体场景。Cortex-M4 在复位时从地址零取得初始主栈指针，从地址四取得复位处理函数的向量。芯片的启动映射让选定的启动存储器出现在地址零区域；主 Flash 本身仍位于 `0x08000000`。这是地址别名，不是先把整片程序复制到 RAM。

实验向量表的两个字分别是 `_estack` 和 `Reset_Handler`。链接后首字为 `0x20020000`，第二字为 `0x08000009`。后者最低位表明 Thumb 状态，实际指令从 `0x08000008` 开始。工具链通过函数标记和重定位编码这个位，不应看到奇数地址就手工再加一。

`0x20020000` 是本例128 KB普通 SRAM的末端后一地址。满递减栈先减指针再存储，因此初始栈顶可以指向这个边界；它不是声称那里还存在一个可直接读写的 RAM 字节。初始MSP项也不是函数指针，不能套用“所有向量都必须为奇数”的说法。

硬件不会去解析 `main` 这个名字，也不会读取 ELF 的文件头来找入口。`ENTRY(Reset_Handler)` 为链接输出提供入口信息，但烧录后的复位流向取决于启动映射和向量内容。带引导程序的应用还要配合应用地址、向量重定位和栈设置，不能仅把链接脚本的起始地址改一行就算完成跳转协议。

## 链接脚本给出地图，不执行搬运

本例假定1 MB Flash和128 KB普通 SRAM。它适合讨论采用该容量配置的 F407，不能直接推广到所有密度型号，更没有把64 KB CCM接在普通 SRAM尾部。核心脚本如下，完整版本还包括对齐、边界符号和空间断言：

```text
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) } > FLASH
  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM AT> FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) :
  {
    _sbss = .;
    *(.bss*) *(COMMON)
    _ebss = .;
  } > RAM
}
```

`MEMORY` 告诉链接器有哪些可分配地址范围；`SECTIONS` 把输入对象中的节归并到输出节。`.text*` 中的星号覆盖按函数拆出的节，配合本例的节回收选项。`KEEP` 防止向量表因为没有普通C调用引用而被回收。遗漏匹配规则可能产生孤儿节，调试时还要检查最终映像，而不能只读自己写下的几行规则。

`> RAM AT> FLASH` 是关键：`.data` 的VMA在RAM，LMA在Flash。链接器把运行引用解析到RAM地址，同时保存可供烧录的初值。`LOADADDR(.data)` 给出初值来源。`_sdata` 等符号是地址标签，不是脚本替你申请的普通C变量，更不代表内存已经发生复制。`(NOLOAD)` 也不负责清零，它只影响装载描述。

## 启动代码兑现这些地址约定

本例汇编用 `ldr` 取得边界地址，按四字节循环从 `_sidata` 复制到半开区间 `[_sdata, _edata)`，然后将 `[_sbss, _ebss)` 清零，最后调用 `main`。完整脚本保证相关边界四字节对齐。可以按下面的状态表手工执行，不依赖“上电RAM恰好是零”：

| 阶段 | 已知状态 | 下一步 |
| --- | --- | --- |
| 镜像烧入Flash后 | 初值地址保存整数7 | 不能假定RAM对象已初始化 |
| 数据搬运完成 | `initialized`为7 | 清零BSS区间 |
| BSS清零完成 | 数组四项和`observed`为0 | 进入`main` |
| 执行第一句后 | `observed`应为7 | 停在循环中 |

最后一行是机制推导的预期，并非本次真机读数。对裸机项目来说，C静态存储期对象的初始值要求由编译器、链接器与启动支持共同实现；并不是 C 标准强制所有平台都存在这些节名。

再看真正的 ST GCC启动模板：本次核对版本先调用 `SystemInit`，再搬运 `.data`、清零 `.bss`，之后调用 `__libc_init_array` 和 `main`。这意味着在该版本的早期 `SystemInit` 中不能随意依赖尚未准备好的全局初值。其他工具链可能借助库入口完成初始化，顺序应以所用模板为准，不能把一条简化流程图当所有编译器的固定实现。

## 两种“链接通过”仍抓不住的问题

第一，脚本长度是你向链接器提供的配置，不是硬件自动探测。把512 KB器件误写成1 MB，链接器可能顺利安排出根本不存在的地址。普通 SRAM 与 CCM还具有不同总线连接；F407 DMA不能访问CCM，把DMA缓冲区挪过去也不能仅靠“地址空间里有RAM”判断可用。

第二，静态节没有溢出，不代表运行期栈一定足够。本例 `ASSERT` 只为静态分配之后至少留下1 KB，不是栈水位监测，也不能证明深递归、大局部数组或中断嵌套不会越界。真实项目应按最坏调用路径估算并测量水位，必要时增加保护与故障记录。

调试器有时会额外初始化RAM或直接停在入口，掩盖脱机复位缺陷。排查“连接调试器才正常”时，要区分下载、软件复位、硬件复位和上电启动路径，不要把一次成功运行等同于启动链完整。

## 练习：为什么改成NOLOAD会丢初值

假设为了缩小文件，把含 `initialized = 7` 的 `.data` 改成 `NOLOAD`，还保留原来的复制循环。这样能否既省Flash又保留七？若数组增加到400项，需要同时核对哪两类边界？

<details><summary>答案与因果解析</summary>

不能。初值七必须从某个可靠来源得到；取消该节的装载内容，却没有设计新的初始化来源，会破坏启动代码与镜像的约定。节名仍叫 `.data`、源代码仍写七，都不能凭空产生Flash中的初值。数组增大时要先检查BSS结束位置是否仍给栈留下足够空间，再检查清零循环是否覆盖新的区间、边界与访问宽度是否正确。链接器只能验证脚本显式表达的静态约束，实际栈峰值还需另行分析。

</details>

## 依据与回到主线

- [PM0214 Rev 10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)：2.1.3 核心寄存器、2.3.4 向量表。
- [RM0090 Rev 22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：2.3 存储组织、2.4 启动配置，注意选用F405/F407对应表。
- [ST GCC启动模板，固定提交](https://github.com/STMicroelectronics/cmsis-device-f4/blob/9192c7b9df75a142f2027ab266601fe061fc00b3/Source/Templates/gcc/startup_stm32f407xx.s)：`Reset_Handler` 的实际调用顺序。
- [GNU ld MEMORY](https://sourceware.org/binutils/docs/ld/MEMORY.html)与[Output Section LMA](https://sourceware.org/binutils/docs/ld/Output-Section-LMA.html)：区域长度、VMA/LMA及运行时搬运示例。

回到主线：[2.4 程序如何跑起来](/lessons/2-4)。相邻主题：[可执行文件格式](/notes/executable-formats)、[向量表](/notes/vector-table)、[弱符号回调](/notes/weak-symbols)。
