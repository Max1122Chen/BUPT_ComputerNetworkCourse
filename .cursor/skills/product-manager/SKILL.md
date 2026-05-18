---
name: product-manager
description: >-
  Writes and reviews product requirements documents (PRD), user stories, and
  release plans for software projects. Use when authoring or rewriting
  requirements, design.md, feature specs, acceptance criteria, or when the user
  asks for PM-style documentation, PRD, or 需求文档.
---

# Product Manager（需求文档）

## 目标读者

需求文档的读者包括：**未参与讨论的开发者**、**课程助教/验收教师**、**同组成员**、**未来的自己**。默认对方 **不了解** 项目背景与内部分期代号。

## 写作原则

1. **先讲清「是什么、为谁、解决什么」**，再讲阶段与细节。
2. **用户语言优先**：用「用户 / 客户端 / 本程序 / 上游 DNS」，少用未解释的缩写（首次出现写全称）。
3. **每条需求可验收**：功能描述必须附带 **验收标准**（可测试、可观察）。
4. **场景驱动**：用「当…则…」或用户故事描述行为，避免只列内部模块名。
5. **分期对外可理解**：阶段用 **用户可感知的能力** 命名（如「仅转发」），内部编号（P1.x）放附录。
6. **边界明确**：单独列出「不做 / 后续版本」，避免读者误以为已承诺。
7. **技术细节分层**：PRD 写 **行为与约束**；模块、数据结构、API 放到 `design-technical.md` 或 ADR。

## 禁止

- 假设读者知道「一期方案 B」「id_map」等内部用语而不解释。
- 只有里程碑表格、没有完整功能说明。
- 把 ADR 编号当作需求正文（可链接，不可代替叙述）。

## PRD 输出结构

撰写或重写需求时，使用 [prd-template.md](prd-template.md) 章节顺序。保存路径默认为项目 `docs/design.md`，版本号递增。

## 工作流

1. 收集已定决策（口头、ADR、旧文档）。
2. 用模板生成 **PRD 正文**（产品语言 + 验收标准）。
3. 将实现级内容剥离到 `design-technical.md`（若存在）。
4. 同步 `test-plan.md`：每个功能至少一条对应用例 ID。
5. 在文档开头写 **修订记录** 与 **一句话摘要**。

## 质量自检（发布前）

- [ ] 陌生人读摘要能在 2 分钟内说清产品做什么
- [ ] 每个功能有验收标准
- [ ] 三期/发布范围各有「用户能做什么」描述
- [ ] 术语表覆盖 DNS、NXDOMAIN、上游等
- [ ] 与 ADR/decisions 无矛盾；矛盾时以 PRD 为准并更新 ADR
