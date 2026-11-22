/* rule_coin.c - RULE "coin": a body whose box OVERLAPS a coin tile collects it: the tile becomes empty ('.') in the caller's level rows and the score rises by coin_value per coin.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93, decision D106). The game tick loop calls cn_collect IN-PROCESS; ops/rule_coin_op.c calls the
 * same function for a harness, an event page / Blueprintz or an AI tool call. The number (coin_value) and the tile character (LEVEL | coin | <char>) are DATA, never constants here. Q8 = 1/256 cell.
 * A coin is walked THROUGH (it is not solid) and is taken by any overlap of the body's box with its cell (a box that only touches the cell's edge does not overlap: x + w - 1 is the last Q8 column of the box). The cells are scanned row by row, left to right; each coin found
 * is replaced by '.' (so it is collected ONCE: a replayed run, a second call and a second body see an empty cell), its cell is written to cx[]/cy[] (at most `cap`) and *score grows by coin_value. Cells outside the level are ignored. rows is a char grid, cell (x, y) = rows[y * stride + x]. Returns the number collected.
 * Harness: case rule_coin. */
#ifndef RULE_COIN_C
#define RULE_COIN_C
#include "phys_field.c"
typedef struct { long value; } CN_Rule;
static PF_UNUSED int cn_collect(const PF_Body *b, char *rows, int stride, int w, int h, char coin, const CN_Rule *r, long *score, int *cx, int *cy, int cap) {
    int got = 0; long x0 = pf_floordiv(b->x, PF_Q), x1 = pf_floordiv(b->x + b->w - 1, PF_Q), y0 = pf_floordiv(b->y, PF_Q), y1 = pf_floordiv(b->y + b->h - 1, PF_Q);
    for (long y = y0; y <= y1; y++) for (long x = x0; x <= x1; x++) {
        if (x < 0 || x >= w || y < 0 || y >= h || got >= cap) continue;
        if (rows[y * stride + x] == coin) { rows[y * stride + x] = '.'; cx[got] = (int)x; cy[got] = (int)y; got++; *score += r->value; } }
    return got;
}
#endif
