# serial 分支的独立 C99 示例

这是九篇 serial 分支共用的隔离验证工程，不依赖网站构建，也不是 STM32 完整固件。

## 一次构建全部主机模型

从仓库根目录运行，使用已安装且支持 C99 的主机编译器、CMake、Ninja：

~~~sh
cmake -S examples/notes/serial -B artifacts/course-completion/serial/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build artifacts/course-completion/serial/build
ctest --test-dir artifacts/course-completion/serial/build --output-on-failure -V
~~~

如果 CMake 选错编译器，在首次配置时显式传入本机已发现的 C 编译器路径。当前验收使用 MinGW GCC 6.3.0；这不是要求读者安装同一路径。Debug 配置保留 assert，不能用删除断言后的空运行代替验证。

预期 CTest 通过四项：

| 测试目标 | 检查什么 |
| --- | --- |
| serial_core | snprintf 截断语义、UART 位序/BRR/带宽、环缓空满/回绕/顺序、文本拒绝、16位小端字段全部值、引号状态机分包/恢复 |
| serial_crc_vectors | CRC16/CRC32 已知向量、逐位与查表一致、分块状态、F407 整字加软件尾部适配模型 |
| callback_model | HAL 注册状态、每对象分派、单次接收、普通 DMA 完成与 UART TC 的区别 |
| flow_model | 高低水位、在途量、队列满、异步 BUSY、借用缓冲的生命周期 |

## 文件布局

~~~text
examples/notes/serial/
  CMakeLists.txt
  core/
    test.c
    printf_probe.c
    CMakeLists.txt
  crc/
    crc.h
    crc.c
    test_crc.c
    CMakeLists.txt
  hal-flow/
    callback_model.c
    flow_model.c
    hal_callback_probe.c
    hal_callback_probe.h
    CMakeLists.txt
    README.md
~~~

core 的 printf_probe 不加入主机 CTest。它用于三种入口的 ARM 交叉链接体积比较。验收使用 Cortex-M4、soft ABI、-Os、newlib-nano、nosys 和 gc-sections，浮点入口额外拉入 _printf_float；text 分别为 354、4373、14498 字节。这个结果不能推广为所有工具链的 printf 固定大小。

## 边界

- 所有主机模型均为单线程确定性测试，不模拟真实中断抢占、HAL 驱动、DMA、USB 总线或硬件寄存器。
- HAL 板级探针依赖 CubeMX 生成的 USART2 句柄、HAL 配置、GPIO/时钟/IRQ 转发，不属于本工程的可执行测试目标。未进行完整固件构建或上板。
- 环缓核心只用于顺序执行或外部同步保护下；C99 的普通字段不天然变成线程安全队列。
- F407 CRC 示例是软件算法映射模型，没有直接访问 CRC_DR。
- printf 尺寸探针使用 nosys 桩，链接器会提示底层系统调用未实现。这些 ELF 不是可下载的完整固件，未运行它们的输出功能。
- 构建和验收产物留在专属 artifacts 目录，不应把 exe、o、ELF、截图或外部参考缓存当成发布固件。

课程正文各自给出对应现象、输入、预测、机制、误区和练习。完整来源与逐篇验收记录位于：

~~~text
artifacts/course-completion/serial/review.md
artifacts/course-completion/serial/test-results.json
artifacts/course-completion/serial/static-audit.json
artifacts/course-completion/serial/mobile-audit.json
~~~
