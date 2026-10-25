---
title: 【拓展】设计模式是什么
description: 从采样记录器的失败恢复出发，区分可替换策略、状态机和命令，并用假设备检验设计取舍。
sourceKey: 【拓展】设计模式是什么
---

# 设计模式是什么

记录器刚开始只有“启动”和“保存读数”。后来加了“存储满了停止”“出错后必须复位”，你又添了 `running`、`failed`、`ready` 三个布尔量。某次保存失败后，程序既显示“正在记录”，又拒绝新读数。你修好这里，启动按钮又绕过了故障锁定。问题不是 `if` 不够多，而是“当前到底处于什么情况”没有唯一答案。

另一个麻烦也同时出现：写入函数直接操作硬件，没有板子就没法验证失败恢复。现在先别背模式名称，做两件小事：用**一个状态**表达互斥情况，把**存储动作**留成可替换的接口。等例子跑通，再给这两个改变命名。

## 先把约定画成能手算的表

一个记录器只有 `IDLE`、`RECORDING`、`FAULT` 三种状态，输入事件只有启动、样本和复位。样本范围限定为 0–100；存储接口返回 `true` 表示本次接受，`false` 表示失败。示例存储只有两个槽位，满了就失败。

| 当前状态 | 输入 | 结果 |
| --- | --- | --- |
| IDLE | START | 进入 RECORDING |
| IDLE | SAMPLE | 忽略，不调用存储 |
| RECORDING | 合法 SAMPLE | 调用一次存储；失败则进入 FAULT |
| RECORDING | 超范围 SAMPLE | 拒绝，但不记为存储故障 |
| RECORDING / FAULT | RESET | 回到 IDLE，不清空已存数据 |
| FAULT | START / SAMPLE | 拒绝，必须先 RESET |

未列出的组合保持原状并返回 `false`。这里故意让一个布尔返回值合并“拒绝”和“存储失败”，通过状态区分二者；产品若需对外报告原因，应改成结果枚举，别靠打印字符串猜。

先手工走一遍：启动后送入 `12、34、101、56`，最后会存下哪些数？`101` 是否会用掉第三个槽位？故障后按启动能恢复吗？复位后两个已保存的数还在吗？

## 可直接运行的记录器

这是 C99 的完整例子。假存储只是内存对象，不调用 HAL、不需要 UART，更没有隐藏的全局单例。

```c
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum { IDLE, RECORDING, FAULT } state_t;
typedef enum { START, SAMPLE, RESET } event_t;
typedef bool (*store_fn)(void *ctx, unsigned value);

typedef struct {
    state_t state;
    store_fn store;
    void *ctx;
} recorder_t;

/* The caller owns ctx, and it must outlive every synchronous step call. */
static bool step(recorder_t *r, event_t event, unsigned value)
{
    if (r == NULL || r->store == NULL || r->ctx == NULL) {
        return false;
    }
    switch (r->state) {
    case IDLE:
        if (event == START) {
            r->state = RECORDING;
            return true;
        }
        return false;
    case RECORDING:
        if (event == RESET) {
            r->state = IDLE;
            return true;
        }
        if (event == SAMPLE && value <= 100u) {
            if (!r->store(r->ctx, value)) {
                r->state = FAULT;
                return false;
            }
            return true;
        }
        return false;
    case FAULT:
        if (event == RESET) {
            r->state = IDLE;
            return true;
        }
        return false;
    default:
        return false;
    }
}

typedef struct {
    unsigned data[2];
    size_t used;
} fake_store_t;

static bool save_to_fake(void *ctx, unsigned value)
{
    fake_store_t *s = ctx;
    if (s->used >= sizeof s->data / sizeof s->data[0]) {
        return false;
    }
    s->data[s->used++] = value;
    return true;
}

int main(void)
{
    fake_store_t sink = {{0, 0}, 0};
    recorder_t r = {IDLE, save_to_fake, &sink};
    bool ok = step(&r, SAMPLE, 12u);
    assert(!ok && sink.used == 0);
    ok = step(&r, START, 0u);
    assert(ok && r.state == RECORDING);
    ok = step(&r, SAMPLE, 12u);
    assert(ok);
    ok = step(&r, SAMPLE, 34u);
    assert(ok && sink.used == 2);
    assert(sink.data[0] == 12u && sink.data[1] == 34u);
    ok = step(&r, SAMPLE, 101u);
    assert(!ok && r.state == RECORDING && sink.used == 2);
    ok = step(&r, SAMPLE, 56u);
    assert(!ok && r.state == FAULT && sink.used == 2);
    puts("stored=12,34 state=FAULT");
    ok = step(&r, START, 0u);
    assert(!ok && r.state == FAULT);
    ok = step(&r, RESET, 0u);
    assert(ok && r.state == IDLE && sink.used == 2);
    puts("reset=IDLE used=2");
    puts("pattern checks: OK");
    return 0;
}
```

在仓库根目录运行：

```sh
gcc -std=c99 -O0 -Wall -Wextra -Werror -pedantic examples/notes/c-functions/patterns_demo.c -o patterns_demo
./patterns_demo
```

Windows 命令提示符用 `patterns_demo.exe` 启动。保持断言启用，输出应为：

```text
stored=12,34 state=FAULT
reset=IDLE used=2
pattern checks: OK
```

先于 START 的样本被忽略；`101` 超范围，没调用存储，也没改变状态；`56` 合法，却遇到容量不足，因此触发故障。RESET 只修复控制状态，不等于清空外设或擦除数据，所以 `used` 仍是二。这个差别如果不写进契约，很容易变成“复位按钮到底复位什么”的争论。

本例已用 MinGW GCC 6.3.0、严格 C99 选项编译并实际运行。测试不只看三行字，还断言两个存储值、每次状态和异常事件的副作用。它验证的是同步状态转换，不是 Flash 掉电恢复或真实外设可靠性。

## 现在再给变化起名字

### 一、把“存到哪”换成策略

`step` 不知道目标是内存、文件还是串口，只要求一个符合 `store_fn` 的操作。组装 `recorder_t` 时同时注入函数和上下文；换实现只换这一对。这种把可互换算法或行为隔离在统一接口后面的做法，通常称为**策略模式**。在 C 里一个函数指针就能承载简单策略，不要求造一套类层次。

真正的收益不是少写一个 `switch`，而是把两类变化分开：状态规则改动不必了解外设；存储实现改动不必重新定义启动规则。这里假设备使“第三次写入一定失败”成为可重复条件。真实设备偶发故障很难碰到，假设备却能让每次测试都走错误分支。

签名相同还远远不够。同步接口必须规定是否借用输入、成功表示“已持久化”还是“已入队”、失败是否可能已经写入部分数据。本例的假存储在失败时不改数据。若换成允许部分写入的硬件驱动，上层就必须处理部分成功；一个函数指针不会自动把不同语义变成相同语义。

### 二、让状态成为唯一事实

枚举消除了三个布尔值产生的多种矛盾组合。状态表让评审者不用读全部代码，就能检查“故障时启动是否被阻止”。这叫**有限状态机**：状态集合、事件集合和转换规则都明确。

它不等于已经实现了经典的对象式 State 模式。经典 State 常把每种状态的行为封装成对象，再通过当前状态对象派发；本例用 `switch` 就足够清楚。只有状态越来越多、每种状态有大量独立行为时，才值得进一步拆成状态处理函数或操作表。把三个分支机械改造成十几个接口，可能只是把容易读的逻辑藏起来。

### 三、与命令和观察者有什么不同？

[主线命令表](/lessons/1-7) 将名字映射到动作；若再把动作所需参数保存为一个可排队、可稍后执行的数据对象，就接近**命令模式**的核心意图。策略回答“这件事用哪种方法做”，命令回答“这次要做的请求能否被保存和传递”，状态回答“当前允许怎样响应”。同样含有函数指针，不代表解决的是同一个问题。

若一次状态变化要通知多个订阅者，可以考虑**观察者**式通知。但订阅者的生存期、取消订阅、回调里再次通知以及执行时间都要定义。不是给每个事件挂十个回调就自然获得了可靠架构。尤其不要因为“命令能撤销”就推断任何外设操作都能恢复：已经发出的字节和已经擦掉的 Flash，未必有逆操作。

## 什么时候值得用，什么时候先别用？

先问：已经出现了哪一种变化？测试是否需要替换依赖？新接口能否让错误条件更容易表达？如果答案明确，再选择小到足够的结构；如果只有一句“以后可能会扩展”，先保留直接函数调用通常更容易维护。

本例付出的成本也很具体：多存一个函数指针和上下文；每次合法样本多一层间接调用；调试时要追到具体策略；调用者要保证 `ctx` 的实际类型与存储函数匹配。这些不是灾难，但也不是免费的。两个记录器可以共用代码，**不能误用同一份可变实例数据**，否则一台设备写满了，另一台也跟着失败。

还有两条重要边界：

- 状态机不是锁。当前例子只允许单线程同步调用。主循环和中断若同时执行 `step`，可能在读取状态和写入状态之间互相打断；加 `volatile` 不能解决整个操作的同步。可先将事件送到有明确并发契约的队列，由一个执行上下文处理。
- RESET 不解决底层故障。这里缓冲区仍然是满的，复位、启动、再送样本会立即再次故障。要“恢复后继续”，必须先明确是消费旧数据、换缓冲区还是报告需要人工处理，不能偷偷覆盖两个有效槽位。

设计模式的价值，是给**问题、约束、解决结构和代价**一个共同名字，方便交流与复用经验，而不是给项目增加一份必须集齐的术语清单。

## 练习：不改 step，替换存储

另写一个策略，只累计最多四个样本的总和，并记录个数。它的上下文含 `unsigned sum` 和 `unsigned count`。建立一个新的记录器，启动后送入 `12、34、56`，应得到什么？第五个合法样本到来时应怎样处理？

<details><summary>参考答案：替换的是策略和实例，不是状态规则</summary>

策略可写为：

```c
typedef struct { unsigned sum, count; } sum_store_t;

static bool save_sum(void *ctx, unsigned value)
{
    sum_store_t *s = ctx;
    if (s->count >= 4u || value > 100u) {
        return false;
    }
    s->sum += value;
    ++s->count;
    return true;
}
```

以 `sum_store_t sum = {0u, 0u};` 和 `recorder_t other = {IDLE, save_sum, &sum};` 组装，原样使用 `step`。三次样本后 `sum.sum == 102u`、`sum.count == 3u`，状态仍为 RECORDING。第四个合法样本可接受；第五个返回失败并触发 FAULT，累计值不改变。总数最多四、每个值最多一百，所以和最多四百，连标准允许的最小 `unsigned int` 范围也足够，不用靠回绕解释结果。

`sum` 必须在所有调用期间存活，不能把它放在已返回函数的局部作用域里再保存地址。若你需要处理任意多次样本，须另行定义溢出和容量策略，不能直接删掉四次限制。

</details>

## 适用范围与技术参考

例子是 hosted C99 的同步模型，适合先在 PC 上验证控制逻辑，再把存储接口适配到工程里；它不是可烧录固件。模式不是 C 标准特性，名称也不能代替接口测试。

- Robert Nystrom 原著在线版 [State](https://gameprogrammingpatterns.com/state.html)，已阅读“Finite State Machines to the Rescue”“Enums and Switches”“The State Pattern”：用于区分状态机与对象式 State，不把“有 switch”说成反模式。
- 同作者 [Command](https://gameprogrammingpatterns.com/command.html)，已阅读“Configuring Input”“Directions for Actors”“Undo and Redo”：用于核对请求数据化、生产者与执行者分离及撤销需要保存状态的边界。本文例子为本课程原创，不照搬游戏代码。

回到 [1.7 函数指针与回调](/lessons/1-7) 和 [1.9 阶段项目](/lessons/1-9)；相关分支：[C 语言里的面向对象](/notes/objects-in-c)、[函数指针与虚表](/notes/function-tables)、[C++ 入门与嵌入式 C++](/notes/embedded-cpp)。
