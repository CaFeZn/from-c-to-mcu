---
title: "【原理】可执行文件格式"
description: "用readelf、objdump和objcopy辨认ELF的节、装载段与原始烧录镜像，解释文件大小为何不等于RAM占用。"
sourceKey: "【原理】可执行文件格式"
---

# 可执行文件格式

同一个固件，调试器打开的 ELF 有十几 KB，导出的 BIN 只有一百多字节；再增加一个未初始化数组，链接报告说 RAM 增加了，BIN 却几乎不变。不是压缩器施了魔法，而是几个文件回答的问题不同：调试器需要名字和源码位置，烧录器需要目标地址及字节，启动代码还需要知道哪些 RAM 必须清零。

## 制作一个可以看穿的镜像

取[本组示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build)中的 `arm` 子目录。核心 C 文件刻意只保留三个对象：

```c
volatile unsigned initialized = 7;
volatile unsigned zeros[4];
volatile unsigned observed;
int main(void)
{
    observed = initialized + zeros[0];
    for (;;) { }
}
```

`volatile` 让这次观察中的对象访问保持可见，并不负责初始化。示例启动汇编只含两个复位向量及数据初始化循环，链接脚本假定 Flash 从 `0x08000000` 开始、普通 SRAM 从 `0x20000000` 开始。**这是离线格式实验，不是可直接烧录的完整板级工程**：它没有完整异常向量、时钟初始化和库初始化。

在仓库根目录的 PowerShell 中执行；前提是 GNU Arm Embedded 工具已加入当前终端搜索路径：

```powershell
$S = "examples/notes/build/arm"
$B = "artifacts/course-completion/build/formats"
New-Item -ItemType Directory -Force $B | Out-Null
$F = @("-mcpu=cortex-m4", "-mthumb", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
       "-ffreestanding", "-ffunction-sections", "-fdata-sections",
       "-fno-unwind-tables", "-fno-asynchronous-unwind-tables")
arm-none-eabi-gcc @F -std=c99 -nostdlib -T "$S/layout.ld" "-Wl,--gc-sections" "$S/startup.S" "$S/image.c" -o "$B/image.elf"
arm-none-eabi-readelf -h -S -l "$B/image.elf"
arm-none-eabi-objdump -h "$B/image.elf"
arm-none-eabi-objcopy -O binary "$B/image.elf" "$B/image.bin"
arm-none-eabi-objcopy -O ihex "$B/image.elf" "$B/image.hex"
```

先预测：`zeros` 的十六个零是否必须真的写进 BIN？读完输出再回答，不要把“存在这个对象”和“文件中逐字保存这个对象”混为一谈。

## 文件头先告诉你该由谁解释

本例在 ARM GCC 12.3.1 下核验，`readelf -h` 显示 `ELF32`、小端、`Machine: ARM` 和 `Type: EXEC`。开头四个字节是 `7f 45 4c 46`，后面还记录地址宽度、字节序、入口地址以及其他表的文件位置。

这比扩展名可靠：把 Windows 程序改名为 `firmware.elf`，并不会获得 ARM 指令。ELF 本身也不只用于可执行程序；对象文件常是 `ET_REL`，已链接的本例是 `ET_EXEC`，共享对象和一些平台的 PIE 则使用 `ET_DYN`。格式、处理器、ABI、运行环境必须一起匹配，不能只检查“它是 ELF”。

本例入口显示 `0x08000009`，而反汇编中复位函数指令地址是 `0x08000008`，最低位用于指示 Thumb 状态。更重要的是，裸机 Cortex-M 复位时不解析 ELF 头，它读取启动映射中的向量表；把 ELF 入口字段改掉，不会自动改掉硬件复位入口。

## 节是组织视角，装载段是内存视角

`readelf -S` 列出节，`objdump -h` 还便于同时看运行地址 VMA 与装载地址 LMA。本次实验的关键数值如下，代码布局随编译器和选项改变是正常的：

| 节 | 大小 | 本次观察 |
| --- | --- | --- |
| `.isr_vector` | 8 字节 | 两个启动字，放在 Flash 开头 |
| `.text` | 100 字节 | 机器指令等内容 |
| `.data` | 4 字节 | VMA为`0x20000000`，LMA为`0x0800006c` |
| `.bss` | 20 字节 | 从`0x20000004`开始，类型为`NOBITS` |

`zeros` 占十六字节，`observed` 占四字节，共二十字节。`.bss` 的节头保存尺寸和地址，但类型 `SHT_NOBITS` 表示它本身不占据对应数量的文件内容字节。不能打开某个文件偏移，期待那里有二十个必需的零。

`readelf -l` 换成装载段视角。本例一个 `LOAD` 段包含向量和代码，另一个关联 `.data` 与 `.bss`，其 `FileSiz=4`、`MemSiz=24`。对于实现 ELF 装载语义的系统，后者差额是需要建立的零填充内存。本实验是裸机，烧录器把初值放进 Flash，启动代码再搬运数据、清零 RAM；没有一个操作系统替它执行通用 ELF 装载过程。

节和段不是一一对应，多个节可以组成一个段。调试信息节也可能只供工具读取，不进入板上的运行镜像。把 `.debug_info` 的文件大小算成 MCU 的 RAM 消耗，就在混用这两种视角。

## 转成BIN时，究竟丢掉了什么

`objcopy -O binary` 按所复制内容的装载地址产生原始字节，最低装载地址对应输出开头。它丢掉符号和重定位信息，也不保留能独立说明“烧到哪里”的文件头。本例 BIN 为 112 字节：8 字节向量、100 字节代码、4 字节数据初值；它不是把十几 KB 的 ELF 整体压缩成了 112 字节。

这解释了两个操作习惯：给烧录器 BIN 时必须提供正确基地址，本例是 `0x08000000`；需要断点、变量名和源码定位时，应保留本次构建对应的 ELF。只保留 BIN 再希望调试器恢复全部局部变量、类型与源文件行号，信息已经不在了。

Intel HEX 则以文本记录携带地址、数据以及校验字段，能表达不连续地址区间，但也不是完整的调试数据库。比较文件体积时，HEX 每个数据字节通常占两个十六进制字符，还带记录开销，因此文本文件比原始有效数据大很正常。

另一个边界是地址空洞。若两个被复制节的 LMA 相隔很远，BIN 可能包含很大的空隙。看到异常巨大的 BIN，先检查装载地址是否误放到 SRAM 区，而不是立即认定程序代码暴涨。不能把“LMA与VMA不同”一律当错；`.data` 本就需要 Flash 初值与 RAM 运行位置分离。

## 对象文件与Windows程序再各看一眼

继续编译一个尚未链接的对象：

```powershell
arm-none-eabi-gcc @F -c "$S/image.c" -o "$B/image.o"
arm-none-eabi-readelf -h -r "$B/image.o"
```

此时类型应是 `REL`，并能看到对相关对象地址的重定位记录。链接后，很多这种静态引用已被解析；不要要求最终镜像保留与对象文件完全相同的重定位表。

Windows 上用 MinGW 构建的本组 `toolchain_probe.exe` 是 PE 映像，不是 ELF。它通常以 `MZ` 开头，偏移 `0x3c` 的字段指出 `PE\0\0` 签名所在位置。PE/COFF也有节、入口和导入信息，但字段布局与装载规则不同；`RVA` 是相对映像基址的虚拟地址，不是磁盘文件偏移。把 ELF 工具对 PE 的“不识别”当成程序损坏，同样是用错了解释器。

## 练习：数组扩大一百倍会改变什么

把 `zeros[4]` 改成 `zeros[400]`，仍只读取首元素。预测链接映像的 `.bss`、BIN 的有效数据和所需 RAM，各会怎样变化？为什么仍需实际检查，而不把预测当成所有项目的固定公式？

<details><summary>答案与观察依据</summary>

本例对象带 `volatile`，且保留被访问数组所在输入节，数组从16字节增加到1600字节，`.bss` 预期增加1584字节，RAM需求相应增长。零初始化通常不需要向 BIN 写入1584个零，BIN一般不会随之增加这么多；链接器对齐或代码布局仍可能带来小变化。若改成非零初始化数组、改变优化或节回收策略，初值存储与保留结果可能不同，所以应重新看节表、装载段和实际文件长度。判断依据是对象的存储需求及输出类型，不是源代码的行数。

</details>

## 依据与适用范围

以上离线结果来自 GNU ARM ELF 工具，未烧录或模拟执行。

- [ELF gABI，4.3草案，第2章](https://gabi.xinuos.com/elf/02-eheader.html)：ELF头与文件类型。
- [第3章 Sections](https://gabi.xinuos.com/elf/03-sheader.html)及[第7章 Program Loading](https://gabi.xinuos.com/elf/07-pheader.html)：`SHT_NOBITS`、`PT_LOAD`、文件与内存尺寸。
- [GNU objcopy](https://sourceware.org/binutils/docs/binutils/objcopy.html)：`binary` 输出的装载地址及信息丢弃规则。
- [Microsoft PE/COFF规范](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)：File Headers、Signature、RVA术语。

回到主线：[2.4 程序如何跑起来](/lessons/2-4)。相邻主题：[预处理器与链接](/notes/preprocessor-linkage)、[启动流程与链接脚本](/notes/startup-linker-script)、[能跑与能调试](/notes/run-vs-debug)。
