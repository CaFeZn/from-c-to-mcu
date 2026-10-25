---
title: 【原理】栈与函数调用
description: 用有界递归和 Cortex-M4 反汇编追踪栈帧、返回地址与局部对象的生存期。
sourceKey: 【原理】栈与函数调用
---

# 栈与函数调用

把一个加一函数从 Debug 改成优化构建，调试器忽然找不到里面的 `local` 了；可输入 41，输出仍是 42。变量丢了吗？再看一个递归函数：同名的 `local` 在四层调用里分别保存 13、12、11、10，返回时又都能找回来。两种现象看似相反，实际上要分开两个问题：**C 规定对象什么时候有效，机器代码决定用什么保存它。**

这篇接着 [1.4 函数与作用域](/lessons/1-4) 做实验。PC 部分只运行浅层、已初始化的代码；机器细节限定为 Cortex-M4、Thumb、AAPCS32，不用故意把栈撑爆来证明原理。

## 先让四次调用算对

仓库中的完整示例带有 `main` 和断言，核心如下：

```c
unsigned stack_leaf(unsigned x)
{
    unsigned local = x + 1u;
    return local;
}

unsigned stack_step(unsigned depth)
{
    unsigned local = 10u + depth;
    if (depth == 0u) {
        return local;
    }
    {
        unsigned child = stack_step(depth - 1u);
        return local + child;
    }
}
```

先手算 `stack_step(3u)`，再从仓库根目录用 Bash 执行：

```bash
mkdir -p artifacts/course-completion/c-functions/abi-build
gcc -std=c99 -Wall -Wextra -Werror -pedantic -O0 \
  examples/notes/c-functions/stack_demo.c \
  -o artifacts/course-completion/c-functions/abi-build/stack-manual.exe
./artifacts/course-completion/c-functions/abi-build/stack-manual.exe
```

给定的调用没有标准输入，固定输出是：

```text
stack_step(0) = 10
stack_step(1) = 21
stack_step(3) = 46
stack_leaf(41) = 42
```

把 `-O0` 换成 `-O2`，输出应相同。完整示例还用循环求和核对深度 0 到 32，不接受任意大深度。这里用 `unsigned`，边界测试中的回绕是无符号运算规则，不是有符号溢出的“实验结果”。

| 当前调用 | 本层保存的值 | 等子调用回来后 |
| --- | --- | --- |
| `stack_step(3)` | `local = 13` | `13 + 33 = 46` |
| `stack_step(2)` | `local = 12` | `12 + 21 = 33` |
| `stack_step(1)` | `local = 11` | `11 + 10 = 21` |
| `stack_step(0)` | `local = 10` | 直接返回 10 |

进入子函数只是暂停外层函数，**没有结束外层块的执行**。这些调用中的自动对象各有自己的实例，所以外层不能被内层同名变量覆盖。这是语言语义；并不意味着编译器必须给每个实例都分配一个可见的内存格子。

## 观察 Cortex-M4 怎样保留现场

已安装 PC GCC 和 ARM GCC 后，运行随附脚本可以同时得到 PC 结果、ARM 汇编、目标文件反汇编和栈用量报告：

```bash
bash examples/notes/c-functions/run-abi-tests.sh
```

脚本从 `PATH` 查找工具，也支持用 `HOST_CC`、`ARM_CC`、`ARM_OBJDUMP`、`ARM_READELF` 指定实际安装路径。只想复现本篇的关键编译，可在仓库根目录执行：

```bash
A=artifacts/course-completion/c-functions/abi-build
arm-none-eabi-gcc -std=c99 -Wall -Wextra -Werror -pedantic \
  -mcpu=cortex-m4 -mthumb -mfloat-abi=soft \
  -O0 -fno-inline -fno-optimize-sibling-calls -fstack-usage \
  -DSTACK_NO_MAIN -S examples/notes/c-functions/stack_demo.c \
  -o "$A/stack-O0.s"
```

`STACK_NO_MAIN` 只去掉 PC 的输出和断言入口，保留同一个算法；这里生成汇编，不需要板上启动文件。关闭内联和尾调用相关优化，是为了便于观察普通嵌套调用，并非产品构建建议。

本次 ARM GCC 12.3.1 的 `-O0` 结果中，`stack_step` 的关键顺序为：

```asm
push {r7, lr}
sub  sp, sp, #16
add  r7, sp, #0
/* ... 保存 depth、local，准备下一层参数 ... */
bl   stack_step
/* ... 使用子调用的返回值 ... */
adds r7, r7, #16
mov  sp, r7
pop  {r7, pc}
```

逐条读，不必一开始就记全部指令：

- `SP` 是栈指针，即 `r13`。此处栈采用满递减方式：压入时先向低地址扩展，弹出时恢复到较高地址。方向来自该平台的约定，不来自 C 语法。
- `push {r7, lr}` 保存两个 32 位寄存器，消耗 8 字节；再减 16，为本层局部数据和布局空隙预留空间。两部分合计 24 字节。
- 本次 `r7` 用作帧指针，便于用固定偏移访问本层数据。它不是 C 变量，也不是每个优化函数都必须具备的寄存器角色。
- `bl` 跳转并把返回位置写入 `LR`，即 `r14`。**普通 `BL` 本身不会把全部寄存器压栈。** 本函数还要调用下一层，必须先保住已有返回信息，否则下一条 `BL` 会覆盖它。
- 尾部撤销空间分配，`pop {r7, pc}` 恢复调用者的 `r7`，并把先前保存的返回地址装入 `PC`，继续调用点之后的代码。

假设进入第一层时 `SP = 0x20001000`，这是用于手算的起点，不是推荐链接脚本地址。保存寄存器后是 `0x20000FF8`，再减 16 后是 `0x20000FE8`。此时 `r7 + 4` 放参数副本，`r7 + 12` 放 `local`，`r7 + 8` 在子调用返回后才写入 `child`；高处的 `r7 + 16`、`r7 + 20` 分别保存旧 `r7` 和 `LR`。

这种为一次活动调用保留的区域叫**栈帧**。对本次代码，深度 3 会进入 3、2、1、0 四层，单算这四帧的峰值为 `4 × 24 = 96` 字节。不是“四个局部变量只需 16 字节”，因为返回信息、参数副本、保存寄存器和对齐都可能占空间。

## 为什么优化后局部变量不在栈上

把上面的编译改为 `-O2`，仍保留两个 `-fno-...` 选项，本次叶函数变成：

```asm
stack_leaf:
    adds r0, r0, #1
    bx   lr
```

`r0` 同时承担本例的参数和返回值，`local` 没有单独的栈位置；它的计算作用已经体现在加一指令里。叶函数没有继续调用别人，返回信息仍可留在 `LR`。与此同时，递归函数的非零路径只保存 `r4` 和 `LR`，基例直接返回。

配套 `-fstack-usage` 报告中，`stack_leaf` 从 24 字节降为 0，`stack_step` 从 24 降为 8。这些是**当前工具链、选项和函数代码的结果**，不是 C 对栈帧大小的承诺。

“自动存储期”回答的是何时有效；“栈、寄存器、优化消除”回答的是怎样实现。不要用前者推导“一定放在栈上”。甚至源代码中存在调用，也可能被内联或改写成循环；安全估算必须检查实际交付的构建。

## 三条容易误判的边界

**地址相同，不代表还是同一个对象。** 两次先后调用可能复用同一块栈空间，也可能不复用，打印地址不能保证它们相等。对不同局部对象做指针减法也不能用来可靠测量栈帧。不要保存已结束生存期的局部地址，再解引用来“看看旧值还在不在”；行为没有定义。需要带走结果时按值返回，或让调用者提供仍有效的缓冲区。内存没清零不等于对象还活着，未初始化局部变量的读取也不是合法的旧值探针。

**单函数用量，不等于整个任务用量。** 栈报告里的 `static` 是“栈使用量可静态确定”的报告类别，不是把变量放到静态存储区。递归要算最大同时活动层数，普通调用链要累加重叠的帧；库函数、间接调用、可变长数组等还需另外核对。逐个顺序调用十次，不等于十个栈帧同时存在。

**中断入栈，不是普通函数调用的另一种写法。** Cortex-M4 的常见异常进入会由硬件建立含 `r0-r3`、`r12`、`LR`、`PC`、`xPSR` 的基本上下文；浮点上下文、对齐、嵌套和尾链还影响实际占用。它与本篇编译器插入的 `push` 不是同一件事。若两者落在同一栈上，需要合算；使用 MSP/PSP 分开的系统则分别预算，不能机械套用“每次调用固定压 8 个寄存器”。

## 练习：把预算算清楚

保持本次 `-O0` 生成代码不变，假设一段调用链给 `stack_step` 留出 120 字节，不计调用者、中断和其他开销。

1. `stack_step(3)` 返回什么，占用多少？
2. `stack_step(5)` 在这个简化预算内吗？
3. 换成优化构建后，能把 24 字节直接当成永久常数吗？

<details>
<summary>展开答案与推导</summary>

1. 返回 `13 + 12 + 11 + 10 = 46`；四个活动栈帧共 96 字节。
2. 进入 5 到 0 共六层，需要 `6 × 24 = 144` 字节，已超出简化预算。这只是算术练习，不要真的缩小栈去运行越界实验。
3. 不能。本次 `-O2` 非零层占 8 字节，基例不再建立帧；其他版本还可能继续改写递归。工程预算必须来自最终构建，并补上真实调用链和异常上下文以及安全余量。

</details>

## 适用范围与技术依据

示例采用 C99；生存期解释按 C11 草案 N1570 的既有规则核对，不使用线程或 C11 新语法。PC 验证使用 MinGW GCC 6.3.0；ARM 部分用 STM32 工具链 GCC 12.3.1 真实编译、反汇编，**未烧录、未测量真机栈水位**。没有从这些数字推断 STM32F407 的工程默认栈容量。

- [WG14 N1570 官方 PDF](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)，§5.1.2.3、§6.2.4。官方下载读取超时，本次实际精读 [公开 HTML 转录的生存期条款](https://port70.net/~nsz/c/c11/n1570.html#6.2.4)，据此核对暂停调用、递归实例及生存期结束规则。
- [Arm AAPCS32，2025Q1](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs32/aapcs32.rst)，§6.1.1、§6.2.1、§6.3：栈、寄存器和子程序调用约定，已读取官方仓库文本。
- [ST PM0214，Rev 10](https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)，§2.1.3、§2.3.7、§3.4.7：SP/LR、异常进入、PUSH/POP；本次读取项目缓存的官方文档文本。
- [GCC 12.3 开发选项](https://gcc.gnu.org/onlinedocs/gcc-12.3.0/gcc/Developer-Options.html)，`-fstack-usage`：逐函数报告和 `static/dynamic/bounded` 含义。

回到主线：[1.4 函数与作用域](/lessons/1-4)。继续读：[调用约定与尾调用优化](/notes/calling-conventions)，把参数与返回值放回寄存器约定中；[函数指针与间接调用](/notes/function-pointer-calls)，分析运行时才确定的调用目标。
