/* rule_spawn.c - RULE "spawn": a body is put STANDING in a cell (centred across it, its feet on the cell floor) with every motion field cleared; a hero is also reset to its starting state (not armed, facing right, no jump or fire pending); a walker starts alive, walking left.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game calls sp_hero (load, a hit, a fall) and sp_walker (load) IN-PROCESS;
 * ops/rule_spawn_op.c calls the same functions for a harness, an event page / Blueprintz or an AI tool call. Numbers are DATA (SP_Rule from RULE body_w, body_h), never constants here. Q8 = 1/256 cell.
 * sp_stand: the body is cleared (memset 0: velocity, grounded, coyote, jump buffer, ...), then w, h, field = -1 (no field chosen yet), x = cx * 256 + (256 - w) / 2 (C integer division), y = cy * 256 + (256 - h).
 * sp_hero: sp_stand, then the caller's per-seat state: jp = 0, pos = cx, armed = 0, facing = 1, fire = 0. sp_walker: sp_stand, then dir = -1 and alive = 1. Harness: case rule_spawn. */
#ifndef RULE_SPAWN_C
#define RULE_SPAWN_C
#include "rule_patrol.c"
typedef struct { long w, h; } SP_Rule;
static PF_UNUSED void sp_stand(PF_Body *b, int cx, int cy, const SP_Rule *r) {
    memset(b, 0, sizeof *b); b->w = r->w; b->h = r->h; b->field = -1;
    b->x = (long)cx * PF_Q + (PF_Q - b->w) / 2; b->y = (long)cy * PF_Q + (PF_Q - b->h);
}
static PF_UNUSED void sp_hero(PF_Body *b, int cx, int cy, const SP_Rule *r, int *jp, int *pos, int *armed, int *facing, int *fire) {
    sp_stand(b, cx, cy, r); *jp = 0; *pos = cx; *armed = 0; *facing = 1; *fire = 0;
}
static PF_UNUSED void sp_walker(RP_Walker *w, int cx, int cy, const SP_Rule *r) { sp_stand(&w->b, cx, cy, r); w->dir = -1; w->alive = 1; }
#endif
