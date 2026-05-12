#include "hysteria_mod.h"

static struct sysctl_ctx_list hysteria_sysctl_ctx;
static struct sysctl_oid *hysteria_sysctl_tree;

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

    return (0);
}

void
hysteria_ctl_fini(void)
{
    sysctl_ctx_free(&hysteria_sysctl_ctx);
}
