# Release 2 测试流程（本地应答 + 拦截 + 透明中继）

本节用于验证 **dns_table + 本地 A/NXDOMAIN 应答**已按 PRD/FR-02~FR-06 实现，同时确认表外域名仍会透明转发上游。

---

## 0. 前置条件

- Windows x64，已安装 CMake + MinGW-w64
- 需要管理员 PowerShell：程序将绑定 UDP `53`
- 当前工作目录为 `dns_relay/`（确保默认 `dnsrelay.txt` 能找到）
- 已知或可达上游 DNS（示例：`220.181.111.1`）

---

## 1. 编译（与 Release 1 相同）

```powershell
cd dns_relay
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## 2. 自动化单元测试（无需管理员，不占 53 端口）

```powershell
.\build\dnsrelay_test.exe
```

通过标准：

```text
Results: X run, 0 failed
```

---

## 3. 端到端验收（需管理员）

### 3.1 启动

管理员 PowerShell：

```powershell
cd <仓库>\dns_relay
.\build\dnsrelay.exe -d 220.181.111.1
```

启动成功标志（stderr）：

```text
dnsrelay: listening UDP/53, upstream <IP>:53 (Release 2 relay)
```

### 3.2 本地命中（FR-03）

```powershell
nslookup test1 127.0.0.1
```

预期：

- 返回 `11.111.11.111`
- 程序日志中应出现 `relay: local A hit ... qname=test1`

### 3.3 拦截 NXDOMAIN（FR-04）

```powershell
nslookup test0 127.0.0.1
```

预期：

- `Non-existent domain`（或等价错误）
- 程序日志中应出现 `relay: local NXDOMAIN block ... qname=test0`

### 3.4 本地命中 2（FR-03）

```powershell
nslookup test2 127.0.0.1
```

预期：

- 返回 `22.22.222.222`

### 3.5 表外中继（FR-05）

```powershell
nslookup www.baidu.com 127.0.0.1
```

预期：

- 返回值与直接查询 `220.181.111.1` 结果一致
- 日志中出现 `relay: id_map ...` 与 `relay -> upstream id ...`

---

## 4. Wireshark（可选）

过滤器：

`udp.port == 53`

观察点：

- 本地命中应答：客户端发往 53 后应直接收到应答；Wireshark 中不应出现同名查询发往上游
- 转发应答：客户端侧 `ID` 与上游侧 `ID` 不相同，回包后客户端侧 `ID`恢复

