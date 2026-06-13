# DNS Relay 代码学习文档

> 面向课程设计验收前快速复习。本文档基于 `dns_relay/` 当前源码整理，重点解释“代码为什么这样写”和“答辩时怎么讲”。

---

## 1. 项目整体介绍

### 1.1 这个项目是做什么的

`dnsrelay` 是一个运行在 Windows 上的 DNS 中继服务器。它监听本机 UDP 53 端口，接收客户端发来的 DNS 查询，然后按照本地静态域名表、缓存和上游 DNS 的结果进行响应。

一句话概括：

```text
客户端 DNS 查询先到本程序，本程序优先查本地域名表和缓存，查不到再转发给上游 DNS。
```

典型场景如下：

| 查询情况 | 程序行为 |
| --- | --- |
| 域名在 `dnsrelay.txt` 中，并且 IP 不是 `0.0.0.0` | 本地直接构造 A 记录响应 |
| 域名在 `dnsrelay.txt` 中，并且 IP 是 `0.0.0.0` | 本地返回 NXDOMAIN，实现拦截 |
| 域名不在静态表，但在 LRU 缓存中 | 直接用缓存 IP 构造 A 记录响应 |
| 静态表和缓存都没有 | 修改 Transaction ID 后转发给上游 DNS |

### 1.2 核心功能

1. **监听 UDP/53**：接收 DNS 查询报文。
2. **命令行配置**：支持 `-h`、`-d`、`-dd`、上游 DNS IP、配置文件路径。
3. **读取静态域名表**：加载 `dnsrelay.txt` 中的 `IP 域名` 配置。
4. **本地 A 记录响应**：静态表命中普通 IP 时，直接构造 DNS Answer。
5. **域名拦截**：静态表命中 `0.0.0.0` 时，返回 NXDOMAIN。
6. **透明中继**：未命中时转发给上游 DNS，再把响应转回客户端。
7. **Transaction ID 映射**：为每个转发请求分配新的上游 ID，避免多客户端并发时串包。
8. **超时清理**：上游 5 秒无响应时删除 pending 映射。
9. **LRU 缓存**：缓存从上游响应中提取到的 A 记录，减少重复转发。
10. **单元测试**：覆盖 DNS 报文、静态表、ID 映射、缓存、配置解析等模块。

### 1.3 技术栈

| 类型 | 使用内容 |
| --- | --- |
| 语言 | C11 |
| 平台 | Windows x64 |
| 网络库 | Winsock2，链接 `ws2_32` |
| 构建工具 | CMake + MinGW GCC |
| 协议 | DNS over UDP，参考 RFC 1035 |
| 并发模型 | 单进程、单线程、`select` 事件循环 |
| 第三方库 | 无 |

### 1.4 运行流程

```text
启动 dnsrelay.exe
    |
    |-- config_parse 解析命令行参数
    |-- debug_init 设置日志级别
    |-- platform_init 初始化 Winsock 和单调时钟
    |-- SetConsoleCtrlHandler 注册 Ctrl+C 停止处理
    |-- relay_run 进入主业务
            |
            |-- dns_socket_open 创建 UDP socket，绑定 53 端口，设置上游地址
            |-- id_map_create 创建 Transaction ID 映射表
            |-- dns_table_load_file 加载 dnsrelay.txt
            |-- dns_cache_create 创建 LRU 缓存
            |-- select 等待 socket 可读或 pending 超时
            |-- 收到客户端查询：handle_client_query
            |-- 收到上游响应：handle_upstream_response
```

常用运行命令：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\dnsrelay_test.exe
.\build\dnsrelay.exe -d 220.181.111.1
```

注意：监听 UDP 53 端口通常需要管理员权限。

---

## 2. 项目目录结构解析

```text
dns_relay/
|-- CMakeLists.txt          构建配置，生成 dnsrelay.exe 和 dnsrelay_test.exe
|-- dnsrelay.txt            默认静态域名表，运行时读取
|-- README.md               项目说明、构建运行命令
|-- AGENTS.md               协作说明
|-- include/                头文件，定义模块对外接口和数据类型
|-- src/                    源码实现，项目核心代码
|-- tests/                  单元测试
|-- samples/                示例配置文件
|-- docs/                   设计文档、测试文档、学习文档
|-- build/                  CMake 构建产物
```

### 2.1 根目录文件

| 文件 | 作用 |
| --- | --- |
| `CMakeLists.txt` | 指定 C 标准、源码列表、头文件路径、链接 Winsock2、创建测试目标 |
| `dnsrelay.txt` | 默认静态表，格式是 `IP 域名` |
| `README.md` | 构建、运行、验收说明 |
| `AGENTS.md` | 团队或 AI 协作入口说明 |

`dnsrelay.txt` 当前内容示例：

```text
0.0.0.0 test0
11.111.11.111 test1
22.22.222.222 test2
123.127.134.10 www.bupt.cn
```

其中 `test0` 会被拦截，返回 NXDOMAIN；`test1`、`test2` 会本地返回配置的 IP。

### 2.2 `include/`

`include/` 存放模块 API，验收时可以理解为“每个模块对外承诺能做什么”。

| 文件 | 类型 | 作用 |
| --- | --- | --- |
| `config.h` | 配置模块 | 定义 `dns_relay_config` 和命令行解析接口 |
| `relay.h` | 主业务接口 | 暴露 `relay_run` 和 `relay_request_stop` |
| `dns_packet.h` | DNS 协议层 | 解析查询、构造 A/NXDOMAIN 响应、提取 A 记录 |
| `dns_table.h` | 静态表 | 加载和查询 `dnsrelay.txt` |
| `dns_cache.h` | 缓存 | LRU 缓存创建、查询、插入 |
| `id_map.h` | ID 映射 | 管理客户端 ID 与上游 ID 的 pending 映射 |
| `socket_io.h` | 网络 I/O | UDP socket 封装 |
| `platform.h` | 平台封装 | Winsock 生命周期和单调时钟 |
| `net_util.h` | 网络工具 | 地址比较、地址格式化 |
| `debug.h` | 日志 | 分级日志输出 |
| `dns_relay_constants.h` | 常量 | 端口、缓存容量、超时时间等 |
| `test_framework.h` | 测试工具 | 简单断言宏 |

### 2.3 `src/`

`src/` 是核心实现。

| 文件 | 重要程度 | 作用 |
| --- | --- | --- |
| `main.c` | 入口 | 程序启动、初始化、退出清理 |
| `relay.c` | 核心 | 事件循环、请求分发、本地响应、中继、缓存 |
| `dns_packet.c` | 核心 | DNS 报文解析和构造 |
| `dns_table.c` | 核心 | 静态域名表哈希表 |
| `dns_cache.c` | 核心 | LRU 缓存 |
| `id_map.c` | 核心 | Transaction ID 映射和超时 |
| `socket_io.c` | 核心 | UDP 收发和上游来源判断 |
| `config.c` | 配置 | 命令行参数解析 |
| `platform_win.c` | 平台 | Winsock 初始化、单调时钟 |
| `net_util.c` | 工具 | IPv4 地址比较和格式化 |
| `debug.c` | 工具 | `-d/-dd` 日志输出 |

### 2.4 `tests/`

`tests/test_main.c` 是单元测试入口。它不真正占用 53 端口，也不依赖真实 DNS 网络，主要测试纯逻辑模块：

| 测试内容 | 对应模块 |
| --- | --- |
| ID 读写、查询解析、响应构造、A 记录提取 | `dns_packet.c` |
| 静态表加载、命中、拦截、未命中 | `dns_table.c` |
| ID 插入、取出、超时 | `id_map.c` |
| 缓存插入、命中、LRU 淘汰、过期 | `dns_cache.c` |
| 命令行默认值和参数解析 | `config.c` |
| 地址比较 | `net_util.c` |

---

## 3. 项目架构分析

### 3.1 分层方式

项目虽然是 C 语言写的，但模块划分比较清晰，可以按下面理解：

```text
入口层
  main.c
    |
配置/平台层
  config.c, platform_win.c, debug.c
    |
业务调度层
  relay.c
    |
协议层
  dns_packet.c
    |
数据结构层
  dns_table.c, dns_cache.c, id_map.c
    |
网络 I/O 层
  socket_io.c, net_util.c
```

这种分层的好处是：

1. `relay.c` 只负责业务决策，不需要自己解析 DNS 报文细节。
2. `dns_packet.c` 只负责协议格式，不关心 socket 从哪里来。
3. `dns_table.c`、`dns_cache.c`、`id_map.c` 各自维护一种数据结构，互不耦合。
4. `socket_io.c` 把 Winsock 的收发细节封装起来，业务层调用更清楚。

### 3.2 模块协作关系

```text
main.c
  |-- config_parse              生成 cfg
  |-- platform_init             初始化 Winsock
  |-- relay_run(cfg)
        |
        |-- dns_socket_open     打开 UDP/53
        |-- dns_table_load_file 加载静态表
        |-- dns_cache_create    创建缓存
        |-- id_map_create       创建 pending 映射
        |
        |-- handle_client_query
        |     |-- dns_packet_parse_query
        |     |-- dns_table_lookup
        |     |-- dns_cache_lookup
        |     |-- dns_packet_build_a_response
        |     |-- dns_packet_build_nxdomain
        |     |-- relay_forward
        |
        |-- handle_upstream_response
              |-- id_map_take
              |-- dns_packet_set_id
              |-- dns_packet_extract_a_for_qname
              |-- dns_cache_insert
```

### 3.3 请求流

客户端查询处理流程：

```text
客户端发 DNS 查询
    |
    v
dns_socket_recv 收包
    |
    v
判断来源是不是上游 DNS
    |
    |-- 是：handle_upstream_response
    |
    |-- 否：handle_client_query
              |
              |-- 解析 DNS 查询
              |-- 如果是 A/IN 查询：
              |       |-- 查静态表
              |       |-- 表命中普通 IP：构造 A 响应
              |       |-- 表命中 0.0.0.0：构造 NXDOMAIN
              |       |-- 表未命中：查缓存
              |       |-- 缓存命中：构造 A 响应
              |
              |-- 前面都没响应：转发给上游 DNS
```

上游响应处理流程：

```text
上游 DNS 返回响应
    |
    v
取出响应报文 Transaction ID
    |
    v
id_map_take 找到原客户端信息
    |
    |-- 找不到：说明超时或未知响应，丢弃
    |
    |-- 找到：
          |-- 把报文 ID 改回客户端原始 ID
          |-- 从响应中提取 A 记录并写入缓存
          |-- sendto 发回原客户端地址
```

### 3.4 是否有前后端、数据库、接口

本项目没有前端页面、没有后端 Web 框架、没有 HTTP API，也没有数据库。

它的“接口”是网络协议接口：UDP 53 端口上的 DNS 查询/响应。它的数据主要存在内存中：

1. 静态域名表：从 `dnsrelay.txt` 加载到哈希表。
2. 缓存：运行时从上游 DNS 响应中提取 A 记录，放入 LRU 缓存。
3. pending 映射：转发请求时临时保存客户端地址、原始 ID、上游 ID。

---

## 4. 代码逐文件讲解

### 4.1 `main.c`

主要职责：程序入口，负责初始化和清理。

关键逻辑：

```c
if (config_parse(argc, argv, &cfg) != 0)
{
    config_print_help(argc > 0 ? argv[0] : "dnsrelay");
    return 1;
}
```

启动时先解析命令行参数。如果参数错误，就打印帮助并退出。

```c
debug_init(cfg.debug_level);

if (platform_init() != 0)
{
    fprintf(stderr, "dnsrelay: Winsock init failed\n");
    return 1;
}
```

`debug_init` 根据 `-d/-dd` 设置日志级别。`platform_init` 初始化 Winsock，这是 Windows 上使用 socket 前必须做的步骤。

```c
SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
r = relay_run(&cfg);
platform_cleanup();
```

这里注册 Ctrl+C 处理函数。用户按 Ctrl+C 时，不是强行退出，而是调用 `relay_request_stop()`，让 `relay_run` 的循环自然结束，然后清理资源。

调用关系：

```text
main.c -> config.c
main.c -> debug.c
main.c -> platform_win.c
main.c -> relay.c
```

### 4.2 `config.c / config.h`

主要职责：保存和解析运行配置。

核心结构体：

```c
typedef struct dns_relay_config
{
    char upstream_ip[46];
    uint16_t upstream_port;
    char table_path[512];
    int debug_level;
    int load_table;
    int show_help;
} dns_relay_config;
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `upstream_ip` | 上游 DNS 服务器 IPv4 地址 |
| `upstream_port` | 上游 DNS 端口，默认 53 |
| `table_path` | 静态域名表路径，默认 `dnsrelay.txt` |
| `debug_level` | 日志等级，`0` 静默，`1` 对应 `-d`，`2` 对应 `-dd` |
| `load_table` | 是否加载静态表，当前默认加载 |
| `show_help` | 是否打印帮助 |

默认值在 `config_set_defaults` 中设置：

```c
strncpy(cfg->upstream_ip, DNS_RELAY_DEFAULT_UPSTREAM, sizeof(cfg->upstream_ip) - 1);
cfg->upstream_port = DNS_RELAY_PORT;
strncpy(cfg->table_path, DNS_RELAY_DEFAULT_TABLE_PATH, sizeof(cfg->table_path) - 1);
cfg->load_table = 1;
```

对应常量：

```c
#define DNS_RELAY_PORT 53
#define DNS_RELAY_DEFAULT_UPSTREAM "202.106.0.20"
#define DNS_RELAY_DEFAULT_TABLE_PATH "dnsrelay.txt"
```

为什么这样设计：

配置集中放在一个结构体里，后面 `relay_run(&cfg)` 只需要接收一个参数，就能拿到所有运行信息，避免全局变量到处传。

### 4.3 `platform_win.c / platform.h`

主要职责：封装 Windows 平台相关代码。

核心函数：

| 函数 | 作用 |
| --- | --- |
| `platform_init` | 调用 `WSAStartup` 初始化 Winsock，并初始化高精度计时器 |
| `platform_cleanup` | 调用 `WSACleanup` |
| `platform_monotonic_ms` | 返回单调递增的毫秒时间 |

`platform_monotonic_ms` 用于 pending 超时和缓存过期。它优先使用 `QueryPerformanceCounter`，如果不可用再用 `GetTickCount64`。

为什么不用普通系统时间：

普通系统时间可能被用户或系统同步修改。超时逻辑需要“经过了多少毫秒”，所以更适合用单调时间。

### 4.4 `socket_io.c / socket_io.h`

主要职责：封装 UDP socket 操作。

核心结构体：

```c
typedef struct dns_socket
{
    SOCKET fd;
    struct sockaddr_in bind_addr;
    struct sockaddr_in upstream_addr;
} dns_socket;
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `fd` | UDP socket 句柄 |
| `bind_addr` | 本地监听地址，绑定 `0.0.0.0:53` |
| `upstream_addr` | 上游 DNS 地址 |

`dns_socket_open` 做三件事：

```text
创建 UDP socket
    |
设置 SO_REUSEADDR
    |
绑定 INADDR_ANY:53
    |
解析并保存上游 DNS 地址
```

关键代码：

```c
sock->bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
sock->bind_addr.sin_port = htons(DNS_RELAY_PORT);

if (bind(sock->fd, (struct sockaddr *)&sock->bind_addr, sizeof(sock->bind_addr)) != 0)
{
    fprintf(stderr, "dnsrelay: bind UDP/%d failed (try Administrator)\n", DNS_RELAY_PORT);
    dns_socket_close(sock);
    return -1;
}
```

这里绑定的是 UDP 53 端口。Windows 上绑定低端口或已占用端口可能失败，所以提示使用管理员权限。

`dns_socket_is_upstream_source` 用于区分收到的包来自客户端还是上游 DNS：

```text
收到 UDP 包
    |
比较来源 IP 和端口是否等于 upstream_addr
    |
是：上游响应
否：客户端查询
```

### 4.5 `relay.c / relay.h`

主要职责：整个项目的业务核心。

它负责：

1. 打开 socket。
2. 创建静态表、缓存、ID 映射表。
3. 用 `select` 进入事件循环。
4. 处理客户端查询。
5. 处理上游响应。
6. 清理超时 pending。

#### 4.5.1 `relay_run`

核心事件循环：

```c
while (!s_stop_requested)
{
    uint64_t now = platform_monotonic_ms();
    uint64_t wait_ms = id_map_next_expire_ms(map, now);

    if (wait_ms > RELAY_SELECT_MAX_MS)
    {
        wait_ms = RELAY_SELECT_MAX_MS;
    }

    FD_ZERO(&readfds);
    FD_SET(sock.fd, &readfds);

    select((int)sock.fd + 1, &readfds, NULL, NULL, &tv);
    id_map_expire(map, platform_monotonic_ms());

    if (FD_ISSET(sock.fd, &readfds))
    {
        dns_socket_recv(...);
        ...
    }
}
```

为什么用 `select`：

项目是单线程模型，不需要为每个请求创建线程。`select` 可以让程序阻塞等待 socket 可读，同时设置超时时间，定期清理 pending 映射。这样逻辑简单，也足够支撑课程设计的 UDP DNS 中继。

#### 4.5.2 `handle_client_query`

这是客户端查询的决策核心。

流程：

```text
解析 DNS 查询
    |
如果是 A/IN 查询：
    |
    |-- 查静态表 dns_table
    |       |-- HIT：构造 A 响应
    |       |-- BLOCK：构造 NXDOMAIN 响应
    |       |-- MISS：继续
    |
    |-- 查 LRU 缓存 dns_cache
            |-- HIT：构造 A 响应
            |-- MISS：继续
    |
转发给上游 DNS
```

关键代码片段：

```c
if (table != NULL && qinfo.qtype == DNS_TYPE_A && qinfo.qclass == DNS_CLASS_IN)
{
    uint32_t ipv4_be = 0;
    dns_table_result r = dns_table_lookup(table, qinfo.qname, &ipv4_be);
    if (r == DNS_TABLE_HIT)
    {
        dns_packet_build_a_response(...);
        dns_socket_sendto(...);
        return;
    }
    else if (r == DNS_TABLE_BLOCK)
    {
        dns_packet_build_nxdomain(...);
        dns_socket_sendto(...);
        return;
    }
}
```

为什么只对 A/IN 查询查静态表和缓存：

`dnsrelay.txt` 存的是 IPv4 地址，对应 DNS 的 A 记录。对于 AAAA、MX、CNAME 等其他类型，本地表没有足够信息构造正确响应，所以直接转发给上游更合理。

#### 4.5.3 `relay_forward`

主要职责：把客户端请求转发给上游 DNS。

关键点是修改 Transaction ID：

```c
id_map_insert(map, client_id, client_addr, client_len, qname, &upstream_id);

memcpy(out, buf, len);
dns_packet_set_id(out, upstream_id);

dns_socket_sendto(sock, out, len, &up_st, up_len);
```

为什么要改 ID：

DNS 报文头前 2 字节是 Transaction ID。客户端用它匹配“哪个响应对应哪个请求”。如果直接把多个客户端的原始 ID 转发给同一个上游，可能出现两个请求 ID 相同的情况，响应回来时就难以区分该发给谁。

本项目的做法是：

```text
客户端原始 ID + 客户端地址
    |
id_map_insert 分配新的 upstream_id
    |
发给上游时使用 upstream_id
    |
上游响应回来后用 upstream_id 查回原客户端
```

#### 4.5.4 `handle_upstream_response`

主要职责：处理上游 DNS 返回的响应。

核心代码：

```c
upstream_id = dns_packet_get_id(buf);
entry = id_map_take(map, upstream_id);
if (entry == NULL)
{
    debug_log(DBG_L2, "relay: unknown or late upstream id %u", (unsigned)upstream_id);
    return;
}

dns_packet_set_id(buf, entry->client_id);
```

先取出上游响应 ID，再用 `id_map_take` 找回 pending 信息。如果找不到，说明响应迟到了或者不是本程序发出的请求，就丢弃。

找到后，把报文 ID 改回客户端原来的 ID，再发回客户端。

它还会尝试把上游响应中的 A 记录写入缓存：

```c
if (dns_packet_extract_a_for_qname(buf, len, entry->qname, &ipv4_be, &ttl_sec) == 0)
{
    dns_cache_insert(cache, entry->qname, ipv4_be, ttl_sec, platform_monotonic_ms());
}
```

为什么在这里写缓存：

只有上游响应才带有真实 TTL 和解析结果。本地静态表的响应是程序自己构造的，不需要进入动态缓存。

### 4.6 `dns_packet.c / dns_packet.h`

主要职责：DNS 协议层。它不负责网络收发，只负责操作 DNS 报文字节。

#### 4.6.1 DNS 报文 ID 读写

```c
uint16_t dns_packet_get_id(const uint8_t *pkt)
{
    return (uint16_t)((pkt[0] << 8) | pkt[1]);
}
```

DNS 报文前两个字节是 Transaction ID，网络字节序是大端，所以高字节在前。

#### 4.6.2 解析查询 `dns_packet_parse_query`

功能：

1. 检查报文长度至少 12 字节。
2. 读取 ID。
3. 检查 `QDCOUNT` 至少为 1。
4. 从偏移 12 开始解析 QNAME。
5. 读取 QTYPE 和 QCLASS。
6. 把域名转成小写。

DNS 查询报文结构可以简化理解为：

```text
Header 12 字节
    |
Question
    |-- QNAME：标签格式，例如 3www7example3com0
    |-- QTYPE：查询类型，例如 A=1
    |-- QCLASS：查询类别，例如 IN=1
```

域名解析函数 `decode_name` 支持 DNS 压缩指针，判断 `(label_len & 0xc0) == 0xc0`。这是因为 DNS 响应中经常用指针复用前面出现过的域名，减少报文大小。

#### 4.6.3 构造 A 响应

关键代码：

```c
out[ans_off + 0] = 0xC0;
out[ans_off + 1] = 0x0C;
```

这表示 Answer 的 NAME 字段使用压缩指针，指向报文偏移 12 的 QNAME。这样不用再把域名完整写一遍。

A 记录 Answer 结构：

```text
NAME      2 字节：0xC00C，指向 Question 中的域名
TYPE      2 字节：1，表示 A
CLASS     2 字节：1，表示 IN
TTL       4 字节：本地响应固定 60 秒
RDLENGTH  2 字节：4
RDATA     4 字节：IPv4 地址
```

代码中特别注意 IP 写入：

```c
memcpy(out + ans_off + 12, &ipv4_be, 4);
```

为什么用 `memcpy` 而不是位移：

`ipv4_be` 已经是网络字节序，直接复制 4 个原始字节最安全。如果在小端机器上用位移拆字节，容易把顺序写反。

#### 4.6.4 构造 NXDOMAIN

`dns_packet_build_nxdomain` 复用 `dns_packet_copy_header_and_question`，区别是：

```text
ANCOUNT = 0
RCODE = 3
```

`RCODE=3` 表示域名不存在，即 NXDOMAIN。

#### 4.6.5 从上游响应提取 A 记录

`dns_packet_extract_a_for_qname` 用于缓存。

它会：

1. 确认报文是响应。
2. 确认 `RCODE=0`，即没有错误。
3. 跳过 Question。
4. 第一遍扫描 Answer，寻找 CNAME。
5. 第二遍扫描 Answer，寻找原域名或 CNAME 目标对应的 A 记录。

为什么支持 CNAME：

很多真实域名不是直接返回 A 记录，而是先返回 CNAME，再返回 CNAME 目标的 A 记录。例如：

```text
www.baidu.com -> a.shifen.com
a.shifen.com  -> 39.156.66.10
```

如果不处理 CNAME，缓存可能无法提取真实 IP。

### 4.7 `dns_table.c / dns_table.h`

主要职责：加载和查询静态域名表。

内部数据结构：

```c
typedef struct dns_table_entry
{
    char *name;
    uint32_t ipv4_be;
    struct dns_table_entry *next;
} dns_table_entry;

struct dns_table
{
    dns_table_entry **buckets;
    size_t bucket_count;
};
```

这是一个哈希表，冲突时用链表解决。

加载流程：

```text
逐行读取 dnsrelay.txt
    |
去掉行首空白
    |
去掉 # 后面的行内注释
    |
解析 "IP domain"
    |
域名转小写，去掉末尾点
    |
IP 是 0.0.0.0：保存 ipv4_be=0，表示 BLOCK
IP 是普通地址：保存网络字节序 IPv4
    |
插入哈希表，重复域名后面的配置覆盖前面的配置
```

查询结果：

| 返回值 | 含义 |
| --- | --- |
| `DNS_TABLE_HIT` | 找到普通 IP |
| `DNS_TABLE_BLOCK` | 找到 `0.0.0.0`，需要拦截 |
| `DNS_TABLE_MISS` | 没找到 |

为什么用哈希表：

DNS 查询频率可能很高，如果每次都线性扫描文件，效率很低。启动时读入内存哈希表，查询接近 O(1)。

### 4.8 `dns_cache.c / dns_cache.h`

主要职责：缓存从上游 DNS 得到的 A 记录。

内部结构由两部分组成：

```text
哈希表：根据 qname 快速查找缓存节点
LRU 双向链表：记录最近使用顺序，满了淘汰最久未使用节点
```

节点结构：

```c
typedef struct dns_cache_node
{
    char *qname;
    uint32_t ipv4_be;
    uint64_t expire_at_ms;
    struct dns_cache_node *hash_next;
    struct dns_cache_node *lru_prev;
    struct dns_cache_node *lru_next;
} dns_cache_node;
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `qname` | 小写域名 |
| `ipv4_be` | IPv4 地址，网络字节序 |
| `expire_at_ms` | 过期时间 |
| `hash_next` | 哈希冲突链表 |
| `lru_prev/lru_next` | LRU 双向链表 |

缓存查找流程：

```text
域名转小写
    |
哈希表查节点
    |
没找到：MISS
    |
找到但过期：删除节点，返回 EXPIRED
    |
找到且未过期：移动到 LRU 头部，返回 HIT
```

缓存插入流程：

```text
TTL 为 0：不缓存
    |
域名已存在：更新 IP 和过期时间，移动到头部
    |
域名不存在：
    |
    |-- 如果容量满：从 LRU 尾部淘汰
    |-- 创建新节点
    |-- 插入哈希表
    |-- 插入 LRU 头部
```

默认容量是：

```c
#define DNS_CACHE_DEFAULT_CAPACITY 256
```

为什么要 LRU：

缓存容量有限，最近访问过的域名更可能再次访问，所以满了时淘汰最久没用的记录更合理。

### 4.9 `id_map.c / id_map.h`

主要职责：保存“客户端请求”和“上游请求”之间的对应关系。

核心结构体：

```c
typedef struct id_map_entry
{
    uint16_t client_id;
    uint16_t upstream_id;
    struct sockaddr_storage client_addr;
    socklen_t client_len;
    uint64_t expire_at_ms;
    char qname[256];
} id_map_entry;
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `client_id` | 客户端原始 Transaction ID |
| `upstream_id` | 程序分配给上游请求的新 ID |
| `client_addr` | 客户端 IP 和端口 |
| `client_len` | 地址长度 |
| `expire_at_ms` | pending 过期时间 |
| `qname` | 查询域名，用于响应回来后写缓存 |

插入流程：

```text
检查 pending 数量是否达到 256
    |
分配未使用 upstream_id
    |
保存 client_id、client_addr、qname、expire_at_ms
    |
按 upstream_id 放入哈希桶
```

取出流程：

```text
上游响应回来
    |
读取 upstream_id
    |
在哈希表中找到节点
    |
从表中删除，并返回 entry
```

为什么 `id_map_take` 取出后要删除：

一个 DNS 查询通常只需要一次响应。响应已经回来后，这条 pending 映射就完成使命了。如果不删除，会浪费内存，也可能影响后续 ID 分配。

超时常量：

```c
#define DNS_ID_MAP_TIMEOUT_MS 5000
#define DNS_ID_MAP_MAX_PENDING 256
```

### 4.10 `net_util.c / net_util.h`

主要职责：网络地址工具。

| 函数 | 作用 |
| --- | --- |
| `net_util_addr_equals` | 比较两个 IPv4 地址和端口是否相同 |
| `net_util_format_endpoint` | 把地址格式化成 `IP:port` 字符串，用于日志 |

`socket_io.c` 用 `net_util_addr_equals` 判断收到的 UDP 包是不是来自上游 DNS。

### 4.11 `debug.c / debug.h`

主要职责：分级日志。

```c
#define DBG_L1 1
#define DBG_L2 2
```

| 参数 | 日志级别 |
| --- | --- |
| 不加参数 | `debug_level=0`，基本不输出调试日志 |
| `-d` | `debug_level=1`，输出主要转发和命中日志 |
| `-dd` | `debug_level=2`，输出更详细的调试信息 |

实现很简单：

```c
if (level > s_debug_level)
{
    return;
}
```

只有日志等级小于等于当前设置时才输出。

### 4.12 `CMakeLists.txt`

主要职责：描述如何构建项目。

核心内容：

```cmake
project(dnsrelay C)
set(CMAKE_C_STANDARD 11)
```

指定项目语言是 C，标准是 C11。

```cmake
add_executable(dnsrelay
    src/main.c
    src/relay.c
    ${DNS_RELAY_COMMON_SOURCES}
)
```

生成主程序 `dnsrelay.exe`。

```cmake
add_executable(dnsrelay_test
    tests/test_main.c
    ${DNS_RELAY_COMMON_SOURCES}
)
```

生成测试程序 `dnsrelay_test.exe`。测试目标没有包含 `main.c` 和 `relay.c`，而是直接测试通用模块。

```cmake
target_link_libraries(dnsrelay PRIVATE ws2_32)
```

Windows 下使用 Winsock2 必须链接 `ws2_32`。

### 4.13 `tests/test_main.c`

主要职责：单元测试。

它使用简单的 `TEST_ASSERT` 和 `run_one`，逐个运行测试函数。

测试覆盖了：

1. DNS 报文 ID 读写。
2. DNS 查询解析。
3. A 响应和 NXDOMAIN 响应构造。
4. 从上游响应提取 A 记录，包括 CNAME 场景。
5. 静态表加载和查询。
6. ID 映射插入、取出、超时。
7. 缓存插入、命中、过期、LRU 淘汰。
8. 配置解析。
9. 网络地址比较。

验收时可以这样说：

> 测试程序主要覆盖不依赖真实网络的核心逻辑，这样不需要管理员权限，也不用占用 53 端口，就能验证 DNS 报文处理、静态表、缓存、ID 映射等模块是否正确。

---

## 5. 核心功能实现原理

### 5.1 启动和初始化

功能入口：`main.c` 的 `main`

涉及文件：

| 文件 | 作用 |
| --- | --- |
| `main.c` | 总入口 |
| `config.c` | 解析参数 |
| `platform_win.c` | 初始化 Winsock |
| `socket_io.c` | 打开并绑定 UDP socket |
| `relay.c` | 创建表、缓存、映射并进入循环 |

执行步骤：

```text
main
    |
    |-- config_parse
    |-- debug_init
    |-- platform_init
    |-- SetConsoleCtrlHandler
    |-- relay_run
            |
            |-- dns_socket_open
            |-- id_map_create
            |-- dns_table_create + dns_table_load_file
            |-- dns_cache_create
            |-- 进入 select 循环
```

答辩回答：

> 程序启动后先解析命令行，确定上游 DNS、配置文件和日志级别；然后初始化 Winsock，绑定 UDP 53 端口；接着加载本地域名表，创建 pending ID 映射和 LRU 缓存；最后进入 `relay_run` 的 `select` 事件循环，持续处理客户端查询和上游响应。

### 5.2 本地 A 记录响应

功能入口：`relay.c` 的 `handle_client_query`

涉及函数：

| 函数 | 作用 |
| --- | --- |
| `dns_packet_parse_query` | 解析客户端查询 |
| `dns_table_lookup` | 查静态表 |
| `dns_packet_build_a_response` | 构造 DNS A 响应 |
| `dns_socket_sendto` | 发回客户端 |

执行步骤：

```text
收到客户端查询
    |
解析出 qname/qtype/qclass
    |
确认是 A/IN 查询
    |
查 dns_table
    |
命中普通 IP
    |
构造 Answer section
    |
sendto 发回客户端
```

为什么这样设计：

静态表是用户明确配置的规则，优先级应该高于缓存和上游 DNS。所以一旦静态表命中，就不再请求上游。

答辩回答：

> 程序先解析 DNS 查询报文，拿到域名、类型和类别。如果是 A/IN 查询，就用小写域名查静态哈希表。命中普通 IP 后，程序复制原始请求头和 Question，再添加一条 A 记录 Answer，最后用 `sendto` 返回客户端。

### 5.3 域名拦截 NXDOMAIN

功能入口：`relay.c` 的 `handle_client_query`

涉及函数：

| 函数 | 作用 |
| --- | --- |
| `dns_table_lookup` | 返回 `DNS_TABLE_BLOCK` |
| `dns_packet_build_nxdomain` | 构造 NXDOMAIN |
| `dns_socket_sendto` | 发回客户端 |

执行步骤：

```text
dnsrelay.txt 中配置 0.0.0.0 test0
    |
查询 test0
    |
dns_table_lookup 返回 DNS_TABLE_BLOCK
    |
构造 RCODE=3、ANCOUNT=0 的响应
    |
客户端收到 Non-existent domain
```

答辩回答：

> 静态表里把 `0.0.0.0` 作为特殊标记，不是真的返回 0.0.0.0，而是表示拦截。查询命中这种记录时，程序构造一个 `RCODE=3` 的 DNS 响应，也就是 NXDOMAIN，表示域名不存在。

### 5.4 转发上游 DNS

功能入口：`relay.c` 的 `relay_forward`

涉及函数：

| 函数 | 作用 |
| --- | --- |
| `id_map_insert` | 保存客户端请求信息并分配上游 ID |
| `dns_packet_set_id` | 改写 DNS 报文 ID |
| `dns_socket_sendto` | 发给上游 DNS |

执行步骤：

```text
静态表和缓存都没命中
    |
id_map_insert 保存 client_id/client_addr/qname
    |
分配 upstream_id
    |
复制原报文
    |
把 Transaction ID 改成 upstream_id
    |
发送给上游 DNS
```

答辩回答：

> 未命中的查询会走透明中继。程序不是原样转发，而是先为这个请求分配一个新的上游 Transaction ID，并把原客户端 ID、客户端地址、域名保存到 `id_map`。这样上游响应回来时，能准确找到应该回给哪个客户端。

### 5.5 上游响应回包

功能入口：`relay.c` 的 `handle_upstream_response`

涉及函数：

| 函数 | 作用 |
| --- | --- |
| `dns_packet_get_id` | 读取上游响应 ID |
| `id_map_take` | 根据上游 ID 找回客户端信息 |
| `dns_packet_set_id` | 把 ID 改回客户端原始 ID |
| `dns_packet_extract_a_for_qname` | 提取 A 记录 |
| `dns_cache_insert` | 写入缓存 |
| `dns_socket_sendto` | 发回客户端 |

执行步骤：

```text
收到上游响应
    |
读取 upstream_id
    |
id_map_take 找 pending
    |
找不到：丢弃
    |
找到：
    |-- ID 改回 client_id
    |-- 尝试提取 A 记录写缓存
    |-- sendto 发给原 client_addr
```

答辩回答：

> 上游响应回来后，程序根据报文里的上游 ID 到 `id_map` 中查找原请求。找到后，把 DNS 报文 ID 改回客户端原始 ID，然后按保存的客户端地址发回去。这样客户端看到的响应 ID 和自己发出的请求 ID 是一致的。

### 5.6 LRU 缓存

功能入口：

| 场景 | 入口 |
| --- | --- |
| 查询缓存 | `handle_client_query` 调用 `dns_cache_lookup` |
| 写入缓存 | `handle_upstream_response` 调用 `dns_cache_insert` |

执行步骤：

```text
查询时：
    |
    |-- 静态表未命中
    |-- 查 dns_cache
    |-- 命中且未过期：构造本地 A 响应
    |-- 未命中或过期：转发上游

上游响应时：
    |
    |-- 从 Answer 中提取 A 记录和 TTL
    |-- 写入 dns_cache
```

答辩回答：

> 缓存使用哈希表加 LRU 双向链表。哈希表负责快速按域名查找，LRU 链表负责记录最近使用顺序。缓存命中时节点移动到链表头部；容量满时淘汰尾部最久未使用节点；同时每条记录保存 TTL 过期时间，过期后会被删除。

### 5.7 超时清理

功能入口：`relay.c` 的 `relay_run`

涉及函数：

| 函数 | 作用 |
| --- | --- |
| `id_map_next_expire_ms` | 计算最近 pending 还有多久过期 |
| `select` | 带超时等待 socket |
| `id_map_expire` | 删除过期 pending |

执行步骤：

```text
每轮循环开始
    |
计算最近 pending 的剩余时间
    |
select 等待 socket 或超时
    |
select 返回后调用 id_map_expire
    |
删除超过 5000ms 的映射
```

答辩回答：

> 每个转发请求都会设置 5 秒过期时间。事件循环每轮根据最近过期时间设置 `select` 的 timeout，醒来后清理超时项。这样即使上游不响应，pending 表也不会无限增长。

---

## 6. 数据结构与数据库说明

本项目没有数据库，也没有数据表。这里的“数据层”主要是内存数据结构。

### 6.1 DNS 静态表 `dns_table`

作用：保存 `dnsrelay.txt` 里的域名规则。

结构：

```text
dns_table
    |
    |-- buckets: 哈希桶数组
            |
            |-- dns_table_entry 链表
                    |-- name
                    |-- ipv4_be
                    |-- next
```

字段：

| 字段 | 含义 |
| --- | --- |
| `name` | 小写域名，不带末尾点 |
| `ipv4_be` | IPv4 网络字节序；为 0 表示拦截 |
| `next` | 哈希冲突链表指针 |

增删改查：

| 操作 | 实现 |
| --- | --- |
| 增 | `dns_table_load_file` 读取文件时插入 |
| 改 | 重复域名后面的记录覆盖前面的记录 |
| 查 | `dns_table_lookup` |
| 删 | 运行时不单独删除，退出时 `dns_table_destroy` 统一释放 |

### 6.2 DNS 缓存 `dns_cache`

作用：缓存上游 DNS 返回的 A 记录。

结构：

```text
dns_cache
    |
    |-- buckets 哈希表：快速定位 qname
    |-- lru_sentinel 双向链表哨兵：维护最近使用顺序
```

字段：

| 字段 | 含义 |
| --- | --- |
| `qname` | 域名 |
| `ipv4_be` | IPv4 地址 |
| `expire_at_ms` | 过期时间 |
| `hash_next` | 哈希冲突链 |
| `lru_prev/lru_next` | LRU 链表 |

增删改查：

| 操作 | 实现 |
| --- | --- |
| 增 | `dns_cache_insert` |
| 改 | 插入同名域名时更新 IP 和过期时间 |
| 查 | `dns_cache_lookup` |
| 删 | 过期删除、容量满时 LRU 淘汰、退出时释放 |

### 6.3 ID 映射表 `id_map`

作用：保存转发请求的上下文。

结构：

```text
id_map
    |
    |-- buckets[256]
            |
            |-- id_map_node 链表
                    |-- id_map_entry
```

字段：

| 字段 | 含义 |
| --- | --- |
| `client_id` | 客户端原始 ID |
| `upstream_id` | 程序分配的上游 ID |
| `client_addr` | 客户端地址 |
| `expire_at_ms` | 超时时间 |
| `qname` | 域名，用于缓存 |

增删改查：

| 操作 | 实现 |
| --- | --- |
| 增 | `id_map_insert` |
| 查并删 | `id_map_take` |
| 超时删 | `id_map_expire` |
| 退出释放 | `id_map_destroy` |

### 6.4 DNS 报文结构

DNS 查询/响应使用 UDP 字节流，不是 C 结构体直接映射。项目用 `uint8_t*` 按字节解析。

简化结构：

```text
Header 12 字节
    |-- ID
    |-- Flags
    |-- QDCOUNT
    |-- ANCOUNT
    |-- NSCOUNT
    |-- ARCOUNT
Question
    |-- QNAME
    |-- QTYPE
    |-- QCLASS
Answer
    |-- NAME
    |-- TYPE
    |-- CLASS
    |-- TTL
    |-- RDLENGTH
    |-- RDATA
```

---

## 7. 接口 / 页面 / 交互说明

### 7.1 页面说明

本项目没有图形页面或 Web 页面。

用户交互方式有两种：

1. 命令行启动程序。
2. 使用 DNS 客户端工具，例如 `nslookup`，向 `127.0.0.1` 发起 DNS 查询。

### 7.2 命令行接口

格式：

```powershell
.\build\dnsrelay.exe [-h|--help] [-d|-dd] [upstream-ip] [config-file]
```

参数说明：

| 参数 | 作用 |
| --- | --- |
| `-h` / `--help` | 打印帮助 |
| `-d` | 一级调试日志 |
| `-dd` | 二级调试日志 |
| `upstream-ip` | 指定上游 DNS IPv4 地址 |
| `config-file` | 指定静态域名表路径 |

例子：

```powershell
.\build\dnsrelay.exe
.\build\dnsrelay.exe -d 220.181.111.1
.\build\dnsrelay.exe -dd 220.181.111.232 dnsrelay.txt
```

### 7.3 DNS 网络接口

监听地址：

```text
UDP 0.0.0.0:53
```

输入：

```text
客户端发来的 DNS Query 报文
```

输出：

```text
DNS Response 报文
```

用户测试：

```powershell
nslookup test1 127.0.0.1
nslookup test0 127.0.0.1
nslookup www.baidu.com 127.0.0.1
```

响应逻辑：

| 用户操作 | 代码响应 |
| --- | --- |
| 查询 `test1` | 静态表命中，返回 `11.111.11.111` |
| 查询 `test0` | 静态表命中 `0.0.0.0`，返回 NXDOMAIN |
| 查询表外域名 | 转发给上游 DNS，响应回来后转给客户端 |
| 第二次查询缓存命中过的域名 | 直接从 `dns_cache` 返回 |

---

## 8. 验收答辩重点

### 8.1 老师可能问的问题和参考回答

**问题 1：这个项目整体是怎么工作的？**

参考回答：

> 程序监听本机 UDP 53 端口，收到 DNS 查询后先解析域名。如果本地域名表命中普通 IP，就直接构造 A 记录响应；如果命中 `0.0.0.0`，就返回 NXDOMAIN；如果静态表没命中，就先查 LRU 缓存，缓存也没有就转发给上游 DNS。上游响应回来后，程序把响应发回原客户端，并提取 A 记录写入缓存。

**问题 2：为什么要修改 Transaction ID？**

参考回答：

> 因为多个客户端可能发出相同 Transaction ID 的 DNS 请求。如果原样转发给上游，响应回来时可能无法区分该发给哪个客户端。本项目为每个转发请求分配新的 `upstream_id`，并在 `id_map` 中保存 `upstream_id -> client_id + client_addr` 的映射。响应回来后再把 ID 改回客户端原始 ID。

**问题 3：怎么防止多客户端响应串包？**

参考回答：

> 每个 pending 请求都保存了客户端地址、客户端端口、原始 ID 和上游 ID。上游响应回来后只能通过 `upstream_id` 找到对应 entry，然后发回这个 entry 中保存的客户端地址。因此即使多个客户端同时查询，也不会串。

**问题 4：为什么用 `select`，没有用多线程？**

参考回答：

> DNS over UDP 的单个请求处理很轻量，项目用单线程 `select` 就能监听 socket，同时通过 timeout 做 pending 超时清理。这样实现简单，避免多线程锁和竞态问题，也符合课程设计规模。

**问题 5：本地表怎么实现域名拦截？**

参考回答：

> 加载 `dnsrelay.txt` 时，如果某条记录 IP 是 `0.0.0.0`，就把内部 `ipv4_be` 保存为 0。查询命中这条记录时，`dns_table_lookup` 返回 `DNS_TABLE_BLOCK`，业务层构造 `RCODE=3`、`ANCOUNT=0` 的 NXDOMAIN 响应。

**问题 6：DNS A 响应是怎么构造的？**

参考回答：

> 程序复制客户端请求的 Header 和 Question，把 flags 设置成响应报文，`ANCOUNT` 设置为 1，然后在 Answer 中写入 `NAME=0xC00C` 指向原 QNAME，`TYPE=1`，`CLASS=1`，`TTL=60`，`RDLENGTH=4`，最后写入 4 字节 IPv4 地址。

**问题 7：缓存如何淘汰？**

参考回答：

> 缓存由哈希表和 LRU 双向链表组成。哈希表负责快速查询，LRU 链表记录访问顺序。查询命中或更新时节点移动到链表头部；容量满时从链表尾部淘汰最久未使用的节点。

**问题 8：为什么只缓存 A 记录？**

参考回答：

> 因为项目本地响应构造目前只实现了 IPv4 A 记录，静态表也是 `域名 -> IPv4`。对于 AAAA、MX 等其他类型，程序没有完整数据结构来正确构造响应，所以直接转发上游，避免返回错误答案。

**问题 9：为什么监听 53 端口需要管理员权限？**

参考回答：

> 53 是 DNS 标准端口，属于系统常用低端口，并且可能被系统服务占用。在 Windows 上绑定这个端口经常需要管理员权限，否则 `bind` 会失败。

**问题 10：如果上游 DNS 不返回怎么办？**

参考回答：

> 每个转发请求在 `id_map` 中都有 `expire_at_ms`，默认 5 秒过期。事件循环会定期调用 `id_map_expire` 删除超时 pending。当前实现对客户端不主动返回 SERVFAIL，而是清理映射并丢弃迟到响应。

### 8.2 项目亮点

1. 模块划分清晰：协议、网络、数据结构、业务逻辑分离。
2. 支持本地解析、域名拦截、透明中继三种核心路径。
3. 使用 Transaction ID 映射解决并发转发问题。
4. 支持 LRU 缓存和 TTL 过期。
5. DNS 报文处理考虑了压缩指针和 CNAME 场景。
6. 单元测试覆盖了核心纯逻辑模块。

### 8.3 项目不足

1. 当前主要支持 IPv4 A 记录，对 AAAA 等类型只转发不缓存。
2. pending 超时后没有主动给客户端返回 SERVFAIL。
3. 单线程模型简单可靠，但极高并发下能力有限。
4. 静态表运行时不支持热更新。
5. 只面向 Windows Winsock2，跨平台支持不完整。

### 8.4 可以如何改进

1. 增加 AAAA 记录支持，支持 IPv6。
2. pending 超时后主动返回 SERVFAIL。
3. 增加配置文件热加载。
4. 支持多上游 DNS 和失败切换。
5. 增加日志文件输出和统计信息。
6. 加入更完整的集成测试。

---

## 9. 学习路线

推荐按这个顺序读代码：

```text
第一步：先跑起来
    |
    |-- README.md
    |-- CMakeLists.txt
    |-- dnsrelay.txt

第二步：看入口和启动
    |
    |-- src/main.c
    |-- src/config.c
    |-- src/platform_win.c

第三步：看主流程
    |
    |-- src/relay.c
        重点看 relay_run、handle_client_query、relay_forward、handle_upstream_response

第四步：看 DNS 协议处理
    |
    |-- include/dns_packet.h
    |-- src/dns_packet.c

第五步：看三个核心数据结构
    |
    |-- src/dns_table.c
    |-- src/id_map.c
    |-- src/dns_cache.c

第六步：看网络封装和工具
    |
    |-- src/socket_io.c
    |-- src/net_util.c
    |-- src/debug.c

第七步：看测试，反向验证理解
    |
    |-- tests/test_main.c
```

阅读重点建议：

1. 先理解 `relay.c` 的三条路径：本地 A 响应、NXDOMAIN 拦截、上游转发。
2. 再理解 `id_map`，这是中继能支持并发的关键。
3. 最后理解 `dns_packet.c` 的字节级处理，这部分细节最多，但答辩时通常讲清结构和关键字段即可。

---

## 10. 10 分钟快速复习版

### 10.1 一句话介绍

这是一个用 C11 + Winsock2 实现的 DNS 中继服务器，监听 UDP 53 端口，支持本地域名解析、域名拦截、上游转发和 LRU 缓存。

### 10.2 必背主流程

```text
main
  -> 解析配置
  -> 初始化 Winsock
  -> relay_run
      -> 绑定 UDP/53
      -> 加载 dnsrelay.txt
      -> 创建 id_map 和 dns_cache
      -> select 循环收包
```

客户端查询：

```text
收到查询
  -> 解析 qname/qtype/qclass
  -> 查静态表
      -> 普通 IP：返回 A 记录
      -> 0.0.0.0：返回 NXDOMAIN
  -> 查缓存
      -> 命中：返回 A 记录
  -> 转发上游
```

上游响应：

```text
收到响应
  -> 根据 upstream_id 查 id_map
  -> 改回 client_id
  -> 提取 A 记录写缓存
  -> 发回原客户端
```

### 10.3 必背核心文件

| 文件 | 一句话 |
| --- | --- |
| `main.c` | 程序入口，初始化后进入 `relay_run` |
| `relay.c` | 核心业务，决定本地响应、拦截、缓存还是转发 |
| `dns_packet.c` | DNS 报文解析和构造 |
| `dns_table.c` | 静态域名表哈希表 |
| `id_map.c` | 客户端 ID 与上游 ID 的映射 |
| `dns_cache.c` | LRU 缓存 |
| `socket_io.c` | UDP socket 收发 |
| `config.c` | 命令行配置 |

### 10.4 必背常量

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `DNS_RELAY_PORT` | `53` | DNS 监听端口 |
| `DNS_RELAY_DEFAULT_UPSTREAM` | `202.106.0.20` | 默认上游 DNS |
| `DNS_RELAY_DEFAULT_TABLE_PATH` | `dnsrelay.txt` | 默认静态表 |
| `DNS_ID_MAP_TIMEOUT_MS` | `5000` | pending 超时 5 秒 |
| `DNS_ID_MAP_MAX_PENDING` | `256` | 最大 pending 数 |
| `DNS_CACHE_DEFAULT_CAPACITY` | `256` | 默认缓存容量 |
| `DNS_LOCAL_TTL` | `60` | 本地构造响应 TTL |
| `DNS_UDP_BUF_SIZE` | `512` | DNS UDP 报文缓冲区 |

### 10.5 答辩最重要的三句话

1. **本地域名表优先**：命中普通 IP 就构造 A 响应，命中 `0.0.0.0` 就返回 NXDOMAIN。
2. **中继靠 ID 映射防串包**：转发前改成新的 `upstream_id`，响应回来后查 `id_map`，再改回客户端原始 ID。
3. **缓存用哈希表 + LRU 链表**：哈希表快速查找，LRU 链表负责满容量时淘汰最久未使用记录，并根据 TTL 判断过期。

### 10.6 验收演示命令

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\dnsrelay_test.exe
.\build\dnsrelay.exe -d 220.181.111.1
```

另开终端：

```powershell
nslookup test1 127.0.0.1
nslookup test0 127.0.0.1
nslookup www.baidu.com 127.0.0.1
```

预期：

```text
test1：返回 11.111.11.111
test0：返回 Non-existent domain
www.baidu.com：通过上游 DNS 正常解析
```
