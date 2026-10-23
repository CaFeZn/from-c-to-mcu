# 从 C 到单片机

从第一句输出，到能运行的嵌入式项目。主线使用 VS Code + CMake，板级内容采用 STM32 + CubeMX + HAL。

网站：https://CaFeZn.github.io/from-c-to-mcu/

## 本地开发

需要 Node.js 24 和 pnpm 11.28.5。

```sh
pnpm install
pnpm dev
```

## 检查与发布

```sh
pnpm test
pnpm build
pnpm check:links
pnpm test:e2e
```

Playwright 测试默认访问 `http://127.0.0.1:4318/from-c-to-mcu/`，先运行 `pnpm preview --port 4318`。可用 `BASE_URL` 指定另一个地址。首次运行浏览器测试需要 `pnpm exec playwright install chromium`，或使用本机 Chrome。

推送 main 后，GitHub Actions 构建并发布到 GitHub Pages。部署使用仓库的 github-pages 环境。

## 内容组织

- `docs/lessons/`：33 节主线正文。
- `docs/chapters/`：章节目录。
- `docs/optional/`、`docs/notes/`：可跳过的原理与已有拓展。
- `docs/topics/`：未完成主题的明确状态，不把占位笔记当成完整课程。
- `docs/.vitepress/theme/components/`：构建、位操作、PWM 与中断的交互演示。

演示用于观察状态和理解过程，不是在线 C 编译器，也不是精确 MCU 仿真器。硬件接线、时钟和外设参数仍需按真实设备核对；目前板级示例尚未完成真实硬件验证。

## 从课程笔记更新

```sh
node scripts/import-course.mjs /path/to/单片机C课程
```

导入脚本只读取明确的课程目录和补充视频映射，不读取整个笔记库，不复制内部写作规范、日记、账号信息或第三方字幕。它会更新生成的课程页和导航数据，保留主页、主题与动画组件。后续直接编辑网页课程时，不应再用旧笔记覆盖对应内容。

第三方资料保留原作者与原始链接。教程组织参考了用户认可的现代 C++ 新手起步页面，本站正文和交互组件不搬运该站实现。
