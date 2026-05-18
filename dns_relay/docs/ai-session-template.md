# AI 会话模板

新开 Cursor 对话时复制本模板，填写 `（）` 部分，并 `@` 引用文件。

---

## 本回合目标

（一句话，例如：P1.2 — id_map insert/take and relay forward with ID rewrite。）

## 阶段与范围

- Release / 里程碑：（R1 · P1.2 / R2 · P2.0 / …）
- 负责模块：（relay / id_map / …）
- 不要改动：（文件或模块）
- 本回合不做：（越界功能）

## 约束

- 平台：Windows x64，CMake + MinGW，C11，无第三方库
- 代码注释：**English only**（见 coding-style.md）
- 行为以 PRD / TDD 为准

## 参考文件（建议 @ 引用）

- @dns_relay/AGENTS.md
- @dns_relay/docs/design-technical.md
- @dns_relay/docs/design.md（若涉及需求边界）
- （其他）

## 验收标准

- [ ] 
- [ ] 

## 上下文（可选）

（报错、Wireshark 现象、队友接口约定等）

---

## 回合结束时期望输出

请 AI 提供：

1. **变更摘要**（文件列表）
2. **如何验证**（命令/步骤）
3. **建议 Git commit**（英文 subject，格式 `type(scope): subject`；若变更大，附 EN+中文 body 草稿）
4. **下一回合建议**（一条）

提交前人读 [git-convention.md](./git-convention.md)，本地 `git commit`（AI 默认不提交）。
