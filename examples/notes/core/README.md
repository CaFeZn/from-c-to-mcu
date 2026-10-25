# core 组课程示例

## 可以在 PC 执行的部分

[test.c](<examples/notes/core/test.c>) 是 C99 确定性模型：所有“主循环/ISR/生产者/消费者”动作都在一个线程内显式排序。它不解引用 MCU 地址、不启动线程，不靠未定义的数据竞争制造现象。

覆盖八篇正文：读改写丢计数及 BSRR；NVIC 挂起合并与优先级；向量表下标；W1C 清除；独占写失败和重试；先公布后填数据的错误时间线；序号快照与两槽所有权；协作/抢占调度时间线。

仓库根目录运行：

```sh
cmake -S examples/notes/core -B artifacts/course-completion/core/host-build -G Ninja
cmake --build artifacts/course-completion/core/host-build
ctest --test-dir artifacts/course-completion/core/host-build --output-on-failure
```

如果 CMake 选错编译器，在首次配置时显式提供本机的 C 编译器路径。当前作者验证环境使用 MinGW GCC 6.3.0；无需安装线程库。[CMakeLists.txt](<examples/notes/core/CMakeLists.txt>) 还构建并运行四个从正文提取的完整程序：

- [read-modify-write.c](<examples/notes/core/snippets/read-modify-write.c>)：final=11, expected=12。
- [vector-table.c](<examples/notes/core/snippets/vector-table.c>)：EXTI0 slot=0x08004058。
- [interrupt-storms.c](<examples/notes/core/snippets/interrupt-storms.c>)：direct=2, or=0, and=1。
- [exclusive-access.c](<examples/notes/core/snippets/exclusive-access.c>)：first=fail, retry=success, value=12。

总计 5 项 CTest。断言不会因 Release 构建的 NDEBUG 静默失效：聚合模型主动取消 NDEBUG，四个独立片段在 GNU/Clang CMake 目标里使用 -UNDEBUG。

## 只能给 Cortex-M4 编译的部分

[board_snippets.c](<examples/notes/core/board_snippets.c>) 使用 ST F407 设备头和 CMSIS，包含短临界区、优先级编码、GPIO BSRR、EXTI5/7 分发、独占自增、一次性发布、三类屏障、单写者序号快照及协作小步接口。

它不是完整固件：没有 main、启动文件、链接脚本、外设初始化，也不提供可直接烧录的映像。选择某一段集成到自己的已确认工程，逐项满足对应正文的假设。特别注意：

- EXTI9_5_IRQHandler 只能有一份；裸寄存器版本不能与 HAL 对同一事件重复应答。
- configure_exti_priority 会修改全局优先级分组，只能在统一初始化设计内使用，不要运行中调整 RTOS 工程。
- GPIO 示例要求已启用时钟、已配置模式并确认引脚接线安全。
- 发布例子只发布一次；序号写者只有一个 ISR，读者只在主循环。不能直接替换成 PC 普通 C11 多线程代码。
- 没有执行板级代码，没有测试中断延迟、VTOR 搬移、DMA 或 RTOS 真实切换。

## 本机验证脚本

[validate.sh](<examples/notes/core/validate.sh>) 记录本工作区的确切工具路径与验证命令，必须从仓库根目录执行。它使用本地只读参考缓存和 [core 参考依赖目录](<artifacts/course-completion/core/reference>)，该缓存不属于学生工程的必选依赖。其他机器应使用自己工程的设备/CMSIS include 路径，不要照搬作者的绝对编译器位置。

脚本只构建本目录例子，将输出放入 core 的审查目录，不运行课程站点构建或 Git 命令。执行结果见 [validation.txt](<artifacts/course-completion/core/validation.txt>)；技术核对、每篇边界和来源见 [review.md](<artifacts/course-completion/core/review.md>)。
