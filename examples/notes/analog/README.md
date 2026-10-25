# Analog note models

五篇 analog 分支的独立 C99 验证程序。它只模拟算术、请求计数和缓冲覆盖，不访问 MCU 寄存器，不是完整固件或并发驱动。

## 运行

从仓库根目录执行；使用当前机器已安装的 C99 编译器。

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic -O2 examples/notes/analog/test.c -lm -o artifacts/course-completion/analog/analog-tests
./artifacts/course-completion/analog/analog-tests
```

也可使用独立 CMake 工程，以下命令不构建课程站点：

```sh
cmake -S examples/notes/analog -B artifacts/course-completion/analog/local-build
cmake --build artifacts/course-completion/analog/local-build
ctest --test-dir artifacts/course-completion/analog/local-build --output-on-failure
```

如环境需要显式选择 Ninja/编译器，可参照[本次验证脚本](../../../artifacts/course-completion/analog/validate.sh)中的本机路径；不要把本机路径当作通用安装位置。CMake 为测试目标撤销 NDEBUG，以免 Release 配置静默禁用断言。

## 五组检查

| 检查 | 固定输入与预期 |
|---|---|
| 采样保持模型 | 21 MHz、τ=0.5 µs；3/56/144 周期误差约 3078.051/19.775/0.004532 LSB |
| 普通 DMA | 八项 100..800；第4项 HT、第8项 TC；MINC=0 只留下最后值 |
| 流式覆盖 | 两圈 1..16；已复制快照仍 1..4，原半区已变成 9..12；时间预算拒绝迟到及32位加法溢出反例 |
| 定时触发 | 84 MHz，PSC83/ARR499→2 kHz；三通道628周期，50 kHz触发容不下完整序列 |
| 参考补偿 | 校准1500、当前1650、校准参考3300→当前3000 mV；拒绝零码/饱和/无效校准/空指针 |

最后一行应为 `analog: all five topic models passed`。

## 边界

没有模拟 ADC 非线性、信号源、总线延迟、中断调度、内存可见性或缓存。快照复制在该顺序模型中立即完成，不能据此宣称真实复制满足时限。工厂校准值在测试中是给定输入，未读取真实芯片。目标端 HAL 启动和 LL 读取片段仅在正文明确假设下作为集成示意；没有完整固件链接或上板验证。
