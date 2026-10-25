---
title: 【实践】用Git保存每一次可用的进展
description: 从一个能运行的 hello 工程出发，看清暂存与差异，保存小步提交并按层次安全撤销。
sourceKey: 【实践】用Git保存每一次可用的进展
---

# 用 Git 保存每一次可用的进展

昨天的 `hello` 能运行，今天改了几处却报错了。你记得“大概只改了一行”，目录里却已经有 `hello-old.c`、`hello-final.c` 和 `hello-final2.c`。究竟哪份验证过？恢复它会不会把另一处好改动一起丢掉？

这次不先注册网站，也不先学复杂分支。我们给已经跑通的工程留一个可识别的记录，再只改一句输出，亲眼看清**下一次提交究竟会保存什么**。

> **前置**：完成 [1.0 环境与 hello](/lessons/1-0)。本篇是之后可选的工程习惯，不是跑第一份程序的门槛。
> **环境**：自己的 `c-lab` 练习目录，Git 2.28 或更高，Windows PowerShell；构建仍沿用 1.0 的 gcc、CMake、Ninja。其他终端的 Git 子命令相同，运行程序的路径写法不同。

## 一、先保存一个确实能跑的起点

用 [Git 官方安装入口](https://git-scm.com/downloads) 安装 Git，重开 VS Code 终端后执行 `git --version`。这里只需要命令行工具，不要求 GitHub 账号。

在 VS Code 打开自己的 `c-lab`，终端当前位置也应是这个目录。下面以 1.0 完整做完后的工程为例：有 `src/hello.c`、`src/greet.c`、根目录构建清单和 `.vscode/settings.json`。**不要在下载的课程源码、团队项目或个人资料总目录里照做。** 已经用 Git 管理的工程不需要再次初始化，也不要为练习改名现有分支；不确定时，另建个人练习目录。

先保存所有文件，按 1.0 的 CMake 命令重新构建，分别运行 `hello` 和 `greet`，应看到 `hello, world` 与 `hi, c-lab`。验证的是磁盘上这份源码，不是昨天留下的可执行文件。

然后在工程根目录新建 `.gitignore`：

```text
/build/
/build-*/
```

这两行忽略本目录下的构建目录，例如 `build`、`build-debug`；源码与构建清单仍要保留。不要照抄一个“忽略所有配置文件”的大模板：工程配置往往正是别人复现结果所需的内容。

确认这是尚未初始化的独立练习工程，再执行：

```powershell
git init -b main
git config user.name "Your Name"
git config user.email "you@example.com"
git status --short
```

两项身份请换成自己准备写进提交记录的名字和邮箱；这里没有 `--global`，只配置当前仓库。**它们不是远端登录凭据**，提交公开后邮箱也可能公开。`main` 只是本例初始分支的名字。

此时 `??` 表示文件还没纳入跟踪。选择这一次确实需要的文件：

```powershell
git add .gitignore CMakeLists.txt src/hello.c src/greet.c .vscode/settings.json
git diff --staged
git commit -m "Save working hello and greet examples"
git status --short
git log --oneline -3
```

先读差异再提交：不应出现可执行文件、构建缓存、密码或令牌。这个初始例子没有别的文件时，提交后的短状态为空；日志里有一条带短标识和说明的记录。标识因人而异，不需要和任何截图一致。

这里已经完成一次**本地提交**。断网也能做；没有一行命令把工程上传到互联网。普通仓库的历史保存在 `.git` 中，丢掉它就不能只靠当前源码找回这段历史。

## 二、只改一句话，再看三份内容

打开 `src/hello.c`，把字符串里的 `world` 改成 `c-lab`，其余不动。保存、构建，运行应得到：

```text
hello, c-lab
```

先看这一次修改：

```powershell
git diff -- src/hello.c
```

关键差异应是下面两行，行号等上下文可能不同：

```diff
-    printf("hello, world\n");
+    printf("hello, c-lab\n");
```

`-` 表示旧行，`+` 表示新行。`--` 把选项和文件路径分开，使“只看这个文件”的意图清楚。接着执行：

```powershell
git add src/hello.c
git diff --staged -- src/hello.c
```

现在先别提交。故意把工作文件里的 `c-lab` 再改成 `again` 并保存。猜一猜：立即提交会保存哪一句？

答案不是“最后保存的那句”。此时有三个不同的版本：

| 位置 | 输出字符串 | 谁决定它 |
|---|---|---|
| 最近一次提交 `HEAD` | `hello, world` | 已保存的历史 |
| 暂存区 | `hello, c-lab` | 上一次 `git add` |
| 工作区 | `hello, again` | 刚才的编辑与保存 |

工作区是你正在改的文件；**暂存区是下一次提交准备采用的内容**。`git add` 把当时的内容登记进去，不是给这个文件安装一个“以后所有修改自动入库”的开关。通常的分支状态下，`HEAD` 指向当前分支，而分支指向最近一次提交。

逐条查看，先预测再对照：

```powershell
git status --short
git diff -- src/hello.c
git diff --staged -- src/hello.c
git diff HEAD -- src/hello.c
```

本例短状态应出现 `MM src/hello.c`：第一列表示暂存区相对 `HEAD` 已修改，第二列表示工作区相对暂存区又修改了。三种差异分别是：

- 普通 `diff`：`c-lab` → `again`，还没暂存的部分。
- `diff --staged`：`world` → `c-lab`，这次准备提交的部分。
- `diff HEAD`：`world` → `again`，工作文件相对最近提交的总体变化。

所以 `git diff` 没输出，只能说明它比较的两边没有差异，**不表示没有待提交内容**；新建的未跟踪文件也不能靠普通 `diff` 找全，要一起看 `status`。

本次要保留经过验证的 `c-lab` 版本：在编辑器里把实验用的 `again` 改回 `c-lab`，保存并重新构建运行。再次检查 `git diff` 为空，而 `git diff --staged` 仍有预期改动，再提交：

```powershell
git commit -m "Change hello greeting to c-lab"
git status --short
git log --oneline -3
```

现在有了两个可解释的小步骤，而不是一个夹杂格式化、改名和新功能的“大杂烩”。如果你决定保留 `again`，应先验证它，再重新 `git add`，最后检查暂存差异。**不要用工作区某个版本的运行结果，为另一个暂存版本背书。**

## 三、撤销前先问：改动停在哪一层？

以下是三种独立情形，不是一串必须连续执行的清理命令。先看 `status` 与两种 `diff`；有犹豫就另存备份，别先按“丢弃”。

### 情形 A：只是选错了提交内容，代码还要留着

已经有初始提交后，把某次修改 `add` 了，但暂时不想提交这个文件：

```powershell
git restore --staged -- src/hello.c
```

默认从 `HEAD` 恢复**暂存区**，工作文件不动。预期是暂存差异消失，修改回到未暂存列表；编辑器中的新文字仍在。它不是“撤销编辑”。第一次提交之前还没有可用的 `HEAD`，不要套用这个前提。

### 情形 B：确认不要工作区里尚未暂存的实验

先确认这个文件的未暂存改动全部可以舍弃，再执行：

```powershell
git diff -- src/hello.c
# 阅读上面的差异，确认全部不要后才继续
git restore --worktree -- src/hello.c
```

这个命令默认用**暂存区**覆盖工作文件，不一定回到最近提交。例如第二节暂存是 `c-lab`、工作区是 `again` 时，结果会是 `c-lab`。它会丢掉被覆盖的未暂存修改，Git 不保证能救回从未保存进历史的内容。不要把单文件命令随意换成 `.` 来影响整个目录。

### 情形 C：已经提交的那次改动确认不该保留

在第二节的干净终点，先确认短状态为空、最近一条确实是改问候语的提交，再执行：

```powershell
git status --short
git log --oneline -3
# 仅当 HEAD 正是要撤销的那一条普通提交时执行
git revert --no-edit HEAD
```

它新增一条“反向修改”的提交，不抹掉原记录。本例把 `c-lab` 改回 `world`；重新构建后应再次输出 `hello, world`，日志则保留起点、改动与撤销三条记录。其他人已经拿到原提交时，这通常比重写共享历史更容易协作。

`revert` 不是任意情形的一键时光倒流。撤销旧提交可能与后续修改冲突，合并提交也需要额外判断。出现冲突时先停止，不理解就用 `git revert --abort` 退出这次撤销，再请协作者一起确认。不要把 `reset --hard`、`clean -fd` 或强制推送当作日常修复按钮。

## 四、本地记录和远端同步不是同一件事

做到这里，Git 的核心用途已经成立。远端只是另一个仓库，GitHub、GitLab 是常见托管服务，不是 Git 本身。`origin` 是惯用的远端别名，不是必须存在的特殊服务器。

| 操作 | 作用 | 不会替你做什么 |
|---|---|---|
| 保存文件 | 改工作区 | 不创建提交 |
| `add` / `commit` | 选内容／保存本地历史 | 不自动上传 |
| `fetch` | 取回远端数据、更新相关远端跟踪引用 | 通常不改当前工作文件，也不自动合并 |
| `push` | 向指定远端发送提交并请求更新引用 | 不上传未提交的编辑 |

需要同步时先用 `git remote -v` 核对地址；新建的本地仓库没有配置远端时，无输出是正常的。只在确认仓库归属、访问权限、公开范围和分支名称后再推送。`pull` 还包含取回后的集成步骤，可能采用合并或变基，不等于只下载看看；推送因历史分歧被拒绝时先检查并协调，不要直接加 `--force`。

远端也不是“所有东西的备份”：忽略的文件、未跟踪文件和未提交改动不会随普通推送保存。构建结果可再生成，密钥应另行安全管理；密钥若已提交，新增忽略规则不会把它从历史中删除，应立即撤销或轮换密钥并处理泄漏。

## 五、两处很容易误判的边界

1. **忽略规则不追溯已跟踪文件。** 某个构建产物已经入库，再写 `.gitignore` 不会让 Git 停止跟踪；先识别哪些是生成物，再单独安排清理提交，不要盲删目录。
2. **整页变红变绿，可能是换行或格式变化。** Windows 的 CRLF、LF 与 Git 换行配置可能影响差异。先检查编辑器状态栏与工程约定，不为消除提示随意改全局设置，也别把批量换行转换混进一行功能修复。

每一小步的顺序可以很朴素：**保存 → 构建运行 → 看差异 → 精确暂存 → 再看暂存差异 → 提交**。提交说明解释“改了什么”；涉及故障时再写清原因与验证条件。

## 自己推演一次

`HEAD` 中问候语是 A，你改成 B 后暂存，再改成 C 并保存。此时只想保留 C 的文件内容，但这次不提交它，该用哪种撤销？接着普通差异与暂存差异各是什么？如果已经把 B 推送给同伴，又要取消 B，应该直接强制改写远端吗？

<details>
<summary>展开答案</summary>

使用 `git restore --staged -- src/hello.c`，暂存区回到 A，工作区仍为 C。普通差异是 A → C，暂存差异为空。不要用工作区恢复命令，否则可能丢掉 C。

已共享的 B 通常用新的纠错提交或 `revert` 取消其效果，保留原有历史；先取得干净工作状态并检查冲突，不直接强制覆盖同伴可能依赖的历史。

</details>

## 适用范围与核对依据

本篇面向单人、小型 C 工程的起步工作流，不覆盖团队分支策略、大文件管理、子模块或历史重写。Git 命令按官方说明核对，版本下限 2.28 是为了使用 `git init -b`；文中的 Git 状态与差异按官方语义给出，属于跟做时应核对的预期，不是某次终端会话的实录。配套 C 问候语版本已用 MinGW GCC 6.3.0、CMake 3.28.1、Ninja 1.11.1 独立编译运行验证。

- [Pro Git 第 2 版 §1.6](https://git-scm.com/book/en/v2/Getting-Started-First-Time-Git-Setup)：身份与仓库级配置；[§2.2](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository)：跟踪、暂存、提交。
- [git-init 2.28.0](https://git-scm.com/docs/git-init/2.28.0)：`-b` 初始化分支；[git-diff 的 DESCRIPTION](https://git-scm.com/docs/git-diff)：三种比较对象。
- [git-restore 的 DESCRIPTION / OPTIONS](https://git-scm.com/docs/git-restore)：恢复源与目标；[git-revert](https://git-scm.com/docs/git-revert)：新增逆向提交及冲突中止。
- [gitignore 的 DESCRIPTION / PATTERN FORMAT](https://git-scm.com/docs/gitignore)：忽略规则不影响已跟踪文件；[Pro Git §2.5](https://git-scm.com/book/en/v2/Git-Basics-Working-with-Remotes)：远端、获取与推送。

回到 [1.0 的可运行工程](/lessons/1-0)，或继续 [1.1 改写自己的输出](/lessons/1-1)。配合 [用 Markdown 写清楚工程](/notes/markdown-notes) 留下复现方法；需要理解构建过程再读 [从源文件到可执行程序](/notes/source-to-program)。
