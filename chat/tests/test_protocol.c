/*
 * test_protocol.c — Tests for protocol constants, TLV framing, and
 * the wire format used between chatd instances and IPC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "protocol.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ── Test: TLV header size ───────────────────────────────── */
static void test_tlv_header_size(void)
{
    TEST(tlv_header_size);
    if (TLV_HDR_SIZE != 5) { FAIL("expected 5"); return; }
    if (sizeof(tlv_header_t) != 5) { FAIL("struct size wrong"); return; }
    PASS();
}

/* ── Test: message type constants ────────────────────────── */
static void test_message_types(void)
{
    TEST(message_type_constants);
    /* Verify all types are unique */
    uint8_t types[] = {
        MSG_AUTH_CHALLENGE, MSG_AUTH_RESPONSE, MSG_HELLO,
        MSG_TEXT, MSG_PEER_LIST, MSG_ACK
    };
    int n = sizeof(types) / sizeof(types[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (types[i] == types[j]) { FAIL("duplicate type"); return; }
        }
    }
    PASS();
}

/* ── Test: IPC type constants ────────────────────────────── */
static void test_ipc_types(void)
{
    TEST(ipc_type_constants);
    uint8_t types[] = {
        IPC_SEND_MSG, IPC_ADD_PEER, IPC_LIST_PEERS,
        IPC_GET_ID, IPC_RESPONSE
    };
    int n = sizeof(types) / sizeof(types[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (types[i] == types[j]) { FAIL("duplicate ipc type"); return; }
        }
    }
    /* IPC types should not collide with wire types */
    uint8_t wire[] = {
        MSG_AUTH_CHALLENGE, MSG_AUTH_RESPONSE, MSG_HELLO,
        MSG_TEXT, MSG_PEER_LIST, MSG_ACK
    };
    int nw = sizeof(wire) / sizeof(wire[0]);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nw; j++) {
            if (types[i] == wire[j]) { FAIL("IPC collides with wire type"); return; }
        }
    }
    PASS();
}

/* ── Test: TLV encode/decode roundtrip ───────────────────── */
static int encode_tlv(uint8_t *buf, uint8_t type, const uint8_t *payload, uint32_t len)
{
    buf[0] = type;
    uint32_t nlen = htonl(len);
    memcpy(buf + 1, &nlen, 4);
    if (len > 0) memcpy(buf + 5, payload, len);
    return 5 + len;
}

static void test_tlv_roundtrip(void)
{
    TEST(tlv_encode_decode_roundtrip);
    uint8_t buf[256];
    const char *payload = "hello world";
    uint32_t plen = strlen(payload);

    int total = encode_tlv(buf, MSG_TEXT, (uint8_t *)payload, plen);

    /* Decode */
    uint8_t type = buf[0];
    uint32_t decoded_len;
    memcpy(&decoded_len, buf + 1, 4);
    decoded_len = ntohl(decoded_len);

    if (type != MSG_TEXT) { FAIL("wrong type"); return; }
    if (decoded_len != plen) { FAIL("wrong length"); return; }
    if (memcmp(buf + 5, payload, plen) != 0) { FAIL("wrong payload"); return; }
    if (total != (int)(5 + plen)) { FAIL("wrong total"); return; }
    PASS();
}

/* ── Test: TLV empty payload ─────────────────────────────── */
static void test_tlv_empty_payload(void)
{
    TEST(tlv_empty_payload);
    uint8_t buf[16];
    int total = encode_tlv(buf, MSG_ACK, NULL, 0);

    uint8_t type = buf[0];
    uint32_t decoded_len;
    memcpy(&decoded_len, buf + 1, 4);
    decoded_len = ntohl(decoded_len);

    if (type != MSG_ACK) { FAIL("wrong type"); return; }
    if (decoded_len != 0) { FAIL("length not 0"); return; }
    if (total != 5) { FAIL("total not 5"); return; }
    PASS();
}

/* ── Test: TLV max message ───────────────────────────────── */
static void test_max_msg_size(void)
{
    TEST(max_message_size);
    if (PROTO_MAX_MSG < 1024) { FAIL("too small"); return; }
    if (PROTO_MAX_MSG > 1024 * 1024) { FAIL("unreasonably large"); return; }
    PASS();
}

/* ── Test: lan_announce_t layout ─────────────────────────── */
static void test_lan_announce_layout(void)
{
    TEST(lan_announce_struct_layout);
    lan_announce_t ann;
    memset(&ann, 0, sizeof(ann));
    memcpy(ann.magic, PROTO_MAGIC, PROTO_MAGIC_LEN);
    snprintf(ann.onion, sizeof(ann.onion), "test1234567890.onion");
    memset(ann.hmac, 0xaa, 32);

    /* Verify fields are accessible */
    if (memcmp(ann.magic, PROTO_MAGIC, PROTO_MAGIC_LEN) != 0) {
        FAIL("magic corrupt"); return;
    }
    if (strcmp(ann.onion, "test1234567890.onion") != 0) {
        FAIL("onion corrupt"); return;
    }
    if (ann.hmac[0] != 0xaa || ann.hmac[31] != 0xaa) {
        FAIL("hmac corrupt"); return;
    }
    PASS();
}

/* ── Test: protocol port constants ───────────────────────── */
static void test_port_constants(void)
{
    TEST(port_constants);
    if (PROTO_PORT != 9876) { FAIL("wrong tcp port"); return; }
    if (PROTO_LAN_PORT != 9877) { FAIL("wrong udp port"); return; }
    if (PROTO_PORT == PROTO_LAN_PORT) { FAIL("ports should differ"); return; }
    PASS();
}

/* ── Test: auth constants ────────────────────────────────── */
static void test_auth_constants(void)
{
    TEST(auth_constants);
    if (AUTH_CHALLENGE_LEN != 32) { FAIL("challenge len"); return; }
    if (AUTH_TOKEN_LEN != 32) { FAIL("token len"); return; }
    if (HELLO_PUBKEY_LEN != 32) { FAIL("pubkey len"); return; }
    PASS();
}

/* ── Test: TLV multiple messages in buffer ───────────────── */
static void test_tlv_multiple(void)
{
    TEST(tlv_multiple_messages_in_buffer);
    uint8_t buf[256];
    int off = 0;

    off += encode_tlv(buf + off, MSG_HELLO, (uint8_t *)"pk", 2);
    off += encode_tlv(buf + off, MSG_TEXT, (uint8_t *)"msg", 3);
    off += encode_tlv(buf + off, MSG_ACK, NULL, 0);

    /* Parse back */
    int pos = 0;

    /* Message 1 */
    if (buf[pos] != MSG_HELLO) { FAIL("msg1 type"); return; }
    uint32_t len1; memcpy(&len1, buf + pos + 1, 4); len1 = ntohl(len1);
    if (len1 != 2) { FAIL("msg1 len"); return; }
    pos += 5 + len1;

    /* Message 2 */
    if (buf[pos] != MSG_TEXT) { FAIL("msg2 type"); return; }
    uint32_t len2; memcpy(&len2, buf + pos + 1, 4); len2 = ntohl(len2);
    if (len2 != 3) { FAIL("msg2 len"); return; }
    pos += 5 + len2;

    /* Message 3 */
    if (buf[pos] != MSG_ACK) { FAIL("msg3 type"); return; }
    uint32_t len3; memcpy(&len3, buf + pos + 1, 4); len3 = ntohl(len3);
    if (len3 != 0) { FAIL("msg3 len"); return; }
    pos += 5;

    if (pos != off) { FAIL("position mismatch"); return; }
    PASS();
}

/* ── Test: socket path ───────────────────────────────────── */
static void test_socket_path(void)
{
    TEST(socket_path_fits_sun_path);
    struct sockaddr_un addr;
    if (strlen(CHATD_SOCK_PATH) >= sizeof(addr.sun_path)) {
        FAIL("path too long for sun_path"); return;
    }
    PASS();
}

int main(void)
{
    printf("=== protocol tests ===\n");

    test_tlv_header_size();
    test_message_types();
    test_ipc_types();
    test_tlv_roundtrip();
    test_tlv_empty_payload();
    test_max_msg_size();
    test_lan_announce_layout();
    test_port_constants();
    test_auth_constants();
    test_tlv_multiple();
    test_socket_path();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
