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

### D. `Unable to locate the kernel source tree. Set SYSDIR to override.`

这是 FreeBSD `bsd.kmod.mk` 的典型错误，表示系统缺少可见的内核源码树（`sys/`）。

**修复路径（推荐顺序）**

1. 检查是否存在内核源码：

```sh
ls /usr/src/sys
```

2. 若不存在，安装/同步 FreeBSD src tree（任选其一，按你的环境）：

```sh
# 方式 A: 使用 release 提供的 src（若系统启用）
bsdinstall distfetch && bsdinstall distextract

# 方式 B: 使用 git 同步 src
git clone --depth=1 -b releng/$(freebsd-version -k | cut -d- -f1) https://git.FreeBSD.org/src.git /usr/src
```

3. 显式指定 `SYSDIR` 后重试：

```sh
make clean SYSDIR=/usr/src/sys
make SYSDIR=/usr/src/sys
```

> 本仓库 `ko/Makefile` 已增加自动探测：若 `/usr/src/sys` 存在会自动设定 `SYSDIR`；若不存在仍会报该错误，这是正确行为。


## 6) 构建配置面（profile / feature gate）

```sh
# release
make PROFILE=release SYSDIR=/usr/src/sys

# debug (INVARIANTS/WITNESS 宏用于额外检查)
make PROFILE=debug SYSDIR=/usr/src/sys

# feature gate
make WITH_INET6=1 WITH_OCF=0 PROFILE=release SYSDIR=/usr/src/sys
```

- `PROFILE`: `release` / `debug`
- `WITH_INET6`: 是否启用 IPv6 相关编译宏
- `WITH_OCF`: 是否启用 OCF 相关编译宏（占位开关）

CI 已加入 FreeBSD 构建矩阵（13.4/14.3 + release/debug）。
