#ifndef _HYSTERIA_MOD_H_
#define _HYSTERIA_MOD_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include "hysteria_session.h"
#include "hysteria_proto.h"
#include "hysteria_bbr.h"
#include "hysteria_brutal.h"

struct hysteria_stats {
    volatile uint64_t packets;
    volatile uint64_t drops;
    volatile uint64_t proto_err_varint;
    volatile uint64_t proto_err_bounds;
    volatile uint64_t proto_err_format;
};

extern struct hysteria_stats g_hysteria_stats;
extern int g_hysteria_mode;

int hysteria_ctl_init(void);
void hysteria_ctl_fini(void);

int hysteria_net_init(void);
void hysteria_net_fini(void);

int hysteria_proto_parse(const uint8_t *buf, size_t len);


#endif
