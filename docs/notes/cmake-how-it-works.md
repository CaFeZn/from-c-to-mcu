---
title: "【原理】CMake如何工作"
description: "从限幅程序的编译日志辨认配置、生成和构建，并用目标依赖传递头文件目录。"
sourceKey: "【原理】CMake如何工作"
---

# CMake如何工作

你给工程加了一个计算函数，编辑器能跳转到定义，构建却报 `undefined reference`；另一次只改了一个开关，CMake 说配置成功，运行结果却没变。两个现象都容易让人把“编辑器知道”“构建清单知道”和“程序已经更新”混成一件事。我们用一个限幅器拆开这三件事：输入负数得到零，输入四十二得到十，并记录调用次数。

## 先让一个目标真正跑起来

使用[本组完整示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build)。其中 `src/main.c` 调用 `clamp_value` 两次，`src/clamp.c` 实现限幅，`include/clamp.h` 声明接口。下面是组织这两个文件的关键配置，完整工程还带两个独立测试目标：

```cmake
cmake_minimum_required(VERSION 3.20)
project(build_notes C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(clamp STATIC src/clamp.c)
target_include_directories(clamp PUBLIC include)
add_executable(clamp_demo src/main.c)
target_link_libraries(clamp_demo PRIVATE clamp)
```

在仓库根目录打开 PowerShell，确认 `gcc`、`cmake`、`ninja` 都可用。先预测：第一条命令结束时，会不会已经有可运行的限幅程序？

```powershell
$S = "examples/notes/build"
$B = "artifacts/course-completion/build/pc"
cmake -S $S -B $B -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build $B --target clamp_demo --verbose
& "$B/clamp_demo.exe"
```

最后应得到：

```text
clamp=0,10 calls=2 mode=1
```

Linux/macOS 上执行不带 `.exe` 的文件即可。本文实测环境是 Windows、GCC 6.3.0、CMake 3.28.1、Ninja 1.11.1；示例采用 C99，不依赖那版老 GCC 的特殊行为。首次配置可能为识别编译器而编译小型探测程序，但这不等于已经构建你的业务目标。

## 日志里的三个动作不是同一层

**配置**发生在 CMake 执行构建描述时。它读取源文件清单、处理 `if` 和 `option`、识别工具链，建立名叫 `clamp` 和 `clamp_demo` 的目标以及它们之间的依赖。这一步如果找不到编译器，修正 C 代码通常无济于事。

**生成**把上述模型转换成选定后端能执行的构建规则。本例 `-G Ninja` 使构建目录出现 `build.ninja`。换成另一个受支持的生成器，产物形式会不同，但你仍应维护目标描述，而不是手改生成出来的命令。配置和生成通常由同一条 `cmake -S ... -B ...` 连续完成，日志里分别能看到 `Configuring done`、`Generating done`。

**构建**由 `cmake --build` 委托后端执行。Ninja 判断哪些输出需要更新，调用 GCC 分别编译两个翻译单元，把限幅对象装入静态库，再链接可执行文件。CMake 不是另一个 C 编译器；换了生成器也不会把不合法的 C 变合法。使用 `--verbose` 能看到实际的 `-I`、`-D`、`-c` 和链接输入，排错时这比猜编辑器按钮含义可靠。

因此，打开工程看到源码不代表文件进入了目标；配置通过不代表编译通过；编译通过也不代表你运行的是刚生成的目标。排查旧输出时，先核对执行路径与目标名，而不是马上清空所有缓存。

## 为什么头文件目录写在库上

`clamp` 公开的接口位于 `include`。它自己编译时需要该目录，使用它的 `clamp_demo` 也需要，所以写 `PUBLIC`。在 `target_include_directories` 中，`PRIVATE` 表示只供这个目标自身使用，`INTERFACE` 表示只供消费者使用，`PUBLIC` 同时包含两边。它们描述构建使用要求，不是 C 语言访问控制关键字。

`target_link_libraries(clamp_demo PRIVATE clamp)` 不仅追加库名，还建立构建顺序，并让消费者获取 `clamp` 的公开使用要求。这就是为什么主程序不用再写一遍同样的 `-I`。这里的 `PRIVATE` 是说主程序的这项链接关系不作为其公开接口继续向外传播，并不阻止它消费库的公开头文件目录。

把库的目录改成 `PRIVATE` 后，库本身仍可能编过，主程序却找不到 `clamp.h`。这恰好证明“能编库”与“库的使用者得到必要条件”是两回事。实际项目还可以把私有实现头放进另一目录，只给库自身使用，避免消费者无意依赖内部细节。

## 开关为什么要经过两次翻译

完整示例有 `option(REPORT_MODE ... ON)`，再用条件分支把它转换成目标上的 `REPORT_MODE=1` 或 `REPORT_MODE=0`。在同一 PowerShell 中执行：

```powershell
cmake -S $S -B $B -DREPORT_MODE=OFF
cmake --build $B --target clamp_demo --verbose
& "$B/clamp_demo.exe"
```

预期最后一项变为 `mode=0`，限幅结果仍为 `0,10`。命令行的 `-DREPORT_MODE=OFF` 是 **CMake 缓存变量**，不会自动成为 C 宏；`target_compile_definitions` 才把选项变成编译器命令上的定义。这条分界也解释了为什么在 CMake 中随便声明一个变量，C 程序不会直接读到它。

构建目录中的 `CMakeCache.txt` 保存工具路径和配置选项。下一次没有再写 `-DREPORT_MODE`，并不表示恢复源码里 `ON` 的默认值，已有缓存通常继续生效。要改值就明确传参；要比较两套配置，可以用两个不同的 `-B` 目录，避免互相覆盖。

## 两个特别容易走偏的边界

第一，**不要在同一个构建目录直接切换 PC 与 ARM 编译器**。工具链选择会影响探测结果、ABI、启动文件和库。建立新的构建目录，并在首次配置时选择对应编译器或工具链文件。系统 `PATH` 变了，也不能据此认定旧缓存已经选中了新编译器。

第二，**生成的构建文件不是配置源**。手改 `build.ninja` 暂时增加一个源文件，下次重新生成就会丢失。把文件登记到 `add_library`、`add_executable` 或 `target_sources` 才能留下可重复的规则。头文件通常不单独变成目标文件；它通过包含关系影响使用者，是否写在源文件清单里与编辑器展示也有关，不能混成链接规则。

另一个常见误会是把 `Debug` 当作所有生成器通用的目录名称。本文使用单配置 Ninja，`CMAKE_BUILD_TYPE` 选构建类型；Visual Studio 或 Ninja Multi-Config 通常在构建时用 `--config Debug`，可执行文件位置也可能多一层配置目录。对照自己的生成器，不要机械照搬路径。

## 练习：少写一行会在哪层坏掉

保持主程序包含 `clamp.h`。分别删除链接关系那一行、把库的头文件目录改为 `PRIVATE`，预测最先出现的问题。为了单独观察漏链接，可临时给主程序显式添加头文件目录。

<details><summary>答案与排查顺序</summary>

删除链接关系后，主程序也失去从库继承的包含目录，往往先报找不到头文件；不能因为还没看到链接错误，就认为漏链接没问题。显式补上包含目录后，主程序能编译，链接时仍找不到函数和计数器的定义。只把库目录改成 `PRIVATE`，则链接依赖仍在，但主程序的编译条件不完整。看第一条失败命令，就能区分“编译时缺接口路径”和“链接时缺实现”。修复时恢复目标公开使用要求及链接关系，避免靠全局包含目录掩盖结构问题。

</details>

## 适用范围与依据

本文讨论 CMake 3.20 以上的目标式 C 工程，具体命令对照 CMake 3.28 文档。交叉编译仍需额外提供目标平台与工具链信息，本文不把 PC 可执行文件当作 MCU 固件。

- [CMake 3.28 cmake(1)](https://cmake.org/cmake/help/v3.28/manual/cmake.1.html)：Introduction to CMake Buildsystems、Generate a Project Buildsystem、Build a Project。
- [target_include_directories](https://cmake.org/cmake/help/v3.28/command/target_include_directories.html)：目标自身与接口目录属性。
- [target_link_libraries](https://cmake.org/cmake/help/v3.28/command/target_link_libraries.html)：Overview 与使用要求传播。

回到主线：[1.8 多文件工程](/lessons/1-8)。继续阅读：[预处理器与链接](/notes/preprocessor-linkage)、[CMake与增量编译](/notes/incremental-builds)、[Windows上的现代GCC](/notes/windows-gcc)。
