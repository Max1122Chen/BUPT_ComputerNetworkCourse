# Release 3 测试流程（LRU 内存缓存）

面向组内成员：在 **Release 2 已通过** 的前提下，按顺序执行即可验证 **三期** 是否工作正常。  
三期在 R2 的「表 → 中继」链路上增加 **内存 LRU 缓存**（FR-10）：表外 A 记录中继成功后写入缓存，未过期时直接应答，减少重复向上游询问。

详细用例表见 [test-plan.md](./test-plan.md)（P3-T01～T02）。

---

## 0. 前置条件

- Windows x64，已安装 **CMake**、**MinGW GCC**
- 需要 **管理员** PowerShell：程序绑定 UDP `53`
- 当前工作目录为 `dns_relay/`（默认加载 `dnsrelay.txt`）
- **Release 2 行为正常**（本地命中、拦截、表外中继）；见 [testing-release2.md](./testing-release2.md)
- 已知或可达上游 DNS（示例：`220.181.111.1`、`220.181.111.232`）
- 验证缓存命中时建议开 **Wireshark**（过滤器 `udp.port == 53`）

---

## 1. 编译（与 Release 1/2 相同）

```powershell
cd dns_relay
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

应生成：

- `build/dnsrelay.exe` — 中继程序（含缓存）  
- `build/dnsrelay_test.exe` — 单元测试  

---

## 2. 自动化单元测试（推荐先做）

**不需要** 管理员权限，**不占用** 53 端口。

```powershell
.\build\dnsrelay_test.exe
```

**通过标准**：

```text
Results: 16 run, 0 failed
```

或：

```powershell
ctest --test-dir build
```

三期相关用例（`tests/test_main.c`）：

| 用例名 | 验证点 |
|--------|--------|
| `dns_cache_insert_lookup` | 插入后命中；域名大小写不敏感 |
| `dns_cache_lru_eviction` | 超容量淘汰 **最久未使用** 项 |
| `dns_cache_expire` | TTL 到期后视为 MISS 并删除 |

---

## 3. 端到端验收（需管理员）

### 3.1 启动

管理员 PowerShell：

```powershell
cd <仓库>\dns_relay
.\build\dnsrelay.exe -d 220.181.111.1
```

需要观察缓存插入细节时，可用 `-dd`：

```powershell
.\build\dnsrelay.exe -dd 220.181.111.1
```

**启动成功标志**（stderr）：

```text
dnsrelay: listening UDP/53, upstream <IP>:53 (Release 3 relay)
relay: started debug_level=1
```

若出现 `bind UDP/53 failed`：换管理员终端，或检查 53 是否被其它程序占用。

### 3.2 缓存未命中 → 中继（P3-T01 第一步）

对 **表外** 域名首次查询：

```powershell
nslookup www.baidu.com 127.0.0.1
```

**预期**：

- 返回与直连同一上游一致的结果
- 日志出现 `relay -> upstream id ... qname=www.baidu.com`
- 随后 `relay <- upstream id ...`
- Wireshark：可见本机 → 上游的 DNS 查询

使用 `-dd` 时，中继成功且响应含匹配 A 记录，还可看到：

```text
dns_cache: insert qname=www.baidu.com ttl=<n> size=<n>
```

### 3.3 缓存命中（P3-T01 / FR-10）

**同一域名** 立即再查一次：

```powershell
nslookup www.baidu.com 127.0.0.1
```

**预期**：

- 解析结果与第一次 **相同**（IP/CNAME 一致）
- 日志出现 `relay: cache A hit ... qname=www.baidu.com`
- **无** `relay -> upstream` 成对日志
- Wireshark：**第二次** 查询后 **不应** 出现同名查询发往上游

### 3.4 Release 2 行为仍成立（回归）

确认缓存 **不替代** 配置文件查找：

```powershell
nslookup test1 127.0.0.1
nslookup test0 127.0.0.1
```

**预期**：

- `test1` → `11.111.11.111`，日志为 `relay: local A hit`（**不是** `cache A hit`）
- `test0` → `Non-existent domain`，日志为 `relay: local NXDOMAIN block`
- 两次查询 Wireshark 均 **无** 上游流量

### 3.5 非 A 查询不走缓存（FR-06）

```powershell
nslookup -type=MX www.baidu.com 127.0.0.1
nslookup -type=MX www.baidu.com 127.0.0.1
```

**预期**：

- 两次均走中继，行为与直连上游一致
- 每次均有 `relay -> upstream` / `relay <- upstream`
- `-dd` 下可见 `relay: skip cache for non-A query ...`

### 3.6 Wireshark（推荐，答辩加分）

1. 过滤器：`udp.port == 53`
2. 对表外 A 记录域名连续查询两次：
   - 第一次：客户端 → 本机 53 → **上游** → 本机 → 客户端
   - 第二次：客户端 → 本机 53 → **直接应答**（无上游段）
3. 本地表项（`test1`）与缓存命中均应 **无** 向上游的同名查询

### 3.7 LRU 淘汰（P3-T02）

默认运行时缓存容量为 **256**（`DNS_CACHE_DEFAULT_CAPACITY`），端到端不易手工灌满。

**推荐**：以单元测试 `dns_cache_lru_eviction` 为准（见 §2）。

若需在实机观察淘汰，须使用 **改小容量后重新编译** 的二进制（仅调试用途），再按「填满 → 访问其中一项 → 插入新项 → 最久未访问项失效」流程验证；细节见 TDD §5.7。

### 3.8 停止

在运行 `dnsrelay` 的窗口按 **Ctrl+C**，应输出 `dnsrelay: stopped`。缓存随进程退出释放，**不会** 写回 `dnsrelay.txt`。

---

## 4. 可选测试

| 项 | 操作 | 预期 |
|----|------|------|
| TTL 过期 | 单元测试 `dns_cache_expire`；或等待某条缓存 TTL 过后再查 | 过期后重新走上游；`-dd` 可见再次 `dns_cache: insert` |
| 并发 | 快速两次 `nslookup` 不同表外域名 | 两次均正确；各自可独立进入缓存 |
| 表外 + 本地混合 | 交替查 `test1` 与 `www.baidu.com` | 本地走表、表外可走缓存，互不干扰 |
| 大小写 | 先查 `WWW.BAIDU.COM`，再查 `www.baidu.com` | 第二次命中缓存（规范化后同一键） |

---

## 5. 三期 **不** 测 / 易混淆点

| 现象 | 说明 |
|------|------|
| `test1` 显示 `cache A hit` | 错误；表项优先，应为 `local A hit` |
| 非 A 查询第二次仍走上游 | **正确**；缓存仅针对 A 记录 |
| 缓存写入 `dnsrelay.txt` | **不会**；仅内存，进程结束即失 |
| NXDOMAIN / 无 A 的上游响应 | 不插入缓存；`-dd` 可见 `skip cache insert` |
| 课程最低验收 | 仍以 **Release 2** 为准；R3 为加分扩展 |

**查找顺序（Release 3）**：配置文件 → 内存缓存 → 中继上游。

---

## 6. 快速验收清单（打勾）

- [ ] `dnsrelay_test` 16/16 通过（含 3 个 `dns_cache_*`）  
- [ ] 管理员启动 `dnsrelay -d <上游>` 显示 `(Release 3 relay)`  
- [ ] 表外域名 **首次** 查询有 `relay -> upstream`  
- [ ] **同一表外域名第二次** 有 `cache A hit` 且无上游查询（Wireshark）  
- [ ] `test1` / `test0` 仍为本地应答，不经缓存  
- [ ] `-type=MX` 连续两次均走上游  

---

## 7. 相关文档

- [test-plan.md](./test-plan.md) — 用例编号 P3-Txx  
- [design.md](./design.md) — FR-10、Release 3 发布计划  
- [design-technical.md](./design-technical.md) — `dns_cache` API、解析链 §6  
- [testing-release2.md](./testing-release2.md) — 二期基线验收  
- [README.md](../README.md) — 构建摘要  
