/*
 * chat — CLI client for chatd
 *
 * Connects to the local chatd daemon via Unix socket and provides
 * an interactive interface for sending/receiving encrypted messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>

#include "protocol.h"
#include "crypto.h"

static int daemon_fd = -1;

static int connect_daemon(void)
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

static int read_response(int fd, char *buf, size_t buflen)
{
    uint8_t hdr[TLV_HDR_SIZE];
    ssize_t n = read(fd, hdr, TLV_HDR_SIZE);
    if (n != TLV_HDR_SIZE) return -1;

    uint32_t plen;
    memcpy(&plen, hdr + 1, 4);
    plen = ntohl(plen);

    if (plen >= buflen) plen = buflen - 1;
    if (plen > 0) {
        n = read(fd, buf, plen);
        if (n <= 0) return -1;
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    return 0;
}

static void cmd_init(const char *secret)
{
    if (crypto_setup() != 0) {
        fprintf(stderr, "Failed to initialize crypto\n");
        return;
    }

    keypair_t kp;
    if (crypto_load_keypair(&kp) != 0) {
        fprintf(stderr, "Failed to load/generate keypair\n");
        return;
    }

    uint8_t hash[32];
    if (crypto_load_secret(secret, hash, sizeof(hash)) != 0) {
        fprintf(stderr, "Failed to set shared secret\n");
        return;
    }

    char fp[17];
    crypto_fingerprint(kp.pk, fp, sizeof(fp));
    printf("Initialized. Key fingerprint: %s\n", fp);
    printf("Shared secret: %s\n", secret ? secret : DEFAULT_SHARED_SECRET);
}

static void cmd_send(const char *msg)
{
    send_tlv(daemon_fd, IPC_SEND_MSG, (uint8_t *)msg, strlen(msg));
    char resp[256];
    read_response(daemon_fd, resp, sizeof(resp));
}

static void cmd_add_peer(const char *onion)
{
    send_tlv(daemon_fd, IPC_ADD_PEER, (uint8_t *)onion, strlen(onion));
    char resp[256];
    if (read_response(daemon_fd, resp, sizeof(resp)) == 0)
        printf("%s\n", resp);
}

static void cmd_peers(void)
{
    send_tlv(daemon_fd, IPC_LIST_PEERS, NULL, 0);
    char resp[4096];
    if (read_response(daemon_fd, resp, sizeof(resp)) == 0) {
        if (resp[0])
            printf("%s", resp);
        else
            printf("No peers connected\n");
    }
}

static void cmd_id(void)
{
    send_tlv(daemon_fd, IPC_GET_ID, NULL, 0);
    char resp[512];
    if (read_response(daemon_fd, resp, sizeof(resp)) == 0)
        printf("%s\n", resp);
}

static void interactive_mode(void)
{
    printf("chat> type messages and press enter (Ctrl-C to quit)\n");

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(daemon_fd, &rfds);
        int maxfd = daemon_fd > STDIN_FILENO ? daemon_fd : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Incoming message from daemon */
        if (FD_ISSET(daemon_fd, &rfds)) {
            char resp[PROTO_MAX_MSG];
            if (read_response(daemon_fd, resp, sizeof(resp)) == 0)
                printf("\r%s\nchat> ", resp);
            else
                break;
            fflush(stdout);
        }

        /* User input */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[4096];
            if (!fgets(line, sizeof(line), stdin))
                break;

            /* Strip newline */
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (line[0] == '\0') continue;

            /* Local commands */
            if (strcmp(line, "/peers") == 0) {
                cmd_peers();
            } else if (strcmp(line, "/id") == 0) {
                cmd_id();
            } else if (strncmp(line, "/add ", 5) == 0) {
                cmd_add_peer(line + 5);
            } else if (strcmp(line, "/quit") == 0 ||
                       strcmp(line, "/q") == 0) {
                break;
            } else if (strcmp(line, "/help") == 0) {
                printf("Commands:\n"
                       "  /peers      - list connected peers\n"
                       "  /id         - show own identity\n"
                       "  /add <onion> - add a peer\n"
                       "  /quit       - exit\n"
                       "  <text>      - send message to all peers\n");
            } else {
                cmd_send(line);
            }
            printf("chat> ");
            fflush(stdout);
        }
    }
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: chat [command] [args]\n"
        "\n"
        "Commands:\n"
        "  init [secret]      Initialize (default secret: \"aurelia-bloom\")\n"
        "  send <message>     Send message to all peers\n"
        "  add-peer <onion>   Connect to a peer via .onion address\n"
        "  peers              List connected peers\n"
        "  id                 Show own .onion address and key fingerprint\n"
        "  (no command)       Interactive mode\n"
    );
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "init") == 0) {
        cmd_init(argc >= 3 ? argv[2] : NULL);
        return 0;
    }

    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 ||
                      strcmp(argv[1], "--help") == 0)) {
        usage();
        return 0;
    }

    daemon_fd = connect_daemon();
    if (daemon_fd < 0) {
        fprintf(stderr, "Cannot connect to chatd. Is the daemon running?\n");
        return 1;
    }

    if (argc < 2) {
        interactive_mode();
    } else if (strcmp(argv[1], "send") == 0 && argc >= 3) {
        cmd_send(argv[2]);
    } else if (strcmp(argv[1], "add-peer") == 0 && argc >= 3) {
        cmd_add_peer(argv[2]);
    } else if (strcmp(argv[1], "peers") == 0) {
        cmd_peers();
    } else if (strcmp(argv[1], "id") == 0) {
        cmd_id();
    } else {
        usage();
        close(daemon_fd);
        return 1;
    }

    close(daemon_fd);
    return 0;
}
