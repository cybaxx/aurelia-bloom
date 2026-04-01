/*
 * irc-gateway — IRC-to-chatd bridge
 *
 * Presents an IRC server on localhost:6667 so irssi (or any IRC client)
 * can be used as a frontend for the Tor chat system.
 *
 * Maps:
 *   IRC PRIVMSG #aurelia <text>  ->  chatd IPC_SEND_MSG
 *   chatd incoming message       ->  IRC PRIVMSG #aurelia :<text>
 *   /add <onion>  in channel     ->  chatd IPC_ADD_PEER
 *   IRC JOIN #aurelia            ->  auto-join, show peers
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

#include "protocol.h"

#define IRC_PORT      6667
#define CHANNEL       "#aurelia"
#define SERVER_NAME   "aurelia-bloom"
#define MAX_LINE      4096

static volatile int running = 1;
static int chatd_fd = -1;
static int irc_client_fd = -1;
static char my_nick[64] = "anon";
static char my_onion[256] = "";
static char my_fingerprint[32] = "";

static void sighandler(int sig) { (void)sig; running = 0; }

/* ── TLV helpers (chatd IPC) ──────────────────────────────── */

static int chatd_send(uint8_t type, const uint8_t *payload, uint32_t len)
{
    if (chatd_fd < 0) return -1;
    uint8_t hdr[TLV_HDR_SIZE];
    hdr[0] = type;
    uint32_t nlen = htonl(len);
    memcpy(hdr + 1, &nlen, 4);
    if (write(chatd_fd, hdr, TLV_HDR_SIZE) != TLV_HDR_SIZE) return -1;
    if (len > 0 && write(chatd_fd, payload, len) != (ssize_t)len) return -1;
    return 0;
}

static int chatd_read_response(char *buf, size_t buflen)
{
    uint8_t hdr[TLV_HDR_SIZE];
    ssize_t n = read(chatd_fd, hdr, TLV_HDR_SIZE);
    if (n != TLV_HDR_SIZE) return -1;
    uint32_t plen;
    memcpy(&plen, hdr + 1, 4);
    plen = ntohl(plen);
    if (plen >= buflen) plen = buflen - 1;
    if (plen > 0) {
        n = read(chatd_fd, buf, plen);
        if (n <= 0) return -1;
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
    }
    return 0;
}

static int connect_chatd(void)
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

/* ── IRC send helpers ─────────────────────────────────────── */

static void irc_send(const char *fmt, ...)
{
    if (irc_client_fd < 0) return;
    char buf[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (len < 0) return;
    buf[len] = '\r';
    buf[len + 1] = '\n';
    write(irc_client_fd, buf, len + 2);
}

static void irc_send_welcome(void)
{
    irc_send(":%s 001 %s :Welcome to Aurelia Bloom Tor Chat", SERVER_NAME, my_nick);
    irc_send(":%s 002 %s :Your host is %s, running chatd", SERVER_NAME, my_nick, SERVER_NAME);
    irc_send(":%s 003 %s :E2E encrypted via X25519 + XChaCha20-Poly1305", SERVER_NAME, my_nick);
    irc_send(":%s 004 %s %s chatd o o", SERVER_NAME, my_nick, SERVER_NAME);
    irc_send(":%s 375 %s :- %s Message of the Day -", SERVER_NAME, my_nick, SERVER_NAME);
    irc_send(":%s 372 %s :- Tor-routed, end-to-end encrypted peer chat", SERVER_NAME, my_nick);
    if (my_onion[0])
        irc_send(":%s 372 %s :- Your address: %s", SERVER_NAME, my_nick, my_onion);
    if (my_fingerprint[0])
        irc_send(":%s 372 %s :- Key fingerprint: %s", SERVER_NAME, my_nick, my_fingerprint);
    irc_send(":%s 372 %s :- Type /msg %s !add <addr.onion> to add peers", SERVER_NAME, my_nick, CHANNEL);
    irc_send(":%s 376 %s :End of MOTD", SERVER_NAME, my_nick);
}

static void irc_join_channel(void)
{
    irc_send(":%s!%s@tor JOIN %s", my_nick, my_nick, CHANNEL);
    irc_send(":%s 332 %s %s :Aurelia Bloom — Tor E2E Chat", SERVER_NAME, my_nick, CHANNEL);
    irc_send(":%s 353 %s = %s :%s @%s", SERVER_NAME, my_nick, CHANNEL, my_nick, SERVER_NAME);
    irc_send(":%s 366 %s %s :End of /NAMES list", SERVER_NAME, my_nick, CHANNEL);

    /* List current peers as joins */
    int pfd = connect_chatd();
    if (pfd >= 0) {
        chatd_send(IPC_LIST_PEERS, NULL, 0);
        char resp[4096];
        if (chatd_read_response(resp, sizeof(resp)) == 0 && resp[0]) {
            /* Parse peer list — each line is: "onion [status] fingerprint" */
            char *line = strtok(resp, "\n");
            while (line) {
                char peer_nick[80];
                /* Extract first token as nick-ish name */
                char *sp = strchr(line, ' ');
                if (sp) {
                    size_t nlen = sp - line;
                    if (nlen > sizeof(peer_nick) - 1) nlen = sizeof(peer_nick) - 1;
                    memcpy(peer_nick, line, nlen);
                    peer_nick[nlen] = '\0';
                } else {
                    snprintf(peer_nick, sizeof(peer_nick), "%s", line);
                }
                irc_send(":%s!peer@tor JOIN %s", peer_nick, CHANNEL);
                line = strtok(NULL, "\n");
            }
        }
        close(pfd);
    }
}

/* ── Fetch identity from chatd ────────────────────────────── */

static void fetch_identity(void)
{
    int fd = connect_chatd();
    if (fd < 0) return;
    int old = chatd_fd;
    chatd_fd = fd;
    chatd_send(IPC_GET_ID, NULL, 0);
    char resp[512];
    if (chatd_read_response(resp, sizeof(resp)) == 0) {
        /* Parse "onion: xxx\nkey:   yyy" */
        char *o = strstr(resp, "onion: ");
        if (o) {
            o += 7;
            char *nl = strchr(o, '\n');
            size_t len = nl ? (size_t)(nl - o) : strlen(o);
            if (len >= sizeof(my_onion)) len = sizeof(my_onion) - 1;
            memcpy(my_onion, o, len);
            my_onion[len] = '\0';
        }
        char *k = strstr(resp, "key:   ");
        if (k) {
            k += 7;
            char *nl = strchr(k, '\n');
            size_t len = nl ? (size_t)(nl - k) : strlen(k);
            if (len >= sizeof(my_fingerprint)) len = sizeof(my_fingerprint) - 1;
            memcpy(my_fingerprint, k, len);
            my_fingerprint[len] = '\0';
        }
    }
    chatd_fd = old;
    close(fd);
}

/* ── Handle IRC client line ───────────────────────────────── */

static void handle_irc_line(char *line)
{
    /* Strip \r\n */
    char *cr = strchr(line, '\r');
    if (cr) *cr = '\0';
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';

    if (strncmp(line, "NICK ", 5) == 0) {
        snprintf(my_nick, sizeof(my_nick), "%s", line + 5);
    } else if (strncmp(line, "USER ", 5) == 0) {
        irc_send_welcome();
    } else if (strncmp(line, "JOIN ", 5) == 0) {
        irc_join_channel();
    } else if (strncmp(line, "PING ", 5) == 0) {
        irc_send(":%s PONG %s %s", SERVER_NAME, SERVER_NAME, line + 5);
    } else if (strncmp(line, "PRIVMSG ", 8) == 0) {
        char *target = line + 8;
        char *text = strchr(target, ' ');
        if (!text) return;
        *text++ = '\0';
        if (*text == ':') text++;

        /* Handle gateway commands */
        if (*text == '!') {
            if (strncmp(text, "!add ", 5) == 0) {
                char *onion = text + 5;
                while (*onion == ' ') onion++;
                chatd_send(IPC_ADD_PEER, (uint8_t *)onion, strlen(onion));
                char resp[256];
                chatd_read_response(resp, sizeof(resp));
                irc_send(":%s PRIVMSG %s :[gateway] %s: %s",
                         SERVER_NAME, CHANNEL, onion, resp);
            } else if (strcmp(text, "!peers") == 0) {
                chatd_send(IPC_LIST_PEERS, NULL, 0);
                char resp[4096];
                if (chatd_read_response(resp, sizeof(resp)) == 0) {
                    char *tok = strtok(resp, "\n");
                    while (tok) {
                        irc_send(":%s PRIVMSG %s :[peers] %s",
                                 SERVER_NAME, CHANNEL, tok);
                        tok = strtok(NULL, "\n");
                    }
                }
            } else if (strcmp(text, "!id") == 0) {
                chatd_send(IPC_GET_ID, NULL, 0);
                char resp[512];
                if (chatd_read_response(resp, sizeof(resp)) == 0) {
                    char *tok = strtok(resp, "\n");
                    while (tok) {
                        irc_send(":%s PRIVMSG %s :[id] %s",
                                 SERVER_NAME, CHANNEL, tok);
                        tok = strtok(NULL, "\n");
                    }
                }
            } else if (strcmp(text, "!help") == 0) {
                irc_send(":%s PRIVMSG %s :!add <addr.onion> — connect to peer",
                         SERVER_NAME, CHANNEL);
                irc_send(":%s PRIVMSG %s :!peers — list connected peers",
                         SERVER_NAME, CHANNEL);
                irc_send(":%s PRIVMSG %s :!id — show identity",
                         SERVER_NAME, CHANNEL);
            }
            return;
        }

        /* Regular message -> broadcast to chatd peers */
        chatd_send(IPC_SEND_MSG, (uint8_t *)text, strlen(text));
        char resp[256];
        chatd_read_response(resp, sizeof(resp));
    } else if (strncmp(line, "QUIT", 4) == 0) {
        running = 0;
    } else if (strncmp(line, "MODE ", 5) == 0) {
        /* Ignore mode requests */
    } else if (strncmp(line, "WHO ", 4) == 0) {
        irc_send(":%s 315 %s %s :End of WHO list", SERVER_NAME, my_nick, line + 4);
    } else if (strncmp(line, "CAP ", 4) == 0) {
        /* Ignore capability negotiation */
    }
}

/* ── Handle incoming chatd message ────────────────────────── */

static void handle_chatd_message(void)
{
    char buf[PROTO_MAX_MSG];
    if (chatd_read_response(buf, sizeof(buf)) != 0) {
        fprintf(stderr, "irc-gateway: chatd disconnected\n");
        close(chatd_fd);
        chatd_fd = -1;
        return;
    }

    if (buf[0] == '\0') return;

    /*
     * chatd sends messages as "[onion-or-fingerprint] text"
     * Map to IRC: :sender!peer@tor PRIVMSG #aurelia :text
     */
    char sender[128] = "peer";
    char *msg = buf;

    if (buf[0] == '[') {
        char *end = strchr(buf, ']');
        if (end) {
            size_t slen = end - buf - 1;
            if (slen >= sizeof(sender)) slen = sizeof(sender) - 1;
            memcpy(sender, buf + 1, slen);
            sender[slen] = '\0';
            msg = end + 1;
            while (*msg == ' ') msg++;
        }
    }

    /* Sanitize sender for IRC nick (replace dots with underscores) */
    for (char *p = sender; *p; p++) {
        if (*p == '.' || *p == ' ' || *p == '@' || *p == '!')
            *p = '_';
    }
    /* Truncate long onion nicks to last 16 chars */
    size_t slen = strlen(sender);
    if (slen > 20) {
        memmove(sender, sender + slen - 16, 17);
    }

    irc_send(":%s!peer@tor PRIVMSG %s :%s", sender, CHANNEL, msg);
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void)
{
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGPIPE, SIG_IGN);

    /* Connect to chatd first */
    chatd_fd = connect_chatd();
    if (chatd_fd < 0) {
        fprintf(stderr, "irc-gateway: cannot connect to chatd. Is it running?\n");
        return 1;
    }
    fprintf(stderr, "irc-gateway: connected to chatd\n");

    fetch_identity();
    fprintf(stderr, "irc-gateway: onion=%s fp=%s\n", my_onion, my_fingerprint);

    /* Listen for IRC client */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(IRC_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(listen_fd, 1);

    fprintf(stderr, "irc-gateway: listening on 127.0.0.1:%d\n", IRC_PORT);
    fprintf(stderr, "irc-gateway: connect with: irssi -c 127.0.0.1 -p %d\n", IRC_PORT);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);

        int maxfd = -1;

        if (irc_client_fd < 0) {
            /* Wait for IRC client */
            FD_SET(listen_fd, &rfds);
            maxfd = listen_fd;
        } else {
            FD_SET(irc_client_fd, &rfds);
            maxfd = irc_client_fd;
        }

        if (chatd_fd >= 0) {
            FD_SET(chatd_fd, &rfds);
            if (chatd_fd > maxfd) maxfd = chatd_fd;
        }

        struct timeval tv = {5, 0};
        int n = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Accept IRC client */
        if (irc_client_fd < 0 && FD_ISSET(listen_fd, &rfds)) {
            irc_client_fd = accept(listen_fd, NULL, NULL);
            if (irc_client_fd >= 0)
                fprintf(stderr, "irc-gateway: IRC client connected\n");
        }

        /* IRC client data */
        if (irc_client_fd >= 0 && FD_ISSET(irc_client_fd, &rfds)) {
            static char irc_buf[MAX_LINE];
            static int irc_buflen = 0;

            ssize_t nr = read(irc_client_fd, irc_buf + irc_buflen,
                             sizeof(irc_buf) - irc_buflen - 1);
            if (nr <= 0) {
                fprintf(stderr, "irc-gateway: IRC client disconnected\n");
                close(irc_client_fd);
                irc_client_fd = -1;
                irc_buflen = 0;
                continue;
            }
            irc_buflen += nr;
            irc_buf[irc_buflen] = '\0';

            /* Process complete lines */
            char *start = irc_buf;
            char *eol;
            while ((eol = strstr(start, "\r\n")) != NULL ||
                   (eol = strchr(start, '\n')) != NULL) {
                *eol = '\0';
                int skip = (*(eol + 1) == '\n') ? 2 : 1;
                if (eol > start && *(eol - 1) == '\r')
                    *(eol - 1) = '\0';
                /* Check if eol+1 exists and is \n for \r\n */
                if (*eol == '\0' && eol + 1 < irc_buf + irc_buflen && *(eol + 1) == '\n')
                    skip = 2;
                handle_irc_line(start);
                start = eol + skip;
            }
            /* Shift remaining */
            irc_buflen = (irc_buf + irc_buflen) - start;
            if (irc_buflen > 0)
                memmove(irc_buf, start, irc_buflen);
        }

        /* chatd data (incoming messages from peers) */
        if (chatd_fd >= 0 && FD_ISSET(chatd_fd, &rfds)) {
            handle_chatd_message();
        }
    }

    if (irc_client_fd >= 0) close(irc_client_fd);
    if (chatd_fd >= 0) close(chatd_fd);
    close(listen_fd);

    fprintf(stderr, "irc-gateway: shutdown\n");
    return 0;
}
