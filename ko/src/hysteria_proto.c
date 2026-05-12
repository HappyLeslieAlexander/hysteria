#include "hysteria_mod.h"

int
hysteria_proto_parse(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len < 1) {
        g_hysteria_stats.drops++;
        return (EINVAL);
    }

    /* 极简示例: 首字节作为 frame type。 */
    switch (buf[0]) {
    case 0x01: /* data */
    case 0x02: /* ack */
        g_hysteria_stats.packets++;
        return (0);
    default:
        g_hysteria_stats.drops++;
        return (EPROTO);
    }
}
