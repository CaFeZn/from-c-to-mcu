---
title: 【拓展】宏的高级用法
description: 用一份命令清单生成枚举与名称表，掌握双层展开、语句宏和可移植的 C99 可变参数宏。
sourceKey: 【拓展】宏的高级用法
---

# 宏的高级用法

> 前置：[预处理与多文件工程](/lessons/1-8)。这里继续解决重复维护问题，不把宏当成另一种万能函数。

## 命令加进去了，日志却叫错名字

你给通信协议新增一个命令，在枚举里登记了编号，却忘记修改另一处名称表。程序照样编译，日志里却出现未知命令；若名称表顺序也错了，甚至会给正确编号打印错误名字。这个问题不是少了一层括号，而是同一份事实被维护了两次。

这次让命令只登记一遍，再分别生成编号和名称表。同时给日志补上统一前缀。先预测两个现象：`CHANNEL` 定义成 `2`，把它转成字符串会得到 `"CHANNEL"` 还是 `"2"`？命令编号是 `1、4、9`，名称表能不能直接按编号下标访问？

## 完整实验：一份清单，两个产物

下面是可直接编译的 C99 程序，不需要输入或开发板。它检查全部已登记编号、三个未知编号，以及函数参数只求值一次的情况。先看清单，再看 `main` 的断言，不必一开始就记住每个宏符号。

```c
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STR_RAW(x) #x
#define STR(x) STR_RAW(x)
#define CAT_RAW(a, b) a ## b
#define CAT(a, b) CAT_RAW(a, b)
#define CHANNEL 2

#define COMMAND_ROWS(X) \
    X(PING, 1)          \
    X(READ, 4)          \
    X(RESET, 9)

#define MAKE_ENUM(name, value) CMD_ ## name = (value),
enum command { COMMAND_ROWS(MAKE_ENUM) };
#undef MAKE_ENUM

typedef struct {
    int code;
    const char *name;
} command_info;

#define MAKE_INFO(name, value) { CMD_ ## name, #name },
static const command_info commands[] = { COMMAND_ROWS(MAKE_INFO) };
#undef MAKE_INFO

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define TRACE(...) do {           \
    fputs("[trace] ", stdout);    \
    printf(__VA_ARGS__);           \
    putchar('\n');                \
} while (0)

static const char *command_name(int code)
{
    for (size_t i = 0; i < COUNT_OF(commands); ++i) {
        if (commands[i].code == code) {
            return commands[i].name;
        }
    }
    return "UNKNOWN";
}

static inline int max_int(int a, int b)
{
    return a > b ? a : b;
}

int main(void)
{
    int CAT(channel_, CHANNEL) = 7;
    assert(strcmp(STR_RAW(CHANNEL), "CHANNEL") == 0);
    assert(strcmp(STR(CHANNEL), "2") == 0);
    assert(channel_2 == 7);
    printf("raw=%s expanded=%s channel=%d\n",
           STR_RAW(CHANNEL), STR(CHANNEL), channel_2);

    assert(COUNT_OF(commands) == 3);
    assert(CMD_PING == 1 && CMD_READ == 4 && CMD_RESET == 9);
    assert(strcmp(command_name(1), "PING") == 0);
    assert(strcmp(command_name(4), "READ") == 0);
    assert(strcmp(command_name(9), "RESET") == 0);
    assert(strcmp(command_name(-1), "UNKNOWN") == 0);
    assert(strcmp(command_name(0), "UNKNOWN") == 0);
    assert(strcmp(command_name(10), "UNKNOWN") == 0);
    printf("names=%s,%s,%s missing=%s\n", command_name(1),
           command_name(4), command_name(9), command_name(0));

    int n = 3;
    int best = max_int(n++, 0);
    assert(best == 3 && n == 4);
    printf("max=%d next=%d\n", best, n);

    if (channel_2 == 7)
        TRACE("ready");
    else
        TRACE("unexpected");
    TRACE("count=%u", (unsigned)COUNT_OF(commands));
    return 0;
}
```

在仓库根目录执行下面两行命令，前提是 GCC 已加入 `PATH`。保持断言开启：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic-errors -O0 examples/notes/c-semantics/advanced-macros.c -o artifacts/course-completion/c-semantics/advanced-macros.exe
./artifacts/course-completion/c-semantics/advanced-macros.exe
```

固定输出是：

```text
raw=CHANNEL expanded=2 channel=7
names=PING,READ,RESET missing=UNKNOWN
max=3 next=4
[trace] ready
[trace] count=3
```

再把优化级别改成 `-O2` 重编译，输出应不变。这里观察到的是标准 C99 宏展开和普通函数行为，不是某个编译器专有的“泛型技巧”。

## 字符串化和拼接，为什么各写两层

`#x` 把实参的预处理记号拼写变成字符串，称为**字符串化**。这个位置不会先展开实参里的宏，所以 `STR_RAW(CHANNEL)` 得到 `"CHANNEL"`。`STR` 外层没有直接使用 `#`，参数先展开成 `2`，随后送到内层字符串化，才得到 `"2"`。两层不是固定仪式，而是分别完成“先展开”和“再取拼写”。

字符串化也不是完整保存源文件文本。首尾空白会被去掉，中间连续空白会归成一个空格。`STR(1 + 2)` 得到的是表达式的拼写 `"1 + 2"`，不会做普通宏参数的算术求值；预处理条件里的 `#if` 则会求值规定的整数常量表达式，这是另一条规则。

`##` 拼接相邻的两个预处理记号，称为**记号拼接**。它附近的参数同样不会先做通常的展开。直接写 `CAT_RAW(channel_, CHANNEL)` 会拼成 `channel_CHANNEL`；经过外层 `CAT`，`CHANNEL` 先变成 `2`，最终生成变量名 `channel_2`。它不是运行时字符串拼接，更不能用运行时变量的数值制造新变量名。拼接结果必须是合法的预处理记号，不能把任意两段文字硬连起来。

## 清单怎样同时变成枚举和表

`COMMAND_ROWS(X)` 暂不决定每一行的形状，只把每行的名字与编号交给传入的宏。第一次让 `X` 代表 `MAKE_ENUM`，每行变成枚举项；第二次代表 `MAKE_INFO`，每行变成一个结构体初始化项。这种用同一清单反复生成代码的写法通常叫 **X-macro**。

用下面的命令只做预处理，查找 `enum command` 和 `commands`，不要把标准头文件展开的内容当作新增业务代码：

```sh
gcc -std=c99 -E -P examples/notes/c-semantics/advanced-macros.c
```

关键展开结果如下；换行和空格可能不同：

```c
enum command { CMD_PING = (1), CMD_READ = (4), CMD_RESET = (9), };
static const command_info commands[] = {
    { CMD_PING, "PING" }, { CMD_READ, "READ" }, { CMD_RESET, "RESET" },
};
```

最终交给编译器的仍是普通 C。条目里放的是专用标识符与整数常量，不是函数调用，也应避免和已有宏重名。两个临时生成宏用完立即 `#undef`，以免影响后文；宏的有效范围不会像局部变量那样自动在右花括号处结束。

编号有空洞，所以 `commands[CMD_RESET]` 会越界。这里逐项比较编号，找到才返回名称；未知值返回固定字符串，不把协议编号误当数组位置。协议编号也显式写出，避免日后在清单中插入一行，就悄悄改变后续命令的线上编码。

## 日志宏为什么套一层假循环

`TRACE` 要调用三次输出函数，却希望调用者把它当成一条语句，放进 `if ... else ...` 也不破坏结构。单独写 `{ ... }` 还不够：调用后的分号会成为额外空语句，可能把 `else` 和对应的 `if` 隔开。

`do { ... } while (0)` 本身是一条循环语句，循环体执行一次，条件为零就结束；调用处的分号正好结束这条语句。宏定义本身不附加末尾分号。这个包装只解决语法组合，不自动解决参数多次求值，也不提供多线程日志原子性。不要在其中放一个想控制调用者外层循环的 `break` 或 `continue`，它们会针对宏自己包出的这层循环。

`...` 与 `__VA_ARGS__` 是 C99 可变参数宏的标准写法。示例把整个参数列表交给 `printf`，因此 `TRACE("ready")` 已经有一个实际参数，不需要额外补逗号。反过来，若定义 `LOG(fmt, ...)` 并展开成 `printf(fmt, __VA_ARGS__)`，在 C99 下不能省略可变部分；即使传了空的可变实参，也会留下不合法的尾随逗号。

网上常见的 `, ##__VA_ARGS__` 吞逗号是 GNU 扩展，不属于本篇 C99 接口；`__VA_OPT__` 也不属于 C99。日志格式应由程序控制，输出外部字符串时用 `TRACE("%s", text)`，不要把外部文本直接当格式串。

## 宏没有替你处理的边界

第一，给实参加括号只能保护表达式分组。`#define SQUARE(x) ((x) * (x))` 仍会复制实参；传入 `i++` 会造成未定义行为，不要运行来猜平方值。本例用 `max_int(n++, 0)` 展示函数只求值一次，但同时向两个实参传 `n++` 仍不合法。计算适合用有明确类型的函数，宏留给拼写生成或条件编译。

第二，`COUNT_OF` 只接受真正的数组。示例操作的是当前作用域的固定数组；把函数形参里的指针交给它，得到的不是元素数。宏没有参数类型声明，不能把“看起来像数组”当作检查已经通过。

第三，清单能消除同步维护，但不会自动检查编号唯一性。若两行用同一个数字，枚举未必报错，线性查找只会返回先遇到的一项。新增条目时仍要补上对应编号、名称与未知值测试；别让生成代码代替协议设计。

## 练习：增加命令，再关闭日志

1. 增加 `STOP`，协议编号为 `7`。除了测试期望，业务声明和名称表能否都只靠修改清单生成？
2. 单独考虑 `int count = 0; TRACE("count=%d", count++);`。若关闭日志时把宏定义成 `#define TRACE(...) ((void)0)`，`count` 会不会递增？如果这次递增是业务逻辑，该怎样摆放？

<details>
<summary>展开答案与检查点</summary>

第一题把清单改为下面四行，记得原来最后一行现在也需要续行反斜杠：

```c
#define COMMAND_ROWS(X) \
    X(PING, 1)          \
    X(READ, 4)          \
    X(RESET, 9)         \
    X(STOP, 7)
```

枚举和名称表都不手改。把条数断言的期望从 `3` 改为 `4`，补上 `assert(CMD_STOP == 7)` 与 `assert(strcmp(command_name(7), "STOP") == 0)`。原有三个名称仍应匹配，最后一行日志变为 `count=4`。测试保留独立的期望，才能发现清单填错。

第二题不会递增。关闭版宏没有在替换结果中使用实参，`count++` 不进入运行时表达式。若递增必须发生，就先单独执行 `++count;`，再把值用于可关闭的日志；不要让功能依赖日志开关。

</details>

## 适用范围与依据

示例是单线程宿主 C99 程序，日志走标准输出，不是 UART 驱动，也未声称上板验证。清单方式适合规模有限、结构一致的命令或状态定义；复杂运行时策略仍由函数处理。

- [WG14 N1256，C99 合并 TC1–TC3 的委员会草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：6.10.3 第 4、10–12 段规定宏参数与可变参数；6.10.3.1–6.10.3.5 规定参数替换、字符串化、拼接、再次扫描和作用范围；6.10.1 第 1、4 段说明条件表达式求值；6.8.5、6.8.5.2 规定 `do` 语句的语法和条件检查位置。
- [GCC 预处理器手册 3.4 Stringizing](https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html)：双层字符串化与空白处理的官方解释。
- [GCC 预处理器手册 3.6 Variadic Macros](https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html)：区分 C99 可变参数形式与 GNU 扩展；本文没有采用扩展。

回到[预处理与多文件工程](/lessons/1-8)，再看日志开关的展开结果；继续读[参数求值顺序与未定义行为](/notes/evaluation-order)，区分“每个实参一次”和“实参彼此有顺序”；[volatile 与编译器优化](/notes/volatile-optimization)会解释为什么重复读取外设也不能随意交给宏。
