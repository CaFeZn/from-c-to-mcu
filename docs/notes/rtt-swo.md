---
title: 【拓展】SEGGER_RTT与SWO替代串口日志
description: 用缓冲满载和断开接收端实验，选择 RTT 或 SWO 后端并识别隐藏的阻塞与丢失。
sourceKey: 【拓展】SEGGER_RTT与SWO替代串口日志
---

# SEGGER RTT 与 SWO 替代串口日志

USART1 已经被设备协议占用，但你还想观察主循环状态。有人建议：“换 RTT，不占串口，几乎没开销。”换完确实能看到文字，可一拔掉探头，系统过一会儿就卡住。

这个故障不是通道名字决定的，而是缓冲满时的策略决定的。先保留 [3.1 串口与日志](/lessons/3-1) 的原则：中断只记事实，主循环格式化；满了要有界地失败，并能统计丢失。本篇比较两个开发期出口，不把它们当成量产日志的自动答案。

## 先算一轮没有接收者的发送

设生产者每 10 ms 产生一条 40 字节消息，发送缓冲实际可用空间为 1000 字节，主机暂时不读。先不考虑协议开销：

```text
生产速率 = 40 / 0.010 = 4000 字节/秒
空缓冲可容纳 = 1000 / 40 = 25 条
约 250 ms 后持续生产将遇到容量边界
```

“把日志复制进 RAM 很快”并没有消除这条算式。你必须决定第 26 条到来时：等接收端、整条丢弃，还是只写剩余部分。三者分别对应阻塞、skip、trim 一类策略。

把主机读取停掉，再观察主循环心跳与丢弃计数，是比“接收窗口出现 hello”更重要的验收。实际 RTT 环形缓冲可能保留位置来区分空和满，所以计算必须使用**可用容量**，不是直接拿数组尺寸。

## RTT：在 RAM 里放一个双方都认识的邮箱

SEGGER RTT 的控制块与上下行环形缓冲放在目标 RAM 中。目标向 up buffer 写，探头通过调试访问读出；down buffer 则让主机向目标提供输入。背景访问模式不要求每输出一个字节就停住 CPU，但仍有复制、格式化、临界区和总线访问的成本。

因此 RTT 不需要独立 UART，也不需要 SWO 线，但需要支持相应内存访问与 RTT 协议的探头和主机软件。原作者的 J-Link RTT 支持不能直接等同于“任意 ST-LINK 加任意 IDE 都能用”；其他工具是否支持，要查各自实现。

在自己的工程加入官方 RTT 源码及配置后，可在主循环初始化位置设置通道 0 为整条跳过模式。下面是**依赖官方 RTT 库的集成片段**，不是完整固件：

```c
#include "SEGGER_RTT.h"

/* 启动时调用一次，并检查返回值是否为 0。 */
int debug_transport_init(void)
{
    SEGGER_RTT_Init();
    return SEGGER_RTT_SetFlagsUpBuffer(
        0u, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}

/* 在单一主循环写入者中调用；返回未接纳的字节数。 */
unsigned debug_send_ping(void)
{
    static const char msg[] = "ping\r\n";
    unsigned length = (unsigned)(sizeof msg - 1u);
    unsigned written = SEGGER_RTT_Write(0u, msg, length);
    return length - written;
}
```

`SEGGER_RTT_Write` 返回的是接纳数量，不是“已经到 PC 的数量”。在 skip 模式，放不下整条就返回零；trim 可返回部分长度；block 模式可能等待主机释放空间。不要把每次循环调用 Init 当作“检查连接”，初始化会影响控制块和缓冲状态。

### RTT 的跟做顺序

1. 使用所选探头支持的 RTT 查看器，加载正确器件与接口；让目标完成控制块初始化。
2. 每 100 ms 发送一次 ping；预期正常读取时可看到连续消息，未接纳计数较低或为零。
3. 关闭读取窗口，保持固件继续生产；缓冲满后预期 skip 模式仍保留心跳，未接纳计数上升。
4. 重新打开读取；只能读到仍留在缓冲中的消息，不能指望恢复被丢弃的历史。

找不到控制块时，先查 ELF 中控制块地址、是否被链接保留、初始化是否执行、探头实际扫描哪个 RAM 区域。不要为了“搜得到”盲目扫描所有地址。F407 的 CCM 和普通 SRAM 访问路径不同，稳妥的入门放置是工具可访问、明确配置的普通 SRAM。F407 没有 D-cache；移植到 M7 等缓存芯片时，必须重新核对 RTT 控制块和数据缓冲的一致性要求。

## SWO：把跟踪包送到一根输出线上

另一条路径是：

```text
程序写 ITM stimulus port
→ ITM 形成跟踪包
→ TPIU
→ TRACESWO 引脚
→ 支持 SWO 的探头与主机解码器
```

F407 的 TRACESWO 复用在 PB3。它不是 PA13 的 SWDIO，也不是 UART TX。SWD 用于配置和调试，SWO 负责单向跟踪输出；探头能下载不代表探头或板子把 SWO 接出来了。普通 USB 转串口的文本模式，也不能直接把 SWO 流当 UART 日志读取。

开始前确认 PB3 未被其他功能占用，探头支持选定的 SWO 编码与速率。配置 ITM、TPIU、TRACE I/O 与端口使能，并让主机知道实际追踪时钟和输出速率。系统时钟改了而解码配置未更新，可能表现为无输出或乱码。不要仅复制网上某个固定分频值。

本轮输入可以是主循环每 100 ms 经 ITM port 0 输出一个 `A`。预期选对端口和时钟时周期性收到 A；故意在主机关闭 port 0 的显示后看不到文字，但这不等于目标没有执行写入。恢复配置再对照，逐层检查，不把“窗口空白”直接判为程序没跑。

## 一个特别容易漏掉的 while

CMSIS 的 `ITM_SendChar` 并非通用的“绝不阻塞打印”：本课程核对的实现先检查 ITM/port 0 使能，随后在端口不可写时执行等待循环。即使注释提到未启用时直接返回，也不能由此推导“任何断线状态都不阻塞”。

需要有界行为时，可以在项目支持的 CMSIS 头文件下写单次尝试函数，例如：

```c
#include "stm32f4xx.h"

/* 单一写入者、已配置好 ITM/TPIU；没有等待循环。 */
int swo_try_char(uint8_t ch)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u ||
        (ITM->TCR & ITM_TCR_ITMENA_Msk) == 0u ||
        (ITM->TER & 1u) == 0u || ITM->PORT[0].u32 == 0u) {
        return 0;
    }
    ITM->PORT[0].u8 = ch;
    return 1;
}
```

这是板级示意，仍依赖有效的目标寄存器访问与单写入者假设；返回 1 也不是接收端确认。多中断抢同一端口、跟踪链过载、接收器没启动，都要另外处理。不能借“非阻塞”三个字承诺零开销或零丢失。

## 怎样接回课程日志层

主线 LOG 已经把格式化接口与后端隔离，但它的 UART/DMA 状态机有完成回调和在途区段。改 RTT 时，不能只把启动 DMA 的一行替换掉，却仍等待 UART 完成回调释放缓冲。要重新定义何时接纳、何时推进读指针、满载如何统计；或者给开发后端单独的、清楚隔离的输送路径。

主线的 flush 语义也不能原样搬到没有主机的 RTT 上。故障处理中等主机读空缓冲可能永远等不到；生产故障应优先保留小记录，恢复启动后再发送。

### 练习：剩 12 字节，来了 20 字节

RTT up buffer 只剩 12 字节，一次请求写入完整 20 字节记录。skip、trim、block 各有什么预期？二进制定长记录更适合哪一种？

<details><summary>答案</summary>

skip 不写入该次记录，返回 0；trim 写入可接纳的 12 字节、丢剩余 8 字节；block 等待空间，可能拖住调用者。定长记录通常优先 skip 加丢弃计数，或使用带长度与同步信息的专用协议，避免 trim 留下无法分辨的半条记录。要求不丢时必须提供足够吞吐与独立可靠存储，不能单靠改成 block。

</details>

## 适用范围与依据

本文读取并核对了官方 API 与 CMSIS 实现，未接探头实测 RTT 吞吐或 SWO 波形。板级片段需要官方库、设备头文件和明确的初始化，主机容量推演不代表实测速率。

- [SEGGER RTT 原作者说明](https://kb.segger.com/RTT)：How RTT works、Channel buffer modes、Control block detection 和 API。
- [SEGGER RTT 头文件](https://github.com/SEGGERMicro/RTT/blob/main/RTT/SEGGER_RTT.h)：SetFlagsUpBuffer、Write 和非阻塞模式定义；集成时应固定实际使用版本。
- [ST RM0090 Rev 22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第 38.14 节 ITM 与第 38 章 TPIU、TRACE 引脚分配。
- [CMSIS core_cm4.h 的 ITM_SendChar](https://github.com/STMicroelectronics/STM32CubeF4/blob/60c8abe0ae88a23bbd1c925358330031e36dd20f/Drivers/CMSIS/Include/core_cm4.h#L2056-L2076)：实际等待循环。

回到 [3.1 串口与日志](/lessons/3-1)；继续看 [SWD 调试原理](/notes/swd-debugging)、[量产日志策略](/notes/production-logging)、[故障记录回放](/notes/fault-persistence)。
