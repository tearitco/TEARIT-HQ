/* phys_field.c - the shared gravity-field body step ("Teario-universe", owner 2026-11-18: round planets with Mario-Galaxy-style gravity and jumping that ALL games can reuse; flat levels
 * are one field, round ones are planets, you hop asteroids).
 *
 * Text-included canonical helper (pure: no files, no clock, no float, no environment). One body step; replay and the live driver call the SAME function, so a run is exactly reproducible
 * on every build (house rule: integer fixed point, a float result could differ between builds and break the ledger replay).
 *
 * UNITS: Q8 = 1/256 of a cell, everything is a long. Time = logical ticks. A velocity is Q8 cells per tick, an acceleration Q8 cells per tick per tick.
 *
 * A LEVEL IS A LIST OF FIELDS (PF_Field):
 *   plane  one constant direction (dx,dy unit axis, e.g. 0,1 = down); always in reach; the "flat level" is this one field.
 *   point  a planet/asteroid: a centre (cx,cy Q8) and a reach (Q8). Inside the reach the pull points at the centre. dirmode AXIS (voxel planets: the dominant axis of the offset, 4 exact
 *          directions, a small hysteresis band so the body does not flicker on a 45 degree boundary) or RADIAL (smooth: the offset normalised with an integer square root).
 * WHICH FIELD PULLS: the nearest point field whose reach contains the body centre (tie: the lower id); else the lowest-id plane; else nothing (free flight). Leaving one reach and entering the next
 * is how you hop from asteroid to asteroid.
 * GRAVITY STRENGTH is a number of the caller (PF_Tune.gravity), normally read from the physics-node/tuning rows; falloff is the caller's: it passes the strength per field (PF_Field.g, 0 = tune default).
 *
 * THE BODY: an axis-aligned box (x,y = top-left, w,h) moved along the WORLD axes and collided with the caller's solid tiles (solid(ctx,cx,cy)). "Up" is the opposite of the pull; controls are
 * SURFACE-RELATIVE: right = along the surface to the body's right when its head points up (so upside down the screen may turn, the control still reads "right").
 * INPUT MASK: 1 = left, 2 = right, 4 = jump. Jump is an impulse along up while grounded (or within `coyote` ticks of leaving the ground); a press is remembered `buffer` ticks before landing.
 * Order of one tick: pick field, gravity into v, tangent input and friction, jump, clamp the fall speed, move x then y with collision, grounded test, coyote and buffer counters.
 * Design doc: XO nov-18 plan (Teario section 8), decision log D90. Harness: case phys_field.
 */
#ifndef PHYS_FIELD_C
#define PHYS_FIELD_C
#include <stdlib.h>
#define PF_UNUSED __attribute__((unused))
#define PF_Q 256L

typedef struct { int id; int kind; /* 0 plane, 1 point */ long cx, cy, reach; int dirmode; /* 0 axis, 1 radial */ int dx, dy; /* plane: unit axis */ long g; /* 0 = tune default */ } PF_Field;
typedef struct { long gravity, jump_v, run_max, accel, friction, fall_max, hyst; int coyote, buffer; } PF_Tune;
typedef struct { long x, y, vx, vy, w, h; int ux, uy; /* the pull as a unit vector in Q8 (0,256 = down) */ int field; /* id of the pulling field, -1 none */ int axis; /* last axis chosen, 0 x 1 y */
                 int grounded, coyote, jbuf, prev_jump; } PF_Body;

static PF_UNUSED PF_Tune pf_default_tune(void) { PF_Tune t = { 16, 200, 48, 6, 8, 240, 64, 4, 5 }; return t; }
static PF_UNUSED long pf_abs(long v) { return v < 0 ? -v : v; }
static PF_UNUSED int pf_sgn(long v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static PF_UNUSED long long pf_isqrt(long long n) { if (n <= 0) return 0; long long r = 0, b = 1LL << 62; while (b > n) b >>= 2; while (b) { if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }
static PF_UNUSED long pf_floordiv(long a, long b) { long q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }

/* which field pulls the body and where: sets b->field, b->ux, b->uy, b->axis; returns 1 when something pulls */
static PF_UNUSED int pf_pick(PF_Body *b, const PF_Tune *t, const PF_Field *f, int nf, long *gout) {
    long cx = b->x + b->w / 2, cy = b->y + b->h / 2; int best = -1; long long bd = 0;
    for (int i = 0; i < nf; i++) if (f[i].kind == 1) {
        long long dx = f[i].cx - cx, dy = f[i].cy - cy, d2 = dx * dx + dy * dy;
        if (d2 > (long long)f[i].reach * f[i].reach) continue;
        if (best < 0 || d2 < bd || (d2 == bd && f[i].id < f[best].id)) { best = i; bd = d2; }
    }
    if (best >= 0) {
        long dx = f[best].cx - cx, dy = f[best].cy - cy;
        if (f[best].dirmode == 0) {          /* axis: dominant axis with hysteresis */
            int ax = b->axis;
            if (b->field != f[best].id) ax = pf_abs(dx) >= pf_abs(dy) ? 0 : 1;
            else if (ax == 0 && pf_abs(dy) > pf_abs(dx) + t->hyst) ax = 1;
            else if (ax == 1 && pf_abs(dx) > pf_abs(dy) + t->hyst) ax = 0;
            b->axis = ax;
            if (ax == 0) { b->ux = (int)(PF_Q * (dx >= 0 ? 1 : -1)); b->uy = 0; } else { b->ux = 0; b->uy = (int)(PF_Q * (dy >= 0 ? 1 : -1)); }
        } else {                             /* radial: offset normalised, integer only */
            long long len = pf_isqrt(bd);
            if (len == 0) { b->ux = 0; b->uy = (int)PF_Q; } else { b->ux = (int)((long long)dx * PF_Q / len); b->uy = (int)((long long)dy * PF_Q / len); }
        }
        b->field = f[best].id; *gout = f[best].g ? f[best].g : t->gravity; return 1;
    }
    int pl = -1;
    for (int i = 0; i < nf; i++) if (f[i].kind == 0 && (pl < 0 || f[i].id < f[pl].id)) pl = i;
    if (pl >= 0) { b->ux = (int)(PF_Q * f[pl].dx); b->uy = (int)(PF_Q * f[pl].dy); b->field = f[pl].id; *gout = f[pl].g ? f[pl].g : t->gravity; return 1; }
    b->field = -1; b->ux = 0; b->uy = 0; *gout = 0; return 0;
}

typedef int (*PF_Solid)(void *ctx, long cx, long cy);
static PF_UNUSED int pf_hits(const PF_Body *b, long x, long y, PF_Solid solid, void *ctx) {
    long x0 = pf_floordiv(x, PF_Q), x1 = pf_floordiv(x + b->w - 1, PF_Q), y0 = pf_floordiv(y, PF_Q), y1 = pf_floordiv(y + b->h - 1, PF_Q);
    for (long cy = y0; cy <= y1; cy++) for (long cx = x0; cx <= x1; cx++) if (solid(ctx, cx, cy)) return 1;
    return 0;
}

/* ONE tick. mask: 1 left, 2 right, 4 jump. */
static PF_UNUSED void pf_step(PF_Body *b, const PF_Tune *t, const PF_Field *f, int nf, int mask, PF_Solid solid, void *ctx) {
    long g = 0; int pulled = pf_pick(b, t, f, nf, &g);
    if (pulled) { b->vx += (long)b->ux * g / PF_Q; b->vy += (long)b->uy * g / PF_Q; }
    long upx = -b->ux, upy = -b->uy, rx = b->uy, ry = -b->ux;                 /* up = away from the pull, right = the surface direction to the right of up */
    int dir = ((mask & 2) ? 1 : 0) - ((mask & 1) ? 1 : 0);
    if (pulled) {
        long vt = (b->vx * rx + b->vy * ry) / PF_Q, vn = (b->vx * b->ux + b->vy * b->uy) / PF_Q;
        if (dir) { vt += (long)dir * t->accel; if (vt > t->run_max) vt = t->run_max; if (vt < -t->run_max) vt = -t->run_max; }
        else if (b->grounded) { if (vt > 0) { vt -= t->friction; if (vt < 0) vt = 0; } else if (vt < 0) { vt += t->friction; if (vt > 0) vt = 0; } }
        int jp = (mask & 4) != 0; if (jp && !b->prev_jump) b->jbuf = t->buffer; b->prev_jump = jp;
        if (b->jbuf > 0 && (b->grounded || b->coyote > 0)) { vn = -t->jump_v; b->jbuf = 0; b->coyote = 0; b->grounded = 0; }
        if (vn > t->fall_max) vn = t->fall_max;
        b->vx = (vt * rx + vn * b->ux) / PF_Q; b->vy = (vt * ry + vn * b->uy) / PF_Q;
        (void)upx; (void)upy;
    } else { b->prev_jump = (mask & 4) != 0; }
    /* move x then y, snapping to the tile edge on a hit */
    long nx = b->x + b->vx;
    if (pf_hits(b, nx, b->y, solid, ctx)) { if (b->vx > 0) nx = pf_floordiv(nx + b->w - 1, PF_Q) * PF_Q - b->w; else nx = (pf_floordiv(nx, PF_Q) + 1) * PF_Q; b->vx = 0; if (pf_hits(b, nx, b->y, solid, ctx)) nx = b->x; }
    b->x = nx;
    long ny = b->y + b->vy;
    if (pf_hits(b, b->x, ny, solid, ctx)) { if (b->vy > 0) ny = pf_floordiv(ny + b->h - 1, PF_Q) * PF_Q - b->h; else ny = (pf_floordiv(ny, PF_Q) + 1) * PF_Q; b->vy = 0; if (pf_hits(b, b->x, ny, solid, ctx)) ny = b->y; }
    b->y = ny;
    if (pulled) {
        long sx = b->ux > 96 ? 1 : b->ux < -96 ? -1 : 0, sy = b->uy > 96 ? 1 : b->uy < -96 ? -1 : 0;
        b->grounded = (sx || sy) && pf_hits(b, b->x + sx, b->y + sy, solid, ctx);
        if (b->grounded) b->coyote = t->coyote; else if (b->coyote > 0) b->coyote--;
        if (b->jbuf > 0) b->jbuf--;
    } else b->grounded = 0;
}
#endif
