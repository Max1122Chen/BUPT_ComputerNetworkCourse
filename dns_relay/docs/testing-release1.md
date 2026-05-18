# Release 1 测试流程（透明中继）

面向组内成员：按顺序执行即可验证 **一期** 是否工作正常。  
一期 **不读** `dnsrelay.txt`，所有查询均 **转发上游**（仅改写 DNS 报头 ID）。

详细用例表见 [test-plan.md](./test-plan.md)（P1-T01～T06）。

---

## 0. 前置条件

- Windows x64，已安装 **CMake**、**MinGW GCC**
- 仓库路径：`dns_relay/`
- 测试上游示例（已验证可用）：
  - `220.181.111.1`
  - `220.181.111.232`
  - 或 `ipconfig /all` 中你当前可用的校园/公网 DNS

---

## 1. 编译

```powershell
cd dns_relay
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

应生成：

- `build/dnsrelay.exe` — 中继程序  
- `build/dnsrelay_test.exe` — 单元测试  

---

## 2. 自动化单元测试（推荐先做）

**不需要** 管理员权限，**不占用** 53 端口。

```powershell
.\build\dnsrelay_test.exe
```

**通过标准**：

```text
Results: 6 run, 0 failed
```

或：

```powershell
ctest --test-dir build
```

覆盖模块：`dns_packet`、`id_map`、`config`、`net_util`（见 `tests/test_main.c`）。

---

## 3. 端到端中继测试（需管理员）

### 3.1 启动

以 **管理员身份** 打开 PowerShell：

```powershell
cd <仓库>\dns_relay
.\build\dnsrelay.exe -d 220.181.111.1
```

将上游换成 `220.181.111.232` 或其它可达 DNS 亦可。

**启动成功标志**（stderr）：

```text
dnsrelay: listening UDP/53, upstream <IP>:53 (Release 1 relay)
relay: started debug_level=1
```

若出现 `bind UDP/53 failed`：换管理员终端，或检查 53 是否被其它程序占用。

### 3.2 发查询（可不改系统 DNS）

```powershell
nslookup www.baidu.com 127.0.0.1
nslookup www.bupt.edu.cn 127.0.0.1
```

### 3.3 与直连上游对比（判断结果是否正确）

```powershell
nslookup www.baidu.com 220.181.111.1
nslookup www.bupt.edu.cn 220.181.111.232
```

**通过标准**：经 `127.0.0.1` 的解析结果与 **同一上游** 直连结果一致（IP/CNAME 相同）。

### 3.4 看程序日志（`-d`）

每次查询应出现 **成对** 日志，例如：

```text
#N relay -> upstream id <client_id>-><upstream_id> client ... qname=www.example.com
#N relay <- upstream id <upstream_id>-><client_id> client ...
```

说明：若两个 ID 数字碰巧相同（如 `2->2`），仍可能已改写；要以 Wireshark 为准。

### 3.5 Wireshark（可选，答辩推荐）

1. 过滤器：`udp.port == 53`
2. 观察：本机 ↔ 上游 的查询 **ID** 与 本机 ↔ 客户端 的 **ID** 不同（转发时），回包时客户端侧 ID 恢复。

### 3.6 停止

在运行 `dnsrelay` 的窗口按 **Ctrl+C**，应输出 `dnsrelay: stopped`。

---

## 4. 可选测试

| 项 | 操作 | 预期 |
|----|------|------|
| 上游故障 | `dnsrelay -d 192.0.2.1`（不可达）再 nslookup | 约 5s 日志 `id_map: timeout`；进程不退出；客户端超时 |
| 并发 | 快速两次 nslookup 不同域名 | 两个结果均正确 |
| 他机 | 他机 DNS 设为本机局域网 IP | 他机 nslookup 正常 |

---

## 5. 一期 **不** 测的内容（避免误判）

| 现象 | 说明 |
|------|------|
| `test0` / `test1` 本地秒回 | 属 **Release 2**（读表） |
| `0.0.0.0` 拦截 | 属 **Release 2** |
| 第二次查询不走上游 | 属 **Release 3**（缓存） |

---

## 6. 快速验收清单（打勾）

- [ ] `dnsrelay_test` 6/6 通过  
- [ ] 管理员启动 `dnsrelay -d <上游>` 无 bind 错误  
- [ ] `nslookup <域名> 127.0.0.1` 有正常解析  
- [ ] 与直连同一上游结果一致  
- [ ] 日志有成对 `relay ->` / `relay <-`  

---

## 7. 相关文档

- [test-plan.md](./test-plan.md) — 用例编号 P1-Txx  
- [design.md](./design.md) — Release 1 产品说明  
- [README.md](../README.md) — 构建摘要  
