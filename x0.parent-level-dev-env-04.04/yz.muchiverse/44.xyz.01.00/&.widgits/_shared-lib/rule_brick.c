/* rule_brick.c - RULE "brick": a body that rises into a brick from BELOW (a head bump) breaks it: the brick tile becomes empty ('.') in the caller's level rows and the score rises by brick_score per brick. From the side or from above nothing happens.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93, decision D106). The game tick loop calls br_bump IN-PROCESS after the body step; ops/rule_brick_op.c calls the
 * same function for a harness, an event page / Blueprintz or an AI tool call. The number (brick_score) and the tile character (LEVEL | brick | <char>) are DATA. Q8 = 1/256 cell, y grows downward, so a rise is vy < 0.
 * INPUT: the body AFTER the step (pf_step snaps a body that hit a ceiling flush under it: y is a multiple of 256) and vy_before, the body's vertical velocity BEFORE that step (the step zeroes vy on a bump).
 * HEAD BUMP = vy_before <= 0 (it was not falling) AND y is a multiple of 256 (the head touches a cell boundary) AND the row above the head (y/256 - 1) holds a brick in a column the body's box overlaps (x/256 .. (x+w-1)/256).
 * Every such brick is broken in the same call (a body straddling two bricks breaks both), scanned left to right; its cell goes to bx[]/by[] (at most `cap`), *score grows by brick_score. A body beside a brick (its y not on a boundary), standing on one (the brick is below, not above the head) or falling into
 * one (vy_before > 0) breaks nothing. Cells outside the level are ignored. rows is a char grid, cell (x, y) = rows[y * stride + x]. Returns the number broken. Harness: case rule_brick. */
#ifndef RULE_BRICK_C
#define RULE_BRICK_C
#include "phys_field.c"
typedef struct { long score; } BR_Rule;
static PF_UNUSED int br_bump(const PF_Body *b, long vy_before, char *rows, int stride, int w, int h, char brick, const BR_Rule *r, long *score, int *bx, int *by, int cap) {
    int got = 0; if (vy_before > 0 || pf_floordiv(b->y, PF_Q) * PF_Q != b->y) return 0;
    long y = pf_floordiv(b->y, PF_Q) - 1, x0 = pf_floordiv(b->x, PF_Q), x1 = pf_floordiv(b->x + b->w - 1, PF_Q);
    for (long x = x0; x <= x1; x++) {
        if (x < 0 || x >= w || y < 0 || y >= h || got >= cap) continue;
        if (rows[y * stride + x] == brick) { rows[y * stride + x] = '.'; bx[got] = (int)x; by[got] = (int)y; got++; *score += r->score; } }
    return got;
}
#endif
