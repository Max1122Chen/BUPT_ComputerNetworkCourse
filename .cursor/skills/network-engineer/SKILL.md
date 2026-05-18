---
name: network-engineer
description: >-
  Authors and reviews low-level network software technical design documents:
  protocol fields, socket architecture, data structures, module APIs, sequence
  diagrams, error handling, and PRD traceability. Use when writing design-technical.md,
  implementing DNS/TCP/UDP services, or when the user asks for network architecture,
  技术设计, RFC-aligned design, or interface specifications.
---

# Network Engineer（技术设计）

## 读者与目标

技术设计面向 **实现者、评审者、答辩追问**。读者已读过 PRD，需要知道 **如何实现且为何如此**。

## 写作原则

1. **需求可追溯**：每个设计块映射 PRD 的 FR-xx（用表格）。
2. **协议可对照 RFC**：列出使用的字段、字节序、长度限制；不复制 RFC 全文。
3. **接口先于实现**：每个模块给出 **头文件级 API**（函数、结构体、错误码、前置/后置条件）。
4. **数据面清晰**：画清报文路径、socket、状态表；标明 **何处改 ID、何处不改包体**。
5. **并发与资源**：说明 pending 表、超时、select 超时计算、无忙等。
6. **常量集中**：端口、超时、缓冲区、默认上游等用宏/配置结构体，不散落魔数。
7. **分期可裁剪**：按 Release 标注模块/API 的 `#if` 或「R1 可选实现」。
8. **错误处理一致**：统一错误码或模块枚举；说明对用户可见行为（如静默丢弃）。

## 必含章节（见模板）

使用 [technical-design-template.md](technical-design-template.md)。默认输出路径：`dns_relay/docs/design-technical.md`。

## 与 PRD / ADR 关系

- **PRD**（`design.md`）定义行为与验收；技术设计不得违背。
- **ADR**（`decisions.md`）记录已接受决策；设计引用 ADR 编号，不重复争论。
- 若实现需要变更协议行为，先提议新 ADR，再改设计。

## 工作流

1. 阅读 PRD + ADR + 既有代码（若有）。
2. 填模板：架构 → 协议 → 模块 API → 数据结构 → 流程/时序 → 常量 → 追溯矩阵。
3. 与 [coding-style.md](../../dns_relay/docs/coding-style.md) 命名一致。
4. 自检：API 是否足够让不同人分模块实现；边界情况是否列出。

## 自检清单

- [ ] 单 socket / 多 socket 结论有理由（ADR-003）
- [ ] ID 映射、超时、512 与 PRD 一致
- [ ] 本地应答与中继路径字段修改范围明确
- [ ] `dns_table` / `id_map` / `dns_packet` 接口完整
- [ ] Release 1/2/3 代码路径差异表存在
- [ ] FR → 模块追溯表完整
