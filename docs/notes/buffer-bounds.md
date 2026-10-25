---
title: 【原理】缓冲区溢出与边界检查
description: 写出先校验剩余容量、失败不改变状态的追加接口，并识别字符串截断与整数回绕。
sourceKey: 【原理】缓冲区溢出与边界检查
---

# 缓冲区溢出与边界检查

接收缓存有 4 个字节，已经放入 `ABC`。现在收到 `XY`，你会怎么处理？“先复制，再看总长度有没有超”已经太晚；“总长度没超过 4 就行”还漏了一件事：两个无符号长度相加，可能先回绕成一个很小的数。

这类故障未必立即崩溃。写坏的可能是旁边的长度、标志或栈上的其他数据，直到很远的地方才出现异常。本篇不靠故意写穿缓存来制造戏剧效果，而是把**不应发生的写入拦在形成目标位置之前**，用确定的状态变化检验防线。

## 先约定：这是二进制追加，不是字符串拼接

我们需要一个接口，接收目标地址、目标容量、已用长度、源地址和本次长度。它成功返回 1，失败返回 0；失败不能部分复制，也不能增加已用长度。容量与长度都按字节计数，不混用“元素个数”。

接口有不能省略的调用前提：`used` 指向有效的长度对象；目标确有 `cap` 字节可写空间；非空输入确有 `n` 字节可读；长度对象不与本次源、目标数据区域重叠。函数可以检查数字，却不能从裸指针反查真实分配大小，也无法判断一个非空地址是不是悬空指针。允许源、目标数据重叠，所以复制选用 `memmove`。

## 给四字节缓存装一道门

在 [1.0 的练习工程](/lessons/1-0) 中建立一个独立 C99 程序。以下输入已固定，无须键盘。先在纸上记录五次追加后 `used` 应是多少，再运行，保持断言启用。

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int append_bytes(unsigned char *dst, size_t cap, size_t *used,
                        const unsigned char *src, size_t n)
{
    if (used == NULL || *used > cap || n > cap - *used) {
        return 0;
    }
    if (n == 0) {
        return 1;
    }
    if (dst == NULL || src == NULL) {
        return 0;
    }
    memmove(dst + *used, src, n);
    *used += n;
    return 1;
}

int main(void)
{
    unsigned char storage[6] = {0xCC, 0, 0, 0, 0, 0xCC};
    unsigned char *payload = storage + 1;
    size_t used = 0;
    assert(append_bytes(payload, 4, &used, (const unsigned char *)"ABC", 3));
    assert(!append_bytes(payload, 4, &used, (const unsigned char *)"XY", 2));
    assert(used == 3 && payload[3] == 0);
    assert(append_bytes(payload, 4, &used, (const unsigned char *)"Z", 1));
    assert(!append_bytes(payload, 4, &used, payload, SIZE_MAX));
    assert(append_bytes(payload, 4, &used, NULL, 0));
    assert(used == 4 && memcmp(payload, "ABCZ", 4) == 0);
    assert(storage[0] == 0xCC && storage[5] == 0xCC);
    printf("used=%zu payload=%.*s guards=ok\n", used, 4, (const char *)payload);

    size_t invalid = 5;
    assert(!append_bytes(payload, 4, &invalid, payload, 0));
    size_t empty = 0;
    assert(append_bytes(NULL, 0, &empty, NULL, 0));
    assert(!append_bytes(NULL, 1, &empty, payload, 1));
    unsigned char overlap[6] = {'a', 'b', 'c', 'd', 0, 0};
    size_t overlap_used = 2;
    assert(append_bytes(overlap, sizeof overlap, &overlap_used, overlap, 4));
    assert(overlap_used == 6 && memcmp(overlap, "ababcd", 6) == 0);

    char text[4];
    int needed = snprintf(text, sizeof text, "%s", "ABCDE");
    assert(needed == 5 && strcmp(text, "ABC") == 0);
    assert(needed >= 0 && (size_t)needed >= sizeof text);
    printf("snprintf needed=%d stored=%s\n", needed, text);
    return 0;
}
```

预期输出：

```text
used=4 payload=ABCZ guards=ok
snprintf needed=5 stored=ABC
```

较新的 GCC 可能对这里故意安排的 `snprintf` 截断发出 `-Wformat-truncation` 警告，这恰好是实验要检测的条件，不是要求忽略普通业务代码的截断。配套独立构建只对这一项预期告警取消“视为错误”，仍保留告警与运行断言。

实验为有效载荷前后各安排一个 `0xCC` 哨兵。它们仍未改变，说明**这些测试路径**没有写坏哨兵；这不是证明所有越界都能由两个哨兵检测到。真正的安全来自校验与接口前提，而不是“程序最后还活着”。

## 每一步究竟拦住了什么

追加 `ABC` 后，`used = 3`，还剩 1 字节。追加 `XY` 请求 2 字节，函数拒绝，原有内容和长度不变。追加 `Z` 刚好填满，`used = 4`。此时长度为 `SIZE_MAX` 的请求同样应立即拒绝；长度为零的请求不读写数据，因此可以在源指针为 `NULL` 时成功。

第一道条件采用短路求值：

```c
if (used == NULL || *used > cap || n > cap - *used) {
    return 0;
}
```

先拒绝无效长度指针，再拒绝“已用超过容量”的坏状态，最后才计算剩余空间。只有先保证 `*used <= cap`，`cap - *used` 才不会无符号下溢。由 `n <= cap - *used` 可进一步知道成功后的 `*used + n <= cap`，所以最后的加法不会回绕。

相反，`if (*used + n > cap)` 看起来简短，却可能先发生 `size_t` 回绕，导致一个超大请求误过关。无符号回绕是语言规定的算术行为，但在这里是逻辑漏洞，不是可以忽略的实现细节。

零长度分支放在目标地址计算前，因此 `append_bytes(NULL, 0, &empty, NULL, 0)` 不做空指针运算，也不调用复制函数。这是**本接口明确定义**的空输入语义，不是“标准库零长度时一律接受任意指针”。

## 为什么还要测试重叠

把 `abcd` 的前四字节追加到从下标 2 开始的位置，源与目标会重叠。最终应是 `ababcd`。`memmove` 按“仿佛经由临时数组”的语义处理重叠；`memcpy` 没有这个保证，重叠调用是未定义行为。

这里不是建议随意混用缓存，而是把接口允许的行为写清楚。如果业务本来禁止重叠，可改用 `memcpy`，但禁止重叠应成为明确前提。两者都不会检查目标真实容量，也不会替你发现目标已经释放。

还要区分两种“长度错误”：容量不足可以被本函数拒绝；调用者谎报源有十字节、实际只有三字节，则单靠这个签名无法拯救。解析数据包时，可信的“实际收到长度”必须从外围接口一路传进来，不能只相信数据包头里自报的长度。

## 二进制装满，字符串却少一个位置

四个字节装下 `ABCZ` 完全合法，但它没有字符串结束零。不能直接把它交给不带精度的 `%s` 或 `strlen`。实验用 `%.*s` 最多输出四个字符；这适用于演示这四个非零文本字符，不是打印任意二进制数据的通用办法，二进制中可能包含零和不可显示字符。

若要保存最多四个可见字符的 C 字符串，至少需要五个字符空间。字符串容量包含结束零，内容长度不包含结束零；不要把二者混成一个量。

`snprintf` 的试验显示另一种失败：给容量 4 的目标写入 `ABCDE`，实际保存 `ABC` 加结束零，返回 5。返回值是“空间充分时本来要写的字符数”，不是实际保存数。正确判定顺序是：

1. `needed < 0`：格式化发生错误；
2. 否则 `(size_t)needed >= sizeof text`：发生截断；
3. 否则完整保存。

先检查负数再转成无符号长度，避免把错误值当成巨大的正长度。不能见到字符串有结束零就宣称业务成功：文件名、控制指令被截断，同样可能产生错误含义。

## 从单字节推广到元素数组

如果申请 `count` 个结构体，先检查 `count <= SIZE_MAX / sizeof *items`，再做乘法分配。对偏移和长度也一样：先证实 `offset <= total`，再检验 `length <= total - offset`，最后才构造 `base + offset`。

另一个常见入口是有符号长度。负数转成 `size_t` 会变成很大的无符号数；应先验证来源和范围，再转换。边界检查不是给每个表达式套一个 `if`，而是沿数据流维护不变式：**实际容量可信、已用不超过容量、每个计算都不溢出、目标对象仍有效**。

普通 C 程序可以通过编译器告警、静态分析和 AddressSanitizer 等工具补充发现问题，但工具支持依赖平台。F407 裸机并不因越界就必定抛出某种异常：错误地址仍可能是可访问 SRAM。既不能把不崩溃当安全，也不能用“总会 HardFault”代替检查。

## 练习：预留结束零

一个字符串缓存容量 8，当前内容长度 5，不包括结束零。准备追加两个字符，是否装得下？追加三个呢？写出一个不会先溢出的判断顺序，并说明成功后结束零应放在哪里。

<details>
<summary>展开答案</summary>

首先要求容量非零、当前长度小于容量，并确认当前内容确是有效字符串。可追加字符数为 `cap - 1 - used`。这里等于 2，因此两个可以，三个不行。先检查 `cap > 0` 与 `used < cap` 再计算，不写成可能溢出的 `used + n + 1 <= cap`。成功复制后，新长度为 `used + n`，并在这个下标写入结束零；若失败，保持旧内容与旧长度。

</details>

## 核对依据和验证边界

本例是 C99、普通内存、单线程接口，不是完整串口驱动。实际执行了刚好装满、拒绝超长、零长度、坏长度状态、数据重叠和格式化截断的断言；没有故意执行越界，没有真板验证。

[WG14 N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) 第 6.2.5 节规定无符号算术，第 6.5.6 节规定指针边界，第 7.21.6.5 节规定 `snprintf`，第 7.24.2.1–2 节规定 `memcpy`/`memmove`。使用 C11 草案核对 C99 已有机制；本次读取 [N1570 文本转录](https://port70.net/~nsz/c/c11/n1570.html#7.21.6.5)，官方 PDF 请求超时。

回到 [1.5 数组与指针](/lessons/1-5)。继续看 [数组退化与指针算术](/notes/array-pointer-arithmetic)、[野指针与悬空指针](/notes/pointer-lifetime)、[字节序之争](/notes/byte-order)。
