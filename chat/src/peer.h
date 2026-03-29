#ifndef PEER_H
#define PEER_H

#include <stdint.h>
#include <sys/socket.h>
#include "crypto.h"
#include "protocol.h"

#define MAX_PEERS       32
#define ONION_ADDR_LEN  64

typedef enum {
    PEER_DISCONNECTED = 0,
    PEER_CONNECTING,
    PEER_AUTH_SENT,
    PEER_AUTH_OK,
    PEER_ESTABLISHED
} peer_state_t;

typedef struct peer {
    int            fd;
    peer_state_t   state;
    char           onion[ONION_ADDR_LEN];
    uint8_t        pubkey[crypto_kx_PUBLICKEYBYTES];
    session_keys_t session;
    int            is_client;       /* 1 if we initiated */
    uint8_t        auth_challenge[AUTH_CHALLENGE_LEN];
    /* Receive buffer for partial TLV reads */
    uint8_t        rxbuf[PROTO_MAX_MSG + TLV_HDR_SIZE];
    size_t         rxlen;
} peer_t;

typedef struct {
    peer_t  peers[MAX_PEERS];
    int     count;
} peer_list_t;

/* Initialize peer list */
void  peer_list_init(peer_list_t *pl);

/* Add a new peer, returns pointer or NULL if full */
peer_t *peer_add(peer_list_t *pl, int fd, const char *onion, int is_client);

/* Remove a peer by fd, closes the fd */
void  peer_remove(peer_list_t *pl, int fd);

/* Find peer by fd */
peer_t *peer_find_by_fd(peer_list_t *pl, int fd);

/* Find peer by onion address */
peer_t *peer_find_by_onion(peer_list_t *pl, const char *onion);

/* Get count of established peers */
int   peer_count_established(peer_list_t *pl);

/* Format peer list as string for IPC response. Returns malloc'd string. */
char *peer_list_format(peer_list_t *pl);

#endif /* PEER_H */
