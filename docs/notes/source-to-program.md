---
title: 【原理】从源码到可执行文件
description: 【原理】从源码到可执行文件 · 从 C 到单片机
sourceKey: 【原理】从源码到可执行文件
---

# 【原理】从源码到可执行文件

> **类型**：二级分支（建议读）　｜　**前置**：读过 [[1.1_Hello_World]]
> 回到 [[00_教程导览]] ｜ 全部分支见 [[分支总表]]
> **读完你能回答**：目标文件里到底装了什么？链接器具体做了哪几件事？`undefined reference` 为什么能指出一个位置？

## 先看一个怪现象

你收到一个快递，打开是一个 `calc.o`。它不是文本，打开是一片乱码；也不能运行，双击没有反应。可它又不是一团乱码：里面有货，有一张货单，还有几张写着"此处待补"的便签。

1.1 里我们说编译器遇到不认识的函数，就贴一张便签。这篇把便签拆开看：它长什么样，是谁来撕，撕了以后换成什么。

> [!question] 先猜一猜
> 我们马上要编译一个文件 `calc.c`，它调用了另一个文件里的函数 `ext_scale`。编译成 `calc.o` 之后，你觉得 `calc.o` 里有没有 `ext_scale` 的机器码？`run` 函数里调用它的那条指令，目标地址写的是什么？

> [!success]- 揭晓
> 没有机器码。调用它的那条指令里，目标地址是 0（占位），旁边挂着一张便签，写着"这里要填 `ext_scale` 的地址"。往下做就能亲眼看到。

## 一、准备两个小文件

这次不用 `printf`，免得标准库的内容把现象淹掉。

```c
/* calc.c */
int counter = 5;               /* 有初值的全局变量 */
int history[100];              /* 没有初值的全局数组 */
const char tag[] = "calc";     /* 只读常量 */

int ext_scale(int x);          /* 声明：它在别的文件里 */

int add(int a, int b) { return a + b; }

int run(void) {
    history[0] = add(counter, 1);
    return ext_scale(history[0]);
}
```

```c
/* scale.c */
int ext_scale(int x) { return x * 3; }
```

下面用单片机的交叉编译器来演示（原因见 [[【原理】交叉编译与工具链]]）。它输出的是 ELF 格式，工具看起来更清楚；在电脑上用普通 `gcc` 思路完全一样，只是 Windows 上产物是另一种格式（PE/COFF）。

```
F="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os"
```

## 二、四站各留下什么

```
arm-none-eabi-gcc $F -E calc.c -o calc.i    # 预处理
arm-none-eabi-gcc $F -S calc.c -o calc.s    # 编译成汇编
arm-none-eabi-gcc $F -c calc.c -o calc.o    # 汇编成目标文件
arm-none-eabi-gcc $F -c scale.c -o scale.o
```

> [!example] 我这台机器上的结果（你的数字会不同，关系应该相似）
> - `calc.c` 12 行，`calc.i` 16 行：这次没有 `#include`，所以只多出几行标记。1.1 里的 hello 因为包含了 `stdio.h`，膨胀到了 447 行。
> - `calc.s` 里 `add` 是两条指令：`add r0, r0, r1` 和 `bx lr`。
> - `calc.o` 1100 字节，`scale.o` 776 字节。

注意一个有意思的细节：打开反汇编（后面会做）你会发现，`run` 里已经**没有 `add` 的调用**了。我们用了 `-Os`，编译器把 `add` 直接展开进了 `run`。编译器不是"忠实翻译"，它在改写你的代码。这也是调试时要用 `-O0` 的原因（见 3.2）。

## 三、拆开 calc.o

### 货：三种"段"

```
arm-none-eabi-size calc.o scale.o
```

> [!example] 我这台机器上的结果
> ```
>    text    data     bss     dec     hex filename
>      33       4     400     437     1b5 calc.o
>       6       0       0       6       6 scale.o
> ```

三列分别是：

| 列 | 装什么 | 在 calc.o 里是谁 |
|---|---|---|
| text | 代码，以及只读常量 | `add`、`run` 的指令（28 字节）和 `tag`（5 字节） |
| data | 有初值的全局变量 | `counter`，4 字节 |
| bss | 没有初值的全局变量 | `history`，400 字节 |

用 `arm-none-eabi-readelf -S calc.o` 看段表，能看到 `.text`、`.data`、`.bss`、`.rodata` 这几个名字。仔细看 `.bss` 的类型是 `NOBITS`：**它在文件里不占空间**，只是声明"程序运行时需要 400 字节，请清零"。这就是为什么 400 字节的数组不会让 `.o` 文件变大 400 字节。

### 货单：符号表

```
arm-none-eabi-nm calc.o
```

> [!example] 我这台机器上的结果
> ```
> 00000000 T add
> 00000000 D counter
>          U ext_scale
> 00000000 B history
> 00000004 T run
> 00000000 R tag
> ```

字母是"这个名字属于哪个段"：`T` 代码，`D` 有初值的数据，`B` 零初值的数据，`R` 只读数据，`U` **未定义**（Undefined）。`ext_scale` 前面的 `U` 就是那张便签的"货单版"：这个名字我用到了，但不在我这里。

大写表示全局可见，小写表示只在本文件可见。把 `counter` 改成 `static int counter` 再看，会变成小写 `d`。

### 便签：重定位项

```
arm-none-eabi-objdump -dr calc.o
```

> [!example] 我这台机器上的结果（节选 `run`）
> ```
> 00000004 <run>:
>    4:  4b03      ldr   r3, [pc, #12]   @ (14 <run+0x10>)
>    6:  6818      ldr   r0, [r3, #0]
>    8:  4b03      ldr   r3, [pc, #12]   @ (18 <run+0x14>)
>    a:  3001      adds  r0, #1
>    c:  6018      str   r0, [r3, #0]
>    e:  f7ff bffe b.w   0 <ext_scale>
>          e: R_ARM_THM_JUMP24   ext_scale
>   12:  bf00      nop
>          ...
>          14: R_ARM_ABS32       .data
>          18: R_ARM_ABS32       .bss
> ```

看带 `R_ARM_...` 的三行，这就是**便签（重定位项）**，每一张写的是"在这个位置，填上某个东西的地址"：

| 位置 | 便签内容 | 说明 |
|---|---|---|
| 偏移 0xe | `ext_scale` | 那条跳转指令的目标。现在是 0，等着填 |
| 偏移 0x14 | `.data` | 一个 32 位常量，是 `counter` 的地址（放在代码旁边的"常量池"里，由上面的 `ldr` 读出来） |
| 偏移 0x18 | `.bss` | 同理，是 `history` 的地址 |

也就是说，不只是调用别的文件的函数需要便签，**访问全局变量的地址也需要**。编译器在编译时根本不知道这个段最终会被放到内存的哪里。

## 四、链接：撕便签，填地址

```
arm-none-eabi-ld -Ttext=0x08000000 -Tdata=0x20000000 -e run calc.o scale.o -Map=calc.map -o calc.elf
```

这里为了看得清楚，直接调用链接器 `ld`，并用最简单的方式告诉它：代码放到 Flash 的起始地址 0x08000000，数据放到 RAM 的起始地址 0x20000000。真正的工程里这件事由**链接脚本**完成（见 [[【原理】启动流程与链接脚本]]）。

链接器干了三件事：

1. **合并段**：把所有 `.o` 里同名的段拼在一起。`.text` 先放 `calc.o` 的，再放 `scale.o` 的，一个挨一个。
2. **解析符号**：把每个 `U` 对上别处的 `T` / `D`。对不上就报错（下一节）。
3. **重定位**：按便签，把真实地址填回去。

看结果：

```
arm-none-eabi-nm -n calc.elf
arm-none-eabi-objdump -d calc.elf
```

> [!example] 我这台机器上的结果（节选）
> ```
> 08000000 T add
> 08000004 T run
> 0800001c T ext_scale
> 08000022 R tag
> 20000000 D counter
> 20000004 B history
>
> 08000004 <run>:
>  ...
>  800000e:  f000 b805   b.w   800001c <ext_scale>
>  ...
>  8000014:  20000000    .word 0x20000000
>  8000018:  20000004    .word 0x20000004
> ```

对照上一节的三张便签：

- 偏移 0xe 的跳转，现在指向 `800001c`，正是 `ext_scale` 被排到的位置；
- 偏移 0x14 和 0x18 的常量，现在是 `0x20000000` 和 `0x20000004`，正是 `counter` 和 `history` 在 RAM 里的地址。

便签被撕掉了，填成了真地址。`size calc.elf` 给出 text 39、data 4、bss 400：就是两个 `.o` 的 text 加起来（33 + 6），data 和 bss 不变。

### map 文件

加的那个 `-Map=calc.map` 会生成一份"分配清单"。节选里能看到谁被放到了哪里：

```
.text           0x08000000       0x22
 .text          0x08000000       0x1c calc.o
 .text          0x0800001c        0x6 scale.o
.rodata         0x08000022        0x5
 .rodata        0x08000022        0x5 calc.o
.data           0x20000000        0x4
 .data          0x20000000        0x4 calc.o
.bss            0x20000004      0x190
 .bss           0x20000004      0x190 calc.o
```

以后"Flash 为什么爆了""RAM 都被谁吃了"，就来翻它。

## 五、两种最常见的链接错误

> [!question] 先猜一猜
> 如果我只链接 `calc.o`，不给 `scale.o`，报错信息里会不会指出**具体是哪个位置**出的问题？

> [!success]- 揭晓
> 会。链接器拿着便签去找人，找不到，就把便签的位置报出来：
> ```
> calc.o: in function 'run':
> calc.c:(.text+0xe): undefined reference to 'ext_scale'
> ```
> `.text+0xe` 就是上面那张便签的偏移 0xe。你看到的 `undefined reference`，就是一张没人认领的便签。

另一种是**重复认领**：两个文件都定义了 `ext_scale`。

```
scale2.o: in function 'ext_scale':
scale2.c:(.text+0x0): multiple definition of 'ext_scale'; scale.o:scale.c:(.text+0x0): first defined here
```

两种错误都是**链接器**报的，不是编译器。编译每个 `.c` 时都没问题，因为每个文件单独看是完整的。这也是为什么头文件里只放声明（见 [[【原理】预处理器与链接]]）：把定义放进头文件，每个包含它的 `.c` 都生成一份，链接时就会撞车。

## 六、库是什么

静态库（`.a`）就是一包 `.o` 的压缩包。链接器看到一张没人认领的便签，就去库里翻，**只把用到的那个 `.o` 取出来**拼进去，所以你用了 `puts` 不会把整个标准库都拼进来。

GNU 的链接器按命令行从左到右处理，所以库一般放在最后。这也解释了 1.1 里 hello 为什么比源文件大这么多：被拼进来的是库里的 `puts` 以及它依赖的一串东西，再加上启动代码。

在电脑上，常见的是**动态链接**：可执行文件里只留便签，运行时由操作系统加载器去找共享库（`.dll` / `.so`）再填。单片机上没有操作系统和加载器，基本都是**静态链接**：所有便签在链接时就撕完。

## 七、构建系统站在哪里

CMake 不是编译器，它也不是链接器。它做的事是：读你写的 `CMakeLists.txt`，**生成**一份构建计划（交给 Ninja 或 Make），由它们去调用上面这些命令。

好处是**增量**：你改了一个 `.c`，Ninja 只重编这一个 `.o`，其余 `.o` 原样复用，最后重新链接一次。用 `cmake --build build --verbose` 能看到每一条被调用的真实命令，和这篇里手敲的几乎一样。

## 记住这几条

1. **编译是一个 .c 一个 .o，互相看不见。** 不认识的名字贴便签。
2. **链接做三件事：合并段、解析符号、重定位。** "找不到"和"重复"都是它在解析符号时报的。
3. **.bss 不占文件空间，.data 占。** 全局变量带不带初值，会影响 Flash 用量。
4. **全局变量的地址、函数的地址，编译时都不知道**，都要靠链接时回填。
5. 想知道谁占了多少，就看 `size` 和 map 文件。

## 练习

1. 把 `history` 改成 `int history[100] = {0};`，先预测 `size` 里 data 和 bss 会怎么变，再测。（预测）
2. 把 `scale.c` 里的函数改名为 `ext_scale2`，只重新编译 `scale.c` 并重新链接，看到的报错属于哪一类？错误位置对应哪张便签？（复现）
3. 用 `-O0` 重新编译 `calc.c`，对比 `objdump -dr`：`add` 还会被展开吗？便签的数量和种类变了吗？（改造）

## 继续往下

> [!info] 原理
> - [[【原理】启动流程与链接脚本|启动流程与链接脚本]]：谁决定"代码放 Flash、数据放 RAM"？`.data` 的初值怎么从 Flash 搬到 RAM？
> - [[【原理】预处理器与链接|预处理器与链接]]：头文件、声明与定义、`static` 如何避免重名。
> - [[【原理】可执行文件格式|可执行文件格式]]：ELF 里的段表、`.bin` 和 `.hex` 是怎么从 ELF 变出来的。
> - [[【原理】交叉编译与工具链|交叉编译与工具链]]：`arm-none-eabi-` 是什么意思，为什么要特意用它。

> [!abstract] 拓展
> - [[【拓展】看反汇编|看反汇编]]：怎么读 `objdump` 的输出，怎么看编译器改写了你的代码。
