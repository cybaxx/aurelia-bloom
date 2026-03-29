#ifndef CRYPTO_H
#define CRYPTO_H

#include <sodium.h>
#include <stddef.h>
#include <stdint.h>

#define CRYPTO_KEY_DIR     ".chatd"
#define CRYPTO_PK_FILE     "public.key"
#define CRYPTO_SK_FILE     "secret.key"
#define CRYPTO_SECRET_FILE "shared.secret"

/* Default shared secret for the cyber range */
#define DEFAULT_SHARED_SECRET "aurelia-bloom"

/* Argon2id parameters for auth token derivation */
#define AUTH_OPSLIMIT  crypto_pwhash_OPSLIMIT_INTERACTIVE
#define AUTH_MEMLIMIT  crypto_pwhash_MEMLIMIT_INTERACTIVE

typedef struct {
    uint8_t pk[crypto_kx_PUBLICKEYBYTES];
    uint8_t sk[crypto_kx_SECRETKEYBYTES];
} keypair_t;

typedef struct {
    uint8_t rx[crypto_kx_SESSIONKEYBYTES]; /* receive key */
    uint8_t tx[crypto_kx_SESSIONKEYBYTES]; /* transmit key */
} session_keys_t;

/* Initialize libsodium. Returns 0 on success. */
int  crypto_setup(void);

/* Load or generate keypair in ~/.chatd/ */
int  crypto_load_keypair(keypair_t *kp);

/* Load or set shared secret in ~/.chatd/ */
int  crypto_load_secret(const char *override, uint8_t *secret_hash,
                        size_t hash_len);

/* Derive auth token from shared secret (Argon2id).
 * salt must be AUTH_CHALLENGE_LEN bytes. */
int  crypto_derive_auth_token(const uint8_t *secret_hash, size_t hash_len,
                              const uint8_t *salt, size_t salt_len,
                              uint8_t *token, size_t token_len);

/* Derive session keys. is_client: 1 if we initiated the connection. */
int  crypto_derive_session(const keypair_t *kp, const uint8_t *peer_pk,
                           int is_client, session_keys_t *sk);

/* Encrypt a message. out must have room for inlen + crypto_aead overhead.
 * Returns ciphertext length or -1. */
int  crypto_encrypt(const session_keys_t *sk, const uint8_t *in, size_t inlen,
                    uint8_t *out, size_t outmax);

/* Decrypt a message. out must have room for inlen bytes.
 * Returns plaintext length or -1. */
int  crypto_decrypt(const session_keys_t *sk, const uint8_t *in, size_t inlen,
                    uint8_t *out, size_t outmax);

/* Compute HMAC for LAN discovery. */
int  crypto_hmac(const uint8_t *secret_hash, size_t hash_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t *mac);

/* Verify HMAC. Returns 0 on match. */
int  crypto_hmac_verify(const uint8_t *secret_hash, size_t hash_len,
                        const uint8_t *data, size_t data_len,
                        const uint8_t *mac);

/* Hex-encode a public key fingerprint (first 8 bytes). */
void crypto_fingerprint(const uint8_t *pk, char *out, size_t outlen);

#endif /* CRYPTO_H */
