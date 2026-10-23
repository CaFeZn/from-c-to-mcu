---
title: 拆开一次构建
description: 拆开一次构建 · 从 C 到单片机
sourceKey: 0.1_拆开一次构建
---

# 0.1 拆开一次构建

> **自学选读，可完全跳过。** 建议先完成 [[1.0_搭建环境]] 和 [[1.1_Hello_World]]，已经运行过程序、想了解构建过程时再读。
> **这一篇要做的事**：从一个已经会运行的 hello 程序出发，查看 CMake 调用了什么，再拆开预处理、编译、汇编和链接。
> **前置**：[[1.0_搭建环境]]　｜　**预计**：阅读 + 动手 40 分钟

> [!tip] 跳读自测：这一节你可能不用读
> 先回答下面四个问题（写下来，别只在心里答）。**全部答对**就可以跳过本篇，回到 [[第0章_工具与构建_自学选读]]；有一题拿不准，就从头读。
> 1. 构建时看到 `` undefined reference to `foo' ``，是编译器还是链接器在报错？
> 2. `gcc -c hello.c` 得到的 `hello.o` 能直接运行吗？为什么？
> 3. 说出可执行文件比 `.c` 源文件大得多的一个原因。
> 4. 想看 CMake 构建时到底调用了哪些编译和链接命令，加什么选项？
>
> > [!success]- 答案，以及跳过后要带走的东西
> > 1. 链接器。编译器只认识你写的那个文件，找不到定义是链接阶段才发现的。
> > 2. 不能。它还没有链接，里面没有库函数的实现，也没有启动代码。
> > 3. 链接时把库函数的实现和程序启动用的代码一起拼进来了。
> > 4. `cmake --build build --verbose`（可简写 `-v`），想让所有文件都重编一遍就再加 `--clean-first`。
> >
> > **带走清单**：本篇不增加主线必需的文件或配置。实验沿用 [[1.0_搭建环境]] 的 `c-lab` 工程。读报错时可以留意：构建输出里 `Building` 那一行失败是编译器投诉，`Linking` 那一行失败是链接器投诉。

## 先看一个怪现象

刚出生的婴儿，第一件事是什么？哭。医生要听到这一声，才敢说一句"活了"。

程序员也有这个传统。拿到一门新语言、一块新板子、一个新环境，第一件事就是让它**开口说话**，输出一句 `hello, world`。这句话不是说给人听的，是说给我们听的：通了，它活着。

这个传统有出处。据说这句话最早出现在 1970 年代贝尔实验室 Brian Kernighan 写的教程里，1978 年那本《C 程序设计语言》又把它传遍了世界（出处 待核对）。

但今天我想问的不是这句话是什么意思，而是它**是怎么被说出来的**。你只会敲五行字，可你按下回车的那一瞬间，电脑要过好几道手，屏幕上才会出现一个字。

> [!question] 先猜一猜
> 我们马上要写的程序，源文件只有 6 行，十几个单词。编译出来的可执行文件，你觉得有多大？
> A. 几百字节　B. 几 KB　C. 几十 KB　D. 几个 MB
> 先在心里选一个，等会儿我们量。

## 动手做

### 第一步：确认环境

这一节在 [[1.0_搭建环境]] 里建好的 `c-lab` 工程里做。如果你还没有，先回去做完 1.0。确认方法很简单：在 `c-lab` 目录里执行

```
cmake --build build
```

没有报错就行。

### 第二步：写下你的第一句话

打开 `c-lab/src/hello.c`（1.0 里已经建好了），内容应该是：

```c
#include <stdio.h>

int main(void) {
    printf("hello, world\n");
    return 0;
}
```

`CMakeLists.txt` 末尾已经有一行 `add_demo(hello)`。构建并运行：

```
cmake --build build
./build/hello
```

你应该看到：

```
hello, world
```

Windows 的命令提示符里把 `./build/hello` 换成 `build\hello.exe` 即可。

### 第三步：让它说你自己的话

把引号里的内容改成你想让电脑说的话，重新构建、运行。先用英文。如果你写了中文，终端里出现乱码，那是终端编码和源文件编码对不上，不是你写错了，先别慌。

### 第四步：故意弄坏它

下面三种"死法"，每种都改 `src/hello.c`、构建一次，记下是**谁**在投诉、它怎么说的（试完一种就改回去）：

1. 删掉 `printf` 那一行末尾的分号
2. 把 `printf` 改成 `pritnf`
3. 把整个 `main` 函数的名字改成 `mian`

> [!question] 先猜一猜
> 这三种错误，是同一个"工种"投诉的，还是不同的工种？
> 提示：看构建输出里失败的是 `[1/2] Building` 还是 `[2/2] Linking`，再看报错信息的开头是 `error:` 还是 `undefined reference`。

> [!success]- 揭晓
> 不是同一个。不同的 gcc 版本措辞略有差别，我这边（Windows，MinGW gcc 6.3.0）看到的是：
> - 漏分号：失败在 `Building` 那一步，**编译器**投诉：
>   ```
>   FAILED: CMakeFiles/hello.dir/src/hello.c.obj
>   …/src/hello.c:5:5: error: expected ';' before 'return'
>   ```
>   注意它指的是 `return` 那一行：编译器读到 `return` 才发现上一句没结束。第二步 `Linking` 根本没有开始。
> - `pritnf`：`Building` 那一步只有一条警告 `implicit declaration of function 'pritnf'`（编译器说：我不认识这个词，先当它存在），通过了。接着失败在 `Linking` 那一步，**链接器**投诉：
>   ```
>   undefined reference to `pritnf'
>   ```
> - `mian`：编译顺利通过，失败在 `Linking`，**链接器**投诉找不到程序入口。我这边的原话是 `` undefined reference to `WinMain@16' ``（Windows 的叫法；Linux 上通常直接说找不到 `main`）。
>
> 所以你按下的那一个"构建"，其实不是一个人在干活。构建输出里 `Building` 和 `Linking` 两个词，已经替你标出了是哪一站出了问题。

### 第五步：抓现行

那个"构建"背后到底请了几个工种？让 CMake 把它实际调用的命令都打印出来：

```
cmake --build build --clean-first --verbose
```

`--clean-first` 让它先清掉旧产物，保证每个文件都重编；`--verbose` 让它不要把命令藏起来。输出最后两行是（路径用 `…` 省略了）：

```
[1/2] C:\MinGW\bin\gcc.exe   -std=c99 -Wall -Wextra -MD -MT CMakeFiles/hello.dir/src/hello.c.obj -MF CMakeFiles\hello.dir\src\hello.c.obj.d -o CMakeFiles/hello.dir/src/hello.c.obj -c …/c-lab/src/hello.c
[2/2] C:\WINDOWS\system32\cmd.exe /C "cd . && C:\MinGW\bin\gcc.exe   CMakeFiles/hello.dir/src/hello.c.obj -o hello.exe -Wl,--out-implib,libhello.dll.a -Wl,--major-image-version,0,--minor-image-version,0  -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 && cd ."
```

> [!example] 我这台机器上的结果（Windows，MinGW gcc 6.3.0，CMake 3.28.1；你的路径和细节会不同）
> 这两行就是真相：
> - 第一行是**编译**。`gcc` 加了 `-c`，意思是"只编译，不链接"，产物是目标文件 `hello.c.obj`（Linux 上叫 `.o`）。`-std=c99 -Wall -Wextra` 是 `CMakeLists.txt` 里写的。`-MD` 一类选项是让 Ninja 记下这个文件用了哪些头文件，以后头文件改了它才知道要重编，现在不用管。
> - 第二行是**链接**（外面包的那层 `cmd.exe /C` 是 Windows 的习惯）。这次 `gcc` 没有 `-c`，输入是刚才的 `.obj`，输出是 `hello.exe`。后面一长串 `-l...` 是 Windows 系统库，CMake 默认替你加上。

CMake 和 Ninja 是在调度，真正动手的还是 `gcc`。那 `gcc` 自己呢？再往里看一层，让它自己招供。先建一个放杂物的目录，免得弄脏 `src/`：

```
mkdir scratch
cd scratch
gcc -v ../src/hello.c -o hello_v
```

输出很长，在里面找带 `cc1`、`as`、`collect2` 的几行（Windows 下名字后面带 `.exe`）。这是它真实调用的几个程序。

> [!example] 我这台机器上的结果（Windows，MinGW gcc 6.3.0；你的数字会不同）
> 请来的工种依次是 `cc1.exe`、`as.exe`、`collect2.exe`（它再去调链接器）。

### 第六步：一站一站拆开看

接着在 `scratch` 目录里，把这一条命令拆成四条，每一站都留下一个产物。这里的 `-std=c99` 要和 CMake 用的保持一致，这样拆开的结果才和上面构建出来的是同一个东西：

```
gcc -std=c99 -E ../src/hello.c -o hello.i     # 预处理
gcc -std=c99 -S ../src/hello.c -o hello.s     # 编译成汇编
gcc -std=c99 -c ../src/hello.c -o hello.o     # 汇编成目标文件
gcc hello.o -o hello_manual                   # 链接成可执行文件
nm hello.o                                    # 看目标文件里有哪些名字
```

> [!example] 我这台机器上的结果（你的数字会不同，关系应该相似）
> - `hello.c` 只有 6 行，`hello.i` 有 **299 行**
> - `hello.s` 里 `main` 调用的是 `call _printf`，而 `_printf` 这个函数体也在这个文件里（它是 `stdio.h` 里贴进来的一小段包装代码），它再去 `call ___mingw_vprintf`
> - `nm hello.o` 里 `___mingw_vprintf` 前面是 `U`（Undefined，未定义），`_main` 前面是 `T`（已定义）
> - `hello.o` 是 **935 字节**，`hello_manual.exe`（以及 CMake 构建出的 `hello.exe`）是 **89,392 字节**
>
> 平台不同，名字也不同：很多 Linux 上你会看到编译器把 `printf("...\n")` 悄悄换成更简单的 `puts`（待核对）。

现在回头看开头那道题：这个源文件只有 6 行，可执行文件却有将近 90 KB。

> [!question] 再猜一猜
> 多出来的这将近 90 KB，是谁写的？是什么时候加进去的？
> 提示：看 `nm` 的结果里，`___mingw_vprintf` 前面那个 `U`。

> [!success]- 揭晓
> 不是你写的，也不是编译器写的。
> 编译器只认识你写的那一个文件。它看到 `___mingw_vprintf` 不知道在哪，只好在目标文件里贴一张便签：此处待补（就是那个 `U`）。
> **链接器**才去库里把它的实现找出来，接到你的程序上，并把程序启动需要的代码也一起接进来。库和启动代码是文件变大的原因之一；文件头、对齐和可能存在的调试信息也会占空间，不能把全部差值都算成库代码。

## 刚才发生了什么

你按下回车到屏幕出现字，中间其实过了七站，每一站有一个"工种"。CMake 和 Ninja 站在这条路的最前面，它们只负责调用 gcc，自己不翻译也不链接。gcc 一个人带着第 ② 到第 ⑤ 站：

| 站 | 工种（真名） | 比喻 | 它做的事 |
|---|---|---|---|
| ① | 你 | 作者 | 写下一份电脑看不懂的稿子（`.c`） |
| ② | 预处理器 | 秘书 | 只会剪刀加胶水：把 `#include` 的整份稿子贴进来 |
| ③ | 编译器 | 翻译官 | 把 C 翻成汇编；不认识的词只贴便签 |
| ④ | 汇编器 | 抄写员 | 把汇编变成机器码，装进 `.o` |
| ⑤ | 链接器 | 剧组统筹 | 按便签找演员，把库和启动代码拼上 |
| ⑥ | 操作系统 | 接生员 | 把程序搬进内存，把输出接到终端 |
| ⑦ | CPU | 演员 | 一条一条执行，最终把字节交给屏幕 |

注意最后一句：CPU 从头到尾不知道 "hello" 是什么意思，它只是机械地执行指令。**屏幕上那一行字，是电脑自己说出来的第一句话，但每一个字，都是你决定的。**

等我们到了单片机上，这条路上好几站都没有了：没有操作系统接生，没有终端，甚至没有屏幕。那时候这句 hello 要怎么说出来？这是后面几章的故事。

### 这套流程到了单片机上有什么不同

先记住几个词，后面会反复碰到，现在只要有个印象：

- **工具链**（toolchain）：编译器、汇编器、链接器这一整套工具的合称。
- **交叉编译**：你在电脑上编译，但编出来的程序是给**另一种 CPU** 跑的。电脑是 x86，单片机是 ARM，指令集不同，电脑编的程序单片机跑不了。所以要用专门的编译器，比如 `arm-none-eabi-gcc`。编译的这台机器叫 **host**，程序跑的那台叫 **target**。
- **构建工具不变**：单片机工程用的还是 CMake 和 Ninja，只是 CMake 被告知"去调用 `arm-none-eabi-gcc`"，而不是你电脑上的 gcc。1.0 练过的配置、构建、运行操作，到这里仍然用得上。
- **没有操作系统**：单片机上没有"接生员"。程序被**烧录**进 Flash。以 Cortex-M 为例，复位后 CPU 依据向量表取得初始栈指针和复位入口，再由**启动代码**准备环境并调用 `main`。
- **产物不一样**：电脑上得到 `.exe` 或可执行文件；单片机上得到 `.elf`、`.bin`、`.hex` 这类文件，再下载进芯片。

具体怎么配、怎么下载，到 2.2 会亲手做一遍。原理放在分支里，想看再点。

每一站具体怎么做，细节放在分支里，想看再点。

## 记住这几条

1. **"编译"和"链接"是两件事**。看到 `undefined reference`，是链接投诉的，去找函数有没有定义、库有没有接上。
2. `printf` 不是 C 语言本身，是标准库提供的函数。语言只有关键字和语法。
3. 构建输出里 `Building` 失败是编译器投诉，`Linking` 失败是链接器投诉；报错信息的**开头**也会告诉你是哪一站，先看它，再看细节。
4. `c-lab` 的 `CMakeLists.txt` 已经替你加了 `-Wall -Wextra`，让编译器多说话。警告不是废话。
5. 文件大小是用来**量**的，不要凭感觉猜。
6. 在电脑上编译、在电脑上运行，叫本机编译；在电脑上编译、拿到单片机上运行，叫**交叉编译**。后面写单片机程序，全部是后者。
7. 想看工具到底干了什么，就让它打印出来：`cmake --build build --verbose`。

## 练习

1. 在 `scratch` 里用 `gcc -std=c99 -c ../src/hello.c -o hello.o` 只编译不链接，然后试着直接运行得到的 `hello.o`，看看会发生什么。（复现）
2. 把 `printf` 换成 `puts("hello, world")`，重新构建，量一量 `hello.exe` 的大小。再另开一个构建目录，用 `cmake -S . -B build-os -G Ninja -DCMAKE_C_FLAGS=-Os` 配置、`cmake --build build-os` 构建，文件大小有变吗？（改造）
3. 如果把 `#include <stdio.h>` 这一行去掉，你预测构建时会发生什么？先写下预测，再试。（预测）

> [!success]- 提示与答案方向
> 1. 它不是可执行文件，系统不会运行它（我这边的 bash 报 `cannot execute binary file`），因为还没链接。
> 2. 我这边换成 `puts` 后降到 40,764 字节，比 `printf` 版的 89,392 小得多；再加 `-Os` 只差几十个字节（40,800）。差别主要来自 `printf` 在这个平台上会拉进一整套格式化代码，大小变化与平台有关，以你自己量到的为准。注意第二个构建目录用的是同一份 `CMakeLists.txt`，只是配置时多传了一个参数。
> 3. 编译器不认识 `printf`：我的 gcc 6.3 只给两条警告（`implicit declaration of function 'printf'` 等），照样能构建运行；较新的 gcc 默认把它当成错误（待核对）。所以不要忽略警告。

## 想深入？

> [!info] 原理 · 建议读，把这一节的"为什么"补齐
> - [[【原理】从源码到可执行文件|从源码到可执行文件]]：这四站各自产出什么？链接到底怎么按便签找人？
> - [[【原理】交叉编译与工具链|交叉编译与工具链]]：为什么电脑编的程序单片机跑不了？`arm-none-eabi-gcc` 这个名字每一段是什么意思？
> - [[【原理】预处理器与链接|预处理器与链接]]：为什么头文件里只放声明？为什么 `static` 能避免重名？

> [!abstract] 拓展 · 自选，往深处走
> - [[【拓展】看反汇编|看反汇编]]：看懂 `hello.s` 里每一行在干什么

---
选读目录：[[第0章_工具与构建_自学选读]] ｜ 回主线：[[1.1_Hello_World]]
