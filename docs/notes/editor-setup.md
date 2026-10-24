---
title: 让 VS Code 看懂 CMake 工程：能构建，却还在画红线
description: 让 VS Code 看懂 CMake 工程：能构建，却还在画红线 · 从 C 到单片机
sourceKey: 【拓展】让VSCode看懂CMake工程
---

# 让 VS Code 看懂 CMake 工程：能构建，却还在画红线

> **按需阅读**：工程已经能构建运行，但代码补全、跳转或头文件提示不正常，或者你准备使用 clangd。
> 先完成 [[1.0_搭建环境]]；这里不重新安装另一套编译器。返回 [[第1章_C语言起步]]。

## 先确认是哪一边出了问题

你点“构建”，程序运行正常；回到编辑器，`#include` 却被画上红线，点击函数也跳不到实现。这时要区分两份工作：**构建工具在生成程序，代码分析扩展在帮助你编辑**。前一份成功，不代表后一份自动拿到了同样的配置。

先看 CMake 的构建结果。如果它也失败，先修第一条构建错误；只有构建成功、提示不对，才沿着下面检查。不要为了消除红线，把真实的头文件和代码删掉。

先确认当前窗口的 Profile：本文的 `c-lab` 用 `C-PC`，板级工程用 `STM32`。扩展装在另一套 Profile 里，不一定在当前窗口启用；先切对配置再检查扩展。后面的 JSON 都写入**当前工程**的 `.vscode/settings.json`，不是向所有 Profile 共用的用户设置里追加。

## 已经用 C/C++ 扩展：先接通现有配置

起步篇默认安装了 Microsoft 的 C/C++ 扩展。先按 [[1.0_搭建环境#第五步：程序能跑，让编辑器也看懂它]]，让它使用 CMake Tools 提供的配置：

```json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

把这一项合并到现有工作区设置，不要用这三行覆盖其他设置。重新配置后，提示、跳转正常，就到这里为止；**不必为了跟完本篇而换扩展。**

## 准备使用 clangd，再走下面这条路线

clangd 是另一种代码分析工具，负责补全、诊断和跳转；它不会替代 CMake 构建你的程序。下面仍使用同一个 c-lab 和同一个 gcc。

### 1. 先让 CMake 导出它正在使用的编译信息

打开 c-lab 根目录的 `CMakeLists.txt`，在 `project(c_lab C)` 之后加入这一行：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

保存，执行 `CMake: Configure`。然后在资源管理器中展开 `build`，应当看到新文件 `compile_commands.json`。

这个文件记录了每个源文件实际使用的编译器、头文件目录、宏和选项。让 clangd 读取它，比自己再手抄一套配置更不容易出错。不要手动编辑这个生成文件；项目设置变化后重新配置，让 CMake 更新它。

本教程使用 Ninja，支持导出这份文件。这个 CMake 选项不是对所有生成器都生效，例如 Visual Studio 生成器不会按此生成编译数据库。

### 2. 安装分析扩展，并让它找到 clangd 程序

先确认打开的是 `c-lab`、当前 Profile 是 `C-PC`，再在扩展面板安装 **clangd**，扩展 ID 是 `llvm-vs-code-extensions.vscode-clangd`。只在需要它的 Profile 中启用，不用为了这一份 PC 练习改动 STM32 窗口的扩展组合。

扩展是 VS Code 和分析程序之间的连接。若它提示找不到 clangd，可以按提示下载 clangd；若电脑上已有一个版本，在设置中的 `clangd.path` 指向实际的可执行文件。安装扩展、找到 clangd 程序和找到 gcc 是三件不同的事，不要把其中一个路径填到另一个位置。

### 3. 只留一套代码分析，避免重复提示

C/C++ 扩展可以保留，用它提供调试能力；但代码分析交给 clangd 后，把 C/C++ 的 IntelliSense 关闭。**这只是在分工，不是把所有报错隐藏掉。**

下面是与 1.0 配置合并后的示例。工程假设放在 `C:/dev/c-lab`，gcc 按默认位置安装在 `C:/msys64/ucrt64/bin`；实际位置不同，要同时改对应路径。

```json
{
  "cmake.generator": "Ninja",
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "C_Cpp.intelliSenseEngine": "disabled",
  "clangd.arguments": [
    "--background-index",
    "--compile-commands-dir=C:/dev/c-lab/build",
    "--query-driver=C:/msys64/ucrt64/bin/gcc.exe"
  ]
}
```

- `C_Cpp.intelliSenseEngine` 只关闭 C/C++ 扩展的代码分析，clangd 的诊断仍然保留。
- `--compile-commands-dir` 要指向**包含** `compile_commands.json` 的目录，不是 JSON 文件本身。
- `--query-driver` 允许 clangd 调用这里指定的 gcc，查询该工具链的系统头文件等信息。只填写自己信任的编译器路径，不要写成任意程序都可匹配的 `*`。
- clangd 和编译数据库中的 gcc 必须对应同一套工具。你的实际编译器不是示例中的 UCRT64 gcc 时，按编译数据库记录的真实路径修改，不要照抄成另一套。

保存后在命令面板执行 `clangd: Restart language server`。如果需要，用 `Developer: Reload Window` 重载工作区。第一次建立索引可能需要等待。

### 4. 用现象确认，而不只看红线消失

1. 把鼠标停在 `printf` 上，检查有没有函数提示。
2. 按住 `Ctrl` 点击 `stdio.h`，检查能否打开正确工具链的头文件。
3. 到 [[1.8_预处理与多文件工程]] 的 `cmdtab` 例子中，尝试从调用处跳到 `dispatch` 的声明或定义。
4. 在可恢复的练习代码中临时制造一个漏分号错误，确认分析工具仍会提示，再立刻改回。

能构建、能得到正确补全、能跳转、真实错误仍能被指出，这几件事要一起成立。只把扩展关掉、让编辑器安静下来，不算完成配置。

## 仍然不对时，先查这几处

| 现象 | 先检查 |
|---|---|
| 没有 `compile_commands.json` | 是否保存了 CMake 选项并重新配置，生成器是否为 Ninja，当前 build 目录是否与设置一致。 |
| 换了工程位置后提示全乱 | `--compile-commands-dir` 是否仍指向旧目录；重新配置生成新数据库，再重启 clangd。 |
| 系统头文件如 `stdio.h` 找不到 | 数据库里实际使用的 gcc 路径是什么，`--query-driver` 是否匹配这一个可信编译器。 |
| 工程自己的 `util.h` 找不到 | `target_include_directories` 是否包含正确目录；不要只改编辑器设置来掩盖构建清单的问题。 |
| 新加的 `.c` 没有正确提示 | 源文件是否加入 CMake 目标，是否重新配置；检查数据库里有没有这一份文件的记录。 |
| 出现两份重复诊断 | 是否同时启用了 C/C++ IntelliSense 与 clangd。 |
| 换 Profile 后扩展命令消失 | 检查当前窗口关联的 Profile，以及其中是否启用了对应扩展；不要只检查另一套配置的扩展列表。 |
| 切到单片机工程后提示不正常 | 确认是 `STM32` Profile，但不能只切 Profile：不要沿用 PC 的 gcc 路径。先构建实际单片机工程，再让分析工具读取该工程的数据库和 ARM 工具链。 |

> [!tip]- 命令行检查，只在需要排查时用
> 在工程目录执行 `cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`，可以明确要求导出数据库。
> 若已能从终端调用 clangd（12 或更新版本），可以用 `clangd --check=src/hello.c --compile-commands-dir=build --query-driver=C:/msys64/ucrt64/bin/gcc.exe` 检查它实际读到了什么。路径按真实工具链修改。
> 日志里若显示 `Generic fallback command`，说明它没有使用你期望的那条数据库记录，先检查目录和文件路径，不要随意堆编译选项。

## 依据与验证边界

- [CMake：CMAKE_EXPORT_COMPILE_COMMANDS](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)：导出内容与生成器限制。
- [clangd：系统头文件与 query-driver](https://clangd.llvm.org/guides/system-headers)：编译命令、系统头文件和可信驱动查询。
- [vscode-clangd](https://github.com/clangd/vscode-clangd)：扩展安装、clangd 程序和配置入口。
- [VS Code C/C++ 设置](https://code.visualstudio.com/docs/cpp/customize-cpp-settings)：配置提供者。

配置项已对照扩展的公开配置定义核对。本机还用现有 MinGW gcc 6.3、CMake + Ninja 导出编译数据库，执行了 `clangd --check`：日志确认读取了数据库、成功查询指定 gcc 的系统头文件，检查完成为 0 errors。

这是命令行链路验证，不代表已经在全新的 MSYS2 安装或每个 VS Code 版本中逐步点击测试。示例中的 UCRT64 路径须按实际安装核对；不要把版本不同导致的界面变化当成代码错误。

回主线：[[1.0_搭建环境]] · [[1.8_预处理与多文件工程]]
