# DNS Relay（计算机网络课程设计）

本目录为 **DNS 中继服务器** 课程设计工程。

| 文档 | 用途 |
|------|------|
| **[docs/design.md](docs/design.md)** | 产品需求（PRD v2.0）— 做什么、怎么验收 |
| **[docs/design-technical.md](docs/design-technical.md)** | 技术设计（TDD v2.0）— 协议、API、数据结构 |
| **[docs/testing-release1.md](docs/testing-release1.md)** | **Release 1 测试流程（透明中继）** |
| **[docs/testing-release2.md](docs/testing-release2.md)** | **Release 2 测试流程（本地 A/NXDOMAIN + 中继）** |
| **[docs/contributing.md](docs/contributing.md)** | **新成员上手与组内协作流程** |
| **[docs/git-convention.md](docs/git-convention.md)** | **Git 提交规范（Conventional Commits）** |
| **[AGENTS.md](AGENTS.md)** | Cursor AI 协作入口 |

**当前路线**：Release 1（仅转发）→ Release 2（课设验收）→ Release 3（LRU 缓存）。

---

## 新成员首次上手（Checklist）

按顺序完成即可开始写代码：

1. [ ] 阅读 [docs/contributing.md](docs/contributing.md) 的 **10 分钟上手路径**
2. [ ] 浏览 [docs/design.md](docs/design.md) 的 **一句话摘要** 与 **发布计划**（弄清 R1/R2/R3）
3. [ ] 阅读 [docs/design-technical.md](docs/design-technical.md) §2 架构、§5 你负责模块的 API
4. [ ] 遵守 [docs/coding-style.md](docs/coding-style.md)（**代码注释仅英文**）
5. [ ] 配置 Git UTF-8：[docs/git-convention.md §3](docs/git-convention.md)（Windows 必做）
6. [ ] 安装 CMake + MinGW，完成下方 **构建** 一次
7. [ ] 与队友确认分工模块，避免同时改同一 `include/*.h`
8. [ ] 用 AI 时：`@AGENTS.md` + [ai-session-template.md](docs/ai-session-template.md)

---

## 文档索引

| 文件 | 用途 |
|------|------|
| [docs/decisions.md](docs/decisions.md) | 架构决策 ADR |
| [docs/test-plan.md](docs/test-plan.md) | 测试与验收勾选 |
| [docs/ai-session-template.md](docs/ai-session-template.md) | 新开 AI 对话模板 |
| [docs/guidance/](docs/guidance/) | 课程 PDF、`dnsrelay-example.txt` |
| [samples/](samples/) | `dnsrelay-minimal.txt` |

目录结构见 [AGENTS.md](AGENTS.md#目录约定)。

---

## 开发环境

| 项 | 选择 |
|----|------|
| 平台 | Windows **x64** |
| 编译器 | MinGW-w64 **GCC** |
| 构建 | **CMake** ≥ 3.16（不使用 Visual Studio） |
| 语言 | **C11**，仅标准库 + Winsock2，无第三方库 |

### 前置条件

- [CMake](https://cmake.org/download/) 与 MinGW-w64 的 `gcc` 在 `PATH` 中（如 MSYS2 UCRT64）

### 构建

```powershell
cd dns_relay
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

| 产物 | 路径 |
|------|------|
| 中继程序 | `build/dnsrelay.exe` |
| 单元测试 | `build/dnsrelay_test.exe` |

### 自动化测试（无需管理员 / 不占 53 端口）

```powershell
.\build\dnsrelay_test.exe
# 或
ctest --test-dir build
```

### 运行与验收（Release 2）

完整步骤（单元测试 + 管理员中继 + 本地应答/拦截验证）：**[docs/testing-release2.md](docs/testing-release2.md)**。

```powershell
# 管理员 PowerShell
.\build\dnsrelay.exe -d 220.181.111.1
nslookup www.baidu.com 127.0.0.1
```

### CLI 帮助

```powershell
.\build\dnsrelay.exe --help
```

会显示参数说明、默认值与命令示例。常用命令：

```powershell
# 默认参数运行（上游=202.106.0.20，配置文件=dnsrelay.txt）
.\build\dnsrelay.exe

# 指定上游 + 一级日志
.\build\dnsrelay.exe -d 220.181.111.1

# 指定上游 + 二级日志 + 指定配置文件
.\build\dnsrelay.exe -dd 220.181.111.232 .\dnsrelay.txt
```

### Release 2 运行步骤（建议）

1. 编译：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

2. 先跑单测（无需管理员）：

```powershell
.\build\dnsrelay_test.exe
```

3. 管理员启动服务：

```powershell
.\build\dnsrelay.exe -d 220.181.111.1
```

4. 另开终端发查询：

```powershell
nslookup test1 127.0.0.1
nslookup test0 127.0.0.1
nslookup www.baidu.com 127.0.0.1
```

5. 停止程序：在服务终端按 `Ctrl+C`。

### 运行产物解读

- `build/dnsrelay.exe`：主程序（二期能力：本地 A、NXDOMAIN、未命中中继）
- `build/dnsrelay_test.exe`：单元测试程序
- 启动日志示例：
  - `dnsrelay: listening UDP/53, upstream ...`：启动成功并监听 53 端口
  - `relay: started debug_level=1`：日志等级生效
- 调试日志（`-d/-dd`）示例：
  - `#N relay -> upstream id A->B client ... qname=...`：向上游转发一次查询
  - `#N relay <- upstream id B->A client ...`：收到上游应答并回发客户端
  - `relay: local A hit ... qname=...`：命中 `dnsrelay.txt`，本地合成 A 应答
  - `relay: local NXDOMAIN block ... qname=...`：命中 `0.0.0.0` 拦截规则
- 结果判定：
  - `test1/test2` 返回配置 IP -> 本地命中正常
  - `test0` 返回 `Non-existent domain` -> 拦截正常
  - 表外域名可解析 -> 中继路径正常

### 运行注意

- 监听 **UDP 53** 需 **管理员** 终端。
- 测试时将本机 DNS 设为 `127.0.0.1`；上游 DNS 用命令行传入（见 PRD FR-09）。

---

## 协作与 Git（摘要）

| 主题 | 文档 |
|------|------|
| 分工、日常流程、AI 用法 | [contributing.md](docs/contributing.md) |
| `feat(scope): subject`、长说明中英双语、UTF-8 | [git-convention.md](docs/git-convention.md) |

**提交示例（短）**：`feat(relay): forward client query with new upstream id`

**AI**：默认不代 `commit`/`push`；结束一轮时请 AI 给出英文 subject 建议，由成员本地提交。

---

## 与 AI 协作（简版）

1. 复制 [docs/ai-session-template.md](docs/ai-session-template.md)，填写本回合目标（如 `P1.2`）。
2. `@dns_relay/AGENTS.md`、`@dns_relay/docs/design-technical.md`。
3. 验收后更新 [docs/test-plan.md](docs/test-plan.md)；按 [git-convention.md](docs/git-convention.md) 提交。

Skills（仓库 `.cursor/skills/`）：`product-manager`（PRD）、`network-engineer`（TDD）。
