/*
 * test_integration.c — Integration tests for chatd daemon
 *
 * Tests the daemon's Unix socket IPC, peer connection via loopback,
 * TLV framing over real sockets, and full auth + key exchange + message flow.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include "protocol.h"
#include "crypto.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ── TLV helpers ─────────────────────────────────────────── */

static int send_tlv(int fd, uint8_t type, const uint8_t *payload, uint32_t len)
{
    uint8_t hdr[TLV_HDR_SIZE];
    hdr[0] = type;
    uint32_t nlen = htonl(len);
    memcpy(hdr + 1, &nlen, 4);
    if (write(fd, hdr, TLV_HDR_SIZE) != TLV_HDR_SIZE) return -1;
    if (len > 0 && write(fd, payload, len) != (ssize_t)len) return -1;
    return 0;
}

static int recv_tlv(int fd, uint8_t *type, uint8_t *payload, uint32_t *len,
                    int timeout_ms)
{
    /* Simple poll with timeout */
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) return -1;

    uint8_t hdr[TLV_HDR_SIZE];
    ssize_t n = read(fd, hdr, TLV_HDR_SIZE);
    if (n != TLV_HDR_SIZE) return -1;

    *type = hdr[0];
    memcpy(len, hdr + 1, 4);
    *len = ntohl(*len);

    if (*len > 0) {
        n = read(fd, payload, *len);
        if (n != (ssize_t)*len) return -1;
    }
    return 0;
}

/* ── IPC connection to chatd ─────────────────────────────── */

static int connect_ipc(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", CHATD_SOCK_PATH);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* ── Start/stop chatd ────────────────────────────────────── */

static pid_t chatd_pid = -1;

static int start_chatd(void)
{
    chatd_pid = fork();
    if (chatd_pid < 0) return -1;

    if (chatd_pid == 0) {
        /* Child: exec chatd */
        execlp("./chatd", "chatd", NULL);
        _exit(1);
    }

    /* Parent: wait for socket to appear */
    for (int i = 0; i < 30; i++) {
        usleep(100000); /* 100ms */
        int fd = connect_ipc();
        if (fd >= 0) {
            close(fd);
            return 0;
        }
    }
    return -1;
}

static void stop_chatd(void)
{
    if (chatd_pid > 0) {
        kill(chatd_pid, SIGTERM);
        int status;
        waitpid(chatd_pid, &status, 0);
        chatd_pid = -1;
    }
    unlink(CHATD_SOCK_PATH);
}

/* ── Test: daemon starts and accepts IPC ─────────────────── */
static void test_daemon_ipc_connect(void)
{
    TEST(daemon_ipc_connect);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("cannot connect to daemon"); return; }
    close(fd);
    PASS();
}

/* ── Test: IPC GET_ID ────────────────────────────────────── */
static void test_ipc_get_id(void)
{
    TEST(ipc_get_id);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("connect"); return; }

    send_tlv(fd, IPC_GET_ID, NULL, 0);

    uint8_t type;
    uint8_t payload[512];
    uint32_t len;
    if (recv_tlv(fd, &type, payload, &len, 2000) < 0) {
        FAIL("no response"); close(fd); return;
    }

    if (type != IPC_RESPONSE) { FAIL("wrong type"); close(fd); return; }
    payload[len] = '\0';

    if (strstr((char *)payload, "key:") == NULL) {
        FAIL("missing key in response"); close(fd); return;
    }
    close(fd);
    PASS();
}

/* ── Test: IPC LIST_PEERS (empty) ────────────────────────── */
static void test_ipc_list_peers_empty(void)
{
    TEST(ipc_list_peers_empty);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("connect"); return; }

    send_tlv(fd, IPC_LIST_PEERS, NULL, 0);

    uint8_t type;
    uint8_t payload[4096];
    uint32_t len;
    if (recv_tlv(fd, &type, payload, &len, 2000) < 0) {
        FAIL("no response"); close(fd); return;
    }

    if (type != IPC_RESPONSE) { FAIL("wrong type"); close(fd); return; }
    /* Empty peer list = empty payload */
    close(fd);
    PASS();
}

/* ── Test: IPC SEND_MSG with no peers ────────────────────── */
static void test_ipc_send_no_peers(void)
{
    TEST(ipc_send_msg_no_peers);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("connect"); return; }

    const char *msg = "hello nobody";
    send_tlv(fd, IPC_SEND_MSG, (uint8_t *)msg, strlen(msg));

    uint8_t type;
    uint8_t payload[256];
    uint32_t len;
    if (recv_tlv(fd, &type, payload, &len, 2000) < 0) {
        FAIL("no response"); close(fd); return;
    }

    payload[len] = '\0';
    if (type != IPC_RESPONSE) { FAIL("wrong type"); close(fd); return; }
    if (strstr((char *)payload, "sent") == NULL) {
        FAIL("expected 'sent'"); close(fd); return;
    }
    close(fd);
    PASS();
}

/* ── Test: IPC ADD_PEER invalid address ──────────────────── */
static void test_ipc_add_peer_invalid(void)
{
    TEST(ipc_add_peer_invalid);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("connect"); return; }

    /* Try connecting to a bogus address (will fail SOCKS5) */
    const char *onion = "nonexistent.onion";
    send_tlv(fd, IPC_ADD_PEER, (uint8_t *)onion, strlen(onion));

    uint8_t type;
    uint8_t payload[256];
    uint32_t len;
    if (recv_tlv(fd, &type, payload, &len, 5000) < 0) {
        FAIL("no response"); close(fd); return;
    }

    payload[len] = '\0';
    if (strstr((char *)payload, "failed") == NULL) {
        FAIL("expected 'failed'"); close(fd); return;
    }
    close(fd);
    PASS();
}

/* ── Test: multiple IPC clients ──────────────────────────── */
static void test_multiple_ipc_clients(void)
{
    TEST(multiple_ipc_clients);
    int fd1 = connect_ipc();
    int fd2 = connect_ipc();
    int fd3 = connect_ipc();
    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        FAIL("connect"); goto done;
    }

    /* All should be able to get id */
    send_tlv(fd1, IPC_GET_ID, NULL, 0);
    send_tlv(fd2, IPC_GET_ID, NULL, 0);
    send_tlv(fd3, IPC_GET_ID, NULL, 0);

    uint8_t type; uint8_t payload[512]; uint32_t len;
    int ok = 1;
    if (recv_tlv(fd1, &type, payload, &len, 2000) < 0) ok = 0;
    if (recv_tlv(fd2, &type, payload, &len, 2000) < 0) ok = 0;
    if (recv_tlv(fd3, &type, payload, &len, 2000) < 0) ok = 0;

    if (!ok) { FAIL("not all clients got response"); }
    else { PASS(); }

done:
    if (fd1 >= 0) close(fd1);
    if (fd2 >= 0) close(fd2);
    if (fd3 >= 0) close(fd3);
}

/* ── Test: TCP listener accepts and sends auth challenge ─── */
static void test_tcp_auth_challenge(void)
{
    TEST(tcp_auth_challenge_on_connect);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { FAIL("socket"); return; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTO_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        FAIL("connect to TCP port"); close(fd); return;
    }

    /* Should receive AUTH_CHALLENGE */
    uint8_t type;
    uint8_t payload[256];
    uint32_t len;
    if (recv_tlv(fd, &type, payload, &len, 2000) < 0) {
        FAIL("no challenge received"); close(fd); return;
    }

    if (type != MSG_AUTH_CHALLENGE) { FAIL("wrong type"); close(fd); return; }
    if (len != AUTH_CHALLENGE_LEN) { FAIL("wrong challenge len"); close(fd); return; }

    close(fd);
    PASS();
}

/* ── Test: TCP auth with correct secret ──────────────────── */
static void test_tcp_auth_success(void)
{
    TEST(tcp_auth_correct_secret);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { FAIL("socket"); return; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTO_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        FAIL("connect"); close(fd); return;
    }

    /* Receive challenge */
    uint8_t type;
    uint8_t challenge[256];
    uint32_t len;
    if (recv_tlv(fd, &type, challenge, &len, 2000) < 0) {
        FAIL("no challenge"); close(fd); return;
    }

    /* Compute auth token using same secret */
    uint8_t secret[32];
    crypto_load_secret(NULL, secret, sizeof(secret));

    uint8_t token[AUTH_TOKEN_LEN];
    crypto_derive_auth_token(secret, 32, challenge, len, token, sizeof(token));

    /* Send auth response */
    send_tlv(fd, MSG_AUTH_RESPONSE, token, AUTH_TOKEN_LEN);

    /* Should get HELLO (public key) back */
    uint8_t hello_payload[256];
    uint32_t hello_len;
    if (recv_tlv(fd, &type, hello_payload, &hello_len, 2000) < 0) {
        FAIL("no HELLO after auth"); close(fd); return;
    }

    if (type != MSG_HELLO) { FAIL("expected HELLO"); close(fd); return; }
    if (hello_len != HELLO_PUBKEY_LEN) { FAIL("wrong pubkey len"); close(fd); return; }

    close(fd);
    PASS();
}

/* ── Test: TCP auth with wrong secret rejected ───────────── */
static void test_tcp_auth_reject(void)
{
    TEST(tcp_auth_wrong_secret_rejected);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { FAIL("socket"); return; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTO_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        FAIL("connect"); close(fd); return;
    }

    /* Receive challenge */
    uint8_t type;
    uint8_t challenge[256];
    uint32_t len;
    recv_tlv(fd, &type, challenge, &len, 2000);

    /* Send wrong token */
    uint8_t bad_token[AUTH_TOKEN_LEN];
    memset(bad_token, 0xaa, AUTH_TOKEN_LEN);
    send_tlv(fd, MSG_AUTH_RESPONSE, bad_token, AUTH_TOKEN_LEN);

    /* Connection should be dropped (read returns 0 or error) */
    uint8_t buf[256];
    usleep(200000);
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n > 0) { FAIL("should have been disconnected"); close(fd); return; }
    close(fd);
    PASS();
}

/* ── Test: full handshake and key exchange ────────────────── */
static void test_full_handshake(void)
{
    TEST(full_handshake_and_key_exchange);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { FAIL("socket"); return; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTO_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        FAIL("connect"); close(fd); return;
    }

    /* Auth */
    uint8_t type;
    uint8_t payload[512];
    uint32_t len;
    recv_tlv(fd, &type, payload, &len, 2000);

    uint8_t secret[32];
    crypto_load_secret(NULL, secret, sizeof(secret));
    uint8_t token[AUTH_TOKEN_LEN];
    crypto_derive_auth_token(secret, 32, payload, len, token, sizeof(token));
    send_tlv(fd, MSG_AUTH_RESPONSE, token, AUTH_TOKEN_LEN);

    /* Receive server's HELLO */
    uint8_t server_pk[HELLO_PUBKEY_LEN];
    recv_tlv(fd, &type, server_pk, &len, 2000);
    if (type != MSG_HELLO || len != HELLO_PUBKEY_LEN) {
        FAIL("bad HELLO"); close(fd); return;
    }

    /* Send our HELLO */
    keypair_t kp;
    crypto_load_keypair(&kp);
    send_tlv(fd, MSG_HELLO, kp.pk, crypto_kx_PUBLICKEYBYTES);

    /* Derive session keys (we're the client) */
    session_keys_t sk;
    if (crypto_derive_session(&kp, server_pk, 1, &sk) != 0) {
        FAIL("session derive"); close(fd); return;
    }

    /* Send an encrypted message */
    const char *msg = "integration test message";
    uint8_t ct[1024];
    int ctlen = crypto_encrypt(&sk, (uint8_t *)msg, strlen(msg), ct, sizeof(ct));
    if (ctlen < 0) { FAIL("encrypt"); close(fd); return; }

    send_tlv(fd, MSG_TEXT, ct, ctlen);

    /* Should get ACK */
    if (recv_tlv(fd, &type, payload, &len, 2000) < 0) {
        FAIL("no ACK"); close(fd); return;
    }
    if (type != MSG_ACK) { FAIL("expected ACK"); close(fd); return; }

    close(fd);
    PASS();
}

/* ── Test: IPC disconnect handled gracefully ─────────────── */
static void test_ipc_disconnect(void)
{
    TEST(ipc_client_disconnect_handled);
    int fd = connect_ipc();
    if (fd < 0) { FAIL("connect"); return; }

    /* Just close immediately — daemon should handle it */
    close(fd);
    usleep(100000);

    /* Daemon should still be responsive */
    fd = connect_ipc();
    if (fd < 0) { FAIL("daemon died after disconnect"); return; }
    close(fd);
    PASS();
}

int main(void)
{
    /* Set up temporary home */
    char test_home[256];
    snprintf(test_home, sizeof(test_home), "/tmp/chatd_integ_%d", getpid());
    mkdir(test_home, 0700);
    setenv("HOME", test_home, 1);

    if (crypto_setup() != 0) {
        fprintf(stderr, "crypto_setup failed\n");
        return 1;
    }

    printf("=== integration tests ===\n");
    printf("  Starting chatd...\n");

    if (start_chatd() != 0) {
        fprintf(stderr, "Failed to start chatd\n");
        stop_chatd();
        return 1;
    }

    test_daemon_ipc_connect();
    test_ipc_get_id();
    test_ipc_list_peers_empty();
    test_ipc_send_no_peers();
    test_ipc_add_peer_invalid();
    test_multiple_ipc_clients();
    test_tcp_auth_challenge();
    test_tcp_auth_success();
    test_tcp_auth_reject();
    test_full_handshake();
    test_ipc_disconnect();

    stop_chatd();

    /* Cleanup */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_home);
    system(cmd);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
