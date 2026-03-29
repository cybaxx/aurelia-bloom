#include "peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void peer_list_init(peer_list_t *pl)
{
    memset(pl, 0, sizeof(*pl));
    for (int i = 0; i < MAX_PEERS; i++)
        pl->peers[i].fd = -1;
}

peer_t *peer_add(peer_list_t *pl, int fd, const char *onion, int is_client)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl->peers[i].fd == -1) {
            peer_t *p = &pl->peers[i];
            memset(p, 0, sizeof(*p));
            p->fd = fd;
            p->is_client = is_client;
            p->state = PEER_CONNECTING;
            if (onion)
                snprintf(p->onion, sizeof(p->onion), "%s", onion);
            pl->count++;
            return p;
        }
    }
    return NULL;
}

void peer_remove(peer_list_t *pl, int fd)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl->peers[i].fd == fd) {
            close(fd);
            pl->peers[i].fd = -1;
            pl->peers[i].state = PEER_DISCONNECTED;
            pl->count--;
            return;
        }
    }
}

peer_t *peer_find_by_fd(peer_list_t *pl, int fd)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl->peers[i].fd == fd)
            return &pl->peers[i];
    }
    return NULL;
}

peer_t *peer_find_by_onion(peer_list_t *pl, const char *onion)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl->peers[i].fd != -1 &&
            strcmp(pl->peers[i].onion, onion) == 0)
            return &pl->peers[i];
    }
    return NULL;
}

int peer_count_established(peer_list_t *pl)
{
    int n = 0;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl->peers[i].state == PEER_ESTABLISHED)
            n++;
    }
    return n;
}

char *peer_list_format(peer_list_t *pl)
{
    char *buf = malloc(MAX_PEERS * 128);
    if (!buf) return NULL;
    buf[0] = '\0';
    int off = 0;
    char fp[17];

    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t *p = &pl->peers[i];
        if (p->fd == -1) continue;

        const char *st;
        switch (p->state) {
        case PEER_CONNECTING:  st = "connecting"; break;
        case PEER_AUTH_SENT:   st = "auth";       break;
        case PEER_AUTH_OK:     st = "auth-ok";    break;
        case PEER_ESTABLISHED: st = "ok";         break;
        default:               st = "unknown";    break;
        }

        crypto_fingerprint(p->pubkey, fp, sizeof(fp));
        off += snprintf(buf + off, MAX_PEERS * 128 - off,
                        "%s [%s] %s\n",
                        p->onion[0] ? p->onion : "(unknown)",
                        st, fp);
    }
    return buf;
}
