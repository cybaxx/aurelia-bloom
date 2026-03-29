/*
 * chatd — Tor-based E2E encrypted chat daemon
 *
 * Listens on TCP port 9876 for peer connections and on a Unix socket
 * for local CLI commands. Peers authenticate via a shared secret,
 * exchange X25519 keys, and communicate with XChaCha20-Poly1305.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "protocol.h"
#include "crypto.h"
#include "peer.h"

static volatile int running = 1;
static keypair_t    my_keys;
static uint8_t      secret_hash[32];
static peer_list_t  peers;
static char         my_onion[ONION_ADDR_LEN];

/* IPC clients (CLI connections to Unix socket) */
#define MAX_IPC_CLIENTS 8
static int ipc_fds[MAX_IPC_CLIENTS];
static int ipc_count;

static void sighandler(int sig)
{
    (void)sig;
    running = 0;
}

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ── TLV send/recv helpers ────────────────────────────────── */

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

/* ── Authentication handshake ─────────────────────────────── */

static int do_auth_as_client(peer_t *p)
{
    /* Read challenge from server */
    uint8_t hdr[TLV_HDR_SIZE];
    ssize_t n = read(p->fd, hdr, TLV_HDR_SIZE);
    if (n != TLV_HDR_SIZE || hdr[0] != MSG_AUTH_CHALLENGE) return -1;

    uint32_t plen;
    memcpy(&plen, hdr + 1, 4);
    plen = ntohl(plen);
    if (plen != AUTH_CHALLENGE_LEN) return -1;

    uint8_t challenge[AUTH_CHALLENGE_LEN];
    n = read(p->fd, challenge, AUTH_CHALLENGE_LEN);
    if (n != AUTH_CHALLENGE_LEN) return -1;

    /* Derive token and respond */
    uint8_t token[AUTH_TOKEN_LEN];
    if (crypto_derive_auth_token(secret_hash, sizeof(secret_hash),
                                  challenge, AUTH_CHALLENGE_LEN,
                                  token, AUTH_TOKEN_LEN) != 0)
        return -1;

    if (send_tlv(p->fd, MSG_AUTH_RESPONSE, token, AUTH_TOKEN_LEN) != 0)
        return -1;

    p->state = PEER_AUTH_SENT;
    return 0;
}

static int do_auth_as_server(peer_t *p)
{
    /* Send challenge */
    uint8_t challenge[AUTH_CHALLENGE_LEN];
    randombytes_buf(challenge, AUTH_CHALLENGE_LEN);
    memcpy(p->auth_challenge, challenge, AUTH_CHALLENGE_LEN);

    if (send_tlv(p->fd, MSG_AUTH_CHALLENGE, challenge, AUTH_CHALLENGE_LEN) != 0)
        return -1;

    p->state = PEER_AUTH_SENT;
    return 0;
}

static int verify_auth_response(peer_t *p, const uint8_t *token, uint32_t len)
{
    if (len != AUTH_TOKEN_LEN) return -1;

    uint8_t expected[AUTH_TOKEN_LEN];
    if (crypto_derive_auth_token(secret_hash, sizeof(secret_hash),
                                  p->auth_challenge, AUTH_CHALLENGE_LEN,
                                  expected, AUTH_TOKEN_LEN) != 0)
        return -1;

    if (sodium_memcmp(token, expected, AUTH_TOKEN_LEN) != 0) {
        fprintf(stderr, "chatd: auth failed for peer fd=%d\n", p->fd);
        return -1;
    }

    p->state = PEER_AUTH_OK;
    /* Send our HELLO (public key) */
    send_tlv(p->fd, MSG_HELLO, my_keys.pk, crypto_kx_PUBLICKEYBYTES);
    return 0;
}

/* ── Key exchange ─────────────────────────────────────────── */

static int handle_hello(peer_t *p, const uint8_t *payload, uint32_t len)
{
    if (len != crypto_kx_PUBLICKEYBYTES) return -1;

    memcpy(p->pubkey, payload, crypto_kx_PUBLICKEYBYTES);

    /* If we're auth-ok as client, send our HELLO back */
    if (p->is_client && p->state == PEER_AUTH_SENT) {
        p->state = PEER_AUTH_OK;
        send_tlv(p->fd, MSG_HELLO, my_keys.pk, crypto_kx_PUBLICKEYBYTES);
    }

    /* Derive session keys */
    if (crypto_derive_session(&my_keys, p->pubkey, p->is_client,
                               &p->session) != 0) {
        fprintf(stderr, "chatd: session key derivation failed\n");
        return -1;
    }

    p->state = PEER_ESTABLISHED;
    char fp[17];
    crypto_fingerprint(p->pubkey, fp, sizeof(fp));
    fprintf(stderr, "chatd: peer established [%s] %s\n",
            p->onion[0] ? p->onion : "?", fp);
    return 0;
}

/* ── Message handling ─────────────────────────────────────── */

static int handle_text(peer_t *p, const uint8_t *payload, uint32_t len)
{
    if (p->state != PEER_ESTABLISHED) return -1;

    uint8_t plain[PROTO_MAX_MSG];
    int plen = crypto_decrypt(&p->session, payload, len, plain, sizeof(plain));
    if (plen < 0) {
        fprintf(stderr, "chatd: decrypt failed from peer fd=%d\n", p->fd);
        return -1;
    }

    /* Null-terminate */
    if ((size_t)plen < sizeof(plain))
        plain[plen] = '\0';

    char fp[17];
    crypto_fingerprint(p->pubkey, fp, sizeof(fp));

    /* Forward to all IPC clients */
    char msg[PROTO_MAX_MSG + 128];
    int mlen = snprintf(msg, sizeof(msg), "[%s] %s",
                        p->onion[0] ? p->onion : fp, (char *)plain);

    for (int i = 0; i < ipc_count; i++) {
        send_tlv(ipc_fds[i], IPC_RESPONSE, (uint8_t *)msg, mlen);
    }

    /* Send ACK */
    send_tlv(p->fd, MSG_ACK, NULL, 0);
    return 0;
}

static void broadcast_to_peers(const uint8_t *plaintext, size_t len)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t *p = &peers.peers[i];
        if (p->fd == -1 || p->state != PEER_ESTABLISHED) continue;

        uint8_t ct[PROTO_MAX_MSG];
        int ctlen = crypto_encrypt(&p->session, plaintext, len, ct, sizeof(ct));
        if (ctlen > 0)
            send_tlv(p->fd, MSG_TEXT, ct, ctlen);
    }
}

/* ── Peer data handler ────────────────────────────────────── */

static void handle_peer_data(peer_t *p)
{
    ssize_t n = read(p->fd, p->rxbuf + p->rxlen,
                     sizeof(p->rxbuf) - p->rxlen);
    if (n <= 0) {
        fprintf(stderr, "chatd: peer disconnected fd=%d\n", p->fd);
        peer_remove(&peers, p->fd);
        return;
    }
    p->rxlen += n;

    /* Process complete TLV messages */
    while (p->rxlen >= TLV_HDR_SIZE) {
        uint8_t type = p->rxbuf[0];
        uint32_t plen;
        memcpy(&plen, p->rxbuf + 1, 4);
        plen = ntohl(plen);

        if (plen > PROTO_MAX_MSG) {
            fprintf(stderr, "chatd: oversized message, dropping peer\n");
            peer_remove(&peers, p->fd);
            return;
        }

        if (p->rxlen < TLV_HDR_SIZE + plen)
            break; /* need more data */

        uint8_t *payload = p->rxbuf + TLV_HDR_SIZE;
        int rc = 0;

        switch (type) {
        case MSG_AUTH_RESPONSE:
            rc = verify_auth_response(p, payload, plen);
            break;
        case MSG_HELLO:
            rc = handle_hello(p, payload, plen);
            break;
        case MSG_TEXT:
            rc = handle_text(p, payload, plen);
            break;
        case MSG_ACK:
            break;
        default:
            fprintf(stderr, "chatd: unknown type 0x%02x\n", type);
        }

        if (rc < 0) {
            peer_remove(&peers, p->fd);
            return;
        }

        /* Shift buffer */
        size_t consumed = TLV_HDR_SIZE + plen;
        memmove(p->rxbuf, p->rxbuf + consumed, p->rxlen - consumed);
        p->rxlen -= consumed;
    }
}

/* ── SOCKS5 connect (for Tor) ─────────────────────────────── */

static int socks5_connect(const char *onion, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in proxy;
    memset(&proxy, 0, sizeof(proxy));
    proxy.sin_family = AF_INET;
    proxy.sin_port = htons(9050);
    inet_pton(AF_INET, "127.0.0.1", &proxy.sin_addr);

    if (connect(fd, (struct sockaddr *)&proxy, sizeof(proxy)) < 0) {
        close(fd);
        return -1;
    }

    /* SOCKS5 greeting: no auth */
    uint8_t greet[] = {0x05, 0x01, 0x00};
    write(fd, greet, 3);
    uint8_t resp[2];
    if (read(fd, resp, 2) != 2 || resp[1] != 0x00) { close(fd); return -1; }

    /* SOCKS5 connect: domain name */
    size_t olen = strlen(onion);
    uint8_t req[4 + 1 + 256 + 2];
    req[0] = 0x05; /* version */
    req[1] = 0x01; /* connect */
    req[2] = 0x00; /* reserved */
    req[3] = 0x03; /* domain */
    req[4] = (uint8_t)olen;
    memcpy(req + 5, onion, olen);
    uint16_t nport = htons(port);
    memcpy(req + 5 + olen, &nport, 2);

    write(fd, req, 5 + olen + 2);

    uint8_t cresp[10];
    if (read(fd, cresp, 10) < 4 || cresp[1] != 0x00) {
        close(fd);
        return -1;
    }

    return fd;
}

static int connect_to_peer(const char *onion)
{
    fprintf(stderr, "chatd: connecting to %s via Tor...\n", onion);
    int fd = socks5_connect(onion, PROTO_PORT);
    if (fd < 0) {
        fprintf(stderr, "chatd: SOCKS5 connect failed to %s\n", onion);
        return -1;
    }

    peer_t *p = peer_add(&peers, fd, onion, 1);
    if (!p) { close(fd); return -1; }

    /* Start client-side auth */
    if (do_auth_as_client(p) != 0) {
        peer_remove(&peers, fd);
        return -1;
    }

    return 0;
}

/* ── IPC handler (local CLI) ──────────────────────────────── */

static void handle_ipc(int ipc_fd)
{
    uint8_t buf[PROTO_MAX_MSG + TLV_HDR_SIZE];
    ssize_t n = read(ipc_fd, buf, sizeof(buf));
    if (n <= 0) {
        /* Remove IPC client */
        for (int i = 0; i < ipc_count; i++) {
            if (ipc_fds[i] == ipc_fd) {
                close(ipc_fd);
                ipc_fds[i] = ipc_fds[--ipc_count];
                break;
            }
        }
        return;
    }

    if (n < TLV_HDR_SIZE) return;

    uint8_t type = buf[0];
    uint32_t plen;
    memcpy(&plen, buf + 1, 4);
    plen = ntohl(plen);
    uint8_t *payload = buf + TLV_HDR_SIZE;

    switch (type) {
    case IPC_SEND_MSG:
        broadcast_to_peers(payload, plen);
        send_tlv(ipc_fd, IPC_RESPONSE, (uint8_t *)"sent", 4);
        break;
    case IPC_ADD_PEER: {
        char onion[ONION_ADDR_LEN];
        size_t cp = plen < sizeof(onion) - 1 ? plen : sizeof(onion) - 1;
        memcpy(onion, payload, cp);
        onion[cp] = '\0';
        int rc = connect_to_peer(onion);
        const char *resp = rc == 0 ? "connecting" : "failed";
        send_tlv(ipc_fd, IPC_RESPONSE, (uint8_t *)resp, strlen(resp));
        break;
    }
    case IPC_LIST_PEERS: {
        char *list = peer_list_format(&peers);
        if (list) {
            send_tlv(ipc_fd, IPC_RESPONSE, (uint8_t *)list, strlen(list));
            free(list);
        }
        break;
    }
    case IPC_GET_ID: {
        char fp[17];
        crypto_fingerprint(my_keys.pk, fp, sizeof(fp));
        char info[256];
        int ilen = snprintf(info, sizeof(info), "onion: %s\nkey:   %s",
                            my_onion, fp);
        send_tlv(ipc_fd, IPC_RESPONSE, (uint8_t *)info, ilen);
        break;
    }
    }
}

/* ── Load .onion address ──────────────────────────────────── */

static void load_onion_address(void)
{
    const char *paths[] = {
        "/var/lib/tor/chatd/hostname",
        "/var/db/tor/chatd/hostname",
        NULL
    };

    my_onion[0] = '\0';
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            if (fgets(my_onion, sizeof(my_onion), f)) {
                /* Strip newline */
                char *nl = strchr(my_onion, '\n');
                if (nl) *nl = '\0';
            }
            fclose(f);
            break;
        }
    }

    if (my_onion[0])
        fprintf(stderr, "chatd: onion address: %s\n", my_onion);
    else
        fprintf(stderr, "chatd: warning: no .onion address found (tor running?)\n");
}

/* ── LAN discovery ────────────────────────────────────────── */

static int setup_lan_discovery(void)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTO_LAN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    set_nonblock(fd);
    return fd;
}

static void send_lan_announce(int udp_fd)
{
    if (!my_onion[0]) return;

    lan_announce_t ann;
    memset(&ann, 0, sizeof(ann));
    memcpy(ann.magic, PROTO_MAGIC, PROTO_MAGIC_LEN);
    snprintf(ann.onion, sizeof(ann.onion), "%s", my_onion);
    crypto_hmac(secret_hash, sizeof(secret_hash),
                (uint8_t *)ann.onion, strlen(ann.onion), ann.hmac);

    struct sockaddr_in bcast;
    memset(&bcast, 0, sizeof(bcast));
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(PROTO_LAN_PORT);
    inet_pton(AF_INET, "192.168.56.255", &bcast.sin_addr);

    sendto(udp_fd, &ann, sizeof(ann), 0,
           (struct sockaddr *)&bcast, sizeof(bcast));
}

static void handle_lan_announce(int udp_fd)
{
    lan_announce_t ann;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    ssize_t n = recvfrom(udp_fd, &ann, sizeof(ann), 0,
                         (struct sockaddr *)&from, &fromlen);
    if (n < (ssize_t)sizeof(ann)) return;
    if (memcmp(ann.magic, PROTO_MAGIC, PROTO_MAGIC_LEN) != 0) return;

    /* Don't connect to ourselves */
    if (my_onion[0] && strcmp(ann.onion, my_onion) == 0) return;

    /* Verify HMAC (proves same shared secret) */
    if (crypto_hmac_verify(secret_hash, sizeof(secret_hash),
                           (uint8_t *)ann.onion, strlen(ann.onion),
                           ann.hmac) != 0)
        return;

    /* Already connected? */
    if (peer_find_by_onion(&peers, ann.onion)) return;

    fprintf(stderr, "chatd: discovered LAN peer %s\n", ann.onion);
    connect_to_peer(ann.onion);
}

/* ── Main event loop ──────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGPIPE, SIG_IGN);

    if (crypto_setup() != 0) return 1;
    if (crypto_load_keypair(&my_keys) != 0) return 1;
    if (crypto_load_secret(NULL, secret_hash, sizeof(secret_hash)) != 0)
        return 1;

    peer_list_init(&peers);
    load_onion_address();

    /* TCP listener */
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(PROTO_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(tcp_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(tcp_fd, 5);
    set_nonblock(tcp_fd);

    /* Unix socket for IPC */
    unlink(CHATD_SOCK_PATH);
    int unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un uaddr;
    memset(&uaddr, 0, sizeof(uaddr));
    uaddr.sun_family = AF_UNIX;
    snprintf(uaddr.sun_path, sizeof(uaddr.sun_path), "%s", CHATD_SOCK_PATH);
    bind(unix_fd, (struct sockaddr *)&uaddr, sizeof(uaddr));
    chmod(CHATD_SOCK_PATH, 0660);
    listen(unix_fd, 5);
    set_nonblock(unix_fd);

    /* LAN discovery */
    int udp_fd = setup_lan_discovery();
    int announce_counter = 0;

    fprintf(stderr, "chatd: listening on port %d\n", PROTO_PORT);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(tcp_fd, &rfds);
        FD_SET(unix_fd, &rfds);
        int maxfd = tcp_fd > unix_fd ? tcp_fd : unix_fd;

        if (udp_fd >= 0) {
            FD_SET(udp_fd, &rfds);
            if (udp_fd > maxfd) maxfd = udp_fd;
        }

        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers.peers[i].fd >= 0) {
                FD_SET(peers.peers[i].fd, &rfds);
                if (peers.peers[i].fd > maxfd) maxfd = peers.peers[i].fd;
            }
        }

        for (int i = 0; i < ipc_count; i++) {
            FD_SET(ipc_fds[i], &rfds);
            if (ipc_fds[i] > maxfd) maxfd = ipc_fds[i];
        }

        struct timeval tv = {5, 0}; /* 5s timeout for LAN announce */
        int nready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (nready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* New TCP connection */
        if (FD_ISSET(tcp_fd, &rfds)) {
            int cfd = accept(tcp_fd, NULL, NULL);
            if (cfd >= 0) {
                peer_t *p = peer_add(&peers, cfd, NULL, 0);
                if (p) {
                    do_auth_as_server(p);
                } else {
                    close(cfd);
                }
            }
        }

        /* New IPC connection */
        if (FD_ISSET(unix_fd, &rfds)) {
            int cfd = accept(unix_fd, NULL, NULL);
            if (cfd >= 0 && ipc_count < MAX_IPC_CLIENTS) {
                ipc_fds[ipc_count++] = cfd;
            } else if (cfd >= 0) {
                close(cfd);
            }
        }

        /* LAN discovery */
        if (udp_fd >= 0 && FD_ISSET(udp_fd, &rfds))
            handle_lan_announce(udp_fd);

        /* Peer data */
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers.peers[i].fd >= 0 &&
                FD_ISSET(peers.peers[i].fd, &rfds))
                handle_peer_data(&peers.peers[i]);
        }

        /* IPC data */
        for (int i = 0; i < ipc_count; i++) {
            if (FD_ISSET(ipc_fds[i], &rfds))
                handle_ipc(ipc_fds[i]);
        }

        /* Periodic LAN announce */
        if (udp_fd >= 0 && ++announce_counter >= 6) {
            send_lan_announce(udp_fd);
            announce_counter = 0;
        }
    }

    /* Cleanup */
    close(tcp_fd);
    close(unix_fd);
    unlink(CHATD_SOCK_PATH);
    if (udp_fd >= 0) close(udp_fd);

    for (int i = 0; i < MAX_PEERS; i++)
        if (peers.peers[i].fd >= 0)
            close(peers.peers[i].fd);

    fprintf(stderr, "chatd: shutdown\n");
    return 0;
}
