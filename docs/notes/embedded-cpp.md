---
title: 【拓展】C++入门与嵌入式C++
description: 用无动态分配的总线守卫体验 C++11 的封装与析构，并完成一次真实的 C/C++ 混合编译。
sourceKey: 【拓展】C++入门与嵌入式C++
---

# C++入门与嵌入式C++

你把 SPI 片选拉低，准备发送数据；检查长度时发现参数错误，于是提前 `return`。下次访问另一颗器件却乱了：上一条路径忘了把片选释放。给每个返回点补一句清理，今天能修好，明天多一个分支又可能漏掉。

这不是“换成 C++ 就不会错”，而是 C++ 提供了一种可以让编译器帮忙执行的约定：**某个局部对象存在期间持有资源，正常离开它的作用域时统一清理**。先在 PC 上用一个假总线验证这个约定，再讨论如何接入单片机。本文明确使用 **C++11**，不要求你把已有 C 工程全部改名为 C++ 文件。

## 先做一个有提前返回的实验

我们要检查三个请求：长度零、长度五、长度三。约定合法范围是 1–4。每次调用都暂时“选中设备”，无论成功还是失败，调用结束后都必须释放；只有合法请求增加写入次数。

先预测：连续执行三个请求后，总线应处于什么状态？写入次数是零、一还是三？没有一行手写在 `return -1` 前的释放语句，是否还能满足约定？

为了一起验证与 C 的互操作，长度检查保留在 C 中。共享头文件采用两种编译器都能读懂的条件声明：

```c
#ifndef C_FUNCTIONS_CPP_C_API_H
#define C_FUNCTIONS_CPP_C_API_H
#ifdef __cplusplus
extern "C" {
#endif
int packet_size_valid(unsigned length);
#ifdef __cplusplus
}
#endif
#endif
```

C 实现很小，仍然交给 C 编译器：

```c
#include "cpp_c_api.h"

int packet_size_valid(unsigned length)
{
    return length > 0u && length <= 4u;
}
```

下面的 C++ 程序使用同一个声明。它不分配堆内存，不使用继承或虚函数，只围绕已存在的 `FakeBus` 建立一个短生命周期守卫。

```cpp
#include <cassert>
#include <cstdio>
#include "cpp_c_api.h"

class FakeBus {
public:
    FakeBus() noexcept : selected_(false), writes_(0u) {}
    void select() noexcept { assert(!selected_); selected_ = true; }
    void release() noexcept { selected_ = false; }
    void write() noexcept { assert(selected_); ++writes_; }
    bool selected() const noexcept { return selected_; }
    unsigned writes() const noexcept { return writes_; }
private:
    bool selected_;
    unsigned writes_;
};

class Selection {
public:
    explicit Selection(FakeBus& bus) noexcept : bus_(bus) { bus_.select(); }
    ~Selection() noexcept { bus_.release(); }
    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;
private:
    FakeBus& bus_;
};

static int send_packet(FakeBus& bus, unsigned length)
{
    Selection guard(bus);
    // Deliberately validate after acquisition to exercise early-return cleanup.
    if (!packet_size_valid(length)) {
        return -1;
    }
    bus.write();
    return 0;
}

int main()
{
    FakeBus bus;
    int early = send_packet(bus, 0u);
    assert(early == -1 && !bus.selected() && bus.writes() == 0u);
    int oversized = send_packet(bus, 5u);
    assert(oversized == -1 && !bus.selected());
    int normal = send_packet(bus, 3u);
    assert(normal == 0 && !bus.selected() && bus.writes() == 1u);
    assert(packet_size_valid(4u) == 1);
    std::printf("early=%d oversized=%d normal=%d\n", early, oversized, normal);
    std::printf("selected=%d writes=%u\n", bus.selected() ? 1 : 0, bus.writes());
    return 0;
}
```

实验故意在获得资源后检查参数，让错误路径也经过清理；实际产品中，能在获得资源前验证的条件通常应提前验证。`FakeBus` 的计数仅供这里的三次请求使用，不是永久运行的溢出计数策略。

## 分别编译，最后一起链接

在仓库根目录执行下面三步。第一步绝不能把 C 源也偷偷交给 `g++`，否则就没有检验混合语言边界。

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic -c examples/notes/c-functions/cpp_c_api.c -o cpp_c_api.o
g++ -std=c++11 -Wall -Wextra -Werror -pedantic examples/notes/c-functions/embedded_cpp_demo.cpp cpp_c_api.o -o embedded_cpp_demo
./embedded_cpp_demo
```

Windows 命令提示符用 `embedded_cpp_demo.exe` 启动。预期输出：

```text
early=-1 oversized=-1 normal=0
selected=0 writes=1
```

把 `Selection` 的析构函数暂时改为空函数，再运行，首个失败请求后的断言应失败。恢复析构函数后，两个错误返回和一个正常返回都使片选回到未选中。不要只验证成功路径，因为清理机制最有价值的地方往往在失败分支。

配套示例已用 MinGW GCC/G++ 6.3.0 分别编译 C99 和 C++11，再实际链接运行；另一次添加 `-Os -fno-exceptions -fno-rtti` 也得到同样输出。断言均启用。这只证明这个不抛异常、没有 RTTI 使用的示例适合这些选项，不证明任意 C++ 库都能这样编。

## 五处新语法，都在保护同一件事

1. **`class` 与访问权限。** `FakeBus` 的数据位于 `private`，外部不能直接把写入次数改成负面测试之外的任意状态。`public` 是可以调用的接口。与 C 的“结构体加函数”相比，编译器开始参与访问限制；`struct` 也能有成员函数，只是默认访问权限不同。
2. **构造与初始化。** `FakeBus()` 将两个字段明确初始化。`Selection` 的冒号初始化列表让引用成员绑定到传入总线。成员按在类中声明的顺序初始化，不按初始化列表的书写顺序；因此列表最好照声明顺序写。`explicit` 避免编译器在没有明确要求时把一个总线隐式变成守卫。
3. **引用与成员函数。** `FakeBus&` 引用现有对象，不复制它，也不延长它的生命。成员函数里有隐式的 `this`；读状态的方法末尾加 `const`，表示不经这个对象接口修改普通数据成员。`const` 不是线程同步。
4. **析构。** `~Selection()` 在该自动对象正常离开作用域时执行，包括本例提前返回。这种将资源获取与对象初始化结合、用析构负责释放的方式称为 **RAII**。它也可以管理文件或锁，不仅是内存，不要求先使用 `new`。
5. **禁止复制与不抛异常。** 两个 `= delete` 禁止复制守卫，避免出现两份对象都自认拥有同一次选中操作。这里也没有提供移动操作。`noexcept` 声明不让异常逃出；若实际让异常逃出，会调用终止机制，而不是自动把错误吞掉。这个例子的获取/释放不报告可恢复失败，所以承诺成立。

如果资源获取本来就可能失败，不能简单照搬一个“总会成功”的构造函数。在禁用异常的项目中，常见做法是显式创建函数返回状态，成功后才构造持有有效资源的对象，或者对象明确表示可检查的无效状态。必须统一失败模型，而不是在构造函数里失去报告错误的渠道。

## 为什么 extern C 不能省，也不能乱加？

C++ 支持重载，工具链通常会把参数类型等信息编码进外部符号名。C 源产出的 `packet_size_valid` 和 C++ 默认期待的符号可能不一致。头文件在 C++ 编译时给函数指定 C 语言链接，在 C 编译时则跳过这段语法；`__cplusplus` 用来识别这个差别。

它不是“这里按 C 语法解释”的开关，也不是跨编译器、跨 ABI 的万能兼容层。参数宽度、结构体布局、浮点调用约定和工具链版本仍需匹配。保留 C 驱动时，让驱动继续按 C 编译，给上层提供简单且明确的 C 接口，最后由 C++ 编译器驱动链接以加入所需运行时支持。

不要把 C++ 模板或标准库头文件包进 `extern "C"`；也不能把非静态成员函数指针强转为 C 回调。需要回调时写一个普通适配函数，再用上下文指针找到存活的对象，具体方式与 [C 语言里的面向对象](/notes/objects-in-c) 的显式对象参数类似。

## 到 STM32F407 之前，先做成本清单

C++ 是语言，不是一种必须开堆、必须开 RTOS 的运行环境。本例 `FakeBus bus;` 和 `Selection guard(bus);` 都没有动态分配请求；普通非虚成员函数也不要求虚表。但是“没有使用 `new`”仍不足以证明整个程序不用堆，选择的字符串、容器、I/O 库可能间接分配。应检查最终映射和符号，而不是根据扩展名猜。

常见决策有以下几项：

- **虚调用与模板各有账单。** 虚函数通常借助对象中的表指针和间接调用实现，布局由 ABI 决定；模板让类型在编译时确定，但多个实例可能增加代码体积。是否更小更快，看目标编译产物，不靠口号。
- **异常和 RTTI 是不同机制。** GCC 的 `-fno-exceptions` 禁止依赖 C++ 异常处理，必须与整个调用链的错误策略一致；`-fno-rtti` 限制运行时类型信息，不等于关闭虚函数。别给会抛异常的依赖随手加开关，也不能把“编译通过”当作第三方预编译库兼容的证明。
- **启动代码也要会初始化 C++。** 具有动态初始化的静态对象可能需要启动流程执行构造；不能在时钟和 GPIO 就绪前让全局构造函数访问外设。C++11 的函数内局部静态初始化还可能需要守卫运行时；不要只为体积随便关闭线程安全初始化，尤其要考虑中断重入。
- **资源结束点要符合硬件事实。** 同步 SPI 传输结束后释放片选可以对应局部守卫；若启动 DMA 后马上返回，析构立即释放就可能太早。所有权应持续到完成事件，不能机械把异步资源绑到短函数作用域。

还有一个必须说清的边界：RAII 不是掉电或故障恢复机制。硬复位、死循环、`abort` 等并不保证走普通作用域退出路径；不要依靠析构完成安全断电。样例里的 `assert` 只是开发期验证，发布固件还需要错误处理与失效安全设计。它也不是互斥锁，两个任务同时选同一个总线仍要协调。

## 练习：让守卫更早结束

在同一个函数里先发送一次，再做耗时的后处理；要求后处理开始前总线已经释放。能否手工写 `guard.~Selection();`？再尝试复制守卫，编译器应有什么反应？

<details><summary>参考答案：用作用域表达资源的有效区间</summary>

用一个内层块结束守卫生命：

```cpp
static void send_then_process(FakeBus& bus)
{
    {
        Selection guard(bus);
        bus.write();
    }
    assert(!bus.selected());
    // Perform post-processing after the resource has been released.
}
```

不要手工调用局部守卫析构后还让它自然离开作用域；那会再次进入析构流程，不是本例的正确用法。在块中添加 `Selection copy = guard;` 应因复制构造函数被删除而编译失败。删掉禁用复制不是修复：那会破坏“一次持有对应一次释放”的所有权约定。需要转移所有权时，应另行设计移动语义及移出对象状态，而不是复制引用成员就结束。

</details>

## 适用范围与技术参考

本文是 C++11 入门，不是完整固件移植指南。假总线没有寄存器、时序和真实片选线；PC 验证不代表已烧录 STM32F407，也没有证明第三方 C++ 标准库在目标板上的体积和实时性。

- ISO C++ 网站 FAQ：[Constructors](https://isocpp.org/wiki/faq/ctors) 的初始化列表及顺序；[Destructors](https://isocpp.org/wiki/faq/dtors) 的局部对象析构与不要手工析构；[Exceptions](https://isocpp.org/wiki/faq/exceptions) 的 RAII 资源管理示例。本文只取已读的相关语义，不把通用桌面建议直接当成硬实时要求。
- ISO C++ 网站 FAQ：[How to mix C and C++](https://isocpp.org/wiki/faq/mixing-c-and-cpp)，已核对条件式 `extern "C"`、适配函数和链接驱动建议。
- GCC 6.3.0 手册：[Code Generation Options](https://gcc.gnu.org/onlinedocs/gcc-6.3.0/gcc/Code-Gen-Options.html) 中 `-fexceptions`；[C++ Dialect Options](https://gcc.gnu.org/onlinedocs/gcc-6.3.0/gcc/C_002b_002b-Dialect-Options.html) 中 `-fno-rtti`、`-fno-threadsafe-statics`。选项行为按工具链版本检查，不将 GCC 专有开关写成语言标准。

回到 [1.7 函数指针与回调](/lessons/1-7) 和 [1.8 多文件工程](/lessons/1-8)；相关分支：[C 语言里的面向对象](/notes/objects-in-c)、[函数指针与虚表](/notes/function-tables)、[设计模式是什么](/notes/design-patterns)。
