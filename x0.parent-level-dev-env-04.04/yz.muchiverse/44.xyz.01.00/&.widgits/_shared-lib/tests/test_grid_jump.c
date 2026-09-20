/* Standalone test for khtpm_grid_jump.c (pure C, no X11/khtpm deps).
 * Run: gcc -std=c11 -Wall -I.. -o /tmp/test_grid_jump test_grid_jump.c && /tmp/test_grid_jump */
#include <stdio.h>
#include "khtpm_grid_jump.c"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int jump(const char *ref, int rows, int cols, int *r, int *c) { return gj_parse(ref, rows, cols, r, c); }

int main(void) {
    int r = -1, c = -1;
    char l[8];

    /* letters <-> index, bijective base-26 */
    gj_col_to_letters(0, l, sizeof l);   CHECK(!strcmp(l, "A"));
    gj_col_to_letters(25, l, sizeof l);  CHECK(!strcmp(l, "Z"));
    gj_col_to_letters(26, l, sizeof l);  CHECK(!strcmp(l, "AA"));
    gj_col_to_letters(701, l, sizeof l); CHECK(!strcmp(l, "ZZ"));
    gj_col_to_letters(702, l, sizeof l); CHECK(!strcmp(l, "AAA"));
    CHECK(gj_letters_to_col("A", 1) == 0);
    CHECK(gj_letters_to_col("aa", 2) == 26);
    CHECK(gj_letters_to_col("ZZ", 2) == 701);
    CHECK(gj_letters_to_col("A1", 2) == -1);
    CHECK(gj_letters_to_col("", 0) == -1);
    for (int i = 0; i < 2000; i++) { gj_col_to_letters(i, l, sizeof l); CHECK(gj_letters_to_col(l, (int)strlen(l)) == i); }

    /* either order, 1-based rows */
    CHECK(jump("a11", 0, 0, &r, &c) && r == 10 && c == 0);
    CHECK(jump("11a", 0, 0, &r, &c) && r == 10 && c == 0);
    CHECK(jump("AA5", 0, 0, &r, &c) && r == 4 && c == 26);
    CHECK(jump("5aa", 0, 0, &r, &c) && r == 4 && c == 26);
    CHECK(jump("c7", 0, 0, &r, &c) && r == 6 && c == 2);
    CHECK(jump("7c", 0, 0, &r, &c) && r == 6 && c == 2);

    /* invalid / out of range */
    CHECK(!jump("", 0, 0, &r, &c));
    CHECK(!jump("a", 0, 0, &r, &c));
    CHECK(!jump("11", 0, 0, &r, &c));
    CHECK(!jump("a0", 0, 0, &r, &c));              /* row 0 is not a 1-based row */
    CHECK(!jump("z9", 26, 5, &r, &c));             /* col Z beyond 5 cols */
    CHECK(!jump("a30", 26, 5, &r, &c));            /* row 30 beyond 26 rows */
    CHECK(jump("e26", 26, 5, &r, &c) && r == 25 && c == 4); /* last cell ok */

    /* buffer append: alnum only, capped, NUL kept */
    char b[GJ_BUF_CAP] = "";
    CHECK(gj_buf_append(b, sizeof b, 'a') && !strcmp(b, "a"));
    CHECK(!gj_buf_append(b, sizeof b, ' '));
    CHECK(!gj_buf_append(b, sizeof b, '-'));
    for (int i = 0; i < 40; i++) gj_buf_append(b, sizeof b, '1');
    CHECK(strlen(b) == GJ_BUF_CAP - 1);

    /* step function: a11 + Enter jumps, Enter again enters the cell */
    GjState st; memset(&st, 0, sizeof st);
    CHECK(gj_step(&st, GJ_KEY_CHAR, 'a') == GJ_BUFFER);
    CHECK(gj_step(&st, GJ_KEY_CHAR, '1') == GJ_BUFFER);
    CHECK(gj_step(&st, GJ_KEY_CHAR, '1') == GJ_BUFFER);
    CHECK(!strcmp(st.jump, "a11"));
    CHECK(gj_step(&st, GJ_KEY_BACKSPACE, 0) == GJ_BUFFER && !strcmp(st.jump, "a1"));
    CHECK(gj_step(&st, GJ_KEY_CHAR, '1') == GJ_BUFFER);
    CHECK(gj_step(&st, GJ_KEY_ENTER, 0) == GJ_JUMPED && st.row == 10 && st.col == 0 && st.jump[0] == 0);
    CHECK(gj_step(&st, GJ_KEY_ENTER, 0) == GJ_ENTER_CELL);   /* empty buffer */
    CHECK(gj_step(&st, GJ_KEY_ESC, 0) == GJ_DISARM);

    /* unparseable buffer: cleared, cursor untouched, no jump */
    memset(&st, 0, sizeof st); st.row = 3; st.col = 2;
    gj_step(&st, GJ_KEY_CHAR, 'z'); gj_step(&st, GJ_KEY_CHAR, 'z');
    CHECK(gj_step(&st, GJ_KEY_ENTER, 0) == GJ_NONE && st.row == 3 && st.col == 2 && st.jump[0] == 0);

    /* arrows: clamp at 0, unbounded vs bounded */
    memset(&st, 0, sizeof st);
    CHECK(gj_step(&st, GJ_KEY_UP, 0) == GJ_NONE && st.row == 0);
    CHECK(gj_step(&st, GJ_KEY_LEFT, 0) == GJ_NONE && st.col == 0);
    CHECK(gj_step(&st, GJ_KEY_DOWN, 0) == GJ_MOVED && st.row == 1);   /* unbounded */
    CHECK(gj_step(&st, GJ_KEY_RIGHT, 0) == GJ_MOVED && st.col == 1);
    st.rows = 2; st.cols = 2;
    CHECK(gj_step(&st, GJ_KEY_DOWN, 0) == GJ_NONE && st.row == 1);    /* bounded */
    CHECK(gj_step(&st, GJ_KEY_RIGHT, 0) == GJ_NONE && st.col == 1);
    st.row = 9; st.col = 9; gj_clamp(&st);
    CHECK(st.row == 1 && st.col == 1);

    /* bounded jump rejects out-of-range, keeps cursor */
    memset(&st, 0, sizeof st); st.rows = 5; st.cols = 3;
    gj_step(&st, GJ_KEY_CHAR, 'z'); gj_step(&st, GJ_KEY_CHAR, '9');
    CHECK(gj_step(&st, GJ_KEY_ENTER, 0) == GJ_NONE && st.row == 0 && st.col == 0);

    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("khtpm_grid_jump: all tests passed\n");
    return 0;
}
