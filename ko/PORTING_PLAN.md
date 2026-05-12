# Hysteria → FreeBSD KLD 1:1 移植总计划（Master Porting Plan）

> 目标：协议语义（wire semantics）、拥塞控制行为（control-law behavior）、恢复路径（recovery behavior）、配置语义（config semantics）、可观测性（observability）达到与原 Go 实现一致（或有可证明差异）。

## 完整度计分规则

- **0%**: 未开始
- **1~30%**: 仅有接口/骨架
- **31~60%**: 子路径可运行，但语义不完整
- **61~85%**: 主路径已对齐，边界/异常场景不完整
- **86~99%**: 几乎完整，仅剩长尾与性能调优
- **100%**: 功能、语义、测试、性能全部达标

---

## A. 架构与构建（Architecture & Build）

当前完整度：**78%**

### 已完成
- [x] KLD 构建骨架（`ko/Makefile` + `bsd.kmod.mk`）
- [x] `SYSDIR` 自动探测（`/usr/src/sys`）
- [x] 基础模块生命周期（`MOD_LOAD` / `MOD_UNLOAD`）

### TODO
- [~] 支持按 FreeBSD 版本矩阵构建（13/14/15）（已落地13.4/14.3 CI矩阵，15待补）
- [x] 增加 kernel option / feature gate（如 INET6, OCF）
- [x] 增加 release + debug profile（`INVARIANTS`/`WITNESS`）
- [~] CI：FreeBSD VM 自动构建 + kldload smoke（已完成自动构建，kldload smoke待补）

---

## B. 协议平面（Protocol Plane）

当前完整度：**78%**

### 已完成
- [x] QUIC varint 编解码（读/写）
- [x] UDPMessage 解析（session_id/packet_id/frag/addr/data）
- [x] TCPRequest 解析（frame type、addr、padding）
- [x] TCPResponse 构建（status + msg_len + msg + padding_len）

### TODO（必须 1:1 对齐）
- [x] TCPRequest 构建函数（与 Go `WriteTCPRequest` 行为一致）
- [x] TCPResponse 解析函数（与 Go `ReadTCPResponse` 对齐）
- [~] padding 策略迁移（随机区间与分布，已完成字符集与区间采样，待对齐分布细节）
- [~] Auth header 序列化语义迁移（`Hysteria-*`，已完成 request/response 解析与 rx=auto 语义，待 HTTP 适配层接线）
- [~] 协议错误码映射与可观测日志（已完成错误分类与sysctl计数器，待结构化日志）
- [~] 边界 case：空消息、大包、截断包、恶意 varint（已加入内核自测入口覆盖部分case）

---

## C. 会话与状态管理（Session & State）

当前完整度：**54%**

### 已完成
- [x] 会话哈希表（bucket + mutex）
- [x] lookup / get-or-create 生命周期骨架

### TODO
- [~] 引用计数（refcount）与延迟回收（deferred free）（已完成refcount生命周期，待加入延迟回收队列）
- [ ] 会话状态机（handshake/auth/ready/closing）
- [ ] 定时器：空闲超时、重传、保活
- [ ] 分片重组缓存（packet_id + frag tuple）
- [ ] 内存上限与逐出策略（eviction policy）

---

## D. 传输平面（Transport Plane）

当前完整度：**12%**

### 已完成
- [x] `hysteria_net_init/fini` 入口骨架

### TODO
- [ ] UDP 收发接入（netisr/socket hook 方案二选一并固化）
- [ ] PMTUD（Path MTU Discovery）迁移
- [ ] 分片发送/重组与乱序处理
- [ ] 重传调度器（RTO/ACK 驱动）
- [ ] 流量整形与 pacing 接口接线

---

## E. 拥塞控制（Congestion Control / BBR + Brutal）

当前完整度：**58%**

### 已完成
- [x] 基础状态机：startup/drain/probe_bw/probe_rtt
- [x] full_bw 探测阈值
- [x] probe_bw gain cycle（8 相位）
- [x] probe_rtt 进入周期与 dwell-time 退出
- [x] 关键观测：mode / bw / min_rtt sysctl

### TODO（对齐 Go BBR 语义）

### Brutal 子系统（`core/internal/congestion/brutal`）

当前完整度：**34%**

#### TODO（必须 1:1 对齐）
- [~] 迁移 `BrutalSender` 状态变量与初始化路径（bps 驱动，已完成基础状态与初始化）
- [~] 迁移 `pacer` 预算模型与 `HasPacingBudget/TimeUntilSend` 语义（已具备ACK/Loss采样注入链路）
- [~] 迁移 `CanSend/GetCongestionWindow` 的 inflight 判定公式（已完成 cwnd 计算，待接入发送路径）
- [~] 迁移 RTT provider 接线与 ACK 反馈回路（已完成ACK率采样窗口，待RTT provider接线）
- [~] 与配置平面打通：`Tx>0` 时优先 Brutal，`Tx=0` 回退 BBR（已提供sysctl动态参数入口）
- [ ] 对齐客户端/服务端选择逻辑（`core/client/client.go` 与 server 对称路径）
- [ ] 增加 Brutal 专项差分回归（Go vs KLD）

- [ ] 轮次（round trip counting）驱动的周期推进
- [ ] inflight model（cwnd 上下界）
- [ ] ACK aggregation 模型
- [ ] app-limited 样本处理
- [ ] pacing 与发送路径闭环联动
- [ ] BBR 参数可调与回归曲线对比

---

## F. 安全平面（Security Plane）

当前完整度：**3%**

### 已完成
- [x] 结构占位（接口层面）

### TODO
- [ ] AEAD 加解密（nonce 管理、key rotation）
- [ ] replay window
- [ ] OCF / ktls 能力探测与策略
- [ ] 密钥生命周期与零化（zeroization）
- [ ] 错误注入与抗 DoS 限流

---

## G. 控制平面与配置（Control Plane & Config）

当前完整度：**35%**

### 已完成
- [x] `net.hysteria.*` sysctl 树
- [x] 统计计数器（packets/drops）
- [x] BBR 运行态观测

### TODO
- [ ] 配置热更新（atomic swap + rollback）
- [ ] ACL / 认证策略下沉
- [ ] 结构化日志与 tracepoint
- [ ] 管理命令通道（可选 ioctl/sysctl 混合）

---

## H. 可观测性与诊断（Observability & Diagnostics）

当前完整度：**34%**

### 已完成
- [x] 基础 sysctl 观测

### TODO
- [ ] 细粒度计数器（per-session/per-error/per-mode）
- [ ] DTrace/ktr tracepoint
- [ ] 速率直方图（RTT/BW/jitter）
- [ ] panic-safe 最小诊断面

---

## I. 测试与验证（Verification）

当前完整度：**18%**

### 已完成
- [x] 文档化的手工构建与排障流程

### TODO
- [ ] 差分回归（Go vs KLD，golden vectors）
- [ ] 协议 fuzz（varint/frame/parser/state transitions）
- [ ] 故障注入（drop/reorder/replay/clock skew）
- [ ] 压测（pps、吞吐、长连稳定性）
- [ ] FreeBSD 版本矩阵回归

---

## J. 性能目标（Performance SLO）

当前完整度：**8%**

### 目标
- [ ] p99 延迟不劣于 Go 实现 + 10%
- [ ] 单核 pps 达到目标硬件基线
- [ ] 长时运行（24h）无内存泄漏/锁争用异常

### TODO
- [ ] 锁分片优化与 cacheline 对齐
- [ ] mbuf copy 减少（zero-copy 优先）
- [ ] hot path 分支预测与内联优化

---

## 总体进度汇总

- 架构与构建：40%
- 协议平面：52%
- 会话管理：45%
- 传输平面：12%
- BBR：58%
- Brutal：0%
- 安全平面：3%
- 控制平面：35%
- 可观测性：22%
- 测试验证：18%
- 性能收敛：8%

**全局加权完整度（当前估算）**：**28%**

---

## 执行顺序（接下来 4 个里程碑）

### M1（协议闭环）
- 完成 TCP request/response 双向编解码 + auth header 语义 + padding 策略

### M2（传输闭环）
- 接入 UDP 收发路径 + 分片重组 + 重传调度

### M3（拥塞控制闭环）
- BBR round/inflight/app-limited 全量迁移 + pacing 联动
- BrutalSender 全量迁移 + 与配置路径联动

### M4（安全与验证闭环）
- AEAD/replay + 差分回归 + fuzz + 压测
