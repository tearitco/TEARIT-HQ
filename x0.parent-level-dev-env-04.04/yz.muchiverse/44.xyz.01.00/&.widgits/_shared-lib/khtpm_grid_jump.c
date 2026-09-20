/* khtpm_grid_jump.c - the PURE part of `<grid>` cell-jump navigation.
 *
 * Text-included canonical helper (house convention, like khtpm_ui_common.c):
 * no .so, no per-app copy. Plain C + <string.h>/<ctype.h>/<stdlib.h> only -
 * no Elem, no X11, no khtpm globals - so khtpm_core_render.c's `<grid>` key
 * handler AND standalone X overlays (tp_arm_placer_rmmv.c's labelled wire
 * grid) can drive the SAME behaviour: arrows move a cell cursor, letters and
 * digits build a jump ref in either order ("a11" or "11a"), Enter resolves
 * it, an Enter with nothing pending means "enter/place this cell", Esc
 * disarms. Behaviour spec: GRID-ELEMENT-DESIGN.md (three states).
 *
 * All functions are static + unused-tolerant so any includer can pull the
 * whole file in. Column letters are bijective base-26 (A=0..Z=25, AA=26);
 * rows are 1-based in refs and 0-based in GjState. rows/cols == 0 means
 * "unbounded" (the csv-hq grid: its manager is the real bounds authority).
 */
#ifndef KHTPM_GRID_JUMP_C
#define KHTPM_GRID_JUMP_C

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define GJ_UNUSED __attribute__((unused))
#else
#define GJ_UNUSED
#endif

#define GJ_BUF_CAP 16 /* jump buffer bytes incl. NUL - a ref like "AA1234" fits */

typedef struct {
    char jump[GJ_BUF_CAP]; /* pending jump ref, [A-Za-z0-9] only */
    int row, col;          /* cursor, 0-based */
    int rows, cols;        /* grid bounds; 0 = unbounded */
} GjState;

typedef enum {
    GJ_KEY_CHAR = 0, /* a typed character in `ch` */
    GJ_KEY_UP, GJ_KEY_DOWN, GJ_KEY_LEFT, GJ_KEY_RIGHT,
    GJ_KEY_ENTER, GJ_KEY_ESC, GJ_KEY_BACKSPACE
} GjKey;

typedef enum {
    GJ_NONE = 0,     /* nothing changed (ignored key, clamped move, bad ref) */
    GJ_MOVED,        /* the cursor moved by one cell */
    GJ_BUFFER,       /* the jump buffer changed (char added / backspace) */
    GJ_JUMPED,       /* Enter resolved a valid ref; cursor moved, buffer cleared */
    GJ_ENTER_CELL,   /* Enter with an empty buffer: enter/place the cursor cell */
    GJ_DISARM        /* Esc: leave the grid */
} GjAction;

/* 0 -> "A", 25 -> "Z", 26 -> "AA", 701 -> "ZZ", 702 -> "AAA". */
GJ_UNUSED static void gj_col_to_letters(int col, char *out, size_t outsz) {
    char tmp[8]; int n = 0;
    long v = (long)col + 1;
    if (outsz == 0) return;
    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('A' + (v - 1) % 26);
        v = (v - 1) / 26;
    }
    size_t i = 0;
    for (; (int)i < n && i < outsz - 1; i++) out[i] = tmp[n - 1 - i];
    out[i] = '\0';
}

/* "A" -> 0, "aa" -> 26; -1 on a non-letter, empty input or runaway length. */
GJ_UNUSED static int gj_letters_to_col(const char *s, int len) {
    long col = 0;
    if (len <= 0) return -1;
    for (int i = 0; i < len; i++) {
        char c = (char)toupper((unsigned char)s[i]);
        if (c < 'A' || c > 'Z') return -1;
        col = col * 26 + (c - 'A' + 1);
        if (col > 1000000) return -1;
    }
    return (int)(col - 1);
}

/* Parse a jump buffer of letters + digits in EITHER order ("a11", "11a",
 * "AA5") into 0-based (*row, *col). Returns 1 on success, 0 when there are no
 * letters or no digits, the letters are unparseable, the row is < 1, or
 * (rows > 0 / cols > 0) the cell is outside the grid. Caller treats 0 as a
 * no-op. */
GJ_UNUSED static int gj_parse(const char *buf, int rows, int cols, int *row, int *col) {
    char letters[GJ_BUF_CAP]; int nl = 0;
    char digits[GJ_BUF_CAP]; int nd = 0;
    if (!buf) return 0;
    for (const char *p = buf; *p; p++) {
        if (isalpha((unsigned char)*p) && nl < (int)sizeof(letters) - 1) letters[nl++] = *p;
        else if (isdigit((unsigned char)*p) && nd < (int)sizeof(digits) - 1) digits[nd++] = *p;
    }
    if (nl == 0 || nd == 0) return 0;
    digits[nd] = '\0';
    int c = gj_letters_to_col(letters, nl);
    if (c < 0) return 0;
    int r = atoi(digits) - 1;
    if (r < 0) return 0;
    if (rows > 0 && r >= rows) return 0;
    if (cols > 0 && c >= cols) return 0;
    *row = r; *col = c;
    return 1;
}

/* Append ch to a jump buffer of `cap` bytes. Only [A-Za-z0-9] is accepted and
 * one byte is always kept for the NUL. Returns 1 if appended. */
GJ_UNUSED static int gj_buf_append(char *buf, size_t cap, char ch) {
    size_t len;
    if (!buf || cap < 2 || !isalnum((unsigned char)ch)) return 0;
    len = strlen(buf);
    if (len + 1 >= cap) return 0;
    buf[len] = ch;
    buf[len + 1] = '\0';
    return 1;
}

/* Pull the cursor back inside the bounds (no-op for unbounded axes). */
GJ_UNUSED static void gj_clamp(GjState *st) {
    if (st->row < 0) st->row = 0;
    if (st->col < 0) st->col = 0;
    if (st->rows > 0 && st->row >= st->rows) st->row = st->rows - 1;
    if (st->cols > 0 && st->col >= st->cols) st->col = st->cols - 1;
}

/* One navigating-state key. Never touches anything but *st. */
GJ_UNUSED static GjAction gj_step(GjState *st, GjKey key, char ch) {
    switch (key) {
    case GJ_KEY_ESC:
        return GJ_DISARM;
    case GJ_KEY_UP:
        if (st->row > 0) { st->row--; return GJ_MOVED; }
        return GJ_NONE;
    case GJ_KEY_DOWN:
        if (st->rows == 0 || st->row + 1 < st->rows) { st->row++; return GJ_MOVED; }
        return GJ_NONE;
    case GJ_KEY_LEFT:
        if (st->col > 0) { st->col--; return GJ_MOVED; }
        return GJ_NONE;
    case GJ_KEY_RIGHT:
        if (st->cols == 0 || st->col + 1 < st->cols) { st->col++; return GJ_MOVED; }
        return GJ_NONE;
    case GJ_KEY_BACKSPACE: {
        size_t len = strlen(st->jump);
        if (len == 0) return GJ_NONE;
        st->jump[len - 1] = '\0';
        return GJ_BUFFER;
    }
    case GJ_KEY_ENTER:
        if (st->jump[0]) {
            int r, c;
            int ok = gj_parse(st->jump, st->rows, st->cols, &r, &c);
            st->jump[0] = '\0';
            if (!ok) return GJ_NONE;
            st->row = r; st->col = c;
            return GJ_JUMPED;
        }
        return GJ_ENTER_CELL;
    case GJ_KEY_CHAR:
        return gj_buf_append(st->jump, sizeof(st->jump), ch) ? GJ_BUFFER : GJ_NONE;
    }
    return GJ_NONE;
}

#endif /* KHTPM_GRID_JUMP_C */
