/* khtpm_reparse_diff_test.c - standalone, headless unit test for
 * khtpm_reparse_diff.c. Zero X11, zero live window, zero
 * khtpm_core_render.c involvement - real Elem-identity assertions
 * only. See 08-roadmap/design-docs/CHTPM-INCREMENTAL-REPARSE-DESIGN.md
 * §0/rollout-step-2: this must be green BEFORE the diff engine is
 * wired into any real window.
 *
 * Build: cc -o khtpm_reparse_diff_test khtpm_reparse_diff_test.c
 * Run:   ./khtpm_reparse_diff_test  (exit 0 + "ALL PASS", or exit 1 +
 *        the first failing assertion printed)
 */
#include "khtpm_css_parser.h"
#include "khtpm_render_core.c"
#include "khtpm_reparse_diff.c"

#include <stdio.h>
#include <stdlib.h>

/* A real, bounded pool this test owns - stands in for
 * khtpm_core_render.c's own g_pool[]/free-list (§3 of the design doc),
 * exercised via the same KhDiffAllocator contract a real caller would
 * use. */
#define TEST_POOL_N 256
static Elem g_test_pool[TEST_POOL_N];
static int g_test_pool_used[TEST_POOL_N];

static Elem *test_alloc(void *ctx) {
    (void)ctx;
    for (int i = 0; i < TEST_POOL_N; i++) {
        if (!g_test_pool_used[i]) {
            g_test_pool_used[i] = 1;
            memset(&g_test_pool[i], 0, sizeof(Elem));
            return &g_test_pool[i];
        }
    }
    return NULL; /* exhausted - exercised by the allocator-exhaustion test */
}
static void test_free(void *ctx, Elem *e) {
    (void)ctx;
    int idx = (int)(e - g_test_pool);
    if (idx >= 0 && idx < TEST_POOL_N) g_test_pool_used[idx] = 0;
}
static void test_pool_reset(void) {
    memset(g_test_pool_used, 0, sizeof(g_test_pool_used));
    memset(g_test_pool, 0, sizeof(g_test_pool));
}

static int g_fail_n = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); g_fail_n++; } \
} while (0)

static Elem *mk(const char *tag, const char *id, const char *target_id, const char *label) {
    Elem *e = test_alloc(NULL);
    if (!e) { fprintf(stderr, "FATAL: test pool exhausted in mk() itself (a test-setup bug, not an engine bug) - fix the test's own pool budgeting\n"); exit(2); }
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    if (id) snprintf(e->id, sizeof(e->id), "%s", id);
    if (target_id) snprintf(e->target_id, sizeof(e->target_id), "%s", target_id);
    if (label) snprintf(e->label, sizeof(e->label), "%s", label);
    return e;
}
static void add_child(Elem *parent, Elem *child) {
    child->parent = parent;
    parent->children[parent->n_children++] = child;
}

/* TEST 1 - content-only change on a keyed element: the SAME Elem*
 * survives, its label updates, and its runtime state (input_buffer)
 * is preserved even though the template's own label= changed. This is
 * the core guarantee the whole design exists for. */
static void test_content_only_change_preserves_identity(void) {
    test_pool_reset();
    Elem *old_root = mk("window", NULL, NULL, NULL);
    Elem *old_cli = mk("cli_io", "address", "address", "http://old-loaded-url.example");
    snprintf(old_cli->input_buffer, sizeof(old_cli->input_buffer), "http://user-is-typing-this");
    old_cli->cursor = 27;
    add_child(old_root, old_cli);

    Elem *new_root = mk("window", NULL, NULL, NULL);
    Elem *new_cli = mk("cli_io", "address", "address", "http://DIFFERENT-loaded-url.example");
    add_child(new_root, new_cli);

    KhDiffAllocator alloc = { test_alloc, test_free, NULL };
    Elem *removed[TEST_POOL_N]; KhDiffRemovedList rl = { removed, TEST_POOL_N, 0 };
    int ok = kh_reparse_diff_patch(old_root, new_root, &alloc, &rl);

    CHECK(ok, "diff should succeed");
    CHECK(old_root->n_children == 1, "root should still have 1 child");
    CHECK(old_root->children[0] == old_cli, "the SAME Elem* must survive a content-only change - this is the whole point");
    CHECK(strcmp(old_cli->label, "http://DIFFERENT-loaded-url.example") == 0, "template field (label) must update from the new tree");
    CHECK(strcmp(old_cli->input_buffer, "http://user-is-typing-this") == 0, "runtime state (input_buffer) must survive - NEVER overwritten from the template");
    CHECK(old_cli->cursor == 27, "runtime state (cursor) must survive");
    CHECK(rl.n == 0, "nothing should have been reported removed");
}

/* TEST 2 - a genuinely removed element gets reported to the caller
 * (so khtpm_core_render.c can check it against g_default_input_elem
 * BEFORE it's actually freed) and its pool slot is released. */
static void test_removed_element_reported_and_freed(void) {
    test_pool_reset();
    Elem *old_root = mk("window", NULL, NULL, NULL);
    Elem *old_a = mk("item", "a", NULL, "A");
    Elem *old_b = mk("item", "b", NULL, "B");
    add_child(old_root, old_a);
    add_child(old_root, old_b);

    Elem *new_root = mk("window", NULL, NULL, NULL);
    Elem *new_a = mk("item", "a", NULL, "A updated");
    add_child(new_root, new_a); /* "b" is gone in the new tree */

    KhDiffAllocator alloc = { test_alloc, test_free, NULL };
    Elem *removed[TEST_POOL_N]; KhDiffRemovedList rl = { removed, TEST_POOL_N, 0 };
    int ok = kh_reparse_diff_patch(old_root, new_root, &alloc, &rl);

    CHECK(ok, "diff should succeed");
    CHECK(old_root->n_children == 1, "root should now have 1 child");
    CHECK(old_root->children[0] == old_a, "surviving element keeps its identity");
    CHECK(rl.n == 1, "exactly one element should be reported removed");
    CHECK(rl.n == 1 && rl.out[0] == old_b, "the removed element reported must be the real old pointer, so the caller can check it against tracked pointers");
    CHECK(g_test_pool_used[(int)(old_b - g_test_pool)] == 0, "the removed element's pool slot must be freed");
}

/* TEST 3 - a genuinely new keyed element gets a fresh Elem, not
 * confused with any existing one. */
static void test_added_element_gets_fresh_elem(void) {
    test_pool_reset();
    Elem *old_root = mk("window", NULL, NULL, NULL);
    Elem *old_a = mk("item", "a", NULL, "A");
    add_child(old_root, old_a);

    Elem *new_root = mk("window", NULL, NULL, NULL);
    Elem *new_a = mk("item", "a", NULL, "A");
    Elem *new_b = mk("item", "b", NULL, "B is new");
    add_child(new_root, new_a);
    add_child(new_root, new_b);

    KhDiffAllocator alloc = { test_alloc, test_free, NULL };
    Elem *removed[TEST_POOL_N]; KhDiffRemovedList rl = { removed, TEST_POOL_N, 0 };
    int ok = kh_reparse_diff_patch(old_root, new_root, &alloc, &rl);

    CHECK(ok, "diff should succeed");
    CHECK(old_root->n_children == 2, "root should now have 2 children");
    CHECK(old_root->children[0] == old_a, "the surviving element keeps its identity and position");
    CHECK(old_root->children[1] != new_b, "the added element must be a FRESH Elem*, never the candidate-tree pointer itself (that pool is the caller's to discard)");
    CHECK(old_root->children[1] && strcmp(old_root->children[1]->label, "B is new") == 0, "the fresh element must carry the new content");
    CHECK(rl.n == 0, "nothing removed in this scenario");
}

/* TEST 4 - reorder by key: two keyed elements swap position in the
 * markup: identity follows the KEY, not the array index. */
static void test_reorder_follows_key_not_position(void) {
    test_pool_reset();
    Elem *old_root = mk("window", NULL, NULL, NULL);
    Elem *old_a = mk("item", "a", NULL, "A");
    Elem *old_b = mk("item", "b", NULL, "B");
    add_child(old_root, old_a);
    add_child(old_root, old_b);

    Elem *new_root = mk("window", NULL, NULL, NULL);
    Elem *new_b = mk("item", "b", NULL, "B");
    Elem *new_a = mk("item", "a", NULL, "A");
    add_child(new_root, new_b); /* b first this time */
    add_child(new_root, new_a);

    KhDiffAllocator alloc = { test_alloc, test_free, NULL };
    Elem *removed[TEST_POOL_N]; KhDiffRemovedList rl = { removed, TEST_POOL_N, 0 };
    int ok = kh_reparse_diff_patch(old_root, new_root, &alloc, &rl);

    CHECK(ok, "diff should succeed");
    CHECK(old_root->n_children == 2, "still 2 children");
    CHECK(old_root->children[0] == old_b, "position 0 must now be the OLD 'b' Elem* (identity follows key, reordered)");
    CHECK(old_root->children[1] == old_a, "position 1 must now be the OLD 'a' Elem* (identity follows key, reordered)");
    CHECK(rl.n == 0, "a pure reorder removes nothing");
}

/* TEST 5 - allocator exhaustion is a clean, reported failure, not a
 * crash or silent partial state left for the caller to trust. */
static void test_allocator_exhaustion_fails_cleanly(void) {
    test_pool_reset();
    Elem *old_root = mk("window", NULL, NULL, NULL);
    Elem *new_root = mk("window", NULL, NULL, NULL);
    /* Fill the pool down to exactly one slot left (old_root + new_root
     * already took 2; reserve 1 more for new_child's own mk() call
     * below), THEN ask the diff to add one more child than the
     * allocator can provide - the diff's OWN internal allocation for
     * that child is what must fail, not this test's own setup. */
    for (int i = 0; i < TEST_POOL_N - 3; i++) test_alloc(NULL);
    Elem *new_child = mk("item", "will-not-fit", NULL, "x");
    add_child(new_root, new_child);

    KhDiffAllocator alloc = { test_alloc, test_free, NULL };
    Elem *removed[TEST_POOL_N]; KhDiffRemovedList rl = { removed, TEST_POOL_N, 0 };
    int ok = kh_reparse_diff_patch(old_root, new_root, &alloc, &rl);
    CHECK(!ok, "diff must report failure (0) when the allocator is exhausted, per its own documented contract - never a silent success");
}

int main(void) {
    test_content_only_change_preserves_identity();
    test_removed_element_reported_and_freed();
    test_added_element_gets_fresh_elem();
    test_reorder_follows_key_not_position();
    test_allocator_exhaustion_fails_cleanly();

    if (g_fail_n) {
        fprintf(stderr, "%d CHECK(S) FAILED\n", g_fail_n);
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
