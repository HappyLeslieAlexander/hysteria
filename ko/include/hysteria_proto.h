#ifndef _HYSTERIA_PROTO_H_
#define _HYSTERIA_PROTO_H_

#include <sys/types.h>

#define HYSTERIA_MAX_ADDRESS_LENGTH 2048
#define HYSTERIA_MAX_MESSAGE_LENGTH 2048
#define HYSTERIA_MAX_PADDING_LENGTH 4096
#define HYSTERIA_MAX_UDP_SIZE 4096


#define HYSTERIA_FRAME_TYPE_TCP_REQUEST 0x401ULL


#define HYSTERIA_HEADER_AUTH "Hysteria-Auth"
#define HYSTERIA_HEADER_UDP_ENABLED "Hysteria-UDP"
#define HYSTERIA_HEADER_CC_RX "Hysteria-CC-RX"
#define HYSTERIA_HEADER_PADDING "Hysteria-Padding"

struct hysteria_auth_request {
    const char *auth;
    uint64_t rx;
};

struct hysteria_auth_response {
    int udp_enabled;
    uint64_t rx;
    int rx_auto;
};

int hysteria_padding_fill(uint8_t *buf, size_t buf_len, size_t min_len, size_t max_len, size_t *out_len);
int hysteria_parse_u64_ascii(const char *s, uint64_t *out);
int hysteria_parse_bool_ascii(const char *s, int *out);
int hysteria_auth_request_from_headers(const char *auth, const char *cc_rx, struct hysteria_auth_request *out);
int hysteria_auth_response_from_headers(const char *udp_enabled, const char *cc_rx, struct hysteria_auth_response *out);
int hysteria_auth_response_ccrx_to_ascii(const struct hysteria_auth_response *resp, char *buf, size_t buf_len);

struct hysteria_tcp_request {
    const uint8_t *addr;
    size_t addr_len;
    const uint8_t *padding;
    size_t padding_len;
};

struct hysteria_tcp_response {
    int ok;
    const uint8_t *msg;
    size_t msg_len;
    const uint8_t *padding;
    size_t padding_len;
};

int hysteria_parse_tcp_request(const uint8_t *buf, size_t len, struct hysteria_tcp_request *out);
int hysteria_build_tcp_request(uint8_t *buf, size_t len, const uint8_t *addr, size_t addr_len, const uint8_t *padding, size_t padding_len, size_t *nwrite);
int hysteria_parse_tcp_response(const uint8_t *buf, size_t len, struct hysteria_tcp_response *out);
int hysteria_build_tcp_response(uint8_t *buf, size_t len, int ok, const uint8_t *msg, size_t msg_len, size_t *nwrite);

struct hysteria_udp_msg {
    uint32_t session_id;
    uint16_t packet_id;
    uint8_t frag_id;
    uint8_t frag_count;
    const uint8_t *addr;
    size_t addr_len;
    const uint8_t *data;
    size_t data_len;
};

int hysteria_varint_read(const uint8_t *buf, size_t len, uint64_t *v, size_t *nread);
int hysteria_varint_write(uint8_t *buf, size_t len, uint64_t v, size_t *nwrite);
int hysteria_parse_udp(const uint8_t *buf, size_t len, struct hysteria_udp_msg *out);

#endif


enum hysteria_proto_errno {
    HYSTERIA_PROTO_OK = 0,
    HYSTERIA_PROTO_ERR_VARINT,
    HYSTERIA_PROTO_ERR_BOUNDS,
    HYSTERIA_PROTO_ERR_FORMAT,
};

enum hysteria_proto_errno hysteria_proto_classify_errno(int err);

int hysteria_proto_selftest(void);
