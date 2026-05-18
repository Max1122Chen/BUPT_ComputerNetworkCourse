# Git 提交规范（dns_relay）

全组成员与 AI 协作时遵守。格式基于 [Conventional Commits](https://www.conventionalcommits.org/)。

---

## 1. 提交信息结构

### 1.1 简短提交（单行即可说明）

**仅使用英文**，一行写完：

```text
<type>(<scope>): <subject>
```

| 字段 | 说明 |
|------|------|
| `type` | `feat` `fix` `docs` `refactor` `test` `chore` `build` `perf` |
| `scope` | 可选，模块名：`relay` `id_map` `dns_table` `cmake` `docs` |
| `subject` | 祈使句、小写开头、无句号，≤72 字符 |

示例：

```text
feat(relay): add upstream response handler with id restore
fix(id_map): avoid upstream_id collision on wrap
docs: update onboarding in README
chore(build): add CMakeLists for MinGW
```

### 1.2 较长提交（多文件 / 需给组员详细说明）

**第一行（subject）仍用英文**；正文 **中英双语**：

```text
feat(dns_table): load dnsrelay.txt with lowercase normalization

EN: Parse IP-hostname lines, skip comments, later rows override duplicates.
Store hostnames lowercased for case-insensitive lookup per PRD FR-02.

中文：实现配置文件加载；支持 # 注释与空行；重复域名以后者为准；
域名入库前转小写，满足 FR-02 与样例 dnsrelay-example.txt。
```

原则：

- **subject 永远英文**（便于 `git log --oneline` 与工具链）。
- **body 先英文后中文**（或英文摘要段 + 空行 + 中文段），便于不同读者。
- 每条 bullet 说清 **为什么**，不只列文件名。

---

## 2. Type 选用

| type | 何时用 |
|------|--------|
| `feat` | 新功能（含新模块、新命令行行为） |
| `fix` | 修 bug |
| `docs` | 仅文档 |
| `refactor` | 不改行为的重构 |
| `test` | 测试 |
| `build` | CMake / 编译脚本 |
| `chore` | 杂项（gitignore、目录整理） |
| `perf` | 性能优化 |

---

## 3. 中文编码（Windows 必读）

避免 `git log` 乱码或提交信息损坏：

### 3.1 仓库级推荐（每位成员执行一次）

```powershell
git config core.quotepath false
git config i18n.commitEncoding utf-8
git config i18n.logOutputEncoding utf-8
```

PowerShell 建议另设（当前会话或 profile）：

```powershell
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$OutputEncoding = [System.Text.UTF8Encoding]::new()
```

### 3.2 提交时

- **优先**使用 `git commit` 默认编辑器（VS Code / Cursor）编写多行说明，并保存为 **UTF-8**。
- 命令行多行提交（PowerShell）示例：

```powershell
git commit -m "feat(relay): implement select loop" -m "EN: Wire socket recv with id_map expiry.`n`n中文：事件循环集成 select 与 pending 超时清理。"
```

- **避免**在 GBK 代码页控制台下直接输入长中文；易乱码。
- 若仅需英文，单行 `-m "fix(socket): handle WSAStartup failure"` 最省事。

### 3.3 仓库 `.gitattributes`（根目录已建议）

文本文件 `* text=auto eol=lf`，`*.md` `*.txt` 显式 `utf-8`，减少跨平台差异。

---

## 4. 分支与协作（建议）

| 做法 | 说明 |
|------|------|
| `main` | 可演示、可编译；不直接 force push |
| 功能分支 | `feat/r1-relay`、`feat/p2-dns-table` |
| 提交粒度 | 一个逻辑变更一次 commit；对应 test-plan 一项更佳 |
| PR/合并前 | `cmake --build` 通过；更新 `test-plan` 勾选（若适用） |

**AI 默认不代提交**；由成员本地 `git commit` 并遵守本文。

---

## 5. 反例

```text
update                    # 太 vague
修复bug                   # subject 不用中文
feat: add stuff.          # subject 句号 / 不清晰
WIP                       # 避免合入 main（可 local only）
```

---

## 6. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-05-18 | 初版：Conventional Commits + 中英 body + UTF-8 |
