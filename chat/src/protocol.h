#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Wire protocol: TLV over TCP
 * [1 byte type][4 bytes length (network order)][N bytes payload] */

#define PROTO_PORT       9876
#define PROTO_LAN_PORT   9877
#define PROTO_MAX_MSG    65536
#define PROTO_MAGIC      "ABLOOM\x01"
#define PROTO_MAGIC_LEN  7

/* Message types */
#define MSG_AUTH_CHALLENGE  0x00
#define MSG_AUTH_RESPONSE   0x01
#define MSG_HELLO           0x02
#define MSG_TEXT             0x03
#define MSG_PEER_LIST       0x04
#define MSG_ACK             0x05

/* Auth challenge/response size */
#define AUTH_CHALLENGE_LEN  32
#define AUTH_TOKEN_LEN      32

/* TLV header */
typedef struct {
    uint8_t  type;
    uint32_t length; /* payload length, network byte order on wire */
} __attribute__((packed)) tlv_header_t;

#define TLV_HDR_SIZE 5

/* HELLO payload: raw X25519 public key */
#define HELLO_PUBKEY_LEN 32

/* LAN discovery datagram */
typedef struct {
    char     magic[PROTO_MAGIC_LEN];
    char     onion[64];   /* .onion address, null-terminated */
    uint8_t  hmac[32];    /* HMAC proving knowledge of shared secret */
} __attribute__((packed)) lan_announce_t;

/* Socket path for local CLI <-> daemon communication */
#define CHATD_SOCK_PATH  "/tmp/chatd.sock"

/* Local IPC message types (CLI to daemon) */
#define IPC_SEND_MSG     0x10
#define IPC_ADD_PEER     0x11
#define IPC_LIST_PEERS   0x12
#define IPC_GET_ID       0x13
#define IPC_RESPONSE     0x14

#endif /* PROTOCOL_H */
