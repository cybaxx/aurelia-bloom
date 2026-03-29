/*
 * test_crypto.c — Unit tests for crypto.c
 *
 * Covers: crypto_setup, keypair generation/loading, shared secret,
 * auth token derivation, session key derivation, encrypt/decrypt,
 * HMAC, fingerprints, and edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

#include "crypto.h"
#include "protocol.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* Use a temporary HOME to avoid polluting real ~/.chatd */
static char test_home[256];

static void setup_test_home(void)
{
    snprintf(test_home, sizeof(test_home), "/tmp/chatd_test_%d", getpid());
    mkdir(test_home, 0700);
    setenv("HOME", test_home, 1);
}

static void cleanup_test_home(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_home);
    system(cmd);
}

/* ── Test: sodium init ───────────────────────────────────── */
static void test_crypto_setup(void)
{
    TEST(crypto_setup);
    int rc = crypto_setup();
    if (rc == 0) PASS(); else FAIL("crypto_setup returned non-zero");
}

/* ── Test: keypair generation ────────────────────────────── */
static void test_keypair_generate(void)
{
    TEST(keypair_generate_fresh);
    keypair_t kp;
    int rc = crypto_load_keypair(&kp);
    if (rc != 0) { FAIL("crypto_load_keypair failed"); return; }

    /* Keys should be non-zero */
    uint8_t zero[crypto_kx_PUBLICKEYBYTES] = {0};
    if (memcmp(kp.pk, zero, sizeof(zero)) == 0) { FAIL("pk is zero"); return; }
    if (memcmp(kp.sk, zero, sizeof(zero)) == 0) { FAIL("sk is zero"); return; }
    PASS();
}

/* ── Test: keypair persistence ───────────────────────────── */
static void test_keypair_persistence(void)
{
    TEST(keypair_persistence);
    keypair_t kp1, kp2;
    crypto_load_keypair(&kp1);
    crypto_load_keypair(&kp2);

    if (memcmp(kp1.pk, kp2.pk, sizeof(kp1.pk)) != 0) {
        FAIL("pk changed on reload"); return;
    }
    if (memcmp(kp1.sk, kp2.sk, sizeof(kp1.sk)) != 0) {
        FAIL("sk changed on reload"); return;
    }
    PASS();
}

/* ── Test: shared secret default ─────────────────────────── */
static void test_secret_default(void)
{
    TEST(secret_default);
    uint8_t hash[32];
    int rc = crypto_load_secret(NULL, hash, sizeof(hash));
    if (rc != 0) { FAIL("load_secret failed"); return; }

    uint8_t zero[32] = {0};
    if (memcmp(hash, zero, 32) == 0) { FAIL("hash is zero"); return; }
    PASS();
}

/* ── Test: shared secret override ────────────────────────── */
static void test_secret_override(void)
{
    TEST(secret_override);
    uint8_t hash1[32], hash2[32];
    crypto_load_secret("secret-one", hash1, sizeof(hash1));
    crypto_load_secret("secret-two", hash2, sizeof(hash2));

    if (memcmp(hash1, hash2, 32) == 0) {
        FAIL("different secrets produced same hash"); return;
    }
    PASS();
}

/* ── Test: shared secret determinism ─────────────────────── */
static void test_secret_determinism(void)
{
    TEST(secret_determinism);
    uint8_t hash1[32], hash2[32];
    crypto_load_secret("test-det", hash1, sizeof(hash1));
    crypto_load_secret("test-det", hash2, sizeof(hash2));

    if (memcmp(hash1, hash2, 32) != 0) {
        FAIL("same secret produced different hashes"); return;
    }
    PASS();
}

/* ── Test: auth token derivation ─────────────────────────── */
static void test_auth_token(void)
{
    TEST(auth_token_derivation);
    uint8_t secret[32];
    crypto_load_secret("test", secret, sizeof(secret));

    uint8_t challenge[AUTH_CHALLENGE_LEN];
    randombytes_buf(challenge, sizeof(challenge));

    uint8_t token1[AUTH_TOKEN_LEN], token2[AUTH_TOKEN_LEN];
    int rc1 = crypto_derive_auth_token(secret, 32, challenge, sizeof(challenge),
                                        token1, sizeof(token1));
    int rc2 = crypto_derive_auth_token(secret, 32, challenge, sizeof(challenge),
                                        token2, sizeof(token2));

    if (rc1 != 0 || rc2 != 0) { FAIL("derive failed"); return; }
    if (memcmp(token1, token2, AUTH_TOKEN_LEN) != 0) {
        FAIL("same input produced different tokens"); return;
    }
    PASS();
}

/* ── Test: auth token differs with different challenge ───── */
static void test_auth_token_different_challenge(void)
{
    TEST(auth_token_different_challenge);
    uint8_t secret[32];
    crypto_load_secret("test", secret, sizeof(secret));

    uint8_t ch1[AUTH_CHALLENGE_LEN], ch2[AUTH_CHALLENGE_LEN];
    randombytes_buf(ch1, sizeof(ch1));
    randombytes_buf(ch2, sizeof(ch2));

    uint8_t tok1[AUTH_TOKEN_LEN], tok2[AUTH_TOKEN_LEN];
    crypto_derive_auth_token(secret, 32, ch1, sizeof(ch1), tok1, sizeof(tok1));
    crypto_derive_auth_token(secret, 32, ch2, sizeof(ch2), tok2, sizeof(tok2));

    if (memcmp(tok1, tok2, AUTH_TOKEN_LEN) == 0) {
        FAIL("different challenges produced same token"); return;
    }
    PASS();
}

/* ── Test: session key derivation ────────────────────────── */
static void test_session_keys(void)
{
    TEST(session_key_derivation);
    keypair_t client, server;

    /* Generate two keypairs */
    crypto_kx_keypair(client.pk, client.sk);
    crypto_kx_keypair(server.pk, server.sk);

    session_keys_t ck, sk;
    int rc1 = crypto_derive_session(&client, server.pk, 1, &ck);
    int rc2 = crypto_derive_session(&server, client.pk, 0, &sk);

    if (rc1 != 0 || rc2 != 0) { FAIL("derive failed"); return; }

    /* Client's TX should equal server's RX and vice versa */
    if (memcmp(ck.tx, sk.rx, crypto_kx_SESSIONKEYBYTES) != 0) {
        FAIL("client tx != server rx"); return;
    }
    if (memcmp(ck.rx, sk.tx, crypto_kx_SESSIONKEYBYTES) != 0) {
        FAIL("client rx != server tx"); return;
    }
    PASS();
}

/* ── Test: encrypt then decrypt ──────────────────────────── */
static void test_encrypt_decrypt(void)
{
    TEST(encrypt_decrypt_roundtrip);
    keypair_t client, server;
    crypto_kx_keypair(client.pk, client.sk);
    crypto_kx_keypair(server.pk, server.sk);

    session_keys_t ck, sk;
    crypto_derive_session(&client, server.pk, 1, &ck);
    crypto_derive_session(&server, client.pk, 0, &sk);

    const char *msg = "Hello, encrypted world!";
    size_t mlen = strlen(msg);
    uint8_t ct[1024], pt[1024];

    int ctlen = crypto_encrypt(&ck, (uint8_t *)msg, mlen, ct, sizeof(ct));
    if (ctlen < 0) { FAIL("encrypt failed"); return; }

    int ptlen = crypto_decrypt(&sk, ct, ctlen, pt, sizeof(pt));
    if (ptlen < 0) { FAIL("decrypt failed"); return; }

    if ((size_t)ptlen != mlen || memcmp(pt, msg, mlen) != 0) {
        FAIL("decrypted text doesn't match"); return;
    }
    PASS();
}

/* ── Test: decrypt with wrong key fails ──────────────────── */
static void test_decrypt_wrong_key(void)
{
    TEST(decrypt_wrong_key_fails);
    keypair_t a, b, c;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);
    crypto_kx_keypair(c.pk, c.sk);

    session_keys_t ab, cb;
    crypto_derive_session(&a, b.pk, 1, &ab);
    crypto_derive_session(&c, b.pk, 1, &cb); /* wrong pair */

    const char *msg = "secret";
    uint8_t ct[1024], pt[1024];
    int ctlen = crypto_encrypt(&ab, (uint8_t *)msg, strlen(msg), ct, sizeof(ct));
    if (ctlen < 0) { FAIL("encrypt failed"); return; }

    int ptlen = crypto_decrypt(&cb, ct, ctlen, pt, sizeof(pt));
    if (ptlen >= 0) { FAIL("decrypt should have failed"); return; }
    PASS();
}

/* ── Test: encrypt output buffer too small ───────────────── */
static void test_encrypt_buffer_too_small(void)
{
    TEST(encrypt_buffer_too_small);
    keypair_t a, b;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);

    session_keys_t sk;
    crypto_derive_session(&a, b.pk, 1, &sk);

    const char *msg = "hello";
    uint8_t ct[2]; /* way too small */
    int ctlen = crypto_encrypt(&sk, (uint8_t *)msg, strlen(msg), ct, sizeof(ct));
    if (ctlen != -1) { FAIL("should return -1"); return; }
    PASS();
}

/* ── Test: decrypt with truncated input ──────────────────── */
static void test_decrypt_truncated(void)
{
    TEST(decrypt_truncated_input);
    keypair_t a, b;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);

    session_keys_t ck, sk;
    crypto_derive_session(&a, b.pk, 1, &ck);
    crypto_derive_session(&b, a.pk, 0, &sk);

    const char *msg = "test";
    uint8_t ct[1024], pt[1024];
    int ctlen = crypto_encrypt(&ck, (uint8_t *)msg, strlen(msg), ct, sizeof(ct));
    if (ctlen < 0) { FAIL("encrypt failed"); return; }

    /* Truncate ciphertext */
    int ptlen = crypto_decrypt(&sk, ct, 5, pt, sizeof(pt));
    if (ptlen >= 0) { FAIL("truncated decrypt should fail"); return; }
    PASS();
}

/* ── Test: encrypt empty message ─────────────────────────── */
static void test_encrypt_empty(void)
{
    TEST(encrypt_decrypt_empty);
    keypair_t a, b;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);

    session_keys_t ck, sk;
    crypto_derive_session(&a, b.pk, 1, &ck);
    crypto_derive_session(&b, a.pk, 0, &sk);

    uint8_t ct[1024], pt[1024];
    int ctlen = crypto_encrypt(&ck, (uint8_t *)"", 0, ct, sizeof(ct));
    if (ctlen < 0) { FAIL("encrypt empty failed"); return; }

    int ptlen = crypto_decrypt(&sk, ct, ctlen, pt, sizeof(pt));
    if (ptlen != 0) { FAIL("expected zero-length plaintext"); return; }
    PASS();
}

/* ── Test: encrypt large message ─────────────────────────── */
static void test_encrypt_large(void)
{
    TEST(encrypt_decrypt_large);
    keypair_t a, b;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);

    session_keys_t ck, sk;
    crypto_derive_session(&a, b.pk, 1, &ck);
    crypto_derive_session(&b, a.pk, 0, &sk);

    size_t mlen = 8192;
    uint8_t *msg = malloc(mlen);
    uint8_t *ct = malloc(mlen + 256);
    uint8_t *pt = malloc(mlen);
    randombytes_buf(msg, mlen);

    int ctlen = crypto_encrypt(&ck, msg, mlen, ct, mlen + 256);
    if (ctlen < 0) { FAIL("encrypt large failed"); goto end; }

    int ptlen = crypto_decrypt(&sk, ct, ctlen, pt, mlen);
    if (ptlen < 0 || (size_t)ptlen != mlen) { FAIL("decrypt large failed"); goto end; }
    if (memcmp(pt, msg, mlen) != 0) { FAIL("data mismatch"); goto end; }
    PASS();
end:
    free(msg); free(ct); free(pt);
}

/* ── Test: HMAC ──────────────────────────────────────────── */
static void test_hmac(void)
{
    TEST(hmac_compute_and_verify);
    uint8_t secret[32];
    crypto_load_secret("hmac-test", secret, sizeof(secret));

    const char *data = "test.onion";
    uint8_t mac[32];
    int rc = crypto_hmac(secret, 32, (uint8_t *)data, strlen(data), mac);
    if (rc != 0) { FAIL("hmac failed"); return; }

    rc = crypto_hmac_verify(secret, 32, (uint8_t *)data, strlen(data), mac);
    if (rc != 0) { FAIL("verify failed"); return; }
    PASS();
}

/* ── Test: HMAC wrong secret fails ───────────────────────── */
static void test_hmac_wrong_secret(void)
{
    TEST(hmac_wrong_secret_fails);
    uint8_t secret1[32], secret2[32];
    crypto_load_secret("s1", secret1, sizeof(secret1));
    crypto_load_secret("s2", secret2, sizeof(secret2));

    const char *data = "test.onion";
    uint8_t mac[32];
    crypto_hmac(secret1, 32, (uint8_t *)data, strlen(data), mac);

    int rc = crypto_hmac_verify(secret2, 32, (uint8_t *)data, strlen(data), mac);
    if (rc == 0) { FAIL("verify should fail with wrong secret"); return; }
    PASS();
}

/* ── Test: HMAC tampered data fails ──────────────────────── */
static void test_hmac_tampered(void)
{
    TEST(hmac_tampered_data_fails);
    uint8_t secret[32];
    crypto_load_secret("test", secret, sizeof(secret));

    const char *data = "good.onion";
    uint8_t mac[32];
    crypto_hmac(secret, 32, (uint8_t *)data, strlen(data), mac);

    const char *bad = "evil.onion";
    int rc = crypto_hmac_verify(secret, 32, (uint8_t *)bad, strlen(bad), mac);
    if (rc == 0) { FAIL("verify should fail with tampered data"); return; }
    PASS();
}

/* ── Test: fingerprint ───────────────────────────────────── */
static void test_fingerprint(void)
{
    TEST(fingerprint_format);
    uint8_t pk[32];
    memset(pk, 0, sizeof(pk));
    pk[0] = 0xab; pk[1] = 0xcd; pk[2] = 0xef; pk[3] = 0x01;
    pk[4] = 0x23; pk[5] = 0x45; pk[6] = 0x67; pk[7] = 0x89;

    char fp[17];
    crypto_fingerprint(pk, fp, sizeof(fp));

    if (strcmp(fp, "abcdef0123456789") != 0) {
        FAIL(fp); return;
    }
    PASS();
}

/* ── Test: fingerprint short buffer ──────────────────────── */
static void test_fingerprint_short_buffer(void)
{
    TEST(fingerprint_short_buffer);
    uint8_t pk[32] = {0xaa};
    char fp[4] = "xxx";
    crypto_fingerprint(pk, fp, sizeof(fp));
    if (fp[0] != '\0') { FAIL("should be empty for short buffer"); return; }
    PASS();
}

/* ── Test: multiple encrypt produce different ciphertexts ── */
static void test_encrypt_nonce_uniqueness(void)
{
    TEST(encrypt_nonce_uniqueness);
    keypair_t a, b;
    crypto_kx_keypair(a.pk, a.sk);
    crypto_kx_keypair(b.pk, b.sk);

    session_keys_t sk;
    crypto_derive_session(&a, b.pk, 1, &sk);

    const char *msg = "same";
    uint8_t ct1[1024], ct2[1024];
    int len1 = crypto_encrypt(&sk, (uint8_t *)msg, 4, ct1, sizeof(ct1));
    int len2 = crypto_encrypt(&sk, (uint8_t *)msg, 4, ct2, sizeof(ct2));

    if (len1 != len2) { FAIL("lengths differ"); return; }
    if (memcmp(ct1, ct2, len1) == 0) {
        FAIL("identical ciphertext (nonce reuse!)"); return;
    }
    PASS();
}

int main(void)
{
    setup_test_home();
    printf("=== crypto tests ===\n");

    test_crypto_setup();
    test_keypair_generate();
    test_keypair_persistence();
    test_secret_default();
    test_secret_override();
    test_secret_determinism();
    test_auth_token();
    test_auth_token_different_challenge();
    test_session_keys();
    test_encrypt_decrypt();
    test_decrypt_wrong_key();
    test_encrypt_buffer_too_small();
    test_decrypt_truncated();
    test_encrypt_empty();
    test_encrypt_large();
    test_hmac();
    test_hmac_wrong_secret();
    test_hmac_tampered();
    test_fingerprint();
    test_fingerprint_short_buffer();
    test_encrypt_nonce_uniqueness();

    cleanup_test_home();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
