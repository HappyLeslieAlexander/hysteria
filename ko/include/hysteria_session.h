#ifndef _HYSTERIA_SESSION_H_
#define _HYSTERIA_SESSION_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/mutex.h>

#define HYSTERIA_SESSION_BUCKETS 256

struct hysteria_session {
    uint64_t sid;
    uint64_t rx_packets;
    uint64_t tx_packets;
    volatile u_int refcnt;
    LIST_ENTRY(hysteria_session) link;
};

int hysteria_session_init(void);
void hysteria_session_fini(void);
struct hysteria_session *hysteria_session_lookup(uint64_t sid);
struct hysteria_session *hysteria_session_get_or_create(uint64_t sid);
void hysteria_session_put(struct hysteria_session *s);

#endif
