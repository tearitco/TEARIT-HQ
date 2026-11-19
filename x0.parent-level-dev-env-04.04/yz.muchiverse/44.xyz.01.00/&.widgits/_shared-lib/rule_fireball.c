/* rule_fireball.c - RULE "fireball": an armed shooter fires a shot that flies straight, dies on a wall or when its life runs out, and kills the first live walker whose box contains the shot's point (Teario's projectile power-up; any game's bolt).
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game tick loop calls fb_spawn and
 * fb_step IN-PROCESS; ops/rule_fireball_op.c calls the same functions for a harness, an event page / Blueprintz or an AI tool call. Targets are rule_patrol.c walkers (RP_Walker), so the two rules compose.
 * Numbers are DATA (FB_Rule from RULE rows fireball_speed, fireball_life, fireball_max, fireball_score), never constants here. Q8 = 1/256 cell.
 * ORDER of one tick (the caller's order, kept by game_core): the shooter's fire input spawns a shot FIRST, then fb_step moves every live shot, so a shot fired on tick t has already moved once on tick t.
 * fb_spawn: needs the shooter ARMED (the caller checks) and a free slot among the first `max` slots; the shot starts at the shooter box centre, vx = speed * facing (-1 or +1).
 * fb_step per live shot: x += vx; then `--life <= 0` or the cell under the shot point solid ends it; else the FIRST live walker whose box contains the point (edges included: x in [wx, wx+ww], y in [wy, wy+wh]) is killed and the shot ends (one kill per shot).
 * Returns the number of kills this tick (the caller adds kills * fireball_score to the score). Harness: case rule_fireball. */
#ifndef RULE_FIREBALL_C
#define RULE_FIREBALL_C
#include "rule_patrol.c"
typedef struct { long x, y, vx; int life, alive; } FB_Shot;
typedef struct { long speed; int life; int max; long score; } FB_Rule;
static PF_UNUSED int fb_spawn(FB_Shot *s, int n, const FB_Rule *r, const PF_Body *shooter, int facing) {
    int lim = r->max < n ? r->max : n;
    for (int k = 0; k < lim; k++) if (!s[k].alive) { s[k].alive = 1; s[k].life = r->life; s[k].x = shooter->x + shooter->w / 2; s[k].y = shooter->y + shooter->h / 2; s[k].vx = r->speed * (facing < 0 ? -1 : 1); return 1; }
    return 0;
}
static PF_UNUSED int fb_step(FB_Shot *s, int n, RP_Walker *w, int nw, PF_Solid solid, void *ctx) {
    int kills = 0;
    for (int k = 0; k < n; k++) if (s[k].alive) {
        s[k].x += s[k].vx;
        if (--s[k].life <= 0 || solid(ctx, pf_floordiv(s[k].x, PF_Q), pf_floordiv(s[k].y, PF_Q))) { s[k].alive = 0; continue; }
        for (int e = 0; e < nw; e++) if (w[e].alive) { const PF_Body *b = &w[e].b;
            if (s[k].x >= b->x && s[k].x <= b->x + b->w && s[k].y >= b->y && s[k].y <= b->y + b->h) { w[e].alive = 0; s[k].alive = 0; kills++; break; } }
    }
    return kills;
}
#endif
