/*
 * test_peer.c — Unit tests for peer.c
 *
 * Covers: peer_list_init, add, remove, find_by_fd, find_by_onion,
 * count_established, peer_list_format, edge cases (full list, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "peer.h"
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

/* Create a fake fd (pipe) so peer_remove can close it safely */
static int make_fake_fd(void)
{
    int fds[2];
    if (pipe(fds) < 0) return -1;
    close(fds[1]); /* close write end */
    return fds[0]; /* return read end */
}

/* ── Test: init ──────────────────────────────────────────── */
static void test_init(void)
{
    TEST(peer_list_init);
    peer_list_t pl;
    peer_list_init(&pl);
    if (pl.count != 0) { FAIL("count not 0"); return; }
    for (int i = 0; i < MAX_PEERS; i++) {
        if (pl.peers[i].fd != -1) { FAIL("fd not -1"); return; }
    }
    PASS();
}

/* ── Test: add ───────────────────────────────────────────── */
static void test_add(void)
{
    TEST(peer_add);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd = make_fake_fd();
    peer_t *p = peer_add(&pl, fd, "test.onion", 1);
    if (!p) { FAIL("add returned NULL"); close(fd); return; }
    if (p->fd != fd) { FAIL("wrong fd"); return; }
    if (strcmp(p->onion, "test.onion") != 0) { FAIL("wrong onion"); return; }
    if (p->is_client != 1) { FAIL("wrong is_client"); return; }
    if (p->state != PEER_CONNECTING) { FAIL("wrong state"); return; }
    if (pl.count != 1) { FAIL("wrong count"); return; }

    peer_remove(&pl, fd);
    PASS();
}

/* ── Test: add with NULL onion ───────────────────────────── */
static void test_add_null_onion(void)
{
    TEST(peer_add_null_onion);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd = make_fake_fd();
    peer_t *p = peer_add(&pl, fd, NULL, 0);
    if (!p) { FAIL("add returned NULL"); close(fd); return; }
    if (p->onion[0] != '\0') { FAIL("onion not empty"); return; }

    peer_remove(&pl, fd);
    PASS();
}

/* ── Test: remove ────────────────────────────────────────── */
static void test_remove(void)
{
    TEST(peer_remove);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd = make_fake_fd();
    peer_add(&pl, fd, "rm.onion", 0);
    peer_remove(&pl, fd);

    if (pl.count != 0) { FAIL("count not 0"); return; }
    PASS();
}

/* ── Test: remove non-existent fd ────────────────────────── */
static void test_remove_nonexistent(void)
{
    TEST(peer_remove_nonexistent);
    peer_list_t pl;
    peer_list_init(&pl);

    /* Should not crash */
    peer_remove(&pl, 999);
    if (pl.count != 0) { FAIL("count changed"); return; }
    PASS();
}

/* ── Test: find by fd ────────────────────────────────────── */
static void test_find_by_fd(void)
{
    TEST(peer_find_by_fd);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd1 = make_fake_fd();
    int fd2 = make_fake_fd();
    peer_add(&pl, fd1, "a.onion", 0);
    peer_add(&pl, fd2, "b.onion", 1);

    peer_t *p = peer_find_by_fd(&pl, fd2);
    if (!p || p->fd != fd2) { FAIL("wrong peer"); goto done; }
    if (strcmp(p->onion, "b.onion") != 0) { FAIL("wrong onion"); goto done; }

    p = peer_find_by_fd(&pl, 999);
    if (p != NULL) { FAIL("should be NULL for unknown fd"); goto done; }
    PASS();

done:
    peer_remove(&pl, fd1);
    peer_remove(&pl, fd2);
}

/* ── Test: find by onion ─────────────────────────────────── */
static void test_find_by_onion(void)
{
    TEST(peer_find_by_onion);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd = make_fake_fd();
    peer_add(&pl, fd, "abc123.onion", 1);

    peer_t *p = peer_find_by_onion(&pl, "abc123.onion");
    if (!p || p->fd != fd) { FAIL("not found"); peer_remove(&pl, fd); return; }

    p = peer_find_by_onion(&pl, "unknown.onion");
    if (p) { FAIL("should be NULL"); peer_remove(&pl, fd); return; }

    peer_remove(&pl, fd);
    PASS();
}

/* ── Test: count established ─────────────────────────────── */
static void test_count_established(void)
{
    TEST(peer_count_established);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd1 = make_fake_fd();
    int fd2 = make_fake_fd();
    int fd3 = make_fake_fd();
    peer_t *p1 = peer_add(&pl, fd1, "a.onion", 0);
    peer_t *p2 = peer_add(&pl, fd2, "b.onion", 0);
    peer_add(&pl, fd3, "c.onion", 0);

    p1->state = PEER_ESTABLISHED;
    p2->state = PEER_ESTABLISHED;
    /* p3 stays CONNECTING */

    int n = peer_count_established(&pl);
    if (n != 2) { FAIL("expected 2"); goto done; }
    PASS();

done:
    peer_remove(&pl, fd1);
    peer_remove(&pl, fd2);
    peer_remove(&pl, fd3);
}

/* ── Test: full list ─────────────────────────────────────── */
static void test_full_list(void)
{
    TEST(peer_add_full_list);
    peer_list_t pl;
    peer_list_init(&pl);

    int fds[MAX_PEERS];
    for (int i = 0; i < MAX_PEERS; i++) {
        fds[i] = make_fake_fd();
        peer_t *p = peer_add(&pl, fds[i], "x.onion", 0);
        if (!p) { FAIL("add failed before full"); goto done; }
    }

    /* One more should fail */
    int extra = make_fake_fd();
    peer_t *p = peer_add(&pl, extra, "extra.onion", 0);
    if (p != NULL) { FAIL("should be NULL when full"); close(extra); goto done; }
    close(extra);
    PASS();

done:
    for (int i = 0; i < MAX_PEERS; i++)
        peer_remove(&pl, fds[i]);
}

/* ── Test: remove then re-add ────────────────────────────── */
static void test_remove_readd(void)
{
    TEST(peer_remove_then_readd);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd1 = make_fake_fd();
    peer_add(&pl, fd1, "a.onion", 0);
    peer_remove(&pl, fd1);

    int fd2 = make_fake_fd();
    peer_t *p = peer_add(&pl, fd2, "b.onion", 1);
    if (!p) { FAIL("re-add failed"); close(fd2); return; }
    if (pl.count != 1) { FAIL("wrong count"); peer_remove(&pl, fd2); return; }

    peer_remove(&pl, fd2);
    PASS();
}

/* ── Test: format empty list ─────────────────────────────── */
static void test_format_empty(void)
{
    TEST(peer_list_format_empty);
    peer_list_t pl;
    peer_list_init(&pl);

    char *s = peer_list_format(&pl);
    if (!s) { FAIL("returned NULL"); return; }
    if (s[0] != '\0') { FAIL("should be empty string"); free(s); return; }
    free(s);
    PASS();
}

/* ── Test: format with peers ─────────────────────────────── */
static void test_format_with_peers(void)
{
    TEST(peer_list_format_with_peers);
    peer_list_t pl;
    peer_list_init(&pl);

    int fd = make_fake_fd();
    peer_t *p = peer_add(&pl, fd, "test123.onion", 0);
    p->state = PEER_ESTABLISHED;

    char *s = peer_list_format(&pl);
    if (!s) { FAIL("returned NULL"); peer_remove(&pl, fd); return; }
    if (strstr(s, "test123.onion") == NULL) { FAIL("missing onion"); }
    else if (strstr(s, "[ok]") == NULL) { FAIL("missing status"); }
    else { PASS(); }
    free(s);
    peer_remove(&pl, fd);
}

/* ── Test: multiple adds and removes ─────────────────────── */
static void test_churn(void)
{
    TEST(peer_add_remove_churn);
    peer_list_t pl;
    peer_list_init(&pl);

    for (int round = 0; round < 5; round++) {
        int fds[4];
        for (int i = 0; i < 4; i++) {
            fds[i] = make_fake_fd();
            peer_add(&pl, fds[i], "churn.onion", 0);
        }
        for (int i = 0; i < 4; i++)
            peer_remove(&pl, fds[i]);
    }

    if (pl.count != 0) { FAIL("count not 0 after churn"); return; }
    PASS();
}

int main(void)
{
    /* Need sodium for fingerprint in peer_list_format */
    sodium_init();

    printf("=== peer tests ===\n");

    test_init();
    test_add();
    test_add_null_onion();
    test_remove();
    test_remove_nonexistent();
    test_find_by_fd();
    test_find_by_onion();
    test_count_established();
    test_full_list();
    test_remove_readd();
    test_format_empty();
    test_format_with_peers();
    test_churn();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
