/* rule_stomp.c - RULE "stomp": a body (the hero) that touches a live walker either STOMPS it (it was falling and its feet were above the walker's head before this tick's move: the walker dies, the body bounces) or is HIT by it (any other touch).
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment), the ONE implementation (standard BOOK 02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md, D92/D93). The game tick loop calls st_contact IN-PROCESS; ops/rule_stomp_op.c calls the
 * same function for a harness, an event page / Blueprintz or an AI tool call. Targets are rule_patrol.c walkers (RP_Walker), so the rules compose. Numbers are DATA (ST_Rule from RULE rows stomp_margin, stomp_bounce, stomp_score), never constants here. Q8 = 1/256 cell.
 * The overlap test is strict (boxes that only touch edges do not overlap). Walkers are taken IN ORDER; for each live walker that overlaps the body:
 *   STOMP  vy > 0 and (y + h) - vy <= walker.y + stomp_margin   (the feet, put back by this tick's fall, were not deeper than `stomp_margin` into the walker): the walker dies, the body's vy becomes -stomp_bounce, stomps++
 *   HIT    anything else: hit = 1 and the scan STOPS (the caller decides what a hit does: score, respawn, disarm)
 * Because a stomp changes vy to a rise, a SECOND overlapping walker in the same tick is a HIT (existing behaviour, kept and pinned by the case). The caller adds stomps * stomp_score to the score. Harness: case rule_stomp. */
#ifndef RULE_STOMP_C
#define RULE_STOMP_C
#include "rule_patrol.c"
typedef struct { long margin, bounce, score; } ST_Rule;
typedef struct { int stomps; int hit; } ST_Result;
static PF_UNUSED int st_overlap(const PF_Body *a, const PF_Body *b) { return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h; }
static PF_UNUSED ST_Result st_contact(PF_Body *hero, RP_Walker *w, int nw, const ST_Rule *r) {
    ST_Result res = { 0, 0 };
    for (int e = 0; e < nw; e++) if (w[e].alive && st_overlap(hero, &w[e].b)) {
        if (hero->vy > 0 && (hero->y + hero->h) - hero->vy <= w[e].b.y + r->margin) { w[e].alive = 0; hero->vy = -r->bounce; res.stomps++; }
        else { res.hit = 1; break; }
    }
    return res;
}
#endif
