#include "hysteria_mod.h"
#include "hysteria_proto.h"

#include <sys/endian.h>
#include <sys/libkern.h>

#define HYSTERIA_MAX_VARINT8 4611686018427387903ULL

int
hysteria_varint_read(const uint8_t *buf, size_t len, uint64_t *v, size_t *nread)
{
    if (buf == NULL || v == NULL || nread == NULL || len < 1)
        return (EINVAL);

    uint8_t first = buf[0];
    uint8_t prefix = first >> 6;
    size_t n = 1U << prefix;
    if (len < n)
        return (EMSGSIZE);

    uint64_t x = first & 0x3f;
    for (size_t i = 1; i < n; ++i)
        x = (x << 8) | buf[i];

    *v = x;
    *nread = n;
    return (0);
}

int
hysteria_varint_write(uint8_t *buf, size_t len, uint64_t v, size_t *nwrite)
{
    if (buf == NULL || nwrite == NULL)
        return (EINVAL);

    if (v <= 63) {
        if (len < 1) return (EMSGSIZE);
        buf[0] = (uint8_t)v;
        *nwrite = 1;
        return (0);
    }
    if (v <= 16383) {
        if (len < 2) return (EMSGSIZE);
        buf[0] = (uint8_t)(v >> 8) | 0x40;
        buf[1] = (uint8_t)v;
        *nwrite = 2;
        return (0);
    }
    if (v <= 1073741823ULL) {
        if (len < 4) return (EMSGSIZE);
        buf[0] = (uint8_t)(v >> 24) | 0x80;
        buf[1] = (uint8_t)(v >> 16);
        buf[2] = (uint8_t)(v >> 8);
        buf[3] = (uint8_t)v;
        *nwrite = 4;
        return (0);
    }
    if (v <= HYSTERIA_MAX_VARINT8) {
        if (len < 8) return (EMSGSIZE);
        buf[0] = (uint8_t)(v >> 56) | 0xc0;
        buf[1] = (uint8_t)(v >> 48);
        buf[2] = (uint8_t)(v >> 40);
        buf[3] = (uint8_t)(v >> 32);
        buf[4] = (uint8_t)(v >> 24);
        buf[5] = (uint8_t)(v >> 16);
        buf[6] = (uint8_t)(v >> 8);
        buf[7] = (uint8_t)v;
        *nwrite = 8;
        return (0);
    }
    return (EOVERFLOW);
}

int
hysteria_parse_udp(const uint8_t *buf, size_t len, struct hysteria_udp_msg *out)
{
    if (buf == NULL || out == NULL || len < 9)
        return (EINVAL);

    out->session_id = be32dec(buf);
    out->packet_id = be16dec(buf + 4);
    out->frag_id = buf[6];
    out->frag_count = buf[7];

    uint64_t addr_len_u64;
    size_t nread;
    int err = hysteria_varint_read(buf + 8, len - 8, &addr_len_u64, &nread);
    if (err != 0)
        return (err);
    if (addr_len_u64 == 0 || addr_len_u64 > HYSTERIA_MAX_MESSAGE_LENGTH)
        return (EPROTO);

    size_t off = 8 + nread;
    size_t addr_len = (size_t)addr_len_u64;
    if (len <= off + addr_len) /* 保持与 Go 实现一致: 数据区至少 1 字节 */
        return (EPROTO);

    out->addr = buf + off;
    out->addr_len = addr_len;
    out->data = buf + off + addr_len;
    out->data_len = len - off - addr_len;
    return (0);
}

int
hysteria_proto_parse(const uint8_t *buf, size_t len)
{
    struct hysteria_udp_msg m;
    int err = hysteria_parse_udp(buf, len, &m);
    if (err != 0) {
        g_hysteria_stats.drops++;
        switch (hysteria_proto_classify_errno(err)) {
        case HYSTERIA_PROTO_ERR_VARINT:
            g_hysteria_stats.proto_err_varint++;
            break;
        case HYSTERIA_PROTO_ERR_BOUNDS:
            g_hysteria_stats.proto_err_bounds++;
            break;
        case HYSTERIA_PROTO_ERR_FORMAT:
            g_hysteria_stats.proto_err_format++;
            break;
        default:
            break;
        }
        return (err);
    }

    g_hysteria_stats.packets++;
    return (0);
}


int
hysteria_parse_tcp_request(const uint8_t *buf, size_t len, struct hysteria_tcp_request *out)
{
    uint64_t frame_type, addr_len_u64, pad_len_u64;
    size_t n, off = 0;
    int err;

    if (buf == NULL || out == NULL)
        return (EINVAL);

    err = hysteria_varint_read(buf + off, len - off, &frame_type, &n);
    if (err != 0)
        return (err);
    off += n;

    if (frame_type != HYSTERIA_FRAME_TYPE_TCP_REQUEST)
        return (EPROTO);

    err = hysteria_varint_read(buf + off, len - off, &addr_len_u64, &n);
    if (err != 0)
        return (err);
    off += n;

    if (addr_len_u64 == 0 || addr_len_u64 > HYSTERIA_MAX_ADDRESS_LENGTH)
        return (EPROTO);
    if (off + addr_len_u64 > len)
        return (EMSGSIZE);

    out->addr = buf + off;
    out->addr_len = (size_t)addr_len_u64;
    off += out->addr_len;

    err = hysteria_varint_read(buf + off, len - off, &pad_len_u64, &n);
    if (err != 0)
        return (err);
    off += n;

    if (pad_len_u64 > HYSTERIA_MAX_PADDING_LENGTH)
        return (EPROTO);
    if (off + pad_len_u64 > len)
        return (EMSGSIZE);

    out->padding = buf + off;
    out->padding_len = (size_t)pad_len_u64;
    return (0);
}

int
hysteria_build_tcp_response(uint8_t *buf, size_t len, int ok,
    const uint8_t *msg, size_t msg_len, size_t *nwrite)
{
    size_t off = 0, n;
    int err;

    if (buf == NULL || nwrite == NULL)
        return (EINVAL);
    if (msg_len > HYSTERIA_MAX_MESSAGE_LENGTH)
        return (EOVERFLOW);

    if (len < 1)
        return (EMSGSIZE);
    buf[off++] = ok ? 0 : 1;

    err = hysteria_varint_write(buf + off, len - off, msg_len, &n);
    if (err != 0)
        return (err);
    off += n;

    if (off + msg_len > len)
        return (EMSGSIZE);
    if (msg_len > 0 && msg != NULL)
        bcopy(msg, buf + off, msg_len);
    off += msg_len;

    err = hysteria_varint_write(buf + off, len - off, 0, &n);
    if (err != 0)
        return (err);
    off += n;

    *nwrite = off;
    return (0);
}


int
hysteria_build_tcp_request(uint8_t *buf, size_t len, const uint8_t *addr,
    size_t addr_len, const uint8_t *padding, size_t padding_len, size_t *nwrite)
{
    size_t off = 0, n;
    int err;

    if (buf == NULL || nwrite == NULL || addr == NULL)
        return (EINVAL);
    if (addr_len == 0 || addr_len > HYSTERIA_MAX_ADDRESS_LENGTH)
        return (EOVERFLOW);
    if (padding_len > HYSTERIA_MAX_PADDING_LENGTH)
        return (EOVERFLOW);

    err = hysteria_varint_write(buf + off, len - off, HYSTERIA_FRAME_TYPE_TCP_REQUEST, &n);
    if (err != 0) return (err);
    off += n;

    err = hysteria_varint_write(buf + off, len - off, addr_len, &n);
    if (err != 0) return (err);
    off += n;

    if (off + addr_len > len)
        return (EMSGSIZE);
    bcopy(addr, buf + off, addr_len);
    off += addr_len;

    err = hysteria_varint_write(buf + off, len - off, padding_len, &n);
    if (err != 0) return (err);
    off += n;

    if (off + padding_len > len)
        return (EMSGSIZE);
    if (padding_len > 0 && padding != NULL)
        bcopy(padding, buf + off, padding_len);
    off += padding_len;

    *nwrite = off;
    return (0);
}

int
hysteria_parse_tcp_response(const uint8_t *buf, size_t len, struct hysteria_tcp_response *out)
{
    uint64_t msg_len_u64, pad_len_u64;
    size_t off = 0, n;
    int err;

    if (buf == NULL || out == NULL || len < 2)
        return (EINVAL);

    out->ok = (buf[off++] == 0);

    err = hysteria_varint_read(buf + off, len - off, &msg_len_u64, &n);
    if (err != 0) return (err);
    off += n;

    if (msg_len_u64 > HYSTERIA_MAX_MESSAGE_LENGTH)
        return (EPROTO);
    if (off + msg_len_u64 > len)
        return (EMSGSIZE);

    out->msg = buf + off;
    out->msg_len = (size_t)msg_len_u64;
    off += out->msg_len;

    err = hysteria_varint_read(buf + off, len - off, &pad_len_u64, &n);
    if (err != 0) return (err);
    off += n;

    if (pad_len_u64 > HYSTERIA_MAX_PADDING_LENGTH)
        return (EPROTO);
    if (off + pad_len_u64 > len)
        return (EMSGSIZE);

    out->padding = buf + off;
    out->padding_len = (size_t)pad_len_u64;
    return (0);
}


static const char hysteria_padding_chars[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

int
hysteria_padding_fill(uint8_t *buf, size_t buf_len, size_t min_len, size_t max_len, size_t *out_len)
{
    size_t n;
    if (buf == NULL || out_len == NULL || min_len >= max_len)
        return (EINVAL);

    n = min_len + (arc4random() % (max_len - min_len));
    if (n > buf_len)
        return (EMSGSIZE);

    for (size_t i = 0; i < n; ++i)
        buf[i] = hysteria_padding_chars[arc4random_uniform((uint32_t)(sizeof(hysteria_padding_chars) - 1))];

    *out_len = n;
    return (0);
}

int
hysteria_parse_u64_ascii(const char *s, uint64_t *out)
{
    unsigned long long v;
    char *end = NULL;

    if (s == NULL || out == NULL)
        return (EINVAL);

    v = strtouq(s, &end, 10);
    if (end == s || *end != '\0')
        return (EINVAL);

    *out = (uint64_t)v;
    return (0);
}

int
hysteria_parse_bool_ascii(const char *s, int *out)
{
    if (s == NULL || out == NULL)
        return (EINVAL);

    if (strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0) {
        *out = 1;
        return (0);
    }
    if (strcasecmp(s, "false") == 0 || strcmp(s, "0") == 0) {
        *out = 0;
        return (0);
    }
    return (EINVAL);
}


int
hysteria_auth_request_from_headers(const char *auth, const char *cc_rx,
    struct hysteria_auth_request *out)
{
    if (auth == NULL || cc_rx == NULL || out == NULL)
        return (EINVAL);

    out->auth = auth;
    return hysteria_parse_u64_ascii(cc_rx, &out->rx);
}

int
hysteria_auth_response_from_headers(const char *udp_enabled, const char *cc_rx,
    struct hysteria_auth_response *out)
{
    int err;
    if (udp_enabled == NULL || cc_rx == NULL || out == NULL)
        return (EINVAL);

    err = hysteria_parse_bool_ascii(udp_enabled, &out->udp_enabled);
    if (err != 0)
        return (err);

    if (strcmp(cc_rx, "auto") == 0) {
        out->rx_auto = 1;
        out->rx = 0;
        return (0);
    }

    out->rx_auto = 0;
    return hysteria_parse_u64_ascii(cc_rx, &out->rx);
}

int
hysteria_auth_response_ccrx_to_ascii(const struct hysteria_auth_response *resp,
    char *buf, size_t buf_len)
{
    int n;

    if (resp == NULL || buf == NULL || buf_len == 0)
        return (EINVAL);

    if (resp->rx_auto) {
        if (buf_len < 5)
            return (EMSGSIZE);
        bcopy("auto", buf, 5);
        return (0);
    }

    n = snprintf(buf, buf_len, "%ju", (uintmax_t)resp->rx);
    if (n < 0 || (size_t)n >= buf_len)
        return (EMSGSIZE);
    return (0);
}


enum hysteria_proto_errno
hysteria_proto_classify_errno(int err)
{
    switch (err) {
    case 0:
        return HYSTERIA_PROTO_OK;
    case EMSGSIZE:
        return HYSTERIA_PROTO_ERR_BOUNDS;
    case EPROTO:
        return HYSTERIA_PROTO_ERR_FORMAT;
    case EINVAL:
    case EOVERFLOW:
    default:
        return HYSTERIA_PROTO_ERR_VARINT;
    }
}


int
hysteria_proto_selftest(void)
{
    uint8_t buf[64];
    size_t nwrite;
    uint64_t v;
    size_t nread;
    struct hysteria_udp_msg m;

    /* Case 1: varint roundtrip */
    if (hysteria_varint_write(buf, sizeof(buf), 0x401, &nwrite) != 0)
        return (EFAULT);
    if (hysteria_varint_read(buf, nwrite, &v, &nread) != 0)
        return (EFAULT);
    if (v != 0x401 || nread != nwrite)
        return (EFAULT);

    /* Case 2: malicious truncated UDP frame must fail */
    bzero(buf, sizeof(buf));
    if (hysteria_parse_udp(buf, 8, &m) == 0)
        return (EFAULT);

    /* Case 3: TCP request with empty addr must fail */
    bzero(buf, sizeof(buf));
    nwrite = 0;
    if (hysteria_varint_write(buf, sizeof(buf), HYSTERIA_FRAME_TYPE_TCP_REQUEST, &nread) != 0)
        return (EFAULT);
    nwrite += nread;
    if (hysteria_varint_write(buf + nwrite, sizeof(buf) - nwrite, 0, &nread) != 0)
        return (EFAULT);
    nwrite += nread;
    if (hysteria_parse_tcp_request(buf, nwrite, (struct hysteria_tcp_request *)&m) == 0)
        return (EFAULT);

    return (0);
}
