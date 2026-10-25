---
title: 【拓展】bxCAN过滤器组详解
description: 把标准与扩展标识符转换成bxCAN比较字，正确安排掩码、列表、FIFO和双CAN共享过滤器组。
sourceKey: 【拓展】bxCAN过滤器组详解
---

# bxCAN过滤器组详解

分析仪能看到`0x105`，发送板也报告成功，接收板却没有回调。把过滤器改成“全收”后马上好了。此时不该先改中断优先级：很可能是硬件已经正确收到了帧，却按你的规则没有把它放进接收FIFO。

本篇在 [7.3 过滤器与错误处理](/lessons/7-3) 的掩码实验上继续，固定研究 **STM32F407的双bxCAN**。过滤器降低CPU接收负担，不减少线上的帧，也不是安全认证。

## 先只允许这一组消息通过

目标明确为：只接收标准数据帧`0x100`到`0x10F`，拒绝相同ID的远程帧，也拒绝扩展帧。先在纸上算：

```text
filter_id = 0x100
id_mask   = 0x7F0
通过条件  = ((received_id XOR filter_id) AND id_mask) == 0
```

高7位必须相同，低4位随意。输入`0x0FF、0x100、0x105、0x10F、0x110`，预期只有中间三个通过。但这个公式还没有比较帧格式与RTR；把它原封不动移到硬件，可能留下意外入口。

32位过滤器实际比较的布局如下。这里的“位”是过滤器寄存器里的位置，不是CAN线上的发送顺序：

```text
31................21 20................3  2    1    0
      STID[10:0]          EXID[17:0]       IDE  RTR  0
标准数据帧：key = standard_id << 21
扩展数据帧：key = extended_id << 3 | 0x4
远程帧：在对应key上再设置bit1
```

因此本例应使用`filter=0x20000000`，`mask=0xFE000006`。最后的`6`要求IDE和RTR都与过滤值中的0一致。高半字掩码不是完整的标准ID掩码`0xFFE0`，而是只保留本例高7位的`0xFE00`。

## 先运行比较器，再配HAL

这是可以在主机编译的完整C99例子：

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int accepts(uint32_t key, uint32_t id, uint32_t mask)
{
    return ((key ^ id) & mask) == 0u;
}

int main(void)
{
    const uint32_t id = UINT32_C(0x100) << 21;
    const uint32_t mask = (UINT32_C(0x7F0) << 21) | 6u;
    const uint32_t standard_data = UINT32_C(0x105) << 21;
    const uint32_t standard_remote = standard_data | 2u;
    const uint32_t extended_data =
        ((UINT32_C(0x105) << 18) << 3) | 4u;

    assert(accepts(standard_data, id, mask));
    assert(!accepts(standard_remote, id, mask));
    assert(!accepts(extended_data, id, mask));
    assert(!accepts(UINT32_C(0x110) << 21, id, mask));
    puts("standard data: pass; remote/extended/outside: drop");
    return 0;
}
```

预期只打印一次上述结果。把`mask`末尾的`| 6u`去掉再预测：两个帧类型断言将失败。实验故意让断言暴露过滤范围扩大，不应把失败当作编译器问题。

在已生成CAN1初始化、完成时钟与引脚配置的HAL工程中，以下是**启动前的配置片段**，不是完整固件：

```c
CAN_FilterTypeDef f = {0};
f.FilterBank = 0u;
f.FilterMode = CAN_FILTERMODE_IDMASK;
f.FilterScale = CAN_FILTERSCALE_32BIT;
f.FilterIdHigh = 0x2000u;
f.FilterIdLow = 0x0000u;
f.FilterMaskIdHigh = 0xFE00u;
f.FilterMaskIdLow = 0x0006u;
f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
f.FilterActivation = ENABLE;
f.SlaveStartFilterBank = 14u;
if (HAL_CAN_ConfigFilter(&hcan1, &f) != HAL_OK) {
    Error_Handler();
}
```

它依赖STM32F4 HAL、已初始化的`hcan1`以及工程自己的错误处理函数。之后按主线启动CAN并打开FIFO0通知。确保原先的“全收”过滤器已停用；多个激活过滤器是多个入口，新增一条窄规则不会把旧的宽规则变窄。

## 一组两个32位寄存器，为什么能放四个ID

一组过滤器是固定的64位存储，但有四种解释方式：

| 尺度与模式 | 一组容纳什么 | 典型用途 |
|---|---|---|
| 32位掩码 | 一对ID与mask | 完整扩展ID或精确控制IDE/RTR |
| 32位列表 | 两个完整比较字 | 两个离散扩展ID |
| 16位掩码 | 两对16位ID与mask | 两组标准ID范围 |
| 16位列表 | 四个16位比较字 | 四个离散标准ID |

16位布局是`STID[10:0]、RTR、IDE、EXID[17:15]`，即标准ID左移5，RTR在bit4、IDE在bit3。它只带扩展ID的一部分，**不能用一条16位规则精确筛完整29位ID**。

若只收标准数据帧`0x100`、`0x205`，可以用16位列表，把四个位置填成`0x2000、0x40A0、0x2000、0x40A0`。两个“空位”必须重复允许值，而不是填0；填0就额外允许标准数据帧`0x000`。同理，16位掩码模式中第二对全0等于放行大量帧，不是关闭半组。

还要小心HAL字段命名。32位模式中，`FilterIdHigh/Low`拼成FR1，`FilterMaskIdHigh/Low`拼成FR2。**16位模式不同**：ST HAL把`FilterIdLow`放FR1低半字、`FilterMaskIdLow`放FR1高半字；把`FilterIdHigh`放FR2低半字、`FilterMaskIdHigh`放FR2高半字。列表模式里带“Mask”的字段也存ID，不再表示掩码。

## 双CAN不是各有一套独立的28组

F407的CAN1、CAN2共享28组，编号0–27。`CAN2SB`给出分界；HAL用`SlaveStartFilterBank`设置它。设14时，CAN1使用0–13，CAN2使用14–27。寄存器 `CAN2SB=28` 的硬件语义是全部给CAN1，`0` 是全部给CAN2。但**寄存器能力不等于HAL API的受支持范围**：本文所引STM32F4 HAL将 `SlaveStartFilterBank` 校验为0..27，启用 `USE_FULL_ASSERT` 时传28会触发断言，不能把28直接当作该API支持的配置；应按实际驱动版本选择经验证的实现。课程使用14不受此问题影响。**14是课程的分配方案，不是F407的物理总组数。**

分界是共享配置，不是每个句柄自己的私有参数。两路初始化都要使用一致的分界，且不能同时改共享过滤器。CAN2还依赖CAN1侧的共享资源与时钟；不要看到只用CAN2就把CAN1时钟关掉。不同STM32料号的单CAN、双CAN、FDCAN配置必须重新查手册。

重配置过滤器涉及FINIT、组停用、尺度、模式、FIFO分配与重新激活。HAL负责这些寄存器步骤，但不会替应用协调重配置窗口：运行中修改可能影响接收，双CAN工程尤其要集中安排，不要在两个中断里各自改一遍。

## FIFO与匹配索引别猜

一帧同时符合多个过滤器时，bxCAN不是给每个FIFO都复制一份。手册优先规则为：32位优先于16位；同尺度时列表优先于掩码；同尺度同模式时较小的过滤器编号优先。匹配结果决定所用FIFO及FMI。

FMI是Filter Match Index，不应不经验证就当作`FilterBank`。其编号与尺度、模式及FIFO分配有关，也不会因为某组停用就简单压缩。新手先记录实际ID、IDE、RTR与FMI来核对，确认后再用FMI分派业务。分配FIFO1还需相应的通知和处理路径；只开FIFO0中断会看起来像“过滤器坏了”。

### 用接收清单验收，而不是只发一帧

先在回环下发送允许范围的两端、范围外紧邻的两个ID，再加入同ID的远程帧和相同基础ID的扩展帧，逐项记录是否入FIFO。进入Normal模式后用独立发送节点重复清单，确认仍符合预期。只发一帧允许的消息并收到，无法证明过滤器够窄；全收规则也会通过这种测试。若拒绝项意外出现，先列出全部激活组，而不是只盯着最后一次配置的组。若允许项完全没有，再沿FIFO分配、通知使能与应用读取路径查下去。

## 练习：只收一个完整标准ID

把本例改成只收标准数据帧`0x205`。求32位filter、mask及HAL四个半字。再解释为什么“不接收某ID”不能证明发送方没得到ACK。

<details><summary>展开答案</summary>

filter=`0x40A00000`，mask=`0xFFE00006`。四个半字依次为`0x40A0、0、0xFFE0、0x0006`。mask比较全部11位ID与IDE/RTR。正常参与总线的控制器会对正确接收到的帧进行链路层确认；是否交给本节点软件由接收过滤决定。ACK没有“我的业务处理完了”的含义，因此发送成功不能证明目标应用收到消息。

</details>

## 核对依据与验证范围

- [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：§32.7.4、图342、Filter priority rules及§32.9.4，核对布局、组数、分界与优先规则。
- [ST官方HAL CAN实现](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_can.c)：`HAL_CAN_ConfigFilter`，核对两种尺度下四个HAL半字怎样写入FR1/FR2；本次读取的是2026-10-25取得的master源码，集成时还要对照本工程驱动版本。

主机断言验证的是比较字与掩码，不模拟ACK、FIFO、中断或真实HAL寄存器访问。板级片段没有本轮上板验证。回到 [7.3](/lessons/7-3) 做接收实验，再读 [CAN扩展帧与远程帧](/notes/can-extended-remote) 与 [CAN总线电气与仲裁](/notes/can-electrical-arbitration)。
