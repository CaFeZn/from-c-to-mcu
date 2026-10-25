# c-lab

电脑上的 C99 问候语练习，不连接单片机。

## 环境

- Windows 64 位，gcc、CMake 3.20 或更高、Ninja 已可在终端调用。
- 下列运行命令使用 PowerShell。
- 在本工程根目录执行，目录里应有 CMakeLists.txt。

## 构建与运行

```powershell
cmake -S . -B build -G Ninja
cmake --build build --target hello
.\build\hello.exe
```

输入：不需要键盘输入，也不读取文件。

预期标准输出：

```text
hello, world
```

成功时程序退出码为 0。仅看到构建完成，不等于已经运行程序。

## 文件入口

- [问候语源码](src/hello.c)
- [构建清单](CMakeLists.txt)

## 已知限制

这里只验证 PC 上的标准输出，不涉及串口、下载器或板上 LED。
