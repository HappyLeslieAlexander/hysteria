#include "hysteria_mod.h"

static struct sysctl_ctx_list hysteria_sysctl_ctx;
static struct sysctl_oid *hysteria_sysctl_tree;



static int
sysctl_hysteria_proto_selftest(SYSCTL_HANDLER_ARGS)
{
    int trigger = 0;
    int err = sysctl_handle_int(oidp, &trigger, 0, req);
    if (err != 0 || req->newptr == NULL)
        return (err);

    if (trigger != 1)
        return (EINVAL);

    return hysteria_proto_selftest();
}



static uint64_t g_brutal_bps_cfg = 10000000;
static uint64_t g_brutal_rtt_us_cfg = 0;

static uint64_t g_brutal_inject_ack = 0;
static uint64_t g_brutal_inject_loss = 0;

static int
sysctl_hysteria_brutal_inject_ack(SYSCTL_HANDLER_ARGS)
{
    uint64_t v = g_brutal_inject_ack;
    int err = sysctl_handle_64(oidp, &v, 0, req);
    if (err != 0 || req->newptr == NULL)
        return (err);
    g_brutal_inject_ack = v;
    return (0);
}

static int
sysctl_hysteria_brutal_inject_loss(SYSCTL_HANDLER_ARGS)
{
    uint64_t v = g_brutal_inject_loss;
    int err = sysctl_handle_64(oidp, &v, 0, req);
    if (err != 0 || req->newptr == NULL)
        return (err);
    g_brutal_inject_loss = v;
    return (0);
}

static int
sysctl_hysteria_brutal_apply_sample(SYSCTL_HANDLER_ARGS)
{
    int trigger = 0;
    int err = sysctl_handle_int(oidp, &trigger, 0, req);
    struct timeval tv;

    if (err != 0 || req->newptr == NULL)
        return (err);
    if (trigger != 1)
        return (EINVAL);

    microuptime(&tv);
    hysteria_brutal_on_congestion_event((uint64_t)tv.tv_sec, g_brutal_inject_ack, g_brutal_inject_loss);
    return (0);
}

static int
sysctl_hysteria_brutal_bps(SYSCTL_HANDLER_ARGS)
{
    uint64_t v = g_brutal_bps_cfg;
    int err = sysctl_handle_64(oidp, &v, 0, req);
    if (err != 0 || req->newptr == NULL)
        return (err);
    if (v == 0)
        return (EINVAL);
    g_brutal_bps_cfg = v;
    hysteria_brutal_set_bps(v);
    return (0);
}

static int
sysctl_hysteria_brutal_rtt_us(SYSCTL_HANDLER_ARGS)
{
    uint64_t v = g_brutal_rtt_us_cfg;
    int err = sysctl_handle_64(oidp, &v, 0, req);
    if (err != 0 || req->newptr == NULL)
        return (err);
    g_brutal_rtt_us_cfg = v;
    hysteria_brutal_set_rtt_us(v);
    return (0);
}

static int
sysctl_hysteria_brutal_cwnd(SYSCTL_HANDLER_ARGS)
{
    struct hysteria_brutal_snapshot snap;
    hysteria_brutal_get_snapshot(&snap);
    return sysctl_handle_64(oidp, &snap.cwnd_bytes, 0, req);
}

static int
sysctl_hysteria_brutal_ack_rate(SYSCTL_HANDLER_ARGS)
{
    struct hysteria_brutal_snapshot snap;
    hysteria_brutal_get_snapshot(&snap);
    return sysctl_handle_64(oidp, &snap.ack_rate_q16, 0, req);
}

static int
sysctl_hysteria_bbr_bw(SYSCTL_HANDLER_ARGS)
{
    struct hysteria_bbr_snapshot snap;
    hysteria_bbr_get_snapshot(&snap);
    return sysctl_handle_64(oidp, &snap.bw_bytes_per_s, 0, req);
}

static int
sysctl_hysteria_bbr_min_rtt(SYSCTL_HANDLER_ARGS)
{
    struct hysteria_bbr_snapshot snap;
    hysteria_bbr_get_snapshot(&snap);
    return sysctl_handle_64(oidp, &snap.min_rtt_us, 0, req);
}

static int
sysctl_hysteria_bbr_mode(SYSCTL_HANDLER_ARGS)
{
    struct hysteria_bbr_snapshot snap;
    hysteria_bbr_get_snapshot(&snap);
    int mode = (int)snap.mode;
    return sysctl_handle_int(oidp, &mode, 0, req);
}

int
hysteria_ctl_init(void)
{
    sysctl_ctx_init(&hysteria_sysctl_ctx);

    hysteria_sysctl_tree = SYSCTL_ADD_NODE(
        &hysteria_sysctl_ctx,
        SYSCTL_STATIC_CHILDREN(_net),
        OID_AUTO,
        "hysteria",
        CTLFLAG_RW,
        0,
        "hysteria kernel module");

    if (hysteria_sysctl_tree == NULL)
        return (ENOMEM);

    SYSCTL_ADD_INT(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "mode", CTLFLAG_RW, &g_hysteria_mode, 0,
        "hysteria runtime mode");

    SYSCTL_ADD_U64(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "stats_packets", CTLFLAG_RD, &g_hysteria_stats.packets, 0,
        "received packets");

    SYSCTL_ADD_U64(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "stats_drops", CTLFLAG_RD, &g_hysteria_stats.drops, 0,
        "dropped packets");
    SYSCTL_ADD_U64(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "proto_err_varint", CTLFLAG_RD, &g_hysteria_stats.proto_err_varint, 0,
        "protocol varint parse errors");
    SYSCTL_ADD_U64(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "proto_err_bounds", CTLFLAG_RD, &g_hysteria_stats.proto_err_bounds, 0,
        "protocol bounds errors");
    SYSCTL_ADD_U64(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "proto_err_format", CTLFLAG_RD, &g_hysteria_stats.proto_err_format, 0,
        "protocol format errors");

    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "bbr_mode", CTLTYPE_INT | CTLFLAG_RD, NULL, 0,
        sysctl_hysteria_bbr_mode, "I", "current bbr mode");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "bbr_bw", CTLTYPE_U64 | CTLFLAG_RD, NULL, 0,
        sysctl_hysteria_bbr_bw, "QU", "bbr bandwidth estimate bytes/s");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "bbr_min_rtt_us", CTLTYPE_U64 | CTLFLAG_RD, NULL, 0,
        sysctl_hysteria_bbr_min_rtt, "QU", "bbr min rtt in microseconds");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "proto_selftest", CTLTYPE_INT | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_proto_selftest, "I", "write 1 to run protocol selftest");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_cwnd", CTLTYPE_U64 | CTLFLAG_RD, NULL, 0,
        sysctl_hysteria_brutal_cwnd, "QU", "brutal congestion window");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_ack_rate_q16", CTLTYPE_U64 | CTLFLAG_RD, NULL, 0,
        sysctl_hysteria_brutal_ack_rate, "QU", "brutal ack rate in q16");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_bps", CTLTYPE_U64 | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_brutal_bps, "QU", "brutal target bandwidth bps");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_rtt_us", CTLTYPE_U64 | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_brutal_rtt_us, "QU", "brutal smoothed rtt (us)");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_inject_ack", CTLTYPE_U64 | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_brutal_inject_ack, "QU", "brutal sample ack packet count");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_inject_loss", CTLTYPE_U64 | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_brutal_inject_loss, "QU", "brutal sample loss packet count");
    SYSCTL_ADD_PROC(&hysteria_sysctl_ctx, SYSCTL_CHILDREN(hysteria_sysctl_tree),
        OID_AUTO, "brutal_apply_sample", CTLTYPE_INT | CTLFLAG_RW, NULL, 0,
        sysctl_hysteria_brutal_apply_sample, "I", "write 1 to apply brutal ack/loss sample");

    return (0);
}

void
hysteria_ctl_fini(void)
{
    sysctl_ctx_free(&hysteria_sysctl_ctx);
}
