---
title: 【拓展】CAN扩展帧与远程帧
description: 拆开29位标识符与RTR字段，推演混合帧仲裁，并避免把远程帧DLC当作实际数据长度。
sourceKey: 【拓展】CAN扩展帧与远程帧
---

# CAN扩展帧与远程帧

分析仪里有两条ID都显示为`0x123`的记录，一条标着STD，一条标着EXT；另一条显示DLC=8，数据栏却空着。如果接收代码只保存一个16位ID和一个长度，三种消息可能被混成同一种。

主线 [7.1](/lessons/7-1) 的小封装刻意只支持**11位标准数据帧**。这不是bxCAN只能处理这些，而是接口把范围收窄了。本篇先认清帧，再讨论怎样扩展接口，不要求你立即改掉主线结构。

## “扩展”扩的是标识符，不是数据区

经典CAN的数据帧有两种格式：标准帧使用11位ID，扩展帧使用29位ID；**两者的数据字段都至多8字节**。想在一帧里发64字节是CAN FD话题，不是把IDE改成扩展格式就能实现。

29位ID在总线上也不是紧挨着连续发送的29位整数。它拆成11位基础ID与18位扩展部分，中间插入SRR和IDE：

```text
标准：SOF | 基础ID 11位 | RTR | IDE=0 | r0 | DLC | ...
扩展：SOF | 基础ID 11位 | SRR=1 | IDE=1 | 扩展ID 18位
          | RTR | r1 | r0 | DLC | ...
```

这里只画结构，不展开位填充。SRR是“替代远程请求位”，在扩展帧里固定隐性；它的名字里有Remote，**不等于这一帧就是远程帧**。真正区分数据帧与远程帧的是各自位置上的RTR。

## 用同一个基础ID做混合仲裁

给定标准ID`0x123`，再构造扩展ID：

```text
base = 0x123
extension = 0x00005
extended_id = (base << 18) | extension = 0x048C0005
```

假定节点已经同轮起发，先预测以下三对谁赢：

| 同轮竞争的帧 | 首个决定胜负的位置 | 预期胜者 |
|---|---|---|
| 标准数据与上述扩展数据 | 标准RTR=0，对扩展SRR=1 | 标准数据 |
| 标准远程与上述扩展数据 | RTR/SRR都为1，再比IDE：0对1 | 标准远程 |
| 相同完整ID的同格式数据与远程 | RTR：0对1 | 数据帧 |

因此“标准优先于扩展”也要有前提：**基础11位相同**。若扩展帧的基础ID更小，它会在前11位就赢。比如扩展ID`0x0003FFFF`的基础ID为0，遇到标准ID`0x100`会更早胜出，尽管把两个ID当普通整数比时`0x0003FFFF`更大。

这就是不能把STD与EXT的数字直接放进一个普通整数排序器的原因。先比较线上的位序，才知道优先级；主线“较小ID先发”的快捷结论需要保留相同格式这个语境。

## 先验证打包与拆包

以下完整C99程序只做无符号算术，输入固定且经过范围断言：

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uint32_t base = UINT32_C(0x123);
    const uint32_t extension = UINT32_C(0x00005);
    uint32_t id;
    assert(base <= UINT32_C(0x7FF));
    assert(extension <= UINT32_C(0x3FFFF));
    id = (base << 18) | extension;
    assert(id == UINT32_C(0x048C0005));
    assert((id >> 18) == base);
    assert((id & UINT32_C(0x3FFFF)) == extension);
    printf("id=0x%08lX base=0x%03lX tail=0x%05lX\n",
           (unsigned long)id, (unsigned long)(id >> 18),
           (unsigned long)(id & UINT32_C(0x3FFFF)));
    return 0;
}
```

预期输出`id=0x048C0005 base=0x123 tail=0x00005`。真实接口应拒绝大于`0x1FFFFFFF`的扩展ID，而不是悄悄截掉高位。标准ID也应单独校验11位范围。这里讲字段宽度；具体网络还可能限制保留ID与消息分配，并非宽度合法就允许随便发。

F4 HAL发送头用`IDE`选择`StdId`还是`ExtId`，接收头也分别提供这两个字段。以下是**头部配置示意**，依赖已包含F4 HAL头文件的工程，并不是发送成功证明：

```c
CAN_TxHeaderTypeDef h = {0};
h.IDE = CAN_ID_EXT;
h.ExtId = 0x048C0005u;
h.RTR = CAN_RTR_DATA;
h.DLC = 2u;
h.TransmitGlobalTime = DISABLE;
```

随后还需按主线检查邮箱、准备驱动要求的有效缓冲区、处理返回值与发送完成。只把`StdId`赋成29位数却保留`CAN_ID_STD`，不会自动转换成扩展帧。

## DLC=8、没有8字节，是合法情况

经典CAN远程帧用于请求某个ID对应的数据。它的RTR为隐性1，**没有DATA FIELD**；DLC表示所请求的对应数据帧长度，而不是远程帧自带的有效负载。

给定协议约定：ID`0x321`的数据长度固定为2，内容是温度值。一个请求方可以发送`STD、ID=0x321、RTR=remote、DLC=2`。预期分析仪显示远程请求没有数据；若另一节点实现了这个服务，它随后发送`STD、ID=0x321、RTR=data、DLC=2`及两个数据字节。两者是**两帧**，不是ACK槽里传回温度。

bxCAN接收到远程帧，不代表它自动找到你RAM里的变量并生成回复。应用或你选用的协议栈必须实现响应；对端也可能根本不支持远程请求。CAN的ACK仍只是链路层应答，即便远程帧得到ACK，也可能一直等不到应用数据。

在接收接口中，应至少保留`id`、标准/扩展标记、RTR、DLC，再在**数据帧分支**里处理有效载荷。不要看到DLC=8，就把远程帧接收缓冲区里的8字节当作新数据；这些字节没有来自这次远程帧的协议意义。即便HAL API要求提供缓冲区，也不改变这个事实。

## 为什么新设计常用主动上报或普通请求帧

远程帧本身不能携带“请读哪个寄存器、哪一页、第几次请求”等参数，也没有应用事务序号。多个消费者同ID、不同DLC同时请求时，仲裁区可能无法区分，后续控制字段的差异不能靠优先级正常解决。因此相应数据长度应由系统一致约定，不要让各节点自由决定。

周期上报、事件触发数据帧，或携带命令与序号的普通数据帧请求/响应，通常更容易定义超时、重试和去重。CiA对新的CANopen设计也不再推荐基于远程帧的服务，在线监测推荐心跳；这不表示已有远程帧设备都应立刻废弃，而是集成时要先确认对端协议。CAN FD本身没有经典CAN的远程帧形式。

## 过滤器和数据结构要一起检查

主线接收器可能先用IDE/RTR排除非标准数据帧，也可能通过过滤器掩码挡住它们。分析仪看到扩展帧、应用看不到，未必是硬件不支持。若要扩展，请同时检查过滤规则、头部解码、存储宽度、日志显示和业务分派，不能只放宽发送函数中的ID上限。

32位bxCAN过滤比较字中的扩展ID是左移3后再加入IDE/RTR，和刚才为了分拆基础ID而左移18的含义不同。前者是寄存器布局，后者是29位ID内部结构。混用两个位移，是扩展帧过滤常见故障。

## 练习：两个容易混淆的“8”

分析仪给出A：`EXT 0x048C0005 DATA DLC=8`，B：`STD 0x123 REMOTE DLC=8`。如果同轮竞争，谁先赢？两帧各有几个有效数据字节？B成功发出后是否保证马上得到八字节回复？

<details><summary>展开答案</summary>

基础ID相同。标准远程帧先在RTR/SRR打平，再以IDE=0击败扩展帧的IDE=1，因此B赢。A有8字节数据，B没有数据字段，DLC=8是对应数据帧的请求长度。ACK不保证任何应用实现这个请求；即便实现了，也还需等总线仲裁、排队和应用处理，不能保证立即回复。

</details>

## 核对依据与边界

- [Bosch CAN Specification 2.0原文存档](https://zlg.cn/data/upload/software/Can/CANag.pdf)：1991年9月Part B §3.2.1、§3.2.2，核对SRR/IDE、基础ID、RTR与远程帧DLC。
- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：§32.3.1、§32.9.3，核对F407的11/29位支持与邮箱字段。
- [CiA Error control protocols](https://www.can-cia.org/can-knowledge/error-control-protocols)：核对远程帧式守护与心跳建议；[CiA CAN FD说明](https://www.can-cia.org/can-knowledge/can-fd-the-basic-idea)核对FD无远程帧。

主机测试验证ID拆装与仲裁比较；HAL片段只展示集成字段，没有完整固件或上板通过承诺。回到 [7.2](/lessons/7-2)，继续 [bxCAN过滤器组详解](/notes/bxcan-filters)、[CAN总线电气与仲裁](/notes/can-electrical-arbitration)、[CAN FD与CANopen简介](/notes/can-fd-canopen)。
