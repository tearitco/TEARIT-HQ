/* rule_pickup.c - RULE "pickup": a body whose CENTRE is in an item's cell collects it: the item is taken, the collector becomes ARMED (the projectile power-up) and the score rises by pickup_score.
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game tick loop calls pk_collect IN-PROCESS; ops/rule_pickup_op.c calls the
 * same function for a harness, an event page / Blueprintz or an AI tool call. The number is DATA (PK_Rule.score from RULE pickup_score), never a constant here. Q8 = 1/256 cell.
 * The body's centre cell is ((x + w/2) / 256, (y + h/2) / 256) with C integer division, which TRUNCATES toward zero: a centre left of or above the world (negative) falls in cell 0 (existing behaviour, kept and pinned: no level lets a body get there).
 * Every item not yet taken whose cell is that cell is collected in the SAME call (two items in one cell are both collected); an item is collected once (taken stays 1). Returns the number collected; sets *armed to 1 when any was; adds collected * pickup_score to *score.
 * cx[], cy[] are the item cells, taken[] the flags (the caller's state). Harness: case rule_pickup. */
#ifndef RULE_PICKUP_C
#define RULE_PICKUP_C
#include "phys_field.c"
typedef struct { long score; } PK_Rule;
static PF_UNUSED int pk_collect(const PF_Body *hero, const int *cx, const int *cy, int *taken, int n, const PK_Rule *r, int *armed, long *score) {
    int got = 0; long hx = (hero->x + hero->w / 2) / PF_Q, hy = (hero->y + hero->h / 2) / PF_Q;
    for (int p = 0; p < n; p++) if (!taken[p] && hx == cx[p] && hy == cy[p]) { taken[p] = 1; *armed = 1; *score += r->score; got++; }
    return got;
}
#endif
