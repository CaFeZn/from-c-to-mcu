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
- `docs/.vitepress/theme/components/`：构建、位操作、PWM、中断及 MCU 原理的交互演示。

演示用于观察状态和理解过程，不是在线 C 编译器，也不是精确 MCU 仿真器。硬件接线、时钟和外设参数仍需按真实设备核对；目前板级示例尚未完成真实硬件验证。

## 内容维护

`docs/` 是课程正文的唯一维护位置。直接修改主线、原理文章与组件，再运行检查、构建和发布；不再维护 Obsidian 副本，也不进行双向同步。

新增主题需要同步维护 `docs/.vitepress/routes.json` 中的双链接目标、`course.json` 中的导航摘要，以及 `docs/topics/index.md` 中的状态。文章里的 Vue 组件可以直接使用，无需重新从外部笔记导入。

本机迁移归档位于 `authoring/obsidian-course/`，完整保留旧原稿、待写分支、第三方参考字幕与内部写作资料。`authoring/` 已被 Git 忽略，不参与公开网站，也不上传公共仓库；不要用归档覆盖更新后的 `docs/`。克隆公共仓库后构建网站不依赖这份本地归档。

`scripts/import-course.mjs` 仅保留作历史迁移工具，必须显式提供来源目录；日常写作不运行它。其演示标记解析仍保留测试，避免旧资料迁移时把代码块里的标记错误执行为组件。

第三方资料保留原作者与原始链接。教程组织参考了用户认可的现代 C++ 新手起步页面，本站正文和交互组件不搬运该站实现。
