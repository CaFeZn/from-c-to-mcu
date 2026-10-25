---
title: 【拓展】CAN FD与CANopen简介
description: 区分CAN FD的链路能力与CANopen的应用语义，计算FD长度编码，并读懂CANopen节点的启动和通信状态。
sourceKey: 【拓展】CAN FD与CANopen简介
---

# CAN FD与CANopen简介

买来的设备写着“CANopen”，新控制板写着“CAN FD”，两根CAN线能接上，是不是两边就能互通？先别发帧。前者主要描述数据与设备如何交互，后者描述帧本身怎样传输；它们不是同一层的两个竞争选项。

主线已经能用F407发送经典CAN的8字节数据帧。本篇接着 [7.2](/lessons/7-2) 与 [7.4 CAN小节点](/lessons/7-4)，先判断需要升级的是硬件链路能力，还是消息语义。它不是CANopen兼容栈实现教程，也不是FDCAN寄存器移植指南。

## 先用两个需求做分类

需求A：“一帧装下20字节采样，还希望数据阶段更快。”这涉及CAN FD。需求B：“不同厂家驱动器都能按统一方式读参数、传过程量、报告在线状态。”这涉及CANopen及设备配置文件。

可以把它们放到这样的位置：

```text
设备功能、参数与过程数据：你的应用 / CANopen等上层协议
帧格式与链路传输：       经典CAN / CAN FD
收发器与线缆：           满足目标速率和电气条件的物理层
```

经典CAN可以承载CANopen CC；CANopen FD则是面向CAN FD发展的另一套通信规范。**“使用FD帧”本身不会自动获得CANopen对象字典，“把CANopen发得更快”也不等于完成CANopen FD迁移。**

## CAN FD多了什么，没改变什么

经典CAN的数据字段至多8字节。CAN FD将上限扩展到64字节，并允许在数据阶段切换到另一位率。仲裁仍在名义位率下进行；BRS位决定是否启用切换，BRS未启用的FD帧也仍是FD帧，不因两个阶段同速就变成经典帧。

FD保留11位与29位ID两种形式，增加了FDF、BRS、ESI等语义，并修改CRC与填充设计。ESI用于表明发送节点的错误状态信息。不要把经典CAN按`47+8d`算出的开销直接套到64字节FD帧，也不要把“数据阶段2 Mbit/s”理解成整个报文都按2 Mbit/s完成。

**STM32F407的bxCAN不支持CAN FD。** 换一个FD收发器不会把控制器升级成FD；更新HAL也不能增加硬件没有的帧逻辑。要用FD，需要支持FD的控制器、满足数据速率要求的收发器和网络布线，同时重新设计名义/数据两套定时。

FD控制器一般也能收发经典帧，但反方向不成立。经典CAN节点看到FD格式可能报告错误，不能仅因二者有同样的H/L引脚就让FD帧直接进入一个未规划的经典网络。混合网络应根据控制器能力明确设计，或者分段并通过网关转换。早期非ISO CAN FD与后续ISO CAN FD也存在兼容问题，采购与抓包工具设置需要确认。

## 20字节不是把DLC填20

DLC是4位长度编码，不是随便放一个字节数。FD中0–8编码与字节数相等，之后变成离散档位：

| 原始DLC | 数据字段字节数 |
|---|---|
| 0–8 | 0–8 |
| 9、10、11 | 12、16、20 |
| 12、13 | 24、32 |
| 14、15 | 48、64 |

希望发送20字节，对应DLC=11；希望发送9个业务字节，需选择12字节字段，并在上层协议中规定另外3字节的含义或填充方式。底层缓冲区要覆盖真实字段长度，不能只有9字节却让驱动读取12字节。HAL宏在不同系列/版本中还可能采用已移位编码，实际API应使用对应驱动宏，不直接把这张原始DLC表当HAL参数。

下面完整C99程序只验证长度解码，不依赖任何STM32库：

```c
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static int fd_length(unsigned dlc, unsigned *bytes)
{
    static const unsigned table[16] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
        12u, 16u, 20u, 24u, 32u, 48u, 64u
    };
    if (dlc >= 16u || bytes == NULL) return 0;
    *bytes = table[dlc];
    return 1;
}

int main(void)
{
    unsigned bytes = 0u;
    assert(fd_length(11u, &bytes) && bytes == 20u);
    printf("DLC=11 bytes=%u\n", bytes);
    assert(fd_length(15u, &bytes) && bytes == 64u);
    printf("DLC=15 bytes=%u\n", bytes);
    assert(!fd_length(16u, &bytes));
    return 0;
}
```

预期两行是`DLC=11 bytes=20`、`DLC=15 bytes=64`。FD没有经典CAN的远程帧；远程请求需求应由上层通信设计承担，而不是寻找一个“64字节RTR请求”。

## CANopen把变量放到可访问的字典里

如果自制协议规定“ID=0x201，前两个字节是温度”，它可以工作，却仍不是CANopen。CANopen设备通过**对象字典**把应用变量、配置与诊断暴露给协议栈。对象用16位index及sub-index定位，不是MCU的RAM地址。设备内部搬了变量位置，不需要因此改变网络对象编号。

比如厂家可以在自定义对象区给一个参数安排`0x2000:01`；这只是示意，具体数据类型、单位、权限和范围必须在设备说明中定义，不能假定所有CANopen设备在该位置都有温度。标准对象与设备profile约定的含义，则应按相应规范实现。

三类服务先分清：

- **SDO**用于访问对象字典，例如读取参数或配置通信行为，是有确认的客户端/服务器服务。小数据可使用expedited方式，更多数据可分段或块传输；这不是“一次SDO永远只占一帧”。
- **PDO**用于过程数据，CANopen CC中一帧可带至多8字节应用数据。数据按预先规定的mapping排列，不需要每帧都携带index；通信参数定义ID、触发方式等，接收方必须知道同样的映射。
- **NMT与心跳**负责节点通信状态与在线监测。心跳告诉消费者节点仍存在及其NMT状态，不是对最近一个控制命令的业务回执。

CAN的ACK、SDO服务确认、驱动器完成动作是三个不同层级。只看到总线上的ACK，不能认定SDO成功；SDO成功写入参数，也未必等于机械动作完成。

## 跟着节点5上电，解释“能读参数却不出PDO”

给定一台符合CANopen CC启动流程、节点ID为5的设备，先只做抓包与状态推演，不向真实执行器发启动命令：

1. 上电后设备初始化。完成时发送boot-up报文，CAN-ID为`0x700+5=0x705`，DLC=1，数据为`00`。
2. 设备进入Pre-operational。此时可通过SDO访问对象字典；PDO通信尚未启用。
3. 若NMT管理者让它进入Operational，且PDO已正确配置、触发条件满足，才开始预期的PDO通信。并非一进入Operational就无条件不断发PDO。
4. 配好生产者心跳后，消费者监测周期性心跳和超时。若运行中又出现`0x705 [00]`，应关注设备重新启动、供电或重置，而不把它当普通周期数据。

所以“SDO读得到参数、PDO却没有”不应直接归咎于CAN过滤器。先查NMT状态、PDO启用/映射、触发方式、CAN-ID与消费者配置。CANopen CC在Stopped状态仍需支持相应的NMT与错误控制行为，但不能把它当作完整业务通信状态。

没有CANopen工具时，也能用纸面记录验证：把`0x705 [00]`解为节点5启动；把`0x70C [00]`解为节点12启动。这个解码依赖明确采用CANopen错误控制报文约定，**不能在任意自制CAN网络里见到0x705就断言它一定是CANopen**。

## 集成前要问的四件事

首先确认控制器支持经典CAN还是FD，并匹配收发器与定时；其次确认上层是CANopen CC、CANopen FD还是厂商私有协议；再核对节点ID、对象字典/EDS以及所用设备profile；最后确认NMT、心跳、PDO mapping、SDO错误与超时都由成熟栈或明确实现负责。

使用F407做CANopen CC并不矛盾，它能提供经典CAN链路，但主线的小节点示例还不包含完整对象字典与CANopen服务。反过来，即使换成FD硬件，旧设备也不会凭空学会新的上层协议。

## 练习：分清三个“不兼容”

A想用F407发送一帧20字节FD数据；B用FD控制器正常发送经典帧，与F407通信；C看到节点5的boot-up后，只因SDO可用就断言PDO已经工作。分别哪里需要修正？

<details><summary>展开答案</summary>

A受限于bxCAN硬件，不能靠扩展ID或FD收发器解决；需FD控制器或重新设计经典CAN分包协议。B在双方经典帧、位定时和物理层兼容的前提下可以通信，FD能力不强迫每帧使用FD。C混淆NMT状态与服务：Pre-operational允许SDO，但PDO要在Operational且配置、触发条件满足时通信。

</details>

## 依据与验证边界

- [CiA：CAN FD基本原理](https://www.can-cia.org/can-knowledge/can-fd-the-basic-idea)：FDF/BRS/ESI、64字节、无远程帧及ISO/非ISO区别。
- [ST官方G4 FDCAN头文件](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/blob/master/Inc/stm32g4xx_hal_fdcan.h)：`FDCAN_data_length_code`，核对DLC档位；仅用于交叉核对编码，不是F407可用驱动。
- [CiA CANopen总览](https://www.can-cia.org/can-knowledge/canopen)、[SDO](https://www.can-cia.org/can-knowledge/sdo-protocol)、[PDO](https://www.can-cia.org/can-knowledge/pdo-protocol)、[NMT](https://www.can-cia.org/can-knowledge/network-management)、[错误控制协议](https://www.can-cia.org/can-knowledge/error-control-protocols)：核对字典、服务与节点启动状态；网页查阅于2026-10-25，具体产品仍需对应版本的CiA规范及设备文件。

本文只运行了主机长度解码与启动ID算术，没有FD硬件测试或CANopen一致性认证。回到 [7.4](/lessons/7-4)，联读 [CAN扩展帧与远程帧](/notes/can-extended-remote)、[总线负载与优先级设计](/notes/can-load-priority)、[CAN总线拓扑与长度](/notes/can-topology)。
