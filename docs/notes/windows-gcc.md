---
title: "【拓展】在Windows上安装现代gcc"
description: "用MSYS2 UCRT64安装GCC、CMake和Ninja，再通过路径、目标平台及C99程序验证实际使用的工具链。"
sourceKey: "【拓展】在Windows上安装现代gcc"
---

# 在Windows上安装现代gcc

你刚安装了新编译器，`gcc --version` 却仍显示多年以前的版本；VS Code说找不到标准头文件，终端又能编译。另一个同学装的是 `arm-none-eabi-gcc`，能生成固件，却无法双击运行。这里至少混了三个问题：工具有没有安装、当前命令找到了哪一份、这份工具为哪个平台生成程序。按顺序验证，比重新装编辑器有效。

## 先确定这次要运行在哪里

本篇目标是**在Windows电脑上编译并运行课程第一章的C程序**。选择MSYS2的UCRT64环境，使用面向64位Windows的MinGW-w64 GCC，配套CMake与Ninja。MSYS2同时提供包管理和类Unix工具，但UCRT64编译器生成的通常是原生Windows程序，不是要求你在虚拟Linux里运行的程序。

“GCC”不是一个固定平台的单一程序。`arm-none-eabi-gcc` 面向裸机ARM，供后续STM32固件使用；MSYS环境的 `/usr/bin/gcc` 与UCRT64的 `/ucrt64/bin/gcc` 又有不同的运行库和使用目标。工具名相似，不意味着对象文件、静态库和最终运行环境可以混搭。

本篇核对了MSYS2官方环境、更新文档与三个包页。MSYS2是滚动发行，不固定一个永久最新版本号；安装时以仓库实际版本和官网支持的Windows版本、体系结构为准。本文没有在本机重新安装MSYS2，后面的安装结果是验收标准，不是一次全新系统安装的实测报告。

## 从官方安装器进入正确环境

从[MSYS2官网](https://www.msys2.org/)取得适合自己系统的安装器，按官方流程安装。以下使用默认的 `C:\msys64` 举例；若选择其他位置，后续路径必须同步改变。不要从不明压缩包复制几个编译器可执行文件，GCC还需要头文件、汇编器、链接器和库等配套内容。

打开开始菜单中的 **MSYS2 UCRT64** 终端，先执行：

```bash
echo "$MSYSTEM"
command -v gcc
```

第一条应为 `UCRT64`。尚未安装GCC时，第二条找不到命令是正常的；若显示别的环境路径，先检查打开的是哪个终端。环境会安排搜索路径，UCRT64通常让 `/ucrt64/bin` 位于 `/usr/bin` 之前，不是仅给窗口换了一个名字。

先完整更新，再安装这次需要的三个原生工具包：

```bash
pacman -Syu
```

若更新提示必须关闭MSYS2进程，按提示完成，重新打开UCRT64终端，再运行一次 `pacman -Syu`，直到完整更新完成。然后执行：

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

这组名字中的 `ucrt`、`x86_64` 是有意义的，不应删成一个不带环境前缀的 `gcc` 包。UCRT指Windows的Universal C Runtime选择；不同运行库版本的静态库和对象不应随意混用。真实软件还可能依赖额外库，应从相同环境安装对应包。

不要长期只运行 `pacman -Sy` 更新索引，再挑几个包升级。MSYS2支持完整系统升级；只更新部分组件可能造成依赖版本不一致。签名、网络或包数据库报错时，先按官方更新文档定位，不要用关闭签名验证来掩盖原因。

## 三个输出一起证明“装对了”

仍在UCRT64终端执行：

```bash
command -v gcc cmake ninja
gcc --version
gcc -dumpmachine
cmake --version
ninja --version
```

预期工具路径来自 `/ucrt64/bin`，GCC目标通常为 `x86_64-w64-mingw32`。注意这个目标字符串本身不唯一证明所选CRT：还要结合包名和路径。版本号证明“哪一版”，目标名说明“给谁生成代码”，路径说明“当前选了哪一份”，三者回答的是不同问题。

安装成功不代表原来已经打开的PowerShell、VS Code自动获得了新环境。先用新开的PowerShell做一个仅影响当前会话的验证，不急着重写全局PATH：

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
Get-Command gcc -All | Select-Object Source
where.exe gcc
gcc --version
gcc -dumpmachine
```

第一条把所选目录放在本终端搜索路径前面；关闭终端后不会保留。`where.exe` 明确调用Windows搜索工具，避免与PowerShell的同名别名混淆。如果列出多个GCC，先看命令实际命中的第一份，不要以“硬盘上已经有新版本”为依据。

若准备长期使用，可以把选定目录加入用户PATH，再完全退出并重新打开编辑器。不要一次把MSYS、MINGW64、UCRT64、其他MinGW及ARM工具所有目录乱加一遍；终端能找到很多同名工具，会让问题更难复现。对于必须共存的工具链，在构建配置中指定路径通常更清楚。

## 用一个C99程序验收，不止看版本

使用[本组独立示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build)的 `src/probe.c`。它要求 `__STDC_VERSION__` 至少为C99，计算一到十之和，用断言验证55后打印固定结果。核心逻辑是：

```c
int sum = 0;
for (int i = 1; i <= 10; ++i) sum += i;
assert(sum == 55);
puts("c99 ok: sum=55");
```

完整源码已有 `assert.h`、`stdio.h` 和 `main`。在仓库根目录、刚配置好搜索路径的PowerShell执行：

```powershell
$B = "artifacts/course-completion/build/windows-probe"
New-Item -ItemType Directory -Force $B | Out-Null
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror examples/notes/build/src/probe.c -o "$B/probe.exe"
& "$B/probe.exe"
$LASTEXITCODE
```

预期先输出 `c99 ok: sum=55`，再显示退出码 `0`。这证明了预处理、编译、汇编、链接与启动运行的最小闭环；只运行 `gcc --version` 不能证明标准头文件和链接库都配套。这里没有使用 `-DNDEBUG`，所以断言没有被关闭。

该C99测试已经用本机现有的旧MinGW GCC 6.3.0编译运行通过，说明测试程序本身可用；这**不等于**已验证全新UCRT64安装。按本篇安装的读者应在自己的新工具链上完成同样检查，并记录实际路径与版本。编译器“现代”也不表示默认语言模式就是本课程需要的版本，显式 `-std=c99` 更便于复现。

## 再把CMake接到同一份编译器

仍在同一PowerShell、仓库根目录执行，使用全新构建目录，不沿用之前可能缓存旧GCC的目录：

```powershell
$B = "artifacts/course-completion/build/ucrt64"
cmake -S examples/notes/build -B $B -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_BUILD_TYPE=Debug
cmake --build $B --verbose
ctest --test-dir $B --output-on-failure
```

应能看到C编译器识别、实际构建命令及三个测试通过。若 `gcc --version` 已是新的，详细构建日志却调用旧路径，优先检查CMake缓存和所选Kit，而不是继续修改系统PATH。CMake首次探测后会保存编译器路径；更换工具链应使用新的构建目录。

VS Code扩展负责编辑和集成，不会凭空提供这套编译器。先让终端闭环通过，再在CMake Tools选择对应工具链并配置工程；代码提示仍异常时，让语言服务使用相同的编译信息。编辑器的红线、命令行构建和程序运行是三个观察面，一个成功不能代替另一个。

## 两个安装后的常见边界

其一，程序只在UCRT64终端能运行，换一台机器却提示缺DLL。原生Windows程序也可能依赖MinGW运行库或第三方DLL。可用同一工具链的 `objdump -p` 查看导入项，再按发布要求携带合法的运行依赖；不要从随机网站下载同名DLL塞进系统目录。PATH能让本机找到依赖，不等于已经解决分发问题。

其二，看到32位/64位对象不兼容或“不是有效的Win32应用程序”。检查 `-dumpmachine` 和对象来源，避免把旧32位MinGW库、UCRT64对象及ARM对象混在一起。改扩展名或移动文件不会改变指令集和ABI。ARM固件应走交叉编译及烧录链，不是通过Windows资源管理器运行。

## 练习：新终端对，旧工程仍错

你在新PowerShell中确认GCC来自UCRT64，版本也符合预期，但旧构建目录的详细日志仍显示 `C:/MinGW/bin/gcc.exe`。需要重新安装MSYS2吗？如何做一次最小验证？

<details><summary>答案与验证步骤</summary>

通常不需要。终端命令查找已正确，旧工程可能仍使用CMake缓存中的编译器路径，或者编辑器Kit指定了旧路径。使用一个新的 `-B` 目录，并显式给出UCRT64编译器路径，重新配置、构建、运行测试；同时查看详细日志。如果新目录正确，问题就定位在旧配置而非安装。之后有控制地更新工程的Kit、预设或工具链选择，不要靠反复修改全局PATH试运气。

</details>

## 官方依据与下一步

- [MSYS2 Environments](https://www.msys2.org/docs/environments/)：UCRT64推荐环境、搜索路径、架构和CRT区别。
- [Updating MSYS2](https://www.msys2.org/docs/updating/)：完整升级与核心更新后的重启终端流程。
- [UCRT64 GCC包](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-gcc)、[CMake包](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-cmake)、[Ninja包](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-ninja)：已核对的安装包名，版本以仓库当前记录为准。

回到主线：[1.0 搭建环境](/lessons/1-0)、[2.2 单片机开发环境](/lessons/2-2)。相邻主题：[CMake如何工作](/notes/cmake-how-it-works)、[可执行文件格式](/notes/executable-formats)、[CMake与增量编译](/notes/incremental-builds)。
