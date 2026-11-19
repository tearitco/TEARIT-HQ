/* rule_patrol.c - RULE "patrol": a walker walks along its surface, turns at a wall or at a ledge, and is gone when it falls out of the world (Teario enemies; any game's walker).
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation of this rule (standard: BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, decision log D92/D93). The
 * tick loop of a game calls rp_step IN-PROCESS (no process per tick); the CLI op ops/rule_patrol_op.c calls the same function for a harness, an event page / Blueprintz or an AI tool call.
 * It builds on the shared body step (phys_field.c): the walker is a PF_Body that is held to a patrol speed and turned by two tests made on the tick it is grounded:
 *   LEDGE  the cell one step ahead of the leading edge and one row under the feet is not solid
 *   WALL   the body did not move this tick (x == the x it started the tick at)
 * A turn also turns the momentum (vx = speed * new dir): without it the next tick would stand still and the wall test would turn it straight back (seen 2026-11-18 as a jitter).
 * speed is Q8 cells per tick (RULE patrol_speed, ruleset data, never a constant here); run_max, accel and friction are all set to it so the walker reaches speed in one tick and stops dead.
 * fall_y is the body y (Q8) below which the walker is gone (the caller passes the level bottom plus two rows).
 * dir is -1 (left) or +1 (right). Harness: case rule_patrol. */
#ifndef RULE_PATROL_C
#define RULE_PATROL_C
#include "phys_field.c"
typedef struct { PF_Body b; int dir; int alive; } RP_Walker;
static PF_UNUSED void rp_step(RP_Walker *w, const PF_Tune *base, long speed, const PF_Field *f, int nf, PF_Solid solid, void *ctx, long fall_y) {
    if (!w->alive) return;
    PF_Tune et = *base; et.run_max = speed; et.accel = speed; et.friction = speed;
    PF_Body *b = &w->b; long ox = b->x;
    pf_step(b, &et, f, nf, w->dir < 0 ? 1 : 2, solid, ctx);
    if (b->grounded) {
        long fx = w->dir > 0 ? b->x + b->w : b->x - 1, fy = b->y + b->h;
        if (!solid(ctx, pf_floordiv(fx, PF_Q), pf_floordiv(fy, PF_Q)) || b->x == ox) { w->dir = -w->dir; b->vx = speed * w->dir; }
    }
    if (b->y > fall_y) w->alive = 0;
}
#endif
