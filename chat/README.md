# Tor-Based E2E Encrypted P2P Chat

Secure peer-to-peer chat for the cyber range. Uses Tor hidden services for NAT traversal and libsodium for end-to-end encryption.

## Architecture

Each attacker VM runs:
- **tor** — publishes a hidden service on port 9876
- **chatd** — C daemon handling peer connections with E2E encryption
- **chat** — CLI client for interactive messaging

Messages are encrypted with X25519 key exchange + XChaCha20-Poly1305 (via libsodium).

## Quick Start

```sh
# On the attacker VM (already provisioned):
chat id                        # show your .onion address
chat add-peer <other.onion>    # connect to another player
chat                           # interactive mode
```

## Commands

```
chat init [secret]       Initialize keys (default secret: "aurelia-bloom")
chat send "message"      Send to all connected peers
chat add-peer <onion>    Connect to a peer's .onion address
chat peers               List connected peers
chat id                  Show own .onion address and key fingerprint
chat                     Interactive mode
```

### Interactive Mode Commands

```
/peers       List connected peers
/id          Show own identity
/add <onion> Add a peer
/quit        Exit
<text>       Send message to all peers
```

## How It Works

1. **Authentication**: Peers prove knowledge of a shared secret (default: `aurelia-bloom`) via Argon2id-derived challenge-response
2. **Key Exchange**: X25519 public keys exchanged after auth
3. **Session Keys**: Derived via libsodium's `crypto_kx` API (separate rx/tx keys)
4. **Encryption**: Every message encrypted with XChaCha20-Poly1305 + random nonce
5. **Transport**: All peer connections routed through Tor hidden services

## LAN Discovery

On the cyber range private network (192.168.56.0/24), chatd broadcasts UDP announcements with HMAC proof of the shared secret. Peers on the same LAN auto-discover each other.

## Building

```sh
cd chat
make          # builds chatd and chat
make install  # installs to /opt/lab/bin/
```

Requires: `libsodium`

## Wire Protocol

TLV (type-length-value) over TCP:

| Type | Name           | Payload |
|------|----------------|---------|
| 0x00 | AUTH_CHALLENGE | 32-byte random challenge |
| 0x01 | AUTH_RESPONSE  | 32-byte HMAC token |
| 0x02 | HELLO          | 32-byte X25519 public key |
| 0x03 | TEXT           | Encrypted message |
| 0x04 | PEER_LIST      | List of .onion addresses |
| 0x05 | ACK            | Empty |
