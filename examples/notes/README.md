# 分支笔记的可执行示例

这些例子与网站「原理与拓展」逐篇对应。它们是相互独立的小工程，不是可以直接下载到所有开发板的统一固件。

## 一次验证全部主机例子

需要 Node.js、支持 C99 的主机 C 编译器、CMake 3.20+ 与 Ninja；C++ 分支还需要支持 C++11 的编译器。工具均需在 PATH 中。仓库根目录执行：

```sh
node scripts/test-note-examples.mjs
```

脚本逐组创建独立的 Debug 构建，保留 `assert` 检查，运行 CTest，并把完整结果写入 `artifacts/note-examples/report.json`。构建产物不会混入源码目录；任意项目失败会返回非零退出码，不把编译失败当作已通过。

## 单独运行一组

```sh
cmake -S examples/notes/c-numbers -B artifacts/my-number-check -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build artifacts/my-number-check
ctest --test-dir artifacts/my-number-check --output-on-failure
```

其它组按目录内的说明操作。serial 组包含独立的 core、crc、hal-flow 子工程；聚合脚本会选择各组顶层工程，若没有顶层 CMakeLists 则发现其子工程。文章中的 ARM 专用反汇编、HAL 集成片段和故意编译失败的诊断实验有各自前提，不会当成 PC 可执行程序批量运行。

## 验证边界

- 主机模型验证算法、状态和边界，不证明外设时序、电压、DMA可达性或中断抢占。
- ARM 编译/链接与反汇编证据不等于上板执行；完整板级验收仍需具体芯片、板卡、连接和工具版本。
- 有意展示的未定义行为不能通过某次输出获得保证；错误示例仅用于明确标示的诊断或静态说明。
- 不运行破坏性的 Git 恢复命令，不把本仓库当作 Git 新手练习工作区。Git 文章要求另建个人练习工程。
