---
title: 【拓展】函数指针与虚表
description: 用共享只读函数表驱动三个独立实例，并建立签名、上下文和生命周期契约。
sourceKey: 【拓展】函数指针与虚表
---

# 函数指针与虚表

你已经在 PC 上写好了“点亮、读取状态”的上层逻辑。换到低电平点亮的板子后，突然发现各处都要补 `if (board_type)`，连“亮等于 1”都开始混乱。更棘手的是，同一种驱动还有两盏灯：换了实现之后，不能把两盏灯的状态也合并。

[1.7 函数指针与回调](/lessons/1-7)用函数表把“做什么”和“怎么做”分开。本篇把表和**每个实例自己的上下文**配对，做成可替换的接口。这里说的虚表，是程序员手工维护的操作表，不是对某个 C++ 编译器二进制布局的猜测。

## 在代码之前，先定下调用契约

实验有三个逻辑 LED：A、B 用普通状态格记录亮灭，C 用 `pin_high` 模拟低电平点亮。它不是实际 GPIO 寄存器；不用板子也能观察逻辑状态与底层表示的区别。

接口只约定两件事：`set(ctx, on)` 将逻辑状态设为 0 或 1；`read(ctx, out)` 把逻辑状态写到输出参数。返回 1 表示成功，0 表示拒绝；拒绝时不改变实例状态，读取失败不写输出参数。实现不能把 1 解释为“高电平”，否则上层虽然能调用，却得不到同一个接口。

还要约定生命周期与调用方式：调用同步完成、不保存传入的输出指针；状态由调用者拥有，表和上下文必须在每次调用期间有效；同一实例暂不允许并发访问；两个操作都是必需项，不把空函数指针当作“什么也不做”。这些规则不是函数签名能够全部表达的。外部调用者只使用 `led_set`、`led_read` 包装函数；表内函数是实现私有入口，以类型配对正确、参数已检查为前提，不直接接收任意输入。

## 运行三个实例

先预测：给 A 写 1、B 写 0、C 写 1 后，三盏灯的逻辑状态是什么？C 的底层 `pin_high` 又是什么？A 与 B 是否可以使用同一张表？下面是完整 C99 实验。

```c
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    int (*set)(void *ctx, unsigned on);
    int (*read)(const void *ctx, unsigned *out);
} LedOps;

typedef struct {
    const LedOps *ops;
    void *ctx;
} Led;

typedef struct { unsigned on; } Cell;
typedef struct { unsigned pin_high; } ActiveLow;

static int cell_set(void *ctx, unsigned on)
{
    Cell *self = ctx;
    self->on = on;
    return 1;
}

static int cell_read(const void *ctx, unsigned *out)
{
    const Cell *self = ctx;
    *out = self->on;
    return 1;
}

static int low_set(void *ctx, unsigned on)
{
    ActiveLow *self = ctx;
    self->pin_high = 1u - on;
    return 1;
}

static int low_read(const void *ctx, unsigned *out)
{
    const ActiveLow *self = ctx;
    *out = 1u - self->pin_high;
    return 1;
}

static const LedOps cell_ops = { .set = cell_set, .read = cell_read };
static const LedOps low_ops = { .set = low_set, .read = low_read };

static Led cell_bind(Cell *state)
{
    Led result = { &cell_ops, state };
    return result;
}

static Led low_bind(ActiveLow *state)
{
    Led result = { &low_ops, state };
    return result;
}

static int led_set(Led *self, unsigned on)
{
    if (self == NULL || self->ctx == NULL || self->ops == NULL ||
        self->ops->set == NULL || on > 1u) {
        return 0;
    }
    return self->ops->set(self->ctx, on);
}

static int led_read(const Led *self, unsigned *out)
{
    if (self == NULL || self->ctx == NULL || self->ops == NULL ||
        self->ops->read == NULL || out == NULL) {
        return 0;
    }
    return self->ops->read(self->ctx, out);
}

int main(void)
{
    Cell a_state = { 0u }, b_state = { 0u };
    ActiveLow c_state = { 1u };
    Led a = cell_bind(&a_state);
    Led b = cell_bind(&b_state);
    Led c = low_bind(&c_state);
    int r1 = led_set(&a, 1u);
    int r2 = led_set(&b, 0u);
    int r3 = led_set(&c, 1u);
    assert(r1 == 1 && r2 == 1 && r3 == 1);
    unsigned av = 99u, bv = 99u, cv = 99u;
    r1 = led_read(&a, &av);
    r2 = led_read(&b, &bv);
    r3 = led_read(&c, &cv);
    assert(r1 == 1 && r2 == 1 && r3 == 1);
    assert(av == 1u && bv == 0u && cv == 1u && c_state.pin_high == 0u);
    assert(a.ops == b.ops && a.ctx != b.ctx);
    printf("same_ops=%d different_ctx=%d\n", a.ops == b.ops, a.ctx != b.ctx);
    printf("logical=%u,%u,%u raw_C=%u\n", av, bv, cv, c_state.pin_high);

    r1 = led_set(&a, 2u);
    assert(r1 == 0 && a_state.on == 1u);
    r2 = led_read(&a, NULL);
    r3 = led_set(NULL, 1u);
    assert(r2 == 0 && r3 == 0);
    Led missing = { NULL, &a_state };
    r1 = led_set(&missing, 1u);
    assert(r1 == 0);
    const LedOps empty_ops = { NULL, NULL };
    missing.ops = &empty_ops;
    r1 = led_set(&missing, 1u);
    unsigned untouched = 99u;
    r2 = led_read(&missing, &untouched);
    assert(r1 == 0 && r2 == 0 && untouched == 99u);
    missing = cell_bind(NULL);
    r1 = led_set(&missing, 1u);
    assert(r1 == 0);
    puts("invalid requests rejected");
    return 0;
}
```

在仓库根目录使用已配置的 PC 编译器运行，不定义 `NDEBUG`：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic -O0 examples/notes/c-functions/tables_demo.c -o tables_demo.exe
./tables_demo.exe
```

预期输出：

```text
same_ops=1 different_ctx=1
logical=1,0,1 raw_C=0
invalid requests rejected
```

第一行验证 A、B 共用 `cell_ops`，但上下文地址不同。第二行说明 C 的上层仍读到“亮”，底层却是低电平。第三行之前真实检查了非法值 2、空对象、空上下文、缺少表或操作、空输出指针；它不是根据编译器是否报错推断出来的。

## 一次调用经过哪些位置

`led_set(&a, 1u)` 先进入公共包装函数。检查通过后，取出 `a.ops->set`，它是 `cell_set` 的函数指针；再把 `a.ctx` 作为第一个实参交进去。实现将上下文还原为 `Cell *`，修改的正是 `a_state.on`。换成 C，包装函数不变，但表选择 `low_set`，上下文则指向 `ActiveLow` 对象。

可以把关系按名字画出来：

```text
A --ops--> cell_ops --set--> cell_set
  --ctx--> a_state
B --ops--> cell_ops
  --ctx--> b_state
C --ops--> low_ops  --set--> low_set
  --ctx--> c_state
```

因此表描述的是一类实现，状态属于具体实例。`static const LedOps` 使操作表具有整个程序执行期的寿命，并禁止修改表中的函数指针；它不冻结 `ctx` 指向的状态，也不在 C 语言层面保证表一定放进 Flash，实际存储区仍由工具链和链接安排。

`Led` 是一对借用的指针，不拥有表或状态，也不负责释放它们。本例三份状态都在 `main` 中存活，覆盖所有调用。如果执行 `Led another = a;`，只是又得到一个指向同一状态的句柄，不会克隆 A。更不能在工厂函数里创建局部状态，再返回指向它的 `Led`；返回后再调用就越过了状态的生命周期。

## void 指针擦掉信息，不会修正错误

`Cell *self = ctx;` 在 C 中合法，因为绑定时确实把 `Cell *` 转成了 `void *`，现在还原回原来的对象指针类型。只读函数同理还原成 `const Cell *`，保留只读意图。这里没有把函数地址放进 `void *`：ISO C 对对象指针的往返保证，不能直接推广成对象指针与函数指针可随意互换。

`void *` 不携带运行时类型标签。若把 `&cell_ops` 与 `ActiveLow` 的地址手动拼成一对，即使两个结构体恰好都只有一个 `unsigned` 成员，也不能据此当成合法替代。错误类型访问可能违反别名规则，布局更复杂时还会越界。使用 `cell_bind(Cell *)`、`low_bind(ActiveLow *)` 集中配对，是为了让错误更早暴露；它们也没有能力识别任意悬空地址。更强的隔离要在模块边界隐藏句柄构造细节。

## 两类强制转换都救不了契约

第一类错误是把已有的 `int typed_set(Cell *, unsigned)` 强转成表需要的 `int (*)(void *, unsigned)`。这两个函数类型不兼容。对象指针实参可以在一次正常调用中转换，不意味着两种函数指针就能相互替代；通过不兼容类型调用是未定义行为，没有任何固定输出可验证。

正确做法是加一个签名完全匹配的适配函数，在函数体内还原对象指针，再正常调用原函数。本例 `cell_set` 本身就是这种入口。不要用关闭警告或转换函数指针来“省下一层”。

第二类错误是强行去掉表的 `const`，再临时修改表以便“只切换 A”。A 与 B 共享这张表，修改共享表本来就不是单实例切换；对实际定义为 `const` 的对象写入更是未定义行为。需要切换时，应构造另一套有效的“表加上下文”配对，并在所有调用停止后更新句柄；不能只换表却继续交出旧类型的上下文。

另一个边界是防御检查的能力：包装函数能拒绝空指针、空操作和非法取值，却不能证明非空地址仍存活、对象类型正确或并发访问安全。这些仍然是持有者与实现者的责任。

## 手工虚表不等于 C++ 标准布局

这种分发表可以帮助理解动态选择实现，但它不定义 C++ 的对象布局、虚表位置、表项编号、成员函数指针表示或析构行为。不能把一个 C++ 对象强转成这里的 `Led` 使用。C++ 的 standard-layout 还是一个有精确定义的类别，带虚函数的类不满足其中“无虚函数”的条件；“C++ 经常用虚表实现虚调用”和“标准保证某个内存布局”是两件事。

对于 MCU，还要分别评估句柄与状态占用、间接调用成本、共享资源冲突。不能仅看 `set` 很短就保证固定指令数；优化器也可能看穿表并去掉间接调用。本例只模拟状态，不验证 GPIO 电气行为、中断时序或硬件吞吐。

## 练习：安全增加低电平灯 D

增加第四个实例 D，仍用低电平表示，但初始为灭。依次设 C 为亮、D 为灭，再把 D 设为亮。要求不复制 `low_ops`，不让 D 的变化影响 C，指出新状态应由谁拥有。

<details>
<summary>展开答案与检查点</summary>

在 `main` 中建立 `ActiveLow d_state = { 1u };`，通过 `Led d = low_bind(&d_state);` 绑定。调用公共 `led_set` 后分别检查返回值，并用 `led_read` 观察结果。前两次设置后，C、D 的底层值分别为 0、1；D 点亮后变为 0、0。两次观察中 C 的逻辑值一直为 1。

应有 `c.ops == d.ops`，同时 `c.ctx != d.ctx`。状态由调用者 `main` 的局部对象持有；当句柄需要保存到更长寿命的驱动管理器时，状态也必须搬到足够长寿命的存储中，不能只把 `Led` 结构体复制过去。补充 `led_set(&d, 2u)` 被拒绝且状态不变的断言。

</details>

## 适用范围与技术参考

适合有多个实现的驱动接口、测试替身、协议后端；只有单一实现时，普通显式 `self` 函数通常更直接。完整示例已在 PC 的 `-O0`、`-O2` 实跑，另经 Cortex-M4 目标文件编译；这不等于固件链接成功或上板通过。

C 语义核对 WG14 [N1570（C11，2011）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：6.3.2.3 第 1、8 段（对象指针往返与函数指针调用）、6.5 第 6、7 段（有效类型与别名）、6.5.2.2 第 9 段（不兼容函数调用）、6.2.4 第 2 段（生命周期）、6.7.3 第 6 段（修改常量对象）。官方 PDF 本次获取超时，实际读了[可信 HTML 转录的对应条款](https://port70.net/~nsz/c/c11/n1570.html#6.3.2.3)；示例本身遵守 C99。

C++ 对照阅读了在线工作草案的[类性质 `[class.prop]`](https://eel.is/c++draft/class.prop)第 2 段与例 2，以及[虚函数 `[class.virtual]`](https://eel.is/c++draft/class.virtual)第 1 段。它们用于澄清 standard-layout 与动态绑定，不是 C99 例子的运行依赖。

回到主线：[1.7 函数指针与回调](/lessons/1-7)。继续分支：[C语言里的面向对象](/notes/objects-in-c)、[函数指针与间接调用](/notes/function-pointer-calls)、[C++入门与嵌入式C++](/notes/embedded-cpp)。
