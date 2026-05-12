# hysteria FreeBSD Kernel Module (KLD)

该目录是将 hysteria 迁移到 FreeBSD 内核模块（kernel module, KLD）的 C 语言骨架。

## 1) 在 FreeBSD 原生环境尝试编译（推荐）

> 核心结论：该 `Makefile` 使用 `bsd.kmod.mk`，应在 **FreeBSD 的 bmake 工具链**中构建，而不是 GNU make。

```sh
cd /path/to/hysteria/ko
make clean
make
```

成功后应生成：

- `hysteria.ko`
- `hysteria.kld`
- 对应 `.o` / `.ko.full` 等中间产物（版本相关）

### 快速验证加载

```sh
sudo kldload ./hysteria.ko
kldstat | grep hysteria
sysctl net.hysteria.mode
sudo kldunload hysteria
```

## 2) 常见失败模式与定位

### A. 在 Linux 上执行 `make -C ko` 报语法错（如 missing separator）

原因：Linux 默认 `make` 是 GNU make，`bsd.kmod.mk` 语法与 GNU make 不兼容。

结论：请在 FreeBSD 系统中使用 `make`（bmake）构建。

### B. `kldload` 失败：`Exec format error` / `Unsupported file type`

原因：模块与当前内核版本（kernel ABI）不匹配，或并非在对应 FreeBSD 版本构建。

处理：在目标系统本机重新编译，确保 `freebsd-version -k` 与构建环境一致。

### C. `kldload` 失败：符号未解析

原因：源码使用了当前内核未导出的符号，或头文件/选项不匹配。

处理：
1. 查看 `dmesg -a | tail -n 200`
2. 检查 `src/*.c` 是否引用了不可见符号
3. 通过 `make clean && make` 重新构建

## 3) 构建前最小检查清单

```sh
uname -a
freebsd-version -kru
which make
make -V .MAKE_VERSION
```

如果 `which make` 不在 FreeBSD 基础系统路径（通常 `/usr/bin/make`），优先修正环境。

## 4) 当前模块能力（骨架阶段）

- 模块生命周期：`MOD_LOAD` / `MOD_UNLOAD`
- sysctl 节点：`net.hysteria.mode` / `stats_packets` / `stats_drops`
- 协议解析入口：`hysteria_proto_parse()`
- BBR 采样入口：`hysteria_bbr_on_ack()`

## 5) 后续迁移路线（工程化）

1. 会话层（session lifecycle）→ 内核对象池（UMA zones）
2. 分片与重组 → mbuf 链（mbuf chain）
3. 密码学路径 → 内核原语或最小内核面 + 用户态协处理
4. 互通测试矩阵（旧实现 vs KLD）+ 失效注入（fault injection）
