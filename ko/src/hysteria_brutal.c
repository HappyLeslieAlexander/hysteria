#include "hysteria_brutal.h"

#define HYSTERIA_BRUTAL_MIN_SAMPLE_COUNT 50ULL
#define HYSTERIA_BRUTAL_MIN_ACK_RATE_Q16 ((uint64_t)(0.8 * 65536.0))
#define HYSTERIA_BRUTAL_CWND_MULTIPLIER 2ULL

struct hysteria_brutal_pkt_info {
    uint64_t ts_sec;
    uint64_t ack_count;
    uint64_t loss_count;
};

struct hysteria_brutal_state {
    uint64_t bps;
    uint64_t max_datagram_size;
    uint64_t smoothed_rtt_us;
    uint64_t ack_rate_q16;
    struct hysteria_brutal_pkt_info slots[HYSTERIA_BRUTAL_PKT_INFO_SLOTS];
};

static struct hysteria_brutal_state g_brutal;

void
hysteria_brutal_init(uint64_t bps)
{
    g_brutal.bps = bps;
    g_brutal.max_datagram_size = 1200;
    g_brutal.smoothed_rtt_us = 0;
    g_brutal.ack_rate_q16 = 65536;
}

void
hysteria_brutal_set_bps(uint64_t bps)
{
    if (bps > 0)
        g_brutal.bps = bps;
}

void
hysteria_brutal_set_rtt_us(uint64_t rtt_us)
{
    g_brutal.smoothed_rtt_us = rtt_us;
}

void
hysteria_brutal_set_max_datagram_size(uint64_t size)
{
    if (size > 0)
        g_brutal.max_datagram_size = size;
}

static void
hysteria_brutal_update_ack_rate(uint64_t now_sec)
{
    uint64_t min_ts = now_sec - HYSTERIA_BRUTAL_PKT_INFO_SLOTS;
    uint64_t ack = 0, loss = 0, total;

    for (int i = 0; i < HYSTERIA_BRUTAL_PKT_INFO_SLOTS; ++i) {
        if (g_brutal.slots[i].ts_sec < min_ts)
            continue;
        ack += g_brutal.slots[i].ack_count;
        loss += g_brutal.slots[i].loss_count;
    }

    total = ack + loss;
    if (total < HYSTERIA_BRUTAL_MIN_SAMPLE_COUNT) {
        g_brutal.ack_rate_q16 = 65536;
        return;
    }

    uint64_t rate_q16 = (ack * 65536ULL) / total;
    if (rate_q16 < HYSTERIA_BRUTAL_MIN_ACK_RATE_Q16)
        rate_q16 = HYSTERIA_BRUTAL_MIN_ACK_RATE_Q16;
    g_brutal.ack_rate_q16 = rate_q16;
}

void
hysteria_brutal_on_congestion_event(uint64_t now_sec, uint64_t acked_pkts, uint64_t lost_pkts)
{
    int slot = now_sec % HYSTERIA_BRUTAL_PKT_INFO_SLOTS;
    if (g_brutal.slots[slot].ts_sec == now_sec) {
        g_brutal.slots[slot].ack_count += acked_pkts;
        g_brutal.slots[slot].loss_count += lost_pkts;
    } else {
        g_brutal.slots[slot].ts_sec = now_sec;
        g_brutal.slots[slot].ack_count = acked_pkts;
        g_brutal.slots[slot].loss_count = lost_pkts;
    }

    hysteria_brutal_update_ack_rate(now_sec);
}

uint64_t
hysteria_brutal_get_cwnd(void)
{
    if (g_brutal.smoothed_rtt_us == 0)
        return 10240;

    uint64_t numerator = g_brutal.bps * g_brutal.smoothed_rtt_us * HYSTERIA_BRUTAL_CWND_MULTIPLIER;
    uint64_t denom = 1000000ULL;
    uint64_t cwnd = (numerator / denom);

    cwnd = (cwnd * 65536ULL) / (g_brutal.ack_rate_q16 ? g_brutal.ack_rate_q16 : 65536ULL);
    if (cwnd < g_brutal.max_datagram_size)
        cwnd = g_brutal.max_datagram_size;
    return cwnd;
}

void
hysteria_brutal_get_snapshot(struct hysteria_brutal_snapshot *out)
{
    uint64_t ack = 0, loss = 0;
    if (out == NULL)
        return;

    for (int i = 0; i < HYSTERIA_BRUTAL_PKT_INFO_SLOTS; ++i) {
        ack += g_brutal.slots[i].ack_count;
        loss += g_brutal.slots[i].loss_count;
    }

    out->bps = g_brutal.bps;
    out->max_datagram_size = g_brutal.max_datagram_size;
    out->smoothed_rtt_us = g_brutal.smoothed_rtt_us;
    out->cwnd_bytes = hysteria_brutal_get_cwnd();
    out->ack_count = ack;
    out->loss_count = loss;
    out->ack_rate_q16 = g_brutal.ack_rate_q16;
}
