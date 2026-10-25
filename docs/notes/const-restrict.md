---
title: 【拓展】const与restrict
description: 从可修改对象的只读视图和数组拷贝实验，分清const限制哪条写入路径、restrict承诺哪些访问不重叠。
sourceKey: 【拓展】const与restrict
---

# const 与 restrict

你把打印数组的函数参数改成 `const int *a`，以为“这个数组从此不会变”。同事却通过另一根指针修改数组，打印结果真的变了。随后你给拷贝函数加上 `restrict`，以为只是一个无害的加速提示，某次重叠拷贝却在优化版出错。

这两个关键词都在告诉编译器事情，但承诺不同。先接着 [1.5 数组与指针](/lessons/1-5) 的“指针加长度”接口，做两个实验，再决定何时该写它们。

## 第一件事：只读的路，不等于冻结了对象

先预测两个断言是否会成功：

```c
#include <assert.h>

int main(void)
{
    int x = 3;
    int y = 4;
    const int *view = &x;
    int *const fixed = &x;

    *fixed = 9;
    assert(*view == 9);
    view = &y;
    assert(*view == 4);
    return 0;
}
```

它们都成功。`x` 本来就是可修改的 `int`；`view` 只是不允许你**通过这条表达式路径**写它，并没有禁止 `fixed` 或 `x` 本身修改它。`view` 这个指针变量也还能指向另一个对象。

从变量名向外读声明，先问“不能改的是指针，还是它指向的值”：

| 声明 | 不能通过它做什么 |
| --- | --- |
| `const int *p` | 不能写 `*p`，可以改 `p` 的指向 |
| `int *const p` | 不能改 `p` 的指向，可以写 `*p` |
| `const int *const p` | 两条限制都有 |

`int const *p` 与第一行同义。把 `const` 放对位置，比把它记成“常量指针”这个有歧义的中文简称更可靠。

### 去掉 const 的强制转换，不会自动让对象可写

下面是**不要执行的未定义行为反例**：

```c
const int limit = 10;
int *wrong = (int *)&limit;
*wrong = 20;  /* limit 本身以 const 限定类型定义。 */
```

即使转换能通过编译，也不授权修改原本以 `const` 限定类型定义的对象。反过来，若原对象确实是可修改的 `int`，先形成只读视图再转回可写指针，并不单因“去掉限定符”就成为未定义行为；但这会掩盖接口设计，不应靠普遍强转来消警告。

同样，`const` 不是存储位置指令。一个配置表最终是否进入 Flash/只读段，取决于实现、链接脚本和使用方式；自动存储期的只读局部对象也不等于静态对象。`const volatile` 则并不矛盾：程序不允许通过它赋值，但硬件仍可能改变值，访问又不能被当成普通冗余读取。

## 第二件事：处理数组时，提前说清重叠规则

考虑“将源数组乘二写到目标数组”的接口。我们希望它处理两个独立区域，不承诺支持原地变换或重叠搬移：

```c
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void scale_copy(size_t n, unsigned *restrict dst,
                       const unsigned *restrict src)
{
    for (size_t i = 0; i < n; ++i) {
        dst[i] = 2u * src[i];
    }
}

int main(void)
{
    unsigned data[6] = {1, 2, 3, 0, 0, 0};
    scale_copy(3, data + 3, data);
    assert(data[3] == 2 && data[4] == 4 && data[5] == 6);
    printf("%u %u %u\n", data[3], data[4], data[5]);
    return 0;
}
```

给定前半段 `1,2,3`，后半段应成为 `2,4,6`。这次调用完全合法：**两根指针可以来自同一个大数组**，关键是本次调用真正访问的元素区域互不重叠。这里读 `data[0..2]`，写 `data[3..5]`。

`restrict` 是 C99 的限定符。对这种块内指针参数，可以先用下面的规则审接口：本次函数执行中，若某个对象通过这根受限指针派生的路径被访问，且该对象又被修改，那么对它的相关访问必须来自这一受限指针的派生路径，不能另有独立入口竞争访问。标准还规定，被这样访问且修改的对象不能由指向 const 类型的受限指针来承担访问。

这是对**执行期间的访问关系**作承诺，不是向编译器发送“请先检查重叠”的请求。编译器可以据此减少重复加载或安排运算；它不一定加速，也不必在调用者撒谎时报警。修改过的对象，哪怕另一路只读，也会参与判断。

### 一个地址不等检查，拦不住重叠

不要把刚才调用改成：

```c
/* 未定义行为反例：不要运行后猜某一种输出。 */
scale_copy(3, data + 1, data);
```

`dst != src` 确实成立，但读写区域共享了元素。`data[1]` 等对象既经 `dst` 路径修改，又经独立的 `src` 路径读取，违反承诺。同理，`scale_copy(3, data, data)` 也不适合这个接口。

若需要原地乘二，写一个只收一个指针的 `scale_in_place`，逐个处理元素即可，不要让调用者伪装成两个独立区域。若需要支持重叠搬移，使用 `memmove`，或者设计并证明正确的遍历方向；只删掉 `restrict` 并不会自动修好一个被前面写入覆盖后面输入的算法。

## 为什么两个 restrict 输入却可以相同

再看一个不同接口：

```c
static void add_vectors(size_t n, unsigned *restrict dst,
                        const unsigned *restrict left,
                        const unsigned *restrict right)
{
    for (size_t i = 0; i < n; ++i) {
        dst[i] = left[i] + right[i];
    }
}
```

只要目标区域与输入区域分离，`add_vectors(3, result, data, data)` 可以合法调用，结果仍为 `2,4,6`。两根输入指针共同读取的对象，在函数执行中没有被修改，因而不是“所有受限指针必须两两不同”。C99 标准 6.7.3.1 的例 3 就专门说明这种情况。

另一个容易过度限制的点是：从 `dst` 计算出的 `dst + i`，或普通局部指针 `unsigned *cursor = dst;`，可以是同一受限路径的派生访问。`restrict` 不是禁止指针值被复制。可是多个独立的 restrict 指针彼此赋值还有额外的块作用域规则；不要为了“越多越快”到处加限定符。本篇只采用容易检查的函数参数形式。

## 跟做验证：不要用错误调用的输出当证据

配套程序把上面的只读视图、同数组分离区间和只读输入共用都放进断言，还用 `memmove` 验证重叠右移：

```c
unsigned moved[4] = {1, 2, 3, 4};
memmove(moved + 1, moved, 3 * sizeof moved[0]);
/* 结果是 1,1,2,3；须包含 <string.h>。 */
```

在仓库根目录运行：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic-errors -O2 examples/notes/c-semantics/const-restrict.c -o qualifier-demo.exe
./qualifier-demo.exe
```

预期输出：

```text
view=4 scaled=2,4,6 shared-input=2,4,6 moved=1,1,2,3
```

这里的 unsigned 运算按无符号规则进行；本题小输入不会回绕，但接口若接收大数，仍应说明是模运算还是需要溢出检查。限定符不提供长度检查、空指针检查、生命周期延长，也不替数组处理函数补齐这些前置条件。特别是 `const int *` 不能救一个已经失效的地址。

本例在 MinGW GCC 6.3.0 的 `-O0`、`-O2` 下实际编译运行，固定输出一致；用 Clang 20.1.8 做了严格 C99 语法检查，用 ARM GCC 12.3.1 编译为 Cortex-M4 目标文件。未运行错误的重叠调用，也不把“没报错”当成程序符合 restrict 的证明。

## 练习：给接口写一行可信的契约

有一个图像处理函数，要支持 `dst == src` 的原地处理，也允许输入输出完全分离，但不允许部分重叠。能直接给两个参数都加 `restrict` 吗？若想保留独立区域的优化机会，你会怎样拆接口？

<details><summary>展开答案</summary>

不能直接沿用本篇双指针读写受限模型去接受相同输入输出对象。可以分成单指针原地函数与双指针非重叠函数；外层无 restrict 的包装器先按已知缓冲区关系选择调用。相等时走单指针版，已确保分离时走受限版，部分重叠则拒绝或明确实现。

包装器也不应随便对不相关对象的指针做减法或大小比较来“检测区间”，那些运算有自己的 C 规则。工程中优先让缓冲区所有者提供可验证的范围信息，或从同一数组及长度关系判断。优化的前提应在接口边界建立，而不是在函数内部假装成立。

</details>

## 适用范围与技术依据

本文讨论 ISO C99，不把 C++ 的 const 常量表达式或编译器的 `__restrict` 扩展混进来；标准 `restrict` 自 C99 提供。对于 MMIO、中断共享对象，还要另看 [volatile 与优化](/notes/volatile-optimization)。类型不同的两根指针能否访问同一对象，则是 [严格别名](/notes/strict-aliasing) 的下一层规则。

- [WG14 N1256（C99 合并技术勘误稿）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：6.7.3 第 5 段（修改真正 const 对象）、脚注 114（存储可由实现安排）；6.7.3.1 第 4–6 段及例 2、例 3（restrict 的正式条件、分离区间和共享只读输入）。
- 同一文档 7.21.2.1、7.21.2.2：`memcpy` 的不重叠前提与 `memmove` 的“如同经临时数组复制”语义。

回到 [1.5 数组与指针](/lessons/1-5) 时，先用 `const` 表达只读接口；只有真正能让所有调用者遵守访问承诺时，再考虑 `restrict`。
