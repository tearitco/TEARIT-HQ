#ifndef BV_RIG_ANIM_DRAW_H
#define BV_RIG_ANIM_DRAW_H
/* Stage 1 of the rig animation (decision D140, BOOK 16.game/TUTORIAL-HARNESS-...-2026-11-26.md section 11.6): the schema-2 pose (bv_rig_anim.h, the ONE evaluator) turned into the SAME six cell-relative boxes
 * bv_rig.h makes ({x0,y0,z0,x1,y1,z1} in blocks, x/z from the cell centre, y up from the feet), so the renderer, the skin models and the cracks need no other change. Boxes cannot tilt in this raymarcher yet (stage 2),
 * so each limb is the box of its own size and scale placed at the CENTRE where the rotated limb really is: a swing is a displacement now, a tilt later. REMOVAL CONDITION: delete the centre approximation when stage 2 renders the same rows.
 * The six parts are found by name, in this box order: head, torso, arm_r, arm_l, leg_r, leg_l (the skin parts of bv_rig.h). Any missing -> 0 and the caller keeps the old stepped rig.
 * Facing: direction as bv_rig.h (2 south, 4 west, 6 east, 8 north, 0 1 3 also accepted); the figure's right is actor +x, forward actor +z; south -> right is world west.
 * bv_rig2_get caches one parsed rig per (dir, name) and re-reads the file at most once a second, and only when its SIZE changed (marker rule, never mtime). */
#include "bv_rig_anim.h"
#include <sys/stat.h>
#include <time.h>
static const char *const BV2_NAMES[6] = { "head", "torso", "arm_r", "arm_l", "leg_r", "leg_l" };
static const int BV2_CI[6] = { 0, 1, 2, 2, 3, 3 };      /* colour index per box: head, torso, arm, arm, leg, leg (bv_rig.h) */
static void bv2_facing(int direction, int *fx, int *fz) { switch (direction) { case 4: case 1: *fx = -1; *fz = 0; break; case 6: *fx = 1; *fz = 0; break; case 8: case 3: *fx = 0; *fz = -1; break; default: *fx = 0; *fz = 1; break; } }
/* Returns 6 and fills out/ci, or 0 (not a schema-2 rig of this shape). t_ms: any millisecond clock. */
static int bv_rig2_boxes(const BvaRig *r, int direction, int pattern, float t_ms, float out[6][6], int ci[6]) {
    if (!r || r->nparts < 6) return 0;
    int idx[6]; for (int i = 0; i < 6; i++) { idx[i] = bva_find_part(r, BV2_NAMES[i]); if (idx[i] < 0) return 0; }
    static BvaPose po[BVA_MAX_PARTS]; bva_eval(r, bva_anim_for_pattern(r, pattern), t_ms, po);
    int fx, fz; bv2_facing(direction, &fx, &fz); int rx = -fz, rz = fx;
    for (int i = 0; i < 6; i++) {
        const BvaPart *p = &r->part[idx[i]]; const BvaPose *q = &po[idx[i]];
        float c[3] = { p->size[0] * 0.5f - p->pivot[0], p->size[1] * 0.5f - p->pivot[1], p->size[2] * 0.5f - p->pivot[2] };      /* box centre in the part's space */
        float w[3]; for (int k = 0; k < 3; k++) w[k] = q->p[k] + q->m[k * 3] * c[0] + q->m[k * 3 + 1] * c[1] + q->m[k * 3 + 2] * c[2];
        float sx = 0.5f * p->size[0] * sqrtf(q->m[0] * q->m[0] + q->m[3] * q->m[3] + q->m[6] * q->m[6]), sy = 0.5f * p->size[1] * sqrtf(q->m[1] * q->m[1] + q->m[4] * q->m[4] + q->m[7] * q->m[7]), sz = 0.5f * p->size[2] * sqrtf(q->m[2] * q->m[2] + q->m[5] * q->m[5] + q->m[8] * q->m[8]);
        float wx = rx * w[0] + fx * w[2], wz = rz * w[0] + fz * w[2], hxw = fx ? sz : sx, hzw = fx ? sx : sz;
        out[i][0] = wx - hxw; out[i][1] = w[1] - sy; out[i][2] = wz - hzw; out[i][3] = wx + hxw; out[i][4] = w[1] + sy; out[i][5] = wz + hzw; ci[i] = BV2_CI[i];
    }
    return 6;
}
/* The parsed rig of `rig:<name>` from <dir>/rig_<name>.pdl, or NULL when the file has no schema-2 parts. */
static const BvaRig *bv_rig2_get(const char *dir, const char *charset, long long now_ms) {
    static struct { char path[4700]; long size; long long checked; int ok; BvaRig rig; } c[4]; static int next = 0; static char buf[65536];
    char path[4700]; snprintf(path, sizeof path, "%s/rig_%s.pdl", dir, charset + 4); int s = -1;
    for (int i = 0; i < 4; i++) if (!strcmp(c[i].path, path)) s = i;
    if (s < 0) { s = next; next = (next + 1) & 3; snprintf(c[s].path, sizeof c[s].path, "%s", path); c[s].size = -1; c[s].checked = -100000; c[s].ok = 0; }
    if (now_ms - c[s].checked >= 1000 || now_ms < c[s].checked) {
        c[s].checked = now_ms; struct stat st;
        if (stat(path, &st) != 0) { c[s].ok = 0; c[s].size = -1; }
        else if (st.st_size != c[s].size) { c[s].size = st.st_size; c[s].ok = 0; FILE *f = fopen(path, "rb");
            if (f) { size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0; if (n < sizeof buf - 1 && bva_parse(buf, &c[s].rig) == 0 && c[s].rig.nparts >= 6) c[s].ok = 1; } }
    }
    return c[s].ok ? &c[s].rig : NULL;
}
#endif
