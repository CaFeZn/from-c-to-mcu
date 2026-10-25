---
title: 【拓展】串口流控与USB CDC
description: 用有限队列实验区分硬件RTS、应用背压与USB CDC异步发送，避免丢字节和提前复用缓冲区。
sourceKey: 【拓展】串口流控与USB CDC
---

# 串口流控与 USB CDC

逐条输入命令都正常，一次粘贴几十行就缺字；接上 RTS/CTS，软件丢包计数还是增加。改成 USB 虚拟串口后又出现另一种现象：连续两次发送，第二次返回 BUSY；把串口工具里的波特率调低，问题也没有自动消失。它们共同指向一个事实：**接口能收发，不等于每一层都有足够空间。**

本文接在 [6.1 串口命令行](/lessons/6-1) 之后，使用 STM32F407 的 USART 和 OTG_FS 作为具体对象。先用主机模型观察背压，再决定硬件或固件怎样发出“暂时别送”的信号，不把未经上板验证的方案当成成品。

## 先做一个没有接线的拥塞实验

[flow_model.c](https://github.com/cafezn/from-c-to-mcu/blob/HEAD/examples/notes/serial/hal-flow/flow_model.c) 提供两组固定输入，只有标准 C99 和 assert，没有 HAL、USB 驱动或线程。仓库根目录执行：

```sh
mkdir -p artifacts/course-completion/serial/hal-flow
gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic examples/notes/serial/hal-flow/flow_model.c -o artifacts/course-completion/serial/hal-flow/flow_model.exe
./artifacts/course-completion/serial/hal-flow/flow_model.exe
```

第一组把接收队列容量设为 16。发送者每拍尝试放入一个顺序编号，消费者每三拍取一个，总共送 40 个。禁用背压时生产者不等消费者；启用时，到 12 个就请求暂停，到 4 个再恢复，并假设停止请求发出后最多还到达 3 个在途字节。实际主机运行得到：

```text
flow: watermarks=off consumed=29 dropped=11 peak=16
flow: watermarks=on consumed=40 dropped=0 peak=14
flow model: PASS
```

停发并未提高消费者速度，只是把等待转移给上游。模型还逐个断言编号，防止“数量对了但顺序错了”。这里的拍不是某个微秒数，峰值 14 也不是实板保证；真正接线后必须测量响应延迟、在途量和最慢处理时间。

第二组只有两个槽，每槽四字节，输入 `ABCD`、`EFGH`、`IJKL`。前两块入队，第三块返回 FULL，生产者保留数据。启动异步发送后，队首直到完成事件才释放；遇到 BUSY 不移除队首。完成一块后重试第三块，最终严格输出 `ABCDEFGHIJKL`。模型还演示提前改借出缓冲区会把 `ABCD` 变成 `XBCD`，这是串行安排事件的所有权反例，不是在主机上制造并发数据竞争。

## RTS 看得见接收器，看不见你的环缓

F407 的硬件流控在 USART_CR3 中分别由 RTSE、CTSE 开启。接线的方向是“本端 RTS 输出接对端 CTS 输入”，另一方向也对应交叉；TX/RX 交叉并共地。两端都要使用匹配的电平、有效引脚和功能设置，USB 转串口模块还必须确实引出并支持硬件握手，不能只看软件里有一个勾选框。

RM0090 §30.3.14 明确规定：RTS 低有效，接收器能够接收新数据时拉低；接收寄存器满时撤销请求。发送端在下一帧开始前检查 CTS，已开始的当前帧会发完，不会立刻从半个字符中间截断。

这个“有空间”指 USART 接收硬件，不是你在 C 里建立的环形缓冲。中断或 DMA 不断取走数据寄存器，硬件就可能继续允许发送；与此同时，主循环不取应用队列，软件仍会溢出。反过来，硬件 ORE 表示数据寄存器未及时被读取，也不是应用队列满标志。DMA 只能减少搬运负担，不能让平均消费速度无限提高。

适用范围也不能省略：F407 的 UART4、UART5 没有这一套硬件 RTS/CTS，不能把别的 F4 型号支持情况搬过来。实际选择 USART1/2/3/6 时仍需核对封装引脚和板上连接。

## 给应用队列设计真正的背压

用 N 表示可用容量，H 表示高水位，B 表示停止请求生效前最多继续到达的数据量，至少要求 `N - H >= B`，实际还需留出测量不确定性和处理裕量。上面的模型是 `16 - 12 >= 3`，不是“等满再通知”。低水位用于迟滞，避免容量刚空一个字节就恢复、马上又暂停。

可以根据协议选择下面一种机制，而不是同时堆上所有功能：

- 独立 GPIO 向对端 CTS 发出软件就绪信号，由应用水位控制。该引脚不能同时交给 USART 自动 RTS；先明确引脚所有者和停止响应上界。
- 约定 XON/XOFF 控制字符，并处理二进制数据中相同字节的转义。两端没有共同约定时，它们只是普通数据，不能擅自吞掉。
- 使用应用层信用或确认：接收者公布剩余可接收量，发送者只发送获准的数据；重试时用序号避免重复执行命令。

115200、8N1 连续发送时，一个字节在线上占 10 位，理论上限是 11520 字节/秒；如果消费端长期只能处理其一半，再大的有限缓存也只能推迟溢出。满时策略必须明确：暂停上游、拒绝本次写入、丢新数据并计数，或者按业务约定覆盖旧数据。日志和执行控制命令通常不能使用同一套随意丢弃策略。

## USB CDC 里的“波特率”不是 USB 线速

PC 出现一个 COM 端口，表示操作系统提供了串口风格接口，不代表 USB D+/D- 按 UART 的起始位、停止位发送。CDC ACM 的 SET_LINE_CODING 请求携带七字节线编码，包括比特率、停止位、校验和数据位；它描述串口参数，不改变 USB 总线自身的速率。

本次读取的 ST 模板把线编码保存起来，并把“应用如何使用它”留给用户实现。纯 USB 命令终端可以记录这些参数而不按它们节流；USB-UART 桥则可能用它重新配置背后的真实 UART，必须同时处理桥接缓存与切换中的数据。因此不能宣称“所有 CDC 都忽略波特率”，也不能宣称选 9600 就自动把 USB 变慢。

F407 的 OTG_FS 设备模式使用 12 Mbit/s 的 Full Speed 总线，但这是物理链路速率，不是应用吞吐量承诺。ST CDC 默认 FS Bulk 包上限为 64 字节；一次应用发送可以涉及多个包，USB 包边界也不等于一条命令边界。应用仍要用换行、长度字段等方法分帧。

## BUSY 不是“已经帮你排队”

所读 ST USB Device 中间件 2.11.6 中，`USBD_CDC_SetTxBuffer` 只保存指针和长度，不复制内容，也不检查 TxState。`USBD_CDC_TransmitPacket` 仅在 TxState 为 0 时发起传输，否则返回 USBD_BUSY。不能忙时先改发送指针，再期待后续 BUSY 把旧指针恢复。

正确的发送适配层应由单一所有者串行提交：准备一个稳定的队首缓冲，在空闲条件下设置指针并启动，成功后标记在途，完成通知到来才释放。主循环和 ISR 都可能触碰队列时，要采用已设计好的临界区或消息传递，不能只加 volatile。常见的 `CDC_Transmit_FS` 是工程生成的包装函数，其检查顺序和返回值也要读本项目版本。

需要保留的不只是地址：返回后的局部数组会结束生存期，仍在作用域内却被重写的数组也会改变后续数据。若上层希望“调用后即可改原数组”，队列必须先复制到自己持有的槽。传输完成也只意味着 USB 传输层完成，不意味着主机应用已经读走、解析或执行了命令。

此版 DataIn 在需要时还会发送零长度包，之后才清 TxState 并调用 TransmitCplt；不要猜“等一毫秒肯定完成”。旧中间件可能没有相同完成接口，不能照搬新版本结构体。

## 接收端也要等空间再重新挂接

USB OUT 是主机向设备发数据。此版 DataOut 调用应用 Receive 后，并不会替应用自动重挂下一次接收；`USBD_CDC_ReceivePacket` 才准备下一次 OUT。接收缓冲中的数据必须先复制或完成消费，再允许它被下一包覆盖。

若要借助端点 NAK 暂停接收，应在空间不足时暂缓重挂，并提供空间释放后的恢复路径。每次重挂前要预留整包空间，而不是只看到一个空位就接收默认最多 64 字节的 FS 包。不要仅从 Receive 返回 BUSY 并以为库会自动管理队列：所读 DataOut 没有使用这个返回值。

NAK 会让主机重试，但上层程序可能超时，断线、复位、取消都需要单独处理。CDC 的 DTR/RTS 控制请求是协议消息，也不会自动连接到应用水位。尤其不要在 USB 中断里循环等待发送不忙，因为清忙状态可能正依赖被你占住的后续 USB 中断。

## 练习：把高水位改成 15 可以吗

保持容量 16、最多三个在途字节不变，把高水位从 12 改为 15。是否仍能保证不丢？若把本地队列改成 1024 字节，能否保证断线后一直产生的日志不丢？

<details>
<summary>展开答案</summary>

不能，剩余一个位置小于最多三个在途字节，不能拿“消费端恰好会取走”作为保证。应降低高水位、减小在途上界，或增大经过预算的容量。1024 字节也只是更晚填满；断线期间若生产持续，必须向生产者传播背压，或明确拒绝、丢弃与计数策略，并在重连时规定是否重放旧数据。

</details>

## 已读资料与验证边界

- [RM0090 官方手册](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：实际读取 Rev 22 缓存，§30.3.14（正文页 998–1000）、USART_CR3 的 RTSE/CTSE（1010 页）、§34.1（1228 页）核对握手与 OTG_FS 速率。
- [ST HAL UART 1.8.5](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_uart.c)：初始化中的硬件流控实例检查和 IRQHandler 的 ORE 处理。
- [ST USB CDC 2.11.6 源码](https://github.com/STMicroelectronics/stm32-mw-usb-device/blob/2df324bd60d4b0bb27404fd70b1c089b467f0e09/Class/CDC/Src/usbd_cdc.c)：实际读取固定提交的 SetTxBuffer、TransmitPacket、DataIn、DataOut、ReceivePacket；并读取 [接口模板](https://github.com/STMicroelectronics/stm32-mw-usb-device/blob/2df324bd60d4b0bb27404fd70b1c089b467f0e09/Class/CDC/Src/usbd_cdc_if_template.c) 的线编码处理，以及 [头文件](https://github.com/STMicroelectronics/stm32-mw-usb-device/blob/2df324bd60d4b0bb27404fd70b1c089b467f0e09/Class/CDC/Inc/usbd_cdc.h) 的 FS 包大小与完成回调。

主机模型已经实际编译运行，只验证选定的所有权和背压逻辑；没有验证 USB 枚举、线缆、电平、时钟、中断延迟或主机驱动兼容性。板级集成应先完成对应 Cube 工程的 USB 时钟、GPIO、描述符、PCD/USB IRQ 和端点缓冲配置，再测连续流量与拔线恢复，不能把模型 PASS 写成上板通过。

回到 [6.1 串口命令行](/lessons/6-1)，继续 [6.2 DMA 与空闲中断](/lessons/6-2)；相关分支：[HAL 回调的注册方式](/notes/hal-callbacks)。
