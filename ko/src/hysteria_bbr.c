#include "hysteria_mod.h"

struct hysteria_bbr_state {
    uint64_t bw_bytes_per_s;
    uint64_t min_rtt_us;
};

static struct hysteria_bbr_state g_bbr = {0, 0};

void
hysteria_bbr_on_ack(uint64_t delivered, uint64_t interval_us)
{
    if (interval_us == 0)
        return;

    uint64_t sample_bw = (delivered * 1000000ULL) / interval_us;
    if (sample_bw > g_bbr.bw_bytes_per_s)
        g_bbr.bw_bytes_per_s = sample_bw;

    if (g_bbr.min_rtt_us == 0 || interval_us < g_bbr.min_rtt_us)
        g_bbr.min_rtt_us = interval_us;
}
