#include "hysteria_bbr.h"

#include <sys/time.h>

#define Q16_ONE 65536ULL
#define BBR_GAIN_STARTUP ((uint64_t)(2.885 * 65536.0))
#define BBR_GAIN_DRAIN ((uint64_t)(0.346 * 65536.0))
#define BBR_GAIN_PROBE_BW Q16_ONE
#define BBR_GAIN_PROBE_RTT ((uint64_t)(0.75 * 65536.0))

#define FULL_BW_THRESH_NUM 5
#define FULL_BW_THRESH_DEN 4
#define FULL_BW_CNT 3
#define PROBE_RTT_INTERVAL_US (10ULL * 1000ULL * 1000ULL)
#define PROBE_RTT_DWELL_US (200ULL * 1000ULL)
#define PROBE_BW_CYCLE_LEN 8

struct hysteria_bbr_state {
    enum hysteria_bbr_mode mode;
    uint64_t bw_bytes_per_s;
    uint64_t min_rtt_us;
    uint64_t pacing_gain_q16;
    uint64_t cwnd_gain_q16;

    uint64_t full_bw;
    uint32_t full_bw_cnt;
    uint64_t last_probe_rtt_ts_us;
    uint8_t probe_bw_idx;
    uint64_t probe_rtt_enter_ts_us;
};

static const uint64_t probe_bw_gains_q16[PROBE_BW_CYCLE_LEN] = {
    (uint64_t)(1.25 * 65536.0),
    (uint64_t)(0.75 * 65536.0),
    Q16_ONE, Q16_ONE, Q16_ONE, Q16_ONE, Q16_ONE, Q16_ONE
};

static struct hysteria_bbr_state g_bbr = {
    .mode = HYSTERIA_BBR_STARTUP,
    .pacing_gain_q16 = BBR_GAIN_STARTUP,
    .cwnd_gain_q16 = BBR_GAIN_STARTUP,
};

static uint64_t
monotonic_us(void)
{
    struct timeval tv;
    microuptime(&tv);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static void
bbr_check_full_bw(uint64_t sample_bw)
{
    if (sample_bw >= (g_bbr.full_bw * FULL_BW_THRESH_NUM) / FULL_BW_THRESH_DEN) {
        g_bbr.full_bw = sample_bw;
        g_bbr.full_bw_cnt = 0;
        return;
    }

    g_bbr.full_bw_cnt++;
    if (g_bbr.full_bw_cnt >= FULL_BW_CNT && g_bbr.mode == HYSTERIA_BBR_STARTUP) {
        g_bbr.mode = HYSTERIA_BBR_DRAIN;
        g_bbr.pacing_gain_q16 = BBR_GAIN_DRAIN;
        g_bbr.cwnd_gain_q16 = BBR_GAIN_STARTUP;
    }
}


static void
bbr_advance_probe_bw_cycle(void)
{
    g_bbr.probe_bw_idx = (uint8_t)((g_bbr.probe_bw_idx + 1) % PROBE_BW_CYCLE_LEN);
    g_bbr.pacing_gain_q16 = probe_bw_gains_q16[g_bbr.probe_bw_idx];
}


static int
bbr_probe_rtt_done(uint64_t now_us)
{
    if (g_bbr.mode != HYSTERIA_BBR_PROBE_RTT)
        return (0);
    return (now_us - g_bbr.probe_rtt_enter_ts_us >= PROBE_RTT_DWELL_US);
}

static void
bbr_maybe_probe_rtt(uint64_t now_us)
{
    if (g_bbr.min_rtt_us == 0)
        return;

    if (now_us - g_bbr.last_probe_rtt_ts_us >= PROBE_RTT_INTERVAL_US) {
        g_bbr.mode = HYSTERIA_BBR_PROBE_RTT;
        g_bbr.pacing_gain_q16 = BBR_GAIN_PROBE_RTT;
        g_bbr.cwnd_gain_q16 = BBR_GAIN_PROBE_RTT;
        g_bbr.last_probe_rtt_ts_us = now_us;
        g_bbr.probe_rtt_enter_ts_us = now_us;
    }
}

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

    bbr_check_full_bw(sample_bw);

    if (g_bbr.mode == HYSTERIA_BBR_DRAIN) {
        g_bbr.mode = HYSTERIA_BBR_PROBE_BW;
        g_bbr.probe_bw_idx = 0;
        g_bbr.pacing_gain_q16 = probe_bw_gains_q16[g_bbr.probe_bw_idx];
        g_bbr.cwnd_gain_q16 = 2 * Q16_ONE;
    }

    if (g_bbr.mode == HYSTERIA_BBR_PROBE_BW)
        bbr_advance_probe_bw_cycle();

    uint64_t now_us = monotonic_us();
    bbr_maybe_probe_rtt(now_us);

    if (bbr_probe_rtt_done(now_us)) {
        g_bbr.mode = HYSTERIA_BBR_PROBE_BW;
        g_bbr.probe_bw_idx = 0;
        g_bbr.pacing_gain_q16 = probe_bw_gains_q16[g_bbr.probe_bw_idx];
        g_bbr.cwnd_gain_q16 = 2 * Q16_ONE;
    }
}

void
hysteria_bbr_get_snapshot(struct hysteria_bbr_snapshot *out)
{
    if (out == NULL)
        return;

    out->mode = g_bbr.mode;
    out->bw_bytes_per_s = g_bbr.bw_bytes_per_s;
    out->min_rtt_us = g_bbr.min_rtt_us;
    out->pacing_gain_q16 = g_bbr.pacing_gain_q16;
    out->cwnd_gain_q16 = g_bbr.cwnd_gain_q16;
}
