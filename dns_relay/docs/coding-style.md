# C 语言编码规范（DNS Relay）

与 [decisions.md](./decisions.md) 中 ADR-000、ADR-001 一致。AI 生成或修改代码时应遵守本文；答辩时保持风格统一。

## 语言与标准

- **ISO C11**（`-std=c11`），不使用 C++ 编译器或语法。
- 仅使用 **C 标准库** 与 **操作系统 API**（Win64：Winsock2 等）；见 ADR-001。
- 数据结构（哈希表、链表、缓冲区等）**自实现**，不引入第三方库。

## 文件与模块

| 类型 | 约定 | 示例 |
|------|------|------|
| 源文件 | 小写 + 下划线，与模块同名 | `dns_table.c` |
| 头文件 | 与 `.c` 同名，放 `include/` | `dns_table.h` |
| 头文件保护 | `DNS_RELAY_<MODULE>_H` | `DNS_RELAY_DNS_TABLE_H` |

每个模块一对 `.h` / `.c`：头文件声明 **对外接口**；仅供本文件使用的函数在 `.c` 内 `static`。

## 命名

### 总体原则

- **可读优先**：名字应表达意图，避免 `tmp`、`buf2`、`do_it`。
- **模块前缀**：对外函数、类型、宏使用 `模块_` 前缀，避免全局命名冲突。
- **长度**：在清晰前提下偏短；DNS 领域可用 `qname`、`rcode` 等协议常用缩写。

### 函数

```c
/* Format: <module>_<verb>[_<object>] */
int dns_table_load(const char *path);
int id_map_insert(uint16_t upstream_id, const struct pending_query *q);
void debug_log(int level, const char *fmt, ...);
```

- 返回 **0 表示成功**，**非 0 表示错误**（或模块内 `enum` 错误码）；不把“成功/失败”藏在输出参数里而不文档化。
- 只做一件事；过长函数应拆分。

### 类型

```c
/* 结构体：struct <module>_<name> 或 typedef */
struct dns_header {
    uint16_t id;
    /* ... */
};

typedef struct id_map_entry id_map_entry_t;  /* optional _t suffix — be consistent project-wide */
```

- 枚举类型名：`dns_rcode` 或 `enum dns_rcode`；枚举值 **全大写 + 模块前缀**：`DNS_RCODE_OK`。

### 变量

| 作用域 | 风格 | 示例 |
|--------|------|------|
| 局部变量 | `snake_case` | `bytes_read`, `client_addr` |
| 函数参数 | `snake_case` | `const char *path` |
| 文件内 static | `snake_case` | `static int s_debug_level` |
| 全局（尽量避免） | `g_` 前缀 | `g_config`（若必须） |

- 循环变量：`i`、`j` 可接受；嵌套深时用有意义名称。
- 布尔语义：用 `int`/`bool`（`stdbool.h`）+ `is_` / `has_` 前缀，如 `is_valid`, `has_expired`。

### 宏与常量

```c
#define DNS_UDP_MAX_SIZE 512
#define DNS_RELAY_DEFAULT_PORT 53

enum { DNS_ID_MAP_TIMEOUT_MS = 5000 };
```

- **宏常量**：全大写 + 下划线。
- 带副作用的宏尽量少用；复杂逻辑用 `static inline` 函数（C11）或普通函数。

### 平台相关

- 集中在 `platform_win.c` / `platform.h`（名称待定），对外仍用 `platform_*` 或 `sock_*` 统一接口，避免在业务模块散落 `WSA*`。

## 格式与布局

- 缩进 **4 空格**，不用 Tab。
- 大括号：**Allman** 风格（函数 `{` 独占一行），全项目统一。
- 行宽建议 ≤ 100 字符；过长表达式换行对齐。
- 头文件顺序：`#include <system>` → `#include <standard>` → `#include "project"`。

## 注释

### 语言（强制）

- **源代码中的注释只允许使用英文**（`//`、`/* */`、文件头、`#` 行内说明均适用）。
- **禁止**在 `.c` / `.h` 中使用中文注释（答辩材料、报告、PRD/TDD 等文档不受此限）。
- `debug_log` 输出给用户看的字符串：英文为主；若课程演示需要中文日志，须在 `debug` 模块集中、并在 PRD/测试中说明，**仍禁止**在普通代码行旁写中文注释。

### 风格

- **少而准**：解释 *why* 与协议/并发 invariant，不复述代码字面意思。
- 可选文件头：一行英文说明模块职责。
- 不保留错误或过时的注释。

示例：

```c
/* id_map: pending relay transactions keyed by upstream DNS ID */
/* Drop late replies after expire — do not send anything to the client (ADR-005) */
```

## 错误处理与资源

- 每次 `malloc` 配对检查；出口统一 `goto cleanup` 或小型 `do { } while(0)` 模式，避免泄漏。
- Winsock：Windows 下注意 `WSAStartup` / `WSACleanup` 生命周期，错误用 `WSAGetLastError()` 记入调试日志。

## 禁止事项（课程设计）

- 引入第三方库（glib、uthash、cJSON 等）。
- 用 C++ 编译 `.cpp` 或混用 STL。
- 复制粘贴大段相似代码而不抽函数（任务书强调的“两处类似代码”问题）。

## 示例：模块边界

```c
/* include/dns_table.h */
#ifndef DNS_RELAY_DNS_TABLE_H
#define DNS_RELAY_DNS_TABLE_H

struct dns_table;

struct dns_table *dns_table_create(void);
void dns_table_destroy(struct dns_table *t);
int dns_table_load(struct dns_table *t, const char *path);
/* Lookup: HIT / BLOCK (0.0.0.0) / MISS — see design-technical.md */
int dns_table_lookup(const struct dns_table *t, const char *host, /* out */ ...);

#endif
```

（API details: [design-technical.md](./design-technical.md).）
