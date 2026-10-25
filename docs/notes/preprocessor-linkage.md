---
title: "【原理】预处理器与链接"
description: "用分阶段编译和故意漏链接实验，区分头文件声明、符号定义与内部链接。"
sourceKey: "【原理】预处理器与链接"
---

# 预处理器与链接

你已经写了 `#include "clamp.h"`，编辑器也知道函数参数类型，但构建最后仍说找不到 `clamp_value`。于是你把整个函数搬进头文件，原来的错误消失，另一个文件又报重复定义。两次修改都绕开了真正的问题：头文件让编译器看到什么，链接器最终拿到哪些实现，是两个不同的问题。

## 从两个源文件得到一个程序

[完整示例](https://github.com/CaFeZn/from-c-to-mcu/tree/main/examples/notes/build)把接口放在 `include/clamp.h`，实现放在 `src/clamp.c`，调用者放在 `src/main.c`。接口只有下面这些内容：

```c
#ifndef BUILD_NOTES_CLAMP_H
#define BUILD_NOTES_CLAMP_H
extern unsigned clamp_calls;
int clamp_value(int value);
#endif
```

实现文件包含该头文件，还包含定义 `CLAMP_LIMIT 10` 的私有配置头。它唯一地定义 `unsigned clamp_calls = 0;`，每次调用增加计数，再把参数限制到零至十。主程序给定 `-3` 和 `42`，用断言检查 `0`、`10` 与两次调用，并打印结果。

先不用 CMake，在仓库根目录的 PowerShell 手工走一遍。输出全部放入专用产物目录；下面不创建或改动主线工程。

```powershell
$S = "examples/notes/build"
$B = "artifacts/course-completion/build/stages"
New-Item -ItemType Directory -Force $B | Out-Null
gcc -std=c99 -I "$S/include" -E -P "$S/src/clamp.c" -o "$B/clamp.i"
gcc -std=c99 -I "$S/include" -S "$S/src/clamp.c" -o "$B/clamp.s"
gcc -std=c99 -I "$S/include" -c "$S/src/clamp.c" -o "$B/clamp.o"
gcc -std=c99 -I "$S/include" -c "$S/src/main.c" -o "$B/main.o"
gcc "$B/main.o" "$B/clamp.o" -o "$B/manual.exe"
& "$B/manual.exe"
```

应输出 `clamp=0,10 calls=2 mode=1`。本例已在 Windows GCC 6.3.0 实际编译运行；补充核验还启用了 `-Wall -Wextra -Wpedantic -Werror -fno-common`。不同平台的汇编语法和符号修饰可能不同，但输入与结果不应改变。

## 先看预处理后的文本，再谈符号

打开生成的 `clamp.i`，寻找限幅判断。`CLAMP_LIMIT` 已被替换成 `10`；包含进来的函数声明仍在；实现里的函数体也在。`-E` 让 GCC 停在预处理之后，`-P` 去掉便于定位源码的行标记，使这个小实验更容易阅读。

宏处理的是预处理记号，不是 C 对象。宏没有自己的运行期存储，也不因为另一个源文件写过 `#define` 就跨文件共享。每个源文件在自己的包含目录与宏环境下预处理，形成编译器处理的翻译单元。把两份源文件同时交给 `gcc`，也不等于先把它们直接拼成一个大文件。

随后编译器检查类型并生成汇编，汇编器编码指令形成对象文件。对象文件除了机器码，还可能携带“这个名字由别人提供”的符号信息和需要稍后修正的引用。主程序已经知道 `clamp_value` 的函数类型，所以可以生成调用；它此时不需要看到另一个文件里的函数体。最终链接器从输入对象和库中选择定义，安排地址并处理重定位，才得到这里的可执行文件。

`-S` 是“停在汇编输出”，`-c` 是“生成对象而不链接”，不是三个必须手动串接的固定文件名。上面的每条命令都可从源代码重新开始相应流程。平时一条 `gcc main.c clamp.c -o app.exe` 会代你完成多步。

## 故意拿走实现，定位错误发生在哪

保持源码不变，执行：

```powershell
gcc "$B/main.o" -o "$B/missing.exe"
```

预期失败，诊断包含 `undefined reference`，指向函数或计数器。头文件再多包含一次也没有帮助，因为编译已结束，链接输入中仍缺 `clamp.o`。这里可以把问题描述得很准确：“调用者有声明，但整个链接没有找到定义。”

再试把实现对象传两遍：

```powershell
gcc "$B/main.o" "$B/clamp.o" "$B/clamp.o" -o "$B/duplicate.exe"
```

这次预期 `multiple definition`。普通外部函数与初始化对象被定义了两遍；这不是文件搜索路径问题。两个预期失败都经过实际核验。失败命令的输出仅用于诊断，不应继续运行可能遗留的同名旧程序。

## 声明、定义与static各管什么

`extern unsigned clamp_calls;` 在本例中声明一个外部对象，让使用者知道名称与类型，但不在这里分配另一个独立计数器。`unsigned clamp_calls = 0;` 是定义，放在一个实现文件里。函数声明与函数定义也分开；实现文件包含自己的公开头文件，可以让编译器及时发现参数和返回类型不一致。

注意 `extern` 不是“无论怎样都不定义”：文件作用域的 `extern unsigned n = 1;` 带有初始化器，仍是定义。`unsigned n;` 则是暂定定义，而不是推荐放在公共头文件里的纯声明。同一个翻译单元中的暂定定义有标准规定的合并规则，不能据此跨多个源文件重复定义共享对象。旧 GCC 的 common 扩展有时会掩盖这个错误；使用 `-fno-common` 可让这类问题更早暴露。

文件作用域的 `static` 让对象或函数具有内部链接。你把计数器定义为 `static` 后，它只属于当前翻译单元，不能再拿它满足别的翻译单元的外部引用。把 `static unsigned count;` 写进公共头文件，则每个包含它的翻译单元会得到自己的对象，编译可能没报错，计数却彼此不一致。这与函数内部 `static` 对象的生命周期问题相关，但不是同一个观察角度。

## 头文件保护为什么救不了重复定义

包含保护只在**一次预处理的宏环境**中生效。某个源文件重复包含同一头文件时，第一次定义了保护宏，第二次跳过内容，能避免结构体重复定义等问题。换到另一个源文件，宏环境重新开始，头文件内容仍会进入另一个翻译单元。

因此不要在普通公共头里定义共享的 `int count = 0;` 或普通外部函数体，期待 `#ifndef` 让全工程只剩一份。也不要用 `#include "clamp.c"` 代替正确链接：当该实现又被构建系统单独编译时，依然可能重复定义。某些有意设计的单文件合并构建是另外一种工程策略，不是初学多文件项目的通用修复方法。

还有一个危险边界：链接成功不是类型检查的总保证。在两个源文件手写不一致的函数声明，名字可能仍然匹配，但调用约定或值解释已经不兼容。应共享接口头，并让定义方也包含它；不要把链接器当成完整的跨文件 C 类型检查器。

## 练习：两个计数器为什么不是共享状态

假设头文件里写 `static unsigned count;`，文件 A 的函数增加它，文件 B 的主函数打印它。两边都包含保护头且构建成功。B 一定能看到 A 增加后的数值吗？怎样改成真正共享，并保留防重复包含机制？

<details><summary>答案与机制解析</summary>

不能。两份内部链接对象分别属于两个翻译单元，名字相同也不是同一对象。应在头文件中保留包含保护，改为 `extern unsigned count;`；在一个实现文件中写 `unsigned count = 0;`，并确认该实现参与链接。包含保护负责单个翻译单元里的重复文本，外部声明加唯一实现负责跨文件共享，两者互不替代。若本意就是每个模块独立统计，保留私有对象也合理，但应该把定义放在各自实现文件中，让这种隔离明确可见。

</details>

## 依据与下一步

语义以 C99 为基线；弱符号、LTO 和动态装载不属于本篇的普通静态链接实验。

- [WG14 N1256，C99及技术勘误汇总草案](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)：6.2.2 链接、6.9 外部定义、6.9.2 暂定定义、6.10.2 包含。
- [GCC预处理器：Once-Only Headers](https://gcc.gnu.org/onlinedocs/cpp/Once-Only-Headers.html)：包含保护的处理范围。
- [GNU ld：Options](https://sourceware.org/binutils/docs/ld/Options.html)：输入对象与 `-l` 库搜索；静态库还受到链接顺序与按需抽取影响。

回到主线：[1.8 多文件工程](/lessons/1-8)。相邻主题：[CMake如何工作](/notes/cmake-how-it-works)、[可执行文件格式](/notes/executable-formats)、[弱符号回调](/notes/weak-symbols)。
