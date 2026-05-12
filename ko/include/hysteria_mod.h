#ifndef _HYSTERIA_MOD_H_
#define _HYSTERIA_MOD_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

struct hysteria_stats {
    volatile uint64_t packets;
    volatile uint64_t drops;
};

extern struct hysteria_stats g_hysteria_stats;
extern int g_hysteria_mode;

int hysteria_ctl_init(void);
void hysteria_ctl_fini(void);

int hysteria_net_init(void);
void hysteria_net_fini(void);

int hysteria_proto_parse(const uint8_t *buf, size_t len);

void hysteria_bbr_on_ack(uint64_t delivered, uint64_t interval_us);

#endif
