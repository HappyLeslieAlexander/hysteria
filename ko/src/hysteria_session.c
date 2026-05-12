#include "hysteria_session.h"

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/refcount.h>

MALLOC_DEFINE(M_HYSTERIA_SESS, "hysteria_sess", "hysteria session objects");

struct hysteria_bucket {
    struct mtx lock;
    LIST_HEAD(, hysteria_session) head;
};

static struct hysteria_bucket g_tbl[HYSTERIA_SESSION_BUCKETS];

static inline uint32_t
hysteria_hash_sid(uint64_t sid)
{
    sid ^= sid >> 33;
    sid *= 0xff51afd7ed558ccdULL;
    sid ^= sid >> 33;
    return ((uint32_t)sid & (HYSTERIA_SESSION_BUCKETS - 1));
}

int
hysteria_session_init(void)
{
    for (uint32_t i = 0; i < HYSTERIA_SESSION_BUCKETS; ++i) {
        mtx_init(&g_tbl[i].lock, "hysteria_sess_bucket", NULL, MTX_DEF);
        LIST_INIT(&g_tbl[i].head);
    }
    return (0);
}

void
hysteria_session_fini(void)
{
    for (uint32_t i = 0; i < HYSTERIA_SESSION_BUCKETS; ++i) {
        struct hysteria_session *s;
        while ((s = LIST_FIRST(&g_tbl[i].head)) != NULL) {
            LIST_REMOVE(s, link);
            free(s, M_HYSTERIA_SESS);
        }
        mtx_destroy(&g_tbl[i].lock);
    }
}

struct hysteria_session *
hysteria_session_lookup(uint64_t sid)
{
    uint32_t h = hysteria_hash_sid(sid);
    struct hysteria_bucket *b = &g_tbl[h];
    struct hysteria_session *s;

    mtx_lock(&b->lock);
    LIST_FOREACH(s, &b->head, link) {
        if (s->sid == sid) {
            refcount_acquire(&s->refcnt);
            mtx_unlock(&b->lock);
            return (s);
        }
    }
    mtx_unlock(&b->lock);
    return (NULL);
}

struct hysteria_session *
hysteria_session_get_or_create(uint64_t sid)
{
    uint32_t h = hysteria_hash_sid(sid);
    struct hysteria_bucket *b = &g_tbl[h];
    struct hysteria_session *s;

    mtx_lock(&b->lock);
    LIST_FOREACH(s, &b->head, link) {
        if (s->sid == sid) {
            refcount_acquire(&s->refcnt);
            mtx_unlock(&b->lock);
            return (s);
        }
    }

    s = malloc(sizeof(*s), M_HYSTERIA_SESS, M_NOWAIT | M_ZERO);
    if (s != NULL) {
        s->sid = sid;
        s->refcnt = 1;
        LIST_INSERT_HEAD(&b->head, s, link);
    }
    mtx_unlock(&b->lock);
    return (s);
}

void
hysteria_session_put(struct hysteria_session *s)
{
    struct hysteria_bucket *b;
    uint32_t h;

    if (s == NULL)
        return;

    if (!refcount_release(&s->refcnt))
        return;

    h = hysteria_hash_sid(s->sid);
    b = &g_tbl[h];
    mtx_lock(&b->lock);
    LIST_REMOVE(s, link);
    mtx_unlock(&b->lock);

    free(s, M_HYSTERIA_SESS);
}
