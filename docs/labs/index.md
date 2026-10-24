---
title: 动画实验
outline: false
---

# 动画实验

<div class="labs-index"><a href="#build">构建与运行</a><a href="#bits">8 位与溢出</a><a href="#pwm">PWM</a><a href="#interrupt">中断</a></div>

## 构建与运行 {#build}

源码先保存，再构建，运行的是构建得到的程序。这里演示状态变化，不会在浏览器里真的调用编译器。对应 [1.0 环境搭建](/lessons/1-0)。

<ClientOnly><BuildRunLab /></ClientOnly>

## 8 位与溢出 {#bits}

一个 uint8_t 只能容纳 0 到 255。每一位的权重不同，加一、置位和翻转也不是同一种操作。对应 [1.3 整型与位运算](/lessons/1-3)。

<ClientOnly><BitsLab /></ClientOnly>

## PWM：高电平持续多久 {#pwm}

占空比是高电平时间与周期的比例。这里把频率放慢到肉眼能观察的范围，不代表实际电机或 LED 驱动应使用的频率。对应 [5.3 PWM 与输入捕获](/lessons/5-3)。

<ClientOnly><PwmLab /></ClientOnly>

## 中断：先记下来，再处理 {#interrupt}

中断处理尽快返回，把需要慢慢做的业务留给主循环。这里是一次事件的执行顺序示意，不模拟中断优先级、多个事件合并或硬件延迟；共享数据的并发问题继续看 [5.4](/lessons/5-4)。

<ClientOnly><InterruptLab /></ClientOnly>

## 单片机原理

- [内核、总线与 Flash 执行](/notes/mcu-architecture)：分别观察取指、SRAM 读数、GPIO 写入，以及复位后的变量初始化。
- [地址译码与寄存器写入](/notes/memory-mapped-io)：把地址拆成区域、基址和偏移，再跟随 BSRR 请求观察 ODR 和引脚逻辑。
