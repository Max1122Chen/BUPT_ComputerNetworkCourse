# DNS Relay — AI 协作说明

**人类协作者请先读**：[docs/contributing.md](./docs/contributing.md)（上手路径、分工、日常流程）。  
**Git 提交**：[docs/git-convention.md](./docs/git-convention.md)。

本文件是 **Cursor Agent 在 `dns_relay/` 下的入口**。

---

## 项目状态

| 项 | 内容 |
|----|------|
| 需求 | [docs/design.md](./docs/design.md) PRD v2.0 |
| 技术设计 | [docs/design-technical.md](./docs/design-technical.md) TDD v2.0 |
| 决策 | [decisions.md](./docs/decisions.md) ADR-000～009 |
| 代码风格 | [coding-style.md](./docs/coding-style.md) — **注释仅英文** |
| 当前焦点 | **Release 1**（透明中继，不读 `dnsrelay.txt`） |

Skills：`.cursor/skills/product-manager/`（PRD）、`.cursor/skills/network-engineer/`（TDD）。

**勿**擅自修改已定稿 PRD/ADR；变更须组内确认并升版本号。

---

## 新成员 / 新会话快速清单

1. `contributing.md` → 10 分钟路径  
2. PRD 发布计划 + TDD §5 负责模块 API  
3. `coding-style.md` + `git-convention.md`（UTF-8）  
4. 会话： [ai-session-template.md](./docs/ai-session-template.md) + `@AGENTS.md`  

---

## 目录约定

```text
dns_relay/
  AGENTS.md              ← 本文件
  README.md              ← 人读：上手 checklist、构建
  docs/
    design.md            ← PRD
    design-technical.md  ← TDD
    contributing.md      ← 组内协作
    git-convention.md    ← 提交规范
    coding-style.md
    test-plan.md
    ai-session-template.md
    decisions.md
    guidance/
  include/  src/  samples/  scripts/  tests/
```

---

## 与 AI 协作规矩

### 每次会话

1. 说明 **本回合目标**（对应 P1.x / P2.x 或 FR-xx）。
2. `@` 引用 `design-technical.md` 相关模块，少贴大段代码。
3. 结束输出：**变更摘要**、**验证步骤**、**建议 commit subject（英文）**、下一项建议。
4. **不**默认 `git commit` / `push`（用户明确要求除外）；commit 格式见 `git-convention.md`。

### AI 应当

- 小步、可编译、可验证。
- 遵守 TDD 模块 API 与 PRD 行为。
- **源代码注释仅英文**。
- 提议更新 `test-plan.md`（勾选由人完成）。
- 架构取舍 → 提议新 ADR。

### AI 不应

- 违背 PRD/ADR（如 0.0.0.0 当 A 返回、超时 SERVFAIL、R1 读表）。
- 单次会话大范围重构或实现 R2/R3 越界功能。
- 忙等；第三方库；中文代码注释。
- 擅自 commit（尤其含乱码中文 subject）。

---

## Release 里程碑

| Release | 名称 | 课程验收 | 状态 |
|---------|------|----------|------|
| R1 | 仅转发 | 否 | 已完成（见 docs/testing-release1.md） |
| R2 | 完整课设 | **是** | 未开始 |
| R3 | LRU 缓存 | 加分 | 未开始 |

细节：PRD「发布计划」、TDD §11。

---

## 平台（摘要）

Windows x64 · C11 · CMake + MinGW · 无第三方库 · 单 UDP socket · 详见 ADR-000/001。

---

## Cursor 规则

`.cursor/rules/dns-relay-workflow.mdc`（编辑 `dns_relay/**` 时生效）。
