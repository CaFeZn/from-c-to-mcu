---
title: "【拓展】CMake与增量编译"
description: "用源码、私有头、公共头和生成文件的变更，追踪Ninja为什么重建或保持不动。"
sourceKey: "【拓展】CMake与增量编译"
---

# CMake与增量编译

第一次构建花了几秒，第二次只说 `no work to do`；改主函数时只编一个文件，改某个公共头却几乎全编。更棘手的是协议描述明明更新了，自动生成的头文件却没变。把这些现象都归因于“缓存坏了”，会让人养成每次删目录重来的习惯，却无法知道依赖关系哪里缺了一条。

## 在独立副本上观察重建范围

使用[本组示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build)。我们有一条明确的小依赖链：`main.c` 包含公共 `clamp.h`；`clamp.c` 同时包含公共头与私有 `config.h`；它们最终链接到 `clamp_demo`。另有 `generated_demo` 依赖由数据文件生成的头文件。

为了保留原始示例，把实验副本和构建结果都放入产物目录。仓库根目录 PowerShell：

```powershell
$S = "artifacts/course-completion/build/incremental-lab/source"
$B = "artifacts/course-completion/build/incremental-lab/output"
cmake -E copy_directory examples/notes/build $S
cmake -S $S -B $B -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build $B --verbose
cmake --build $B --verbose
```

首次会编译、归档和链接，第二次应有 `ninja: no work to do.`。不要把两次耗时当主要证据，观察实际执行的命令数更可靠；磁盘缓存、杀毒软件和系统负载都会影响秒数。

接下来每次只改一个输入，构建后再改下一个。可以在副本相应文件末尾加一句注释，保存，再运行构建。注释不改功能，却仍能让输入的修改时间变化。先预测哪份对象需要更新，再对照下表：

| 改动 | 预期重新编译 | 随后动作 |
| --- | --- | --- |
| `src/main.c` | 仅主程序对象 | 重链接程序 |
| `include/config.h` | 仅限幅库对象 | 重建静态库并重链接 |
| `include/clamp.h` | 主程序和限幅库对象 | 重建静态库并重链接 |
| 没有改动 | 无 | 无 |

本次用 Windows GCC 6.3.0、CMake 3.28.1、Ninja 1.11.1 逐项实测与表一致。实验脚本修改的是另一个独立副本，未把注释写回公开源码。

## 后端依据什么决定要不要重建

CMake把目标关系转换为构建图：源文件和头文件是输入，对象文件、静态库、可执行文件是输出，边描述如何由输入得到输出。Ninja检查输出是否缺失、输入是否比输出新，并记录构建命令是否变化。它不是每次都把所有C文件解析一遍再判断“这次修改有没有影响功能”。

这解释了为什么只加注释也可能重新编译，为什么修改 `-O0` 为 `-O2` 即使源码没变也要重编。增量正确性首先意味着“所有受影响输出都更新”，并不保证“每次只做从语义上绝对必要的最少工作”。过度依赖降低效率，缺少依赖则可能产生貌似成功的旧产物。

CMake还会为自己的输入建立重新生成关系。修改构建描述后，再执行 `cmake --build`，后端通常先请求CMake重新配置、生成，再继续构建。这不是每次都应手动运行所有步骤，也不代表任意外部数据变化都会自动被CMake识别。

## 编译器比构建工具更懂包含关系

头文件可以条件包含：同一个源文件在不同宏和包含目录下，真正读到的文件集合不同。GCC能在编译时输出依赖信息，CMake为Ninja生成相应的依赖收集规则，Ninja再把它存进自己的依赖数据库。因此，公共头没有被单独编成对象，依然能触发所有消费者重编。

手工观察时，可以让GCC额外输出一份依赖文件：

```powershell
gcc -std=c99 -I "$S/include" -MMD -MF "$B/clamp.d" -c "$S/src/clamp.c" -o "$B/probe-clamp.o"
Get-Content "$B/clamp.d"
ninja -C $B -t deps
```

前一条结果应列出源文件以及两个用户头文件。`-MMD` 不列系统头依赖，不能据此推断系统头永远不影响编译。CMake/Ninja管理的临时依赖文件也可能在导入数据库后删除，所以“找不到某个.d文件”本身不能证明头文件依赖失效。

想知道Ninja为什么准备重建，可执行 `ninja -C $B -d explain -n`。`-n` 只预览，不会真的更新输出；如果之前已经完成构建，再看自然可能无事可做。诊断最好在保存改动之后、真正构建之前进行。

## 配置选项改变，也会改变输入

完整工程的 `REPORT_MODE` 只影响主程序的编译定义。执行：

```powershell
cmake -S $S -B $B -DREPORT_MODE=OFF
cmake --build $B --verbose
& "$B/clamp_demo.exe"
```

预期输出末尾为 `mode=0`，只重编主程序并重链接，不重新编译限幅库。原因不是CMake理解了宏的业务语义，而是只有该目标的编译命令从定义1变为定义0。把这项定义做成全局选项，可能扩大重编范围；这也是目标式配置有利于控制影响面的原因之一。

对比新旧配置时，既要看缓存值，也要看真实编译命令。环境变量、代码生成工具版本或编译器二进制变化，不一定都被普通的时间戳构建图自动覆盖。更换工具链时建立新构建目录，比期待旧目录猜出所有环境变化可靠。

## 生成文件必须写清输入、输出与消费者

本例 `schema.txt` 初始只有数字7，`generate.cmake` 检查范围并生成 `#define RECORD_COUNT 7`。主程序据此输出 `records=7`。关键规则不是“构建前随便运行一次脚本”，而是一个有输入输出的节点：

```cmake
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/schema.txt"
          "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generated.h"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/generate.cmake"
  DEPENDS schema.txt generate.cmake
  VERBATIM)
add_executable(generated_demo src/generated.c
               "${CMAKE_CURRENT_BINARY_DIR}/generated.h")
target_include_directories(generated_demo PRIVATE
                           "${CMAKE_CURRENT_BINARY_DIR}")
```

在副本里把数字7改成9再构建，应先运行生成器，再编译消费者，程序输出 `records=9`。本次已实测，随后无改动再构建恢复为 `no work to do`。`OUTPUT` 表示主要产物；`DEPENDS` 同时列数据与生成脚本，因为改生成逻辑也应使产物失效；将输出列为目标源文件，使首次构建时先生成再编译。`VERBATIM` 让工具尽量正确传递参数，尤其是含空格的路径。

若脚本还产生其他会被消费的文件，应按需要声明 `BYPRODUCTS`。只加“先构建某个目标”的顺序依赖，不总等于“这个可执行生成器更新时要重新生成文件”的文件级依赖。忽略这些区别，串行构建可能侥幸成功，并行时却读取不存在或陈旧的文件。

## 三个比清空缓存更早的检查

首先看正在构建的目录和目标，避免改了副本A却运行B。其次看缺失的依赖边：生成脚本是否声明输入，公共头是否真的被包含，条件分支是否选中了你想编的源文件。最后再考虑时间戳异常，如恢复了保留旧时间的文件、同步工具改写时间或系统时钟漂移。Ninja不是全目录内容哈希审计器。

新增源文件也不同于修改已登记源文件。显式在目标清单里加文件，会触发CMake重新生成。普通 `file(GLOB ...)` 只在配置时扫描，新增文件可能不被现有构建图感知；`CONFIGURE_DEPENDS` 会加入重新扫描检查，但有生成器支持与每次扫描成本，CMake官方仍不推荐用它收集一般工程源文件。不要把“自动扫描”当成无需维护构建意图。

## 练习：为什么第一次正确，第二次错了

一个生成命令声明了 `OUTPUT generated.h`，却没有 `DEPENDS schema.txt`。干净构建成功，随后改数据文件，程序仍打印旧值。为什么删除输出能暂时修好，而不能算真正修复？

<details><summary>答案与依赖解析</summary>

首次输出不存在，构建图有理由运行生成器；之后输出存在，却没有从数据文件连到输出的依赖边，后端就不知道数据变化与它有关。删除输出只是再次触发“产物缺失”条件，下次改数据还会复发。正确修复是声明输入文件与生成脚本依赖，并让消费者依赖输出。验证要包含干净构建、改输入后的增量构建、无改动构建三个阶段，而不只证明清空目录后能跑一次。

</details>

## 依据与适用范围

- [Ninja手册](https://ninja-build.org/manual.html)：Comparison to Make、The Ninja log、C/C++ header dependencies、depfile与deps。
- [CMake 3.28 add_custom_command](https://cmake.org/cmake/help/v3.28/command/add_custom_command.html)：Generating Files、DEPENDS、BYPRODUCTS。
- [CMake 3.28 file](https://cmake.org/cmake/help/v3.28/command/file.html)：GLOB与CONFIGURE_DEPENDS的说明和限制。

本文使用单配置Ninja；其他构建后端的日志、依赖记录及诊断选项不同。回到主线：[1.8 多文件工程](/lessons/1-8)。相邻主题：[CMake如何工作](/notes/cmake-how-it-works)、[预处理器与链接](/notes/preprocessor-linkage)。
