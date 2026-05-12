# hysteria FreeBSD Kernel Module (KLD)

该目录提供将项目核心链路下沉为 FreeBSD 内核模块（kernel module, KLD）的**初始重写骨架**，目标是：

- 内核态 UDP 数据通道（kernel-space UDP datapath）
- 简化版协议封装/拆封（framing）
- 内核态拥塞控制（congestion control，当前为 BBR 风格占位实现）
- sysctl 参数平面与运行时统计

> 注意：原仓库为大型 Go 用户态系统，完整“逐行等价重写”需要多阶段工程化迁移（协议一致性验证、fuzz、kTLS/crypto 适配、兼容性回归）。本目录先落地可编译的 KLD 框架。

## 构建

在 FreeBSD 上执行：

```sh
cd ko
make
```

加载/卸载：

```sh
sudo kldload ./hysteria.ko
sudo kldunload hysteria
```

## 当前模块能力

- `hysteria_mode` sysctl：运行模式开关（默认 `1`）
- `hysteria_stats_packets`：收包计数
- `hysteria_stats_drops`：丢包计数
- 协议解析入口 `hysteria_proto_parse()`
- BBR 状态机占位 `hysteria_bbr_on_ack()`

## 后续迁移路线（建议）

1. 将用户态会话层（session lifecycle）映射为内核对象池（UMA zones）。
2. 用 mbuf 链重写分片、重组、填充与 ACK 处理路径。
3. 将加密从用户态库迁移到内核可用原语（或保持最小内核面 + userland crypto helper）。
4. 建立协议互通测试矩阵（旧实现 vs KLD 实现）。
