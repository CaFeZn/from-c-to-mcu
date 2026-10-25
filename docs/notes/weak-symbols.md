---
title: "【原理】弱符号如何让回调生效"
description: "用ARM符号表和静态库抽取实验，验证用户回调如何覆盖弱实现并定位不生效的原因。"
sourceKey: "【原理】弱符号如何让回调生效"
---

# 弱符号如何让回调生效

你在自己的源文件里写了 `HAL_GPIO_EXTI_Callback`，没有调用注册函数，按键中断却能进入它。换一个工程，把同样的源文件装进静态库，程序又只执行库里的空回调。问题不在于弱符号“时灵时不灵”，而在于两个判断先后发生：哪些对象真正进入链接，以及同名定义进入之后选谁。

## 先做一个没有外设的覆盖实验

使用[本组weak示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build/weak)。接口头只声明 `unsigned on_event(unsigned value);` 与 `unsigned dispatch(unsigned value);`，没有弱属性。默认实现和调用者放在一个源文件里：

```c
#include "api.h"
__attribute__((weak)) unsigned on_event(unsigned value)
{
    return value + 1u;
}
unsigned dispatch(unsigned value)
{
    return on_event(value);
}
```

用户实现放在另一个文件，保持相同名称和类型，但不加弱属性：

```c
#include "api.h"
unsigned on_event(unsigned value)
{
    return value + 100u;
}
```

主函数执行 `callback_result = dispatch(7u);`，之后停在循环。先预测：不链接用户对象时，结果应为八；直接加入用户对象时，应为一百零七。这里故意使用 ARM ELF，不把不同 Windows PE 工具链对弱符号的处理当成统一保证。

在仓库根目录的 PowerShell 编译，前提是 ARM GCC、nm、ar 已在搜索路径里：

```powershell
$S = "examples/notes/build"
$B = "artifacts/course-completion/build/weak"
New-Item -ItemType Directory -Force $B | Out-Null
$F = @("-mcpu=cortex-m4", "-mthumb", "-O0", "-g",
       "-ffreestanding", "-ffunction-sections", "-fdata-sections")
foreach ($n in @("main", "default", "override")) {
  arm-none-eabi-gcc @F -std=c99 -c "$S/weak/$n.c" -o "$B/$n.o"
}
arm-none-eabi-gcc @F -c "$S/arm/startup.S" -o "$B/startup.o"
$L = @("-nostdlib", "-T", "$S/arm/layout.ld", "-Wl,--gc-sections",
       "$B/startup.o", "$B/main.o", "$B/default.o")
arm-none-eabi-gcc @F @L -o "$B/default.elf"
arm-none-eabi-gcc @F @L "$B/override.o" -o "$B/direct.elf"
arm-none-eabi-nm "$B/default.elf"
arm-none-eabi-nm "$B/direct.elf"
```

本次 ARM GCC 12.3.1 离线核验中，默认镜像的 `on_event` 为 `W`，直接加入覆盖对象后为 `T`。再用 `arm-none-eabi-objdump -d` 检查，`dispatch` 的调用目标指向最终选中的函数，直接覆盖版本中能看到增加100的指令。**这些是链接和反汇编证据，八与一百零七是由指令推导的预期值，不是板上运行读数。** 示例启动文件不含完整异常向量，不能当作可烧录固件。

## W与T背后的选择规则

弱符号是目标文件及链接器支持的机制，GCC的 `__attribute__((weak))` 是实现扩展，不是 ISO C99 关键字。对这里的 GNU ELF 链接，普通全局定义与弱定义同时进入时，普通定义胜出。常把普通定义称作“强定义”；`nm` 的 `T` 表示本例全局代码符号，`W` 表示弱符号。

链接器处理的是名字、绑定属性及引用，并不是执行期间监视你是否写了某个函数。最终的调用指令已经指向选定地址；无需每次进中断再动态搜索函数名。也因此，弱覆盖与“把函数指针存进句柄，运行时注册回调”不同，后者可以按实例切换，前者通常是整个链接映像对该名字的选择。

这里的前提是覆盖方类型兼容、具有合适的外部链接，而且确实被纳入当前目标。两个普通定义一般会造成重复定义错误。两个弱定义不能用作可靠的优先级系统，不应让业务行为依赖碰巧的链接顺序。不要为了“对称”而把 `weak` 写进所有使用者共享的声明，再让用户定义包含那个声明；属性可能被继承，结果双方都弱，覆盖意图就不清楚了。

## 为什么装进静态库后强定义没进来

继续执行，不改源代码：

```powershell
arm-none-eabi-ar rcs "$B/liboverride.a" "$B/override.o"
arm-none-eabi-gcc @F @L "$B/liboverride.a" -o "$B/archive.elf"
arm-none-eabi-nm "$B/archive.elf"
arm-none-eabi-gcc @F @L "-Wl,--whole-archive" "$B/liboverride.a" "-Wl,--no-whole-archive" -o "$B/whole.elf"
arm-none-eabi-nm "$B/whole.elf"
```

本次结果：普通归档链接仍为 `W`，限定范围的 `--whole-archive` 链接变为 `T`。前者不违反强覆盖弱：链接器读到这个归档时，默认对象已提供 `on_event` 定义，不存在迫使它抽取仅含用户覆盖函数的成员的未解决需求；强定义根本没参与最后的竞争。

静态库不是“把里面所有对象都无条件传给链接器”。GNU ld按当时的未解决引用抽取成员，命令顺序、其他引用和归档内容都可能改变结果。本例因此固定了对象组合和顺序；不能把结果归纳成“所有静态库都无法覆盖弱符号”。若该成员还定义一个被外部明确需要的初始化函数，它可能因那个符号被抽取，覆盖随后才发生。

工程修复通常优先让用户回调的源文件直接属于最终固件目标，或使用对象库明确加入对象。`--whole-archive` 适合证明原因或有边界地使用，但会强制带入成员，可能增加体积或暴露重复定义；必须及时用 `--no-whole-archive` 收住范围，不能把所有库都粗暴套进去。

## 回到HAL：有覆盖，还要有调用链

核对的 STM32F4 HAL GPIO源码在检测到指定 EXTI挂起后先清标志，再调用 `HAL_GPIO_EXTI_Callback(GPIO_Pin)`；默认回调带 `__weak` 且只消除未使用参数警告。用户实现应放在用户源文件，保持签名，例如下面片段需要真实HAL工程的头文件与中断配置，不是独立PC程序：

```c
#include "stm32f4xx_hal.h"
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) {
        /* Set a short, ISR-safe event flag here. */
    }
}
```

`GPIO_Pin` 是位掩码，`GPIO_PIN_0` 不是编号零。弱覆盖只解决“最终调用哪份函数”，不会替你开启GPIO/EXTI时钟、配置触发沿、使能NVIC，也不会替中断入口调用HAL处理函数。排查时应沿真实链路走：中断有没有进入，HAL处理函数有没有执行，挂起条件是否成立，最终符号和调用地址是否指向用户代码。

如果符号表里已经是用户的普通定义，但仍进不去，应先检查上游事件和分发条件；若仍是默认弱定义，则检查目标源文件清单、归档抽取与拼写。不要一开始就修改厂商默认函数，下一次更新或代码生成很容易覆盖这种修改。

## 容易伪装成“覆盖失败”的边界

`static` 用户函数具有内部链接，不能替换别的翻译单元的外部名字；声明与定义冲突时还可能直接编译报错。C++文件可能产生名字修饰，应使用库提供的C接口声明，必要时通过正确的 `extern "C"` 边界保持符号一致，而不是随意改链接器参数。

另一个边界是回调上下文。覆盖成功并不让它变成普通主循环任务；从中断调用的回调依然受中断延迟、共享状态和可重入性约束。里面长时间打印、等待依赖同级中断更新的计数，仍可能阻塞系统。符号机制与并发设计是两项独立检查。

## 练习：看到强符号却没有打印

你确认最终映像 `HAL_GPIO_EXTI_Callback` 是用户对象中的普通定义，但按键后没有打印。列出至少三个比“给函数再加weak”更有价值的检查点。

<details><summary>答案与排查解释</summary>

先在外部中断入口观察是否进入，检查引脚复用、触发沿、EXTI映射与NVIC；再确认入口调用了对应 `HAL_GPIO_EXTI_IRQHandler`，且传入的是正确位掩码；随后检查用户回调内的条件是否把事件过滤掉。最后单独验证打印通道是否在中断上下文可用，不能把无输出等同于未执行。加弱属性不会修复任何一项上游条件，反而可能破坏本来明确的覆盖选择。

</details>

## 依据与相邻主题

- [GCC 12.3 Common Function Attributes](https://gcc.gnu.org/onlinedocs/gcc-12.3.0/gcc/Common-Function-Attributes.html)：`weak`、`alias`，特别是覆盖方类型要求和目标格式支持。
- [GNU ld Options](https://sourceware.org/binutils/docs/ld/Options.html)：`-l` 的归档抽取、`--whole-archive` 与作用范围。
- [STM32F4 HAL GPIO固定提交](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/1f6451c3e07728b4c830744de380e56bf5bc0026/Src/stm32f4xx_hal_gpio.c)：`HAL_GPIO_EXTI_IRQHandler` 和默认回调。

回到主线：[1.7 函数指针与回调](/lessons/1-7)、[5.1 中断入门](/lessons/5-1)。相邻主题：[预处理器与链接](/notes/preprocessor-linkage)、[HAL回调](/notes/hal-callbacks)、[启动流程与链接脚本](/notes/startup-linker-script)。
