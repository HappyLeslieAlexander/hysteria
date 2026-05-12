#ifndef _HYSTERIA_BRUTAL_H_
#define _HYSTERIA_BRUTAL_H_

#include <sys/types.h>

#define HYSTERIA_BRUTAL_PKT_INFO_SLOTS 5

struct hysteria_brutal_snapshot {
    uint64_t bps;
    uint64_t max_datagram_size;
    uint64_t smoothed_rtt_us;
    uint64_t cwnd_bytes;
    uint64_t ack_count;
    uint64_t loss_count;
    uint64_t ack_rate_q16;
};

void hysteria_brutal_init(uint64_t bps);
void hysteria_brutal_set_bps(uint64_t bps);
void hysteria_brutal_set_rtt_us(uint64_t rtt_us);
void hysteria_brutal_set_max_datagram_size(uint64_t size);
void hysteria_brutal_on_congestion_event(uint64_t now_sec, uint64_t acked_pkts, uint64_t lost_pkts);
uint64_t hysteria_brutal_get_cwnd(void);
void hysteria_brutal_get_snapshot(struct hysteria_brutal_snapshot *out);

#endif
