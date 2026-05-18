# 协作指南（dns_relay）

面向 **新加入成员** 与 **使用 Cursor AI 的队友**。请先读本文，再读 PRD / TDD。

---

## 10 分钟上手路径

```text
1. README.md           → 环境、构建、文档地图
2. docs/design.md       → 产品做什么（PRD）
3. docs/design-technical.md → 怎么实现（API/协议）
4. docs/decisions.md    → 已拍板决策（勿擅自推翻）
5. docs/coding-style.md → 写代码规矩（含英文注释）
6. docs/git-convention.md → 提交信息规矩
7. AGENTS.md            → 与 AI 协作入口
```

当前开发焦点：**Release 1（透明中继）** → 见 PRD「发布计划」。

---

## 成员分工建议（2～3 人）

| 方向 | 模块 | 参考 TDD 章节 |
|------|------|----------------|
| 传输与循环 | `platform`, `socket_io`, `relay` | §4、§5.2–5.3、§5.8 |
| 协议与配置 | `dns_packet`, `config`, `debug` | §3、§5.4、§5.9 |
| 数据与中继 | `id_map`（R1）；`dns_table`（R2）；`dns_cache`（R3） | §5.5–5.7 |

**接口先行**：改 `include/*.h` 或 TDD 接口时，在群里/sync 一声，避免两人改同一模块。

---

## 日常协作流程

### 开工前

1. `git pull` 同步 `main`（或你们的主分支）。
2. 确认本迭代目标（如 `P1.2 id_map`），对照 [design-technical.md §11.1](./design-technical.md)。
3. 新开 AI 对话：复制 [ai-session-template.md](./ai-session-template.md)，`@AGENTS.md` + `@design-technical.md`。

### 开发中

- 只改 `dns_relay/`；代码注释 **仅英文**（见 coding-style）。
- 行为以 PRD 为准；实现以 TDD 的 API 为准。
- 不引入第三方库；不 busy-wait。

### 提交前（自检）

- [ ] `cmake --build build` 通过
- [ ] 本阶段应用 `-d` 能跑、能 `nslookup`（若已到该阶段）
- [ ] `git status` 无多余 `build/`、`.exe`（应在 `.gitignore`）
- [ ] commit message 符合 [git-convention.md](./git-convention.md)
- [ ] 若改行为：更新 `test-plan.md` 对应用例（未测可保持未勾选）

### 合并 / 验收前

- 组内互相拉分支看一眼 **接口与注释**（能否答辩）。
- Wireshark 截图可放报告素材目录（团队自定，勿提交巨大 pcap 除非必要）。

---

## 与 AI 协作（Cursor）

| 场景 | 做法 |
|------|------|
| 新功能 | `@design-technical.md` 指定模块 §5；说明 Release 编号 |
| 写需求/改 PRD | 使用 skill `product-manager` |
| 写/改技术设计 | 使用 skill `network-engineer` |
| 结束一轮 | 要求 AI 给出：变更文件、验证命令、**建议 commit subject（英文）** |
| 提交 | **人**本地 commit；AI 默认不 push |

规则文件：仓库 `.cursor/rules/dns-relay-workflow.mdc`（打开 `dns_relay` 下文件时生效）。

---

## 文档修改谁负责

| 文档 | 何时改 |
|------|--------|
| `design.md` | 需求变更（组内一致 + 版本号） |
| `design-technical.md` | 接口/协议实现变更 |
| `decisions.md` | 新 ADR（架构取舍） |
| `test-plan.md` | 新增/完成测试项 |
| `coding-style` / `git-convention` | 组内规范变更 |

---

## 常见问题

| 问题 | 处理 |
|------|------|
| bind 53 失败 | 管理员终端；检查系统是否占用 53 |
| 中文 commit 乱码 | 见 [git-convention.md §3](./git-convention.md) |
| AI 一次写太多 | 会话模板里写清「本回合仅 P1.x」 |
| 与队友接口冲突 | 先对齐 `include/*.h`，再分头写 `.c` |

---

## 相关链接

- [README.md](../README.md)
- [AGENTS.md](../AGENTS.md)
- [test-plan.md](./test-plan.md)
