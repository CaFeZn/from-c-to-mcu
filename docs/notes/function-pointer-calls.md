---
title: 【原理】函数指针与间接调用
description: 用安全的整数比较和 qsort 回调，看清函数地址、签名匹配以及间接调用的边界。
sourceKey: 【原理】函数指针与间接调用
---

# 函数指针与间接调用

你把一个比较大小的函数交给排序库，编译器却说“指针类型不兼容”。明明都是比较两个东西，给函数名加一次强制转换不就能用了？这恰好是函数指针最危险的误会：**地址决定去哪里，类型决定怎样把参数交过去、怎样接回结果**。一个地址看起来像对的，不代表双方按同一份协议说话。

[主线 1.7](/lessons/1-7) 已经做过“名字→处理函数”的命令表。这次不再加命令，而是把一次调用拆开：先调用自己的比较器，再让 `qsort` 回头调用它。最后解释为什么中间需要一层适配函数，而不是一记类型转换。

## 先预测：同一个函数，谁来调用有什么不同？

输入数组固定为 `9, -7, 0, 9, -2`。比较规则是小于返回负数，相等返回零，大于返回正数。先写下三个预测：

1. `order(7, 9)` 和把 `order` 存进变量后调用，结果是否相同？
2. `qsort` 交给比较函数的，是两个整数，还是两个元素的地址？
3. 把比较器写成 `return a - b;`，普通小数值能通过，是否说明它适用于全部 `int`？

下面是完整 C99 程序。先原样运行，再改比较方向；不要先改函数签名。

```c
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*relation_fn)(int, int);

static int order(int a, int b)
{
    return (a > b) - (a < b);
}

static int call_order(relation_fn fn, int a, int b, int *out)
{
    if (fn == NULL || out == NULL) {
        return -1;
    }
    *out = fn(a, b);
    return 0;
}

static int order_elements(const void *pa, const void *pb)
{
    const int *a = pa;
    const int *b = pb;
    return order(*a, *b);
}

int main(void)
{
    relation_fn fn = order;
    int result = 99;
    int rc = call_order(fn, 7, 9, &result);
    int data[] = {9, -7, 0, 9, -2};
    const int expected[] = {-7, -2, 0, 9, 9};
    const size_t n = sizeof data / sizeof data[0];

    assert(rc == 0 && result == -1);
    assert(fn == &order);
    assert((*fn)(9, 9) == 0);
    printf("direct=%d indirect=%d\n", order(7, 9), result);

    qsort(data, n, sizeof data[0], order_elements);
    for (size_t i = 0; i < n; ++i) {
        assert(data[i] == expected[i]);
    }
    printf("sorted=%d,%d,%d,%d,%d\n",
           data[0], data[1], data[2], data[3], data[4]);

    assert(order(INT_MIN, INT_MAX) == -1);
    assert(order(INT_MAX, INT_MIN) == 1);
    rc = call_order(NULL, 1, 2, &result);
    assert(rc == -1 && result == -1);
    rc = call_order(fn, 1, 2, NULL);
    assert(rc == -1);
    puts("edge checks: OK");
    return 0;
}
```

在仓库根目录可单独构建这个例子，不需要构建网站：

```sh
gcc -std=c99 -O0 -Wall -Wextra -Werror -pedantic examples/notes/c-functions/pointer_demo.c -o pointer_demo
./pointer_demo
```

Windows 命令提示符运行最后一步时用 `pointer_demo.exe`。断言需要保持启用，不要加 `-DNDEBUG`。预期三行是：

```text
direct=-1 indirect=-1
sorted=-7,-2,0,9,9
edge checks: OK
```

配套程序还断言了 `INT_MIN`、`INT_MAX` 的比较、相等比较，以及空函数指针和空输出参数被拒绝。这里不打印具体整数上下界，因此不依赖某台电脑的 `int` 一定是 32 位。本文例子已用 MinGW GCC 6.3.0 严格 C99 选项实际编译运行；不是在 STM32 上调用过排序库的证据。

## 函数指针里装的不是函数副本

看 `relation_fn fn = order;`。在这样的表达式中，函数名会转换成指向该函数的指针；写 `&order` 也能取得相应指针。赋值没有复制机器指令，没有再次执行函数，更没有顺便记住上次参数。`fn(7, 9)` 才是调用。

声明 `int (*fn)(int, int)` 可以从名字往外读：`fn` 被 `*` 修饰，是指针；括号外的参数表说明它指向函数；最前面的 `int` 是返回类型。与之对照，`int *fn(int, int)` 声明的是“返回 `int *` 的函数”。区别来自后缀 `()` 比 `*` 结合得紧，**不是星号先和返回类型粘在一起**。`typedef` 只是给前一种完整类型起别名，方便你看到接口而不是陷入括号。

通过函数指针写 `fn(a, b)` 和 `(*fn)(a, b)` 都可以。后者把取出函数这一步写得更显眼，不代表多了一次机器指针解引用。在 C 的调用规则里，两者都按对应的函数类型处理。函数名直接写出来叫“直接调用”的源代码形式；从变量取得目标叫“间接调用”的源代码形式。优化器如果能证明 `fn` 永远等于 `order`，可能消掉间接调用甚至内联，所以不能仅靠 C 写法保证最后一定有一条间接跳转指令。

## 为什么 qsort 要两层比较器？

`order` 知道怎样比较两个 `int` 值，却不知道数组元素占多少字节。`qsort` 恰好相反：它知道数组首地址、元素个数和每个元素的大小，却不知道元素内容该怎样排序。因此它传给 `order_elements` 的是两个**元素地址**，签名必须是 `int (*)(const void *, const void *)`。

适配函数做两件事：把对象指针还原成 `const int *`，读取本次要比较的整数；然后调用原有规则。这里的还原成立，是因为传入排序的确实是 `int` 数组，`sizeof data[0]` 也确实对应这个元素类型。若把别的结构体数组交进来，不能只因为两种对象“差不多大”就继续这样读。

不要写下面这种修补；它只是让某些诊断消失，不会让整数参数变成元素地址：

```c
/* 错误反例：不要执行，不依赖任何输出。 */
/* qsort(data, n, sizeof data[0],
         (int (*)(const void *, const void *))order); */
```

C 允许一种函数指针转换成另一种函数指针再转回原类型，并保证转回后比较相等；这不等于可以用不兼容类型去调用。**调用发生时类型不兼容，是未定义行为**。在不同 ABI 下，参数可能经过不同寄存器、不同宽度或不同返回位置，偶尔看起来能跑不构成理由。

## 小数值能跑通，还剩哪些边界？

- **比较不能靠有符号减法。** `a - b` 在极端输入下可能超出 `int` 范围。`(a > b) - (a < b)` 只对零和一做减法，结果限定为负一、零或一。规则必须前后一致，不能在比较途中改变数组排序依据；相等元素的相对顺序也不能靠 `qsort` 保证。
- **空指针检查不是万能验真器。** `call_order` 把空值当作配置错误拒绝，但非空并不能证明地址可调用、函数签名正确或内存没有被破坏。函数指针应来自兼容的函数声明和受控注册，不要把串口输入的一个整数当地址执行。
- **`void *` 不是所有指针的万能盒子。** 标准对对象指针往返 `void *` 有保证，不能自动推广到函数指针。也别为了显示地址就给函数指针套 `%p`：`%p` 要求 `void *` 参数，而可移植 C 没有要求函数指针可这样转换。这里用指针相等比较验证身份，避开表示形式。
- **原型要完整。** 本课程按 C99 写 `void callback(void)` 表示不接收参数。C99/C11 中，单独声明 `void callback();` 不提供参数类型信息，不能拿它当“通吃签名”；C23 对空参数表规则有变化，不反推到本课工具链。

有状态的回调还要把“代码入口”和“这次操作的对象”分开：通常接口另外传 `void *ctx`，由配套的适配函数还原成正确对象。上下文的生命周期、是否异步保存、是否允许在中断里回调，都属于接口合同，不是函数指针类型自动解决的事。这些问题在对象和虚表分支继续展开。

## 动手改：降序，但仍然安全

把数组按降序排列，要求不写整数减法、不改变 `qsort` 要求的签名；给出相等和极端输入的验证。还能不能只把现有比较结果取负？

<details><summary>参考答案：交换比较方向，不交换参数类型</summary>

在 `order_elements` 最后一行返回 `order(*b, *a)` 即可。对应期望数组改为 `{9, 9, 0, -2, -7}`，再断言逆向比较 `INT_MIN` 与 `INT_MAX` 时得到正一，相等时仍得到零。

本例的 `order` 只返回负一、零、一，取负也安全；但任意库比较器可能返回 `INT_MIN`，对它取负又可能溢出。因此“交换两参数”比对一个没有数值范围保证的返回值直接取负更稳妥。不要把输出文本改成降序却漏改断言；两者一起检查才是在验证规则。

</details>

## 适用范围与技术依据

这篇的可执行部分是 hosted C99，使用标准库 `qsort`；裸机能否直接链接它取决于 C 库与工程配置。数组很小仅用于观察，不据此承诺排序时延、栈用量或稳定性。语言规则按 [WG14 N1570 草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) 第 6.3.2.1(4)、6.3.2.3(8)、6.5.2.2(9)、6.7.6.3 和 7.22.5.2 节 核对；该文档是 C11 草案，这些规则在本文的 C99 用法中同样适用。本次官方 PDF 网络读取超时，实际精读了 [N1570 的 HTML 转录](https://port70.net/~nsz/c/c11/n1570.html#6.5.2.2)，没有把它当成新的标准版本。

回到 [1.7 函数指针与回调](/lessons/1-7)；继续看 [函数指针与虚表](/notes/function-tables)、[C 语言里的面向对象](/notes/objects-in-c)、[调用约定与尾调用优化](/notes/calling-conventions)。
