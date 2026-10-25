---
title: 【原理】C语言里的面向对象
description: 用两个独立计数器理解显式 self、状态所有权、值复制与对象生命周期。
sourceKey: 【原理】C语言里的面向对象
---

# C语言里的面向对象

你给一个按键写了计数函数，内部放着一个 `static unsigned count`，每按一次就累加。一个按键时很好用；接上第二个按键，两路计数却混在了一起。把函数复制成左右两份能暂时救急，但第三路又要复制。问题不是函数不够多，而是“正在修改哪一份状态”没有成为参数。

[1.4 函数与作用域](/lessons/1-4)已经解决了“同一段逻辑只写一次”。这次继续问：**同一段逻辑怎样服务多个互不干扰的实例？** 先不用继承、虚函数，也不需要动态内存。

## 先做出两个互不干扰的计数器

设定规则：计数器从零开始，各有一个上限；增加后若会超过上限，就拒绝整个操作，原值不变。A 的上限是 5，B 的上限是 10。顺序输入 A 加 3、B 加 8、A 再加 4，先预测两个最终值，以及三次操作的成功标志。

完整实验如下。在仓库根目录运行后面的命令即可，不依赖开发板。

```c
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    unsigned value;
    unsigned limit;
} Counter;

static Counter counter_make(unsigned limit)
{
    Counter result = { 0u, limit };
    return result;
}

/* A failed update leaves the object unchanged. */
static int counter_add(Counter *self, unsigned amount)
{
    if (self == NULL || self->value > self->limit ||
        amount > self->limit - self->value) {
        return 0;
    }
    self->value += amount;
    return 1;
}

/* Precondition: self points to a live, initialized Counter. */
static unsigned counter_value(const Counter *self)
{
    return self->value;
}

int main(void)
{
    Counter a = counter_make(5u);
    Counter b = counter_make(10u);
    int r1 = counter_add(&a, 3u);
    int r2 = counter_add(&b, 8u);
    int r3 = counter_add(&a, 4u);
    assert(r1 == 1 && r2 == 1 && r3 == 0);
    assert(counter_value(&a) == 3u && counter_value(&b) == 8u);
    printf("A=%u B=%u accepted=%d,%d,%d\n",
           counter_value(&a), counter_value(&b), r1, r2, r3);

    Counter snapshot = a;
    Counter *alias = &a;
    r1 = counter_add(&a, 1u);
    r2 = counter_add(alias, 1u);
    assert(r1 == 1 && r2 == 1);
    assert(a.value == 5u && b.value == 8u && snapshot.value == 3u);
    printf("A=%u snapshot=%u alias=%u\n",
           counter_value(&a), counter_value(&snapshot), counter_value(alias));

    Counter edge = counter_make(UINT_MAX);
    r1 = counter_add(&edge, UINT_MAX);
    r2 = counter_add(&edge, 1u);
    r3 = counter_add(&edge, 0u);
    assert(r1 == 1 && r2 == 0 && r3 == 1);
    assert(edge.value == UINT_MAX);
    r1 = counter_add(NULL, 1u);
    assert(r1 == 0);
    Counter zero = counter_make(0u);
    r1 = counter_add(&zero, 0u);
    r2 = counter_add(&zero, 1u);
    assert(r1 == 1 && r2 == 0 && zero.value == 0u);
    puts("boundary checks passed");
    return 0;
}
```

已配置课程 PC 编译器时，在仓库根目录执行；Windows 之外可去掉可执行文件的 `.exe` 后缀。不要定义 `NDEBUG`，这样断言会参与验证。

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic -O0 examples/notes/c-functions/objects_demo.c -o objects_demo.exe
./objects_demo.exe
```

预期输出是：

```text
A=3 B=8 accepted=1,1,0
A=5 snapshot=3 alias=5
boundary checks passed
```

A 的第三次请求被完整拒绝，所以不是 5，更不是 7。第二行来自两次后续加一，暂时不要忽略 `snapshot` 和 `alias` 的区别。最后一行表明上限为零、空指针、最大无符号数这些边界也经过了断言，而不是仅打印“测试成功”。

## 显式 self 究竟多了什么

`Counter` 把属于同一个计数器的 `value` 与 `limit` 放在一起。`counter_add(&a, 3u)` 中的第一个实参回答“改谁”，第二个回答“改多少”。函数内部的 `self` 只是普通形参名，不是 C 关键字；写成 `counter` 也一样。它接收对象地址，再通过 `->` 修改调用者持有的那份状态。

C 仍然按值传参：这里复制的是地址值，而不是整个结构体。两个调用共享同一份函数代码，却分别访问 A、B 的存储。这就是把“行为”和“每个实例的状态”联系起来的最小做法，不需要每个实例存一份函数体，也不要求任何函数指针。

`counter_value` 使用 `const Counter *`，表示它不通过这个指针修改对象。这不是“对象永远不变”：随后 `counter_add` 仍可通过可写指针修改 A。文件作用域函数前的 `static` 则限制函数名的链接可见性，与函数内部那个会被所有调用共享的静态计数变量不是一回事。

## 封装首先是维护约束

这份接口的核心约束是 `0 <= value <= limit`。创建函数建立它，增加函数保持它，查询函数只观察它。先检查 `amount > limit - value`，再做加法，避免先算 `value + amount` 后溢出回绕，使超限检查失效。减法之前又检查 `value > limit`，所以即使外部误改了字段，也不会在这一步下溢。

`counter_add` 对空指针有明确失败返回；`counter_value` 的前提则是非空、仍存活、已初始化的对象。并非每个接口都必须防守所有错误，但前提与错误策略必须写清楚。空指针检查也不能检查任意悬空地址是否有效。

这时可以给已经做到的事命名：把状态和操作组织成可独立使用的实例，是一种面向对象的组织方式；用少数入口维护状态约束，是封装。C 不提供 `private`，本例公开了字段，调用者仍能直接写坏它。工程中可把操作放进实现文件，把本例的匿名结构体改为带标签的 `struct Counter`，只在公开头文件中声明 `typedef struct Counter Counter;`，在实现文件保留完整定义，以隐藏具体字段。

不过隐藏布局有代价：调用者不能再凭不完整类型在栈上直接声明实例。需要模块提供静态对象池、明确的分配接口，或选择公开布局但约定只经 API 修改。**封装不等于必须上堆，也不等于把所有字段机械地改成 getter/setter。** 先根据实例数量和内存所有权选择方案。

## 谁拥有状态，谁决定它活多久

本例的 A、B 由 `main` 创建和拥有，方法只在调用期间借用地址，不保存地址，也不释放对象。它们的自动存储期覆盖所有这些调用；离开所在块之后，不能再使用指向它们的指针访问状态。

`counter_make` 返回的是结构体的值，不是其局部变量的地址。值被交给调用者后，调用者持有自己的对象，所以这是安全的。相反，返回局部 `Counter` 的地址，再在函数返回后调用方法，会访问生命周期已经结束的对象，属于未定义行为，不能靠“这次仍打印正确”验收。

观察第二行：`Counter snapshot = a;` 复制了两个整数成员，所以快照保持 3；`Counter *alias = &a;` 只复制地址，`alias` 与 `&a` 指向同一个实例，所以通过它加一仍会改变 A。现在结构体只有整数，值复制适合充当快照。将来若加入缓冲区指针，结构体赋值只会复制指针，不会深拷贝缓冲区，更不会自动决定谁负责释放。对象一旦管理外部资源，就要另行定义能否复制、如何转交所有权以及如何销毁。

## 三个容易把设计弄坏的改动

1. **把实例状态移回方法里的 `static`。** 它确实活得久，却重新变成所有调用共享的一份状态。多个实例要独立，状态就必须来自对应的 `self`；共享统计量要明确标成共享状态。
2. **认为封装后自动线程安全。** 不同实例可以互不干扰，同一个实例被中断与主循环同时修改却仍有冲突。`counter_add` 包含读取、判断和写回，并非不可分割操作；`volatile` 不能把它变成互斥。上板前要由调度模型或临界区另行保证访问秩序。
3. **以为对象一定要带函数表。** 只有一种实现时，直接调用这些函数已经足够。等上层确实需要同一接口切换实现，再引入[函数指针与虚表](/notes/function-tables)，不要为了“像 C++”增加间接层。

## 练习：清零谁，快照要不要跟着变

在现有程序中增加 `counter_reset(Counter *self)`：空指针返回 0；合法对象清零并返回 1。对第二行输出之后的 A 调用它，再观察 A、B、快照和别名。写下预期值，并说明为什么不能通过把函数内计数器设成 `static` 来实现这个需求。

<details>
<summary>展开答案与检查点</summary>

```c
static int counter_reset(Counter *self)
{
    if (self == NULL) {
        return 0;
    }
    self->value = 0u;
    return 1;
}
```

调用 `counter_reset(&a)` 后，A 是 0、B 是 8、快照是 3，`counter_value(alias)` 是 0。别名观察的是 A；快照是另一份整数状态。该函数不改变上限，所以仍满足原约束。方法内部的静态变量既不能表示“这次清零 A”，也不会自动与 `a.value` 建立关系。将调用结果保存到变量后再 `assert`，并另外测试空指针返回 0。

</details>

## 适用范围与技术参考

这种写法适合计数器、滤波器状态、协议解析器和设备句柄。它没有自动构造、析构或继承；这些不是写出可靠 C 模块的前提。本例是 C99，在 PC 上实跑两种优化级别，并用 Cortex-M4 编译器生成目标文件；未链接完整固件，也未做真机或并发测试。

语义依据为 WG14 [N1570（2011 年 C11 委员会草案）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：6.2.4 第 2、3、5、6 段核对生命周期，6.5.2.2 第 4 段核对参数赋值，6.5.16.1 核对结构体赋值，6.7.3 核对 `const`。官方 PDF 本次获取超时，实际阅读了[可信 HTML 转录](https://port70.net/~nsz/c/c11/n1570.html#6.2.4)对应条款；这里只使用同样适用于 C99 的规则，不使用 C11 线程或原子特性。

回到主线：[1.4 函数与作用域](/lessons/1-4)、[1.7 函数指针与回调](/lessons/1-7)。继续分支：[函数指针与虚表](/notes/function-tables)、[设计模式是什么](/notes/design-patterns)。
