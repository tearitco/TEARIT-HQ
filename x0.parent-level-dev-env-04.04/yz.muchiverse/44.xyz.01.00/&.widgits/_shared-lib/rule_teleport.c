/* rule_teleport.c - RULE "teleport": a body whose CENTER cell is a SENDING teleport tile is moved to the paired EXIT tile; the score rises by teleport_fee.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93, decision D111). The game tick loop calls tp_step IN-PROCESS; ops/rule_teleport_op.c calls the same function for a harness,
 * an event page / Blueprintz or an AI tool call. The number (teleport_fee) and the tile character (BLOCK | teleport | <char> in blocks.pdl) are DATA. Q8 = 1/256 cell.
 * PAIRING (derived from the level rows, no second list): the teleport tiles are numbered in reading order (top row first, left to right). Tile 0 sends to tile 1, tile 2 to tile 3: an EVEN tile with a successor is a DOOR (as the house DOOR rows of rpg-pet/rooms.pdl: a trigger cell and an arrival cell), an ODD tile is only an EXIT, an unpaired last tile is inert.
 * One-way and no bounce by construction: standing on an exit sends nothing, so there is no cooldown state.
 * ARRIVAL: the body's center goes to the exit cell's center in x, its feet to the exit cell's floor in y (y = (cell + 1) * 256 - h), vy = 0, vx kept. Cells outside the level are ignored. rows is a char grid, cell (x, y) = rows[y * stride + x]. Returns 1 when it teleported (cells in *fx,*fy,*tx,*ty), else 0.
 * Harness: case rule_teleport. */
#ifndef RULE_TELEPORT_C
#define RULE_TELEPORT_C
#include "phys_field.c"
typedef struct { long fee; } TP_Rule;
static PF_UNUSED int tp_step(PF_Body *b, const char *rows, int stride, int w, int h, char tp, const TP_Rule *r, long *score, int *fx, int *fy, int *tx, int *ty) {
    long cx = pf_floordiv(b->x + b->w / 2, PF_Q), cy = pf_floordiv(b->y + b->h / 2, PF_Q);
    if (cx < 0 || cx >= w || cy < 0 || cy >= h || rows[cy * stride + cx] != tp) return 0;
    int k = 0, found = 0, dx = 0, dy = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { if (rows[y * stride + x] != tp) continue;
        if (y == cy && x == cx) { if (k & 1) return 0; found = 1; }
        else if (found == 1) { dx = x; dy = y; found = 2; }
        k++; }
    if (found != 2) return 0;
    *fx = (int)cx; *fy = (int)cy; *tx = dx; *ty = dy;
    b->x = (long)dx * PF_Q + PF_Q / 2 - b->w / 2; b->y = ((long)dy + 1) * PF_Q - b->h; b->vy = 0; *score += r->fee;
    return 1;
}
#endif
