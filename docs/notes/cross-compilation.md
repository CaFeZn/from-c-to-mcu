---
title: 【原理】交叉编译与工具链
description: 【原理】交叉编译与工具链 · 从 C 到单片机
sourceKey: 【原理】交叉编译与工具链
---

# 【原理】交叉编译与工具链

> **类型**：二级分支（建议读）　｜　**前置**：读过 [[1.1_Hello_World]]
> 回到 [[00_教程导览]] ｜ 全部分支见 [[分支总表]]
> **读完你能回答**：为什么电脑编的程序单片机跑不了？`arm-none-eabi-gcc` 这个名字每一段是什么意思？CMake 里那份工具链文件到底在告诉它什么？

## 先看一个怪现象

你在上海的电脑前写菜谱，可菜要在深圳一间设备完全不同的厨房里做：灶具不同，锅不同，连火候的刻度都不同。

菜谱就得**按那间厨房写**，不能按你自己家的灶台写。你自己的厨房顶多拿来试试刀工。

编程里，你的电脑是上海那间厨房，单片机是深圳那间。让电脑为另一种 CPU 写出能跑的程序，就叫**交叉编译**。

> [!question] 先猜一猜
> 把 PC 上编出来的 `hello.exe` 拷到单片机上，能跑吗？反过来，把单片机的程序拿到 PC 上跑呢？至少说出三个可能的原因。

> [!success]- 揭晓
> 都不行。至少有三个原因：
> 1. **指令集不同**。PC 是 x86，单片机（Cortex-M）是 ARM。同样是"加法"，机器码完全不是一回事。
> 2. **没有操作系统**。PC 的可执行文件靠操作系统加载、接输出；单片机上没有这一层。
> 3. **入口和内存布局不同**。单片机要的是"向量表加复位代码放在 Flash 起始处"，PC 的文件格式里没有这些。
>
> 我们来反方向验证一下。把一个 ARM 的 ELF 文件改名成 `fake.exe`，在电脑上运行：
> ```
> ./fake.exe
> bash: ./fake.exe: cannot execute binary file: Exec format error
> ```
> 电脑拒绝执行它。

## 一、同一段 C，两种机器

```c
int add(int a, int b) { return a + b; }
```

分别用电脑的 `gcc` 和单片机的 `arm-none-eabi-gcc` 编译成汇编：

```
gcc -O1 -S add.c -o add_x86.s
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O1 -S add.c -o add_arm.s
```

> [!example] 我这台机器上的结果（节选）
> 电脑（x86）：
> ```
> _add:
>     movl  8(%esp), %eax
>     addl  4(%esp), %eax
>     ret
> ```
> 单片机（ARM）：
> ```
> add:
>     add   r0, r0, r1
>     bx    lr
> ```

C 代码一字不差，机器指令完全两样：x86 从栈上取参数，ARM 的参数直接放在寄存器 `r0`、`r1` 里。这是编译器要负责的"翻译"，**翻译成哪种语言，取决于目标 CPU**。

再看目标文件的格式：

```
objdump -f add_x86.o
arm-none-eabi-objdump -f add_arm.o
```

> [!example] 我这台机器上的结果
> ```
> add_x86.o:  file format pe-i386     architecture: i386
> add_arm.o:  file format elf32-littlearm   architecture: armv7e-m
> ```

电脑上是 Windows 的 PE 格式、i386 架构；单片机这边是 ELF 格式、ARMv7E-M 架构（Cortex-M4 属于它）。

## 二、三个"机器"：build、host、target

给编译器自己做"出生证"时，会涉及三台机器：

| 名字 | 意思 | 例子 |
|---|---|---|
| **build** | 这个编译器是在哪台机器上**造**出来的 | 一台 Linux 服务器 |
| **host** | 这个编译器在哪台机器上**运行** | 你的 Windows 电脑 |
| **target** | 这个编译器**产出**的程序给谁跑 | 单片机的 ARM 内核 |

三者相同，叫本机编译（你在 PC 上编、在 PC 上跑）；host 和 target 不同，叫**交叉编译**。看看你手上的交叉编译器怎么说：

```
arm-none-eabi-gcc -v
```

> [!example] 我这台机器上的结果（节选，来自 ST 的 GNU 工具链 12.3.1）
> ```
> Target: arm-none-eabi
> Configured with: ... --build=x86_64-linux-gnu --host=x86_64-w64-mingw32 --target=arm-none-eabi ...
> ```

这个编译器是在 Linux（build）上造的，运行在 Windows（host）上，产出 ARM 代码（target），三台机器各不相同。

## 三、名字怎么读：arm-none-eabi

交叉编译器的名字通常是"目标"的描述，叫**目标三元组**，大致是"架构-系统-ABI"：

| 名字 | 架构 | 系统 | ABI 或环境 |
|---|---|---|---|
| `arm-none-eabi` | arm | none（**没有操作系统**） | eabi（嵌入式应用二进制接口） |
| `arm-linux-gnueabihf` | arm | linux | gnueabihf（GNU，硬浮点） |
| `mingw32`（本机 PC 的 gcc 报的） | 32 位 x86 | Windows | MinGW |

所以 `arm-none-eabi` 里的 `none` 很关键：它在告诉编译器，**目标上没有操作系统**。没有操作系统，就没有 `main` 之前的加载器，没有文件系统，也没有系统调用，这些要么自己提供，要么不用。

## 四、几个选项：把 CPU 说清楚

CubeMX 生成的工程里，编译选项长这样：

```
-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
```

| 选项 | 意思 |
|---|---|
| `-mcpu=cortex-m4` | 目标 CPU 是 Cortex-M4，可以用它支持的指令 |
| `-mthumb` | 用 Thumb 指令集（Cortex-M 只支持这一套） |
| `-mfpu=fpv4-sp-d16` | 带单精度硬件浮点单元，用哪一种 |
| `-mfloat-abi=hard` | 浮点参数用 FPU 的寄存器传递 |

最后一项决定了函数之间怎么"交接浮点数"，**所有要链接在一起的文件必须一致**。如果不一致呢？

> [!question] 先猜一猜
> 我把 `scale.c` 用 `-mfloat-abi=hard` 编译，另一个文件 `scale2.c` 用默认选项（软浮点）编译，再把它们链接在一起，会怎样？

> [!success]- 揭晓
> 链接器会拒绝：
> ```
> error: dup.elf uses VFP register arguments, scale2.o does not
> failed to merge target specific data of file scale2.o
> ```
> 浮点怎么传递是一种**约定**（ABI），双方必须一致。这是你在复制别人的 `.o` 或预编译库时可能撞上的坑：库和你的工程，CPU 选项要对得上。

标准库也是这样：交叉编译器自带了**一整套为不同 CPU 预编译好的库**，叫 multilib，用 `arm-none-eabi-gcc -print-multi-lib` 能列出来：

> [!example] 我这台机器上的结果（节选）
> ```
> thumb/nofp;@mthumb@mfloat-abi=soft
> thumb/v7/nofp;@mthumb@march=armv7@mfloat-abi=soft
> thumb/v7+fp/hard;@mthumb@march=armv7+fp@mfloat-abi=hard
> ```

编译器会根据你给的 `-mcpu`、`-mfloat-abi` 选出匹配的那一套。所以**库本身也是交叉编译出来的**。

## 五、工具链里有什么

"工具链"是一整套配合使用的工具。本机 CubeCLT 里的 GNU 工具链包含：

| 工具 | 做什么 |
|---|---|
| `arm-none-eabi-gcc` / `g++` | 总指挥：按需调用下面几个 |
| `arm-none-eabi-as` | 汇编器 |
| `arm-none-eabi-ld` | 链接器 |
| `arm-none-eabi-ar` | 打包 `.a` 静态库 |
| `arm-none-eabi-objcopy` | 格式转换，如 `.elf` 转 `.bin`、`.hex` |
| `arm-none-eabi-objdump` / `readelf` / `nm` / `size` | 查看产物 |
| `arm-none-eabi-gdb` | 调试器 |

另外还有 C 标准库：`newlib`，以及体积更小的 `newlib-nano`（编译选项 `--specs=nano.specs`）；还有没有操作系统时用来"占位"系统调用的 `nosys.specs`。

分清两样东西：**工具链是通用的**（任何 Cortex-M 都用它）；**和具体芯片相关的**，是厂商提供的启动文件、链接脚本、寄存器头文件、HAL 库，这些是 CubeMX 生成进工程里的。

## 六、CMake 怎么知道它是在交叉编译

CMake 默认认为你是在为这台电脑写程序。要告诉它"不是"，就要靠**工具链文件**。看 CubeMX 生成的 `gcc-arm-none-eabi.cmake`（节选）：

```cmake
set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(TOOLCHAIN_PREFIX                arm-none-eabi-)
set(CMAKE_C_COMPILER                ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE                      ${TOOLCHAIN_PREFIX}size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")
```

| 这几行 | 在说什么 |
|---|---|
| `CMAKE_SYSTEM_NAME Generic` | 目标系统是"通用"，也就是**没有操作系统**。一旦设置它，CMake 就知道自己在交叉编译 |
| `CMAKE_SYSTEM_PROCESSOR arm` | 目标 CPU 家族 |
| `TOOLCHAIN_PREFIX` 与几个 `CMAKE_*_COMPILER` | 用带前缀的工具，而不是电脑自己的 gcc |
| `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` | CMake 配置时会先"试编一个小程序"检查编译器好不好用。裸机上没有启动代码和系统调用，那个小程序**链接不成可执行文件**，所以改成只试编静态库 |
| `TARGET_FLAGS` | 上一节那组 CPU 选项，后面会加到编译和链接选项里 |

在 VS Code 里用 CMake Tools 选 **Kit** 或 **Preset**，本质上就是在选"用哪份工具链文件"。第 1 章你选的是电脑自己的 gcc；到单片机，选的是这份。

## 七、没法在电脑上运行，怎么测试

交叉编译的产物只能在 target 上跑，所以：

- **下载到板子上运行**，通过 SWD 下载和调试（见 2.2、3.2）。
- 把**不依赖硬件**的代码单独拿出来，**在电脑上用本机编译测试**。本教程里很多模块（例如命令表、帧解析、环形缓冲）都是这么验证的，因为它们不碰 HAL，同一份源码换个编译器就能在电脑上跑。这也是把硬件相关部分用函数表隔离开的好处（见 1.7）。

## 记住这几条

1. **交叉编译 = 在 A 机器上编，给 B 机器跑。** host 和 target 不同。
2. **`arm-none-eabi` 里的 `none` 是"没有操作系统"。**
3. **CPU 选项必须全工程一致**，尤其是浮点 ABI，不一致链接器会拒绝。
4. **工具链通用，芯片相关的文件来自厂商。**
5. **工具链文件是在告诉 CMake "不要为这台电脑编程"。**

## 练习

1. 用 `-dumpmachine` 查你电脑上每一个编译器的目标，并说出每一段的意思。（复现）
2. 用 `-mcpu=cortex-m0` 和 `-mcpu=cortex-m4` 编译同一个带乘法和除法的函数，比较汇编的差异，并说出你的猜测。（改造）
3. 有人说"我把 `-mfloat-abi=hard` 的库拷给了软浮点的工程，只是一个选项而已，应该没事"。你怎么反驳？（预测）

## 继续往下

> [!info] 原理
> - [[【原理】从源码到可执行文件|从源码到可执行文件]]：交叉编译的四站，每一站留下什么。
> - [[【原理】可执行文件格式|可执行文件格式]]：ELF、`.bin`、`.hex` 的区别，下载进 Flash 的到底是哪个。
> - [[【原理】启动流程与链接脚本|启动流程与链接脚本]]：没有操作系统，谁来做 `main` 之前的准备？

> [!abstract] 拓展
> - [[【拓展】Cortex-M系列差异|Cortex-M系列差异]]：M0、M3、M4、M7 的指令和浮点单元有什么不同。
> - [[【拓展】STM32Cube工具生态|STM32Cube 工具生态]]：CubeMX、CubeCLT、CubeProgrammer 各管什么。
