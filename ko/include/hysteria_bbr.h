#ifndef _HYSTERIA_BBR_H_
#define _HYSTERIA_BBR_H_

#include <sys/types.h>

enum hysteria_bbr_mode {
    HYSTERIA_BBR_STARTUP = 0,
    HYSTERIA_BBR_DRAIN,
    HYSTERIA_BBR_PROBE_BW,
    HYSTERIA_BBR_PROBE_RTT,
};

struct hysteria_bbr_snapshot {
    enum hysteria_bbr_mode mode;
    uint64_t bw_bytes_per_s;
    uint64_t min_rtt_us;
    uint64_t pacing_gain_q16;
    uint64_t cwnd_gain_q16;
};

void hysteria_bbr_on_ack(uint64_t delivered, uint64_t interval_us);
void hysteria_bbr_get_snapshot(struct hysteria_bbr_snapshot *out);

#endif
