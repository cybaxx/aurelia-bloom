#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

static char keydir[256];

static int ensure_keydir(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(keydir, sizeof(keydir), "%s/%s", home, CRYPTO_KEY_DIR);
    if (mkdir(keydir, 0700) < 0 && errno != EEXIST)
        return -1;
    return 0;
}

int crypto_setup(void)
{
    if (sodium_init() < 0) {
        fprintf(stderr, "crypto: sodium_init failed\n");
        return -1;
    }
    return ensure_keydir();
}

static int write_file(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    chmod(path, 0600);
    return (w == len) ? 0 : -1;
}

static int read_file(const char *path, uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t r = fread(data, 1, len, f);
    fclose(f);
    return (r == len) ? 0 : -1;
}

int crypto_load_keypair(keypair_t *kp)
{
    char pk_path[512], sk_path[512];
    snprintf(pk_path, sizeof(pk_path), "%s/%s", keydir, CRYPTO_PK_FILE);
    snprintf(sk_path, sizeof(sk_path), "%s/%s", keydir, CRYPTO_SK_FILE);

    if (read_file(pk_path, kp->pk, sizeof(kp->pk)) == 0 &&
        read_file(sk_path, kp->sk, sizeof(kp->sk)) == 0) {
        return 0;
    }

    /* Generate new keypair */
    if (crypto_kx_keypair(kp->pk, kp->sk) != 0)
        return -1;

    if (write_file(pk_path, kp->pk, sizeof(kp->pk)) < 0 ||
        write_file(sk_path, kp->sk, sizeof(kp->sk)) < 0)
        return -1;

    fprintf(stderr, "crypto: generated new keypair\n");
    return 0;
}

int crypto_load_secret(const char *override, uint8_t *secret_hash,
                       size_t hash_len)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", keydir, CRYPTO_SECRET_FILE);

    const char *secret = override ? override : DEFAULT_SHARED_SECRET;

    /* If override provided or no file exists, derive and store */
    if (override || access(path, F_OK) != 0) {
        /* Hash the secret with a fixed salt for deterministic storage */
        uint8_t salt[crypto_pwhash_SALTBYTES];
        memset(salt, 0, sizeof(salt));
        memcpy(salt, "abloom-secret", 13);

        if (crypto_pwhash(secret_hash, hash_len,
                          secret, strlen(secret),
                          salt, AUTH_OPSLIMIT, AUTH_MEMLIMIT,
                          crypto_pwhash_ALG_ARGON2ID13) != 0) {
            fprintf(stderr, "crypto: pwhash failed\n");
            return -1;
        }
        write_file(path, secret_hash, hash_len);
        return 0;
    }

    return read_file(path, secret_hash, hash_len);
}

int crypto_derive_auth_token(const uint8_t *secret_hash, size_t hash_len,
                              const uint8_t *salt, size_t salt_len,
                              uint8_t *token, size_t token_len)
{
    (void)hash_len;
    (void)token_len;
    /* Use HMAC: token = HMAC-SHA256(secret_hash, salt) */
    return crypto_auth_hmacsha256(token, salt, salt_len, secret_hash);
}

int crypto_derive_session(const keypair_t *kp, const uint8_t *peer_pk,
                           int is_client, session_keys_t *sk)
{
    if (is_client) {
        return crypto_kx_client_session_keys(sk->rx, sk->tx,
                                              kp->pk, kp->sk, peer_pk);
    } else {
        return crypto_kx_server_session_keys(sk->rx, sk->tx,
                                              kp->pk, kp->sk, peer_pk);
    }
}

int crypto_encrypt(const session_keys_t *sk, const uint8_t *in, size_t inlen,
                    uint8_t *out, size_t outmax)
{
    size_t nonce_len = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    size_t overhead  = crypto_aead_xchacha20poly1305_ietf_ABYTES;
    size_t needed    = nonce_len + inlen + overhead;

    if (outmax < needed) return -1;

    /* Prepend random nonce */
    randombytes_buf(out, nonce_len);

    unsigned long long clen;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            out + nonce_len, &clen,
            in, inlen,
            NULL, 0,   /* no additional data */
            NULL,       /* nsec unused */
            out,        /* nonce */
            sk->tx) != 0)
        return -1;

    return (int)(nonce_len + clen);
}

int crypto_decrypt(const session_keys_t *sk, const uint8_t *in, size_t inlen,
                    uint8_t *out, size_t outmax)
{
    (void)outmax;
    size_t nonce_len = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    size_t overhead  = crypto_aead_xchacha20poly1305_ietf_ABYTES;

    if (inlen < nonce_len + overhead) return -1;

    unsigned long long mlen;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            out, &mlen,
            NULL,           /* nsec unused */
            in + nonce_len, inlen - nonce_len,
            NULL, 0,        /* no additional data */
            in,             /* nonce is first bytes */
            sk->rx) != 0)
        return -1;

    return (int)mlen;
}

int crypto_hmac(const uint8_t *secret_hash, size_t hash_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t *mac)
{
    (void)hash_len;
    return crypto_auth_hmacsha256(mac, data, data_len, secret_hash);
}

int crypto_hmac_verify(const uint8_t *secret_hash, size_t hash_len,
                        const uint8_t *data, size_t data_len,
                        const uint8_t *mac)
{
    (void)hash_len;
    return crypto_auth_hmacsha256_verify(mac, data, data_len, secret_hash);
}

void crypto_fingerprint(const uint8_t *pk, char *out, size_t outlen)
{
    if (outlen < 17) { out[0] = '\0'; return; }
    for (int i = 0; i < 8; i++)
        snprintf(out + i * 2, outlen - i * 2, "%02x", pk[i]);
}
