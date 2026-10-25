---
title: 【拓展】strtok为什么不可重入
description: 复现嵌套分词丢失外层位置的故障，用独立游标修复，并分清可重入、线程安全与缓冲区所有权。
sourceKey: 【拓展】strtok为什么不可重入
---

# strtok 为什么不可重入

串口收到一行 `led,on;motor,off`。你先按分号取出每条命令，再按逗号取出名称和参数。第一条正常，第二条却不见了。没有中断，没有另一个线程，输入也没有超长，为什么一个看似普通的库函数就让解析停了？

先把这件事复现，再讨论“不可重入”。关键不是函数是否足够短，而是**两次解析能否各自保存自己的进度**。

## 一、只用单线程，也能丢掉第二条命令

前置是[命令解析器](/lessons/1-9)的原地分词。仓库已有独立 C99 示例，在仓库根目录运行；保留断言，不要定义 `NDEBUG`：

```sh
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O0 examples/notes/c-semantics/strtok-reentrancy.c -o strtok-reentrancy
./strtok-reentrancy
```

Windows 也可以运行生成的 `strtok-reentrancy.exe`。程序没有交互输入，测试中固定使用上述命令串。先预测这段核心调用：

```c
char text[] = "led,on;motor,off";
char *row = strtok(text, ";");
char *name = strtok(row, ",");
char *value = strtok(NULL, ",");
row = strtok(NULL, ";");
```

这是调用片段，不是独立程序；完整测试包含 `<string.h>`、入口以及逐步断言。第三行后名称是 `led`，第四行后参数是 `on`；第五行的 `row` 是空指针，而不是 `motor,off`。这里使用可写数组，所有访问都在有效寿命内，不是在借未定义行为制造故障。

完整测试固定打印：

```text
strtok: rows=1 (outer state lost)
cursor: rows=2 (independent state)
edges: empty repeated delimiters interleaving ok
strtok-reentrancy: ok
```

第一行确认错误写法会丢外层进度，第二行是后文修复后的结果。不要只看程序没有崩溃；逻辑错误完全可能平静地返回一个合法的空指针。

## 二、NULL 不是“继续外层”，只是“继续最近的现场”

`strtok` 的首次调用传入字符串首地址，后续调用以空指针表示继续。它先跳过分隔符，找到词的起点，再寻找下一个分隔符；找到后把它改为字符 `'\0'`，并保存后面的位置，供下次调用使用。

第一次处理 `text` 后，分号已经被改成字符串结束符，外层还想继续处理的位置在 `motor`。可是对 `row` 的下一次非空参数调用，启动的是一轮新的分词。用表追踪：

| 调用 | 本次得到 | 保存的后续位置 |
| --- | --- | --- |
| `strtok(text, ";")` | `led,on` | `motor,off` |
| `strtok(row, ",")` | `led` | `on` |
| `strtok(NULL, ",")` | `on` | 内层已到结尾 |
| `strtok(NULL, ";")` | 空指针 | 不能恢复外层 |

更换分隔符并不能指定“回到哪一层”。标准允许每次调用使用不同分隔符，但没有提供解析会话编号、保存现场或恢复现场的参数。

许多实现用静态指针保存位置，但不必假定所有库都采用同一份进程全局变量；实现可能采用不同的隐藏存储。我们依赖的是标准接口的行为：**调用者没有给两个交错会话各自提供进度的位置**。即使某个库在线程之间隔离状态，同一线程里的嵌套解析仍不能靠这个接口独立继续。

## 三、把进度移到每次解析自己的变量里

下面的 C99 辅助函数采用显式游标。它不是完整命令语言解析器，只提供“跳过连续分隔符，返回非空词”的行为。`strspn` 计算开头连续分隔符的长度；`strcspn` 计算随后连续非分隔符的长度。两个标准函数完成扫描，我们只负责保存位置和放置结束符。

```c
#include <stddef.h>
#include <string.h>

static char *next_token(char **cursor, const char *delimiters)
{
    char *start;
    char *end;

    if (cursor == NULL || *cursor == NULL || delimiters == NULL) {
        return NULL;
    }
    start = *cursor;
    start += strspn(start, delimiters);
    if (*start == '\0') {
        *cursor = NULL;
        return NULL;
    }
    end = start + strcspn(start, delimiters);
    if (*end == '\0') {
        *cursor = NULL;
    } else {
        *end = '\0';
        *cursor = end + 1;
    }
    return start;
}
```

调用者先让游标指向有效的、以零字符结束的可写数组；分隔符字符串必须有效，且不能与被修改的输入重叠。用两份变量分别管理行与字段：

```c
char text[] = "led,on;motor,off";
char *rows = text;
char *row;

while ((row = next_token(&rows, ";")) != NULL) {
    char *fields = row;
    char *name = next_token(&fields, ",");
    char *value = next_token(&fields, ",");
    /* Check name/value, then handle this command. */
    (void)name;
    (void)value;
}
```

内层只更新 `fields`，外层的 `rows` 仍指着下一条命令。完整测试不是忽略字段：它断言两行依次得到 `led/on` 和 `motor/off`，并确认每行没有第三个词。还交错处理两份独立输入，验证左、右游标互不替换。

这里没有使用动态内存。返回值只是借用原数组的一段，不能单独释放。数组离开作用域、被覆盖或被释放后，保存的词指针也失效；若命令要延迟执行，就需要具有足够寿命的自有存储，不能把这些借用指针直接留给下一轮输入。

## 四、可重入与线程安全不要混成一句话

“可重入”关注一个操作尚未完成时，再次进入是否会破坏先前现场；“线程安全”关注按接口约定从多个线程调用时是否仍正确。它们不能仅凭有没有局部变量、有没有锁来判断。

本例中每次 `strtok` 都已经返回，随后才发生下一次调用；严格说复现的是**两个分词会话的交错串扰**，不是在函数执行到一半时真的用中断抢占。这个最小实验已经显示隐藏续读状态的限制，但不能用它冒充线程或中断测试。

逐次给 `strtok` 调用加锁也不能保存两个会话：单次调用结束释放锁，另一个调用仍能替换续读位置。把整段解析串行化又会限制嵌套和回调，不能自动获得“每层独立现场”。

POSIX 的 `strtok_r` 增加调用者提供的状态指针，可以让每条并行处理的字符串使用独立状态；它属于 POSIX，不是 ISO C99 保证存在的函数。采用它仍要保证各调用不竞争修改同一输入缓冲区，不是只换函数名就可以任意共享数据。

C11 的 N1570 明确说 `strtok` 不必避免与其他 `strtok` 调用的数据竞争，这是比 C99 多出的线程语境说明。本文测试没有线程。我们的显式游标函数也不能仅因消除了共享游标，就宣称可从任意中断或信号处理程序调用；库函数调用链、输入所有权、平台约束仍须分别核对。

## 五、状态独立以后，还有三种语义不能忽略

- **输入必须可写，而且确实是字符串。** 把 `"led,on"` 字符串字面量直接传给修改型分词函数不成立；尝试修改字面量属于未定义行为。只有地址却没有结尾零字符的串口接收片段也不满足要求，必须先限定长度并构造合法输入。
- **分隔符是字符集合，不是子串。** `",;"` 表示逗号或分号中的任意一个。`"::"` 也不是只匹配连续两个冒号。按这个接口拆 UTF-8 文本中的多字节分隔符，也不是按完整 Unicode 字符匹配。
- **空字段会丢失。** `a,,b,` 只得到 `a` 和 `b`；空串或全是分隔符的串没有词。空分隔符字符串则不切开非空输入。这些行为都有测试，但它们不适合需要保留空字段或识别引号、转义的协议。

这解释了主线为何采用完整一次扫描：把状态放在调用内部，在返回前准备好所有参数。仍要检查参数数量上限与缓冲区寿命，但不需要把“下次从哪里继续”藏在另一个库函数里。

## 练习：多一份指针就一定修好了吗

你把两轮解析都改成 `next_token`，却把同一个 `cursor` 同时传给外层和内层，会怎样？若输入是 `set,,42`，要保留中间空字段，现有函数能做到吗？

<details><summary>展开答案：独立状态是每个会话各有一份</summary>

只换函数名不够。内层仍会改变同一个游标，外层现场照样丢失。需要像示例一样分别使用 `rows` 与 `fields`；若两个并发调用还会修改同一缓冲区，则还需要所有权或同步约束。

现有函数会跳过连续逗号，只返回 `set`、`42`，无法表示中间缺了一项。应按协议设计保留空字段的扫描规则，例如逐个识别分隔位置并记录零长度字段。引号和转义还需要额外状态，不能把“有两个参数”误当成协议已校验通过。

</details>

## 适用范围与核对依据

本篇适用于可写、零结尾的字节字符串，示例只在 PC 上进行单线程 C99 验证，不是 CSV 解析器，也未进行 RTOS、信号或 MCU 中断实测。

- [WG14 N1256：C99 合并勘误草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：§7.21.5.8p2–7 核对保存位置、改写分隔符及空指针返回；§7.21.5.3、§7.21.5.6 核对两个扫描函数；§6.4.5p6、§7.1.4p4 核对字面量修改与标准库可重入性边界。
- [WG14 N1570：C11 委员会草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)：§7.24.5.8p6 核对数据竞争表述；这一线程说明不倒写成 C99 的原文。
- [POSIX.1-2017，Issue 7：strtok 与 strtok_r](https://pubs.opengroup.org/onlinepubs/9699919799/functions/strtok.html)：DESCRIPTION、APPLICATION USAGE 核对调用者提供的状态、独立状态要求及线程安全约定。以上均已读取对应官方原文。

回到[命令解析器](/lessons/1-9)检查分词状态，再联系[结构体、枚举与 typedef](/lessons/1-6)把状态归入对象。相关分支：[动态内存与碎片](/notes/dynamic-memory)、[const 与 restrict](/notes/const-restrict)、[volatile 与编译器优化](/notes/volatile-optimization)。
