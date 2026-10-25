---
title: 【拓展】C为什么不检查溢出
description: 通过安全加法和长度乘法实验，理解 C 的溢出契约并选择检查、回绕或饱和策略。
sourceKey: 【拓展】C为什么不检查溢出
---

# C 为什么不检查溢出

采集器一次要保存很多条记录，你用“记录数 × 每条字节数”申请缓冲区。测试十条、百条都正常；某次输入很大，申请到的缓冲区却意外变小，后面的写入才把程序弄坏。最先出错的地方不是指针，而是更早的整数乘法。为什么 C 不在那个乘号处立刻叫停？

先把问题说准确：C99 **不要求普通整数算术自动提供运行时溢出报告**，但不代表编译器不能检查，也不代表所有超范围情况都“只丢掉高位”。接着 [1.2 变量、运算与控制流](/lessons/1-2) 做两个安全实验，就能看见哪些责任要由程序承担。

## 把失败变成一个返回值

下面程序不触发有符号溢出。它先问“还能不能加”，确认后才执行加法。把它作为 [1.0 的 PC 工程](/lessons/1-0) 中一个独立演示目标运行。

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* out 必须指向可写对象；失败时保持 *out 原值。 */
static bool add_i32(int32_t a, int32_t b, int32_t *out) {
    if ((b > 0 && a > INT32_MAX - b) ||
        (b < 0 && a < INT32_MIN - b)) {
        return false;
    }
    *out = a + b;
    return true;
}

int main(void) {
    int32_t result = 99;
    bool ok = add_i32(INT32_MAX, 1, &result);
    assert(!ok && result == 99);
    printf("ok=%d result=%ld\n", (int)ok, (long)result);

    ok = add_i32(1200, -700, &result);
    assert(ok && result == 500);
    printf("ok=%d result=%ld\n", (int)ok, (long)result);
    return 0;
}
```

预期输出是：

```text
ok=0 result=99
ok=1 result=500
```

第一行没有“先算出一个坏结果再补救”。它根本没有执行那次危险的加法。第二行则照常计算，检查并不等于拒绝所有大数或所有负数。

为什么检查表达式自己不会溢出？只有 `b > 0` 时才计算 `INT32_MAX - b`，结果位于 0 到 `INT32_MAX - 1`；只有 `b < 0` 时才计算 `INT32_MIN - b`，结果也一定在范围内。逻辑运算的短路求值把这两个前提写进了程序。不要随意去掉前半个条件。

## 三种不同的“装不下”

在继续修程序前，先把主线中的“溢出”拆开：

- **无符号算术回绕**是规定好的模运算。用无符号计数器表示循环编号可能正合适；用它表示分配字节数却通常是错误。语言行为有定义，不代表业务结果正确。
- **有符号算术超范围**在本课 C99 基线下是未定义行为。不能保证变成负数，不能保证报错，也不能保证只影响那个变量。
- **整数转到装不下的有符号类型**，C99/C11 把结果或信号交给实现定义。它不等于加法溢出。具体规则见 [补码与整型转换](/notes/integer-conversions)。

小类型还会先提升。`uint8_t x = 255; x++;` 中的加一通常在 `int` 中得到 256，之后转回八位才得到 0，不能拿这个现象证明 `int` 加法也必然回绕。

## 为什么不统一插入一个检查

Ritchie 在《The Development of the C Language》中回顾，C 是为早期 Unix 的系统实现发展起来的语言，诞生环境资源有限，后来又要跨多种机器移植。这是设计背景，不是“当年某台机器没有检查指令”的单一故事。

强制每次运算报告超范围，会要求实现统一规定：怎样检查、怎样报告、出错后是否继续，以及没有相应硬件指令时怎样补上代码。系统软件有时恰好需要模运算，有时需要失败返回，有时需要输出限幅；语言不能替业务选出同一个答案。普通 C 算术选择了较少的运行时承诺，留下了优化空间，也把范围论证的负担交给了程序员。

现代 CPU 可以有进位、溢出标志，但“某条指令设置了标志”不等于 C 程序已经检查它。编译器可能重排、合并甚至消除运算，普通 C 源码没有一个自动绑定每次加法的“上次溢出标志”可随手读取。检查必须通过受支持的接口或有定义的表达式表达。

## “算完再比较”为什么可能被删掉

下面是**反例，只用于推理或查看生成代码；不要用最大值调用并依赖输出**：

```c
int increased(int x) {
    return x + 1 > x;
}
```

对所有使 `x + 1` 有定义的输入，结果都为真。`x == INT_MAX` 时表达式没有标准定义的结果，因此优化器可以把函数简化成返回 1，不必保留一个“绕回负数”的分支。这不是优化器承诺你会得到 1，而是说明拿未定义行为来测试检查机制没有可靠性。

同理，`int s = a + b; if (s < a) ...` 不是通用的有符号加法检查。需要像开头那样先证明，或在足够宽的类型里运算再检查。对于已存在的 `int32_t` 和 `int64_t`，两个 32 位操作数的和在 64 位中可表示；不过“扩大一档”不能无限递推，两个最大 64 位数相加并没有标准要求存在的 128 位兜底类型。

## 回到缓冲区：乘法先用除法守门

记录数与长度采用 `size_t`，检查应使用它的上界，而不是随手写一个 `UINT32_MAX`。这个片段依赖 `stddef.h`、`stdint.h`、`stdbool.h`，可以加入前面的程序；`out` 同样要求非空且可写：

```c
static bool size_product(size_t count, size_t each, size_t *out) {
    if (each != 0 && count > SIZE_MAX / each) {
        return false;
    }
    *out = count * each;
    return true;
}
```

以输入 `(10, 24)` 验证，应返回真并得到 240；输入 `(SIZE_MAX, 2)` 应返回假。`each != 0` 既避免除零，也让零长度乘法得到定义明确的 0。实际分配时，还要独立决定是否接受零长度、是否超过产品规定的最大消息尺寸，以及分配器是否成功；“乘法没溢出”不等于“内存一定足够”。

若随后还要加头部长度，也要单独检查加法。例如已得到 `body`，加 `header` 前判断 `body > SIZE_MAX - header`。一次检查不会替整条算术链买保险。

## 三层防线可以同时使用

第一层是接口契约：写明输入范围、输出单位、出错时输出是否变化。长度计算失败应拒绝请求；毫秒计数可以有意回绕；控制量可能需要 [饱和运算](/notes/saturating-arithmetic)。不要把一种处理策略偷偷当成所有场景的默认值。

第二层是代码检查。GCC 提供 `__builtin_add_overflow`、`__builtin_sub_overflow`、`__builtin_mul_overflow`：按无限精度数学结果判断能否放入输出类型，返回真表示发生超范围，所有输入值的行为都有定义。这是编译器扩展，不是 C99 标准库。它**会写出转换后的结果**，与本文失败时保留输出的接口不同；调用后必须先检查布尔值再使用结果。

第三层是测试与诊断。`-Wall -Wextra -Wconversion` 能暴露不少类型问题，但不可能知道每个运行时输入。支持相关运行库的宿主工具链可使用 `-fsanitize=undefined` 或 `-fsanitize=signed-integer-overflow` 检测实际执行到的路径；没有报告不等于所有路径都安全，裸机工具链也未必有可直接链接的运行库。

GCC 的 `-fwrapv` 是另一份编译契约，规定有符号加、减、乘按补码回绕；它不是自动报警，不会修复数组越界、移位错误或除法异常。`-ftrapv` 为有符号加、减、乘生成陷阱检查，也不是覆盖全部整数危险操作的总开关。不要在不理解项目约定时拿一个选项掩盖错误。

## 练习：给减法加一道门

仿照 `add_i32` 写 `sub_i32(a, b, out)`：可表示时存入 `a - b` 并返回真；失败时不改输出。用 `(INT32_MIN, 1)`、`(INT32_MAX, -1)`、`(5, -3)` 检查。为什么不能先把减法改成 `a + (-b)`？

<details>
<summary>展开答案</summary>

`b > 0` 时，若 `a < INT32_MIN + b` 则拒绝；`b < 0` 时，若 `a > INT32_MAX + b` 则拒绝。其余情况才执行 `*out = a - b`。两个门限都在各自条件下可表示。前两个输入应失败，第三个成功得到 8。把减法改成取负再加，会在 `b == INT32_MIN` 时先遇到无法表示的 `-b`；即使最终数学差能表示，也已经来不及。

</details>

## 适用范围与技术依据

示例以 C99 为基线，固定宽度函数要求实现提供相应类型。这里验证的是数值算法，不是内存分配器、实时故障处理或 STM32 真机安全策略。所有未定义行为反例都不作为运行结果测试。

- [WG14 N1570 文本镜像](https://port70.net/~nsz/c/c11/n1570.html#6.5)：第 6.5 节第 5 段、6.2.5、6.3.1.3、6.5.5；对照 [官方草案入口](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)。
- [GCC 检查算术内建函数](https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html)、[代码生成选项](https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html)、[运行时检测选项](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)：已核对返回值、写出结果与覆盖范围。
- [Dennis M. Ritchie，The Development of the C Language](https://cscie26.dce.harvard.edu/~dce-lib113/reference/c/c_history.html)：1993，Abstract、Introduction、Critique；作者原文的大学镜像，用于历史背景，不代替语言规范。

回到 [1.2 变量、运算与控制流](/lessons/1-2) 和 [1.3 整型与位运算](/lessons/1-3)。相关分支：[补码与整型转换](/notes/integer-conversions)、[饱和运算](/notes/saturating-arithmetic)、[浮点与定点](/notes/float-fixed-point)。
