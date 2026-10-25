---
title: 【拓展】状态机设计
description: 从带引号和转义的串口输入构造逐字节状态机，验证分包不变性、超长丢弃和错误恢复。
sourceKey: 【拓展】状态机设计
---

# 状态机设计

命令解析器遇到空格就分词，`label hello` 很好办；用户输入 `label "hello world"`，却被切成三个词。你加一个 `in_quote`，又加一个 `escape`，很快出现“已经出引号但还在转义”“行超长但仍执行尾巴”之类的组合。问题不是少写了一个判断，而是代码没有清楚说明：**已经读到哪里，接下来允许什么。**

[1.9 命令解析器](/lessons/1-9) 暂时没有支持引号。本篇先不重写整套命令行，而是做一个小零件：从连续字节中收取一个带双引号的文本字段。它可以在将来的词法层复用，也能帮助读懂 [6.3 协议解析器](/lessons/6-3)。

## 先定输入规则，再给状态取名字

规则故意写得严格：每行只有一个双引号字段，以 LF 结束；内容是可打印 ASCII，允许空格；反斜线只转义双引号和反斜线自身；引号外不允许空白或垃圾；最多解码出 11 字节，另留一个结尾零。CRLF 应由外层按约定归一化，本例不偷偷忽略 CR。

先预测下面四次输入，注意“输入中的反斜线”和 C 字符串字面量中的转义不是一层：

~~~text
"hello world"<LF>   -> 成功：hello world
"a\"b"<LF>          -> 成功：a"b
"bad\q"<LF>         -> 错误：不支持 \q
"abcdefghijkl"<LF>  -> 错误：12 字节超限
~~~

读同一个双引号，会因前文不同而做不同事情：开头的引号开始字段，内容中的引号结束字段，反斜线后的引号则是普通数据。把这些前文信息压缩成五个状态：等开引号、读内容、等转义字符、等行尾、丢弃坏行。状态是“未来行为不同的历史”，不是给每一行代码配一个枚举值。

| 当前状态 | 收到的事件 | 动作与下一状态 |
| --- | --- | --- |
| OPEN | 双引号 | 清空内容，进入 TEXT |
| TEXT | 普通可打印字节 | 有空间则保存，仍在 TEXT |
| TEXT | 反斜线 | 进入 ESCAPE |
| TEXT | 双引号 | 进入 EOL |
| ESCAPE | 双引号或反斜线 | 保存该字节，回 TEXT |
| EOL | LF | 交出完整字段，回 OPEN |
| 非 EOL | LF | 报一条错误，回 OPEN |
| 任意非法组合或超长 | 非 LF | 进入 DROP，丢到 LF |

这里“有空间”是条件，“保存字节”是动作，“进入 TEXT”是状态转换。名词是从可执行规则里长出来的，而不是先背术语再寻找用途。

## 逐字节推进，不等下一字节

下面是核心 C99 实现，对象在使用前调用 `q_init`，每次调用只处理当前字节。它既不等待串口，也不执行设备命令，适合由主循环取到队列字节后调用。

~~~c
#include <stddef.h>
#include <stdint.h>

typedef enum { Q_OPEN, Q_TEXT, Q_ESCAPE, Q_EOL, Q_DROP } qstate_t;
typedef enum { Q_MORE = 0, Q_DONE = 1, Q_ERROR = -1 } qresult_t;
typedef struct {
    qstate_t state;
    size_t n;
    char text[12];
} quoted_t;

static void q_init(quoted_t *q)
{
    q->state = Q_OPEN;
    q->n = 0;
    q->text[0] = '\0';
}

static void q_append(quoted_t *q, uint8_t b)
{
    if (q->n == sizeof q->text - 1u) {
        q->state = Q_DROP;
        return;
    }
    q->text[q->n++] = (char)b;
    q->text[q->n] = '\0';
}

static qresult_t q_feed(quoted_t *q, uint8_t b)
{
    if (b == '\n') {
        qresult_t result = q->state == Q_EOL ? Q_DONE : Q_ERROR;
        q->state = Q_OPEN;
        return result;
    }
    switch (q->state) {
    case Q_OPEN:
        q->n = 0; q->text[0] = '\0';
        q->state = b == '"' ? Q_TEXT : Q_DROP;
        break;
    case Q_TEXT:
        if (b == '"') q->state = Q_EOL;
        else if (b == '\\') q->state = Q_ESCAPE;
        else if (b < 0x20u || b > 0x7eu) q->state = Q_DROP;
        else q_append(q, b);
        break;
    case Q_ESCAPE:
        if (b != '"' && b != '\\') q->state = Q_DROP;
        else { q->state = Q_TEXT; q_append(q, b); }
        break;
    case Q_EOL:
        q->state = Q_DROP;
        break;
    case Q_DROP:
        break;
    }
    return Q_MORE;
}
~~~

`Q_MORE` 表示还没有一行的最终结果；`Q_ERROR` 只在行尾报一次，不会对坏行的每个字节刷一条日志；`Q_DONE` 才允许读取输出。超长后不能回到 OPEN 重新找引号，否则坏行后半段可能被当作一条新指令。

在同一源文件后加下面的入口，可以直接验证带转义的输入。这里 C 字面量 `"\"a\\\"b\"\n"` 表示线上双引号、a、反斜线、双引号、b、双引号、LF。

~~~c
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    quoted_t q;
    q_init(&q);
    const char input[] = "\"a\\\"b\"\n";
    unsigned completed = 0;
    for (size_t i = 0; i < sizeof input - 1u; ++i) {
        qresult_t r = q_feed(&q, (uint8_t)input[i]);
        assert(r != Q_ERROR);
        if (r == Q_DONE) ++completed;
    }
    assert(completed == 1u && strcmp(q.text, "a\"b") == 0);
    puts(q.text); /* 预期 a"b */
    return 0;
}
~~~

输出缓冲仍然属于对象。接收到下一行字节时会被复用；如果应用要异步保存结果，应复制数据或移交独立缓冲所有权，不能把这个指针长久排队后继续改写同一对象。

## 为什么分成几批输入都应得到同样结果

串口、DMA、USB 或主机读取 API 可能把数据切在任何地方，甚至恰好切在反斜线后。假设第一批是开引号、a、反斜线，此时对象停在 ESCAPE；第二批的第一个双引号就必须作为内容，而不是结束符。

状态不能是接收回调里的局部临时变量，更不能每次收到一批数据就初始化。只在新对象开始、明确重置或协议规定的恢复点初始化。每条输入流持有自己的对象，两路串口各一个；不能共享一个全局 `state` 和行数组。

本组测试对 `"a\"b"<LF>` 的每一个切分位置都做了两批喂入，结果均为一次完成和内容 `a"b`；还交错喂入两个对象，确认它们互不污染。这个性质叫**分包不变性**：没有额外超时事件时，传输层如何分块不改变语法结果。

## 超时也应成为明确的事件

只有收到字节才工作，半条输入可能永远停在 TEXT。是否允许用户慢慢打字，必须由产品规定；交互命令行可允许较长等待，机器协议则常有字节间超时。若需要超时，主循环应定期检查，在非 OPEN 状态超过阈值时报告一次错误并重置，而不是在 `q_feed` 内忙等下一字节。

使用 32 位毫秒计数时，可以用 `(uint32_t)(now - last) >= limit` 判断经过时间，但必须约定检查间隔和真实未观察时间小于计数器完整回绕周期，阈值通常限制在半周期以内。不要改成 `now >= last + limit`，加法回绕会破坏普通大小比较。测试里覆盖了跨零的 9 ms 与 10 ms 边界；它验证算术，不验证系统节拍精度。

同一时刻超时与新字节哪个先处理也要规定。例如先检查超时、重置半帧，再把新字节作为新输入；反过来先更新时间，就可能掩盖超时。状态机的价值不是消灭选择，而是把选择变成可检查的规则。

## 状态机不能替你解决并发

所谓“处理一个事件到完成”，是把当前事件引发的状态修改和有限动作做完，再接受下一个事件，不是在其中长时间轮询硬件。不要在 TEXT 的动作里启动电机后等它转完。长操作应该发出请求，随后由完成、失败或超时事件推动下一步。

同一个对象不应由中断和主循环同时调用。推荐中断只入队，主循环按顺序调用解析器；这与 [6.1 串口命令行](/lessons/6-1) 的分工相同。函数没有动态内存也不等于可重入，回调里递归再次喂入同一个解析器也会打断它的状态约定。

设计复杂时，还要区分“状态”和“数据”。等待第几个负载字节可用一个计数，而不必建十六个几乎相同的状态；只有下一步规则真的不同才拆状态。错误统计、已接收长度、事务号放进同一对象的数据字段，而不是硬塞进枚举名字。

## 练习：坏行之后还能继续吗

输入 `"abcdefghijkL"\n"ok"\n`，缓冲最多保存 11 字节。应产生几次错误、几次完成？如果把超长分支改成直接进入 OPEN，会留下什么漏洞？

<details><summary>展开答案</summary>

第一行的第十二个内容字节 L 导致 DROP，到 LF 才报告一次错误；第二行正常完成，结果是 ok。因此一次错误、一次完成。超长后直接进入 OPEN 会把后续双引号误认为新字段开头，坏行的剩余内容可能被重新解释；丢弃到协议认可的同步点才能防止这个尾部重解释问题。

</details>

## 范围、验证与依据

这是平坦、顺序推进的有限状态机示例，不是完整 shell，也不是支持 Unicode 或所有 C/JSON 转义的词法器。独立 C99 测试已经实跑正常输入、空字段、转义、非法转义、超长、缺引号、闭引号后垃圾、坏行恢复、全部切分位置与两实例交错；未进行真机 UART 接收测试。

参考实际核读的 [W3C SCXML 1.0 Recommendation](https://www.w3.org/TR/scxml/) 第 3.5 节 transition、第 3.13 节转换选择与执行，以及附录 D 事件循环的 run-to-completion 说明。本篇借用其事件、条件与转换的设计思想，并未实现 SCXML 的层次状态、并行状态和内部事件优先级完整语义。

回到 [1.9 命令解析器](/lessons/1-9) 或 [6.3 设计自己的协议](/lessons/6-3)；继续看 [文本协议与二进制协议](/notes/text-binary-protocols)、[环形缓冲区](/notes/ring-buffer)、[HAL 回调的注册方式](/notes/hal-callbacks)。
