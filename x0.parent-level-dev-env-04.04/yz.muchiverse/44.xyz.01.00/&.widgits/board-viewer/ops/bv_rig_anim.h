#ifndef BV_RIG_ANIM_H
#define BV_RIG_ANIM_H
/* Rig node tree + keyframe animation, schema 2 (BOOK 16.game/TUTORIAL-HARNESS-ONBOARDING-GAMIFICATION-AND-NEAR-TERM-DRIVE-PLAN-2026-11-26.md sections 9 and 11; decision D140).
 * ONE pure evaluator, text-included by the renderer and by the test op rig_pose_op (condition 1 of D140: two copies would be two authorities). No I/O, no globals, no allocation: the caller
 * hands over the file text and gets structs back.
 *
 * File rows (`|` separated, fields trimmed, triples are three space-separated numbers; a row type this file does not know is IGNORED, extra columns are IGNORED = forward compatible):
 *   SCHEMA | rig | 2
 *   PART  | name | parent | shape | res | size x y z | pivot x y z | pos x y z | rot x y z | scale x y z | alpha | material
 *   ANIM  | name | length_ms | loop            (loop = 1 or the word loop)
 *   TRACK | anim | part | channel | t_ms | value | ease
 * parent: `-` = the actor root; a parent must be named by an EARLIER PART row (so there are no cycles by construction).
 * Geometry: the part's box is [-pivot, size-pivot] in the part's own space, so the pivot (the shoulder, the hip) is the part's origin; pos is where that pivot sits in the PARENT's space.
 * shape/res/material are carried for the renderer (box, voxel, sphere...): the evaluator does not read them. Block units, y up (as bv_rig.h). Rotation in degrees, local matrix = Rz*Ry*Rx (x first).
 * channels: pos_x pos_y pos_z rot_x rot_y rot_z scale_x scale_y scale_z alpha. A channel WITH a track takes the track value (absolute); one without keeps the PART row value.
 * ease (of the segment that STARTS at a key): linear | sine | step | ease_in | ease_out. A looping animation interpolates from the last key back to the first at t = length.
 * World: M = Mparent * (R * diag(scale)), P = Pparent + Mparent * pos. alpha = own alpha (0..255) * parent alpha / 255. Non-uniform scale under rotation is exact only for the affine map (no shear fix).
 * -DBV_RIG_ANIM_MUTANT_NO_PARENT is the harness mutant: parents are ignored (a child no longer follows its parent) and the parent check must turn red. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BVA_MAX_PARTS 32
#define BVA_MAX_ANIMS 16
#define BVA_MAX_TRACKS 512
#define BVA_NCH 10

typedef struct { char name[32]; int parent; char shape[16]; int res; float size[3], pivot[3], pos[3], rot[3], scale[3]; int alpha; char material[24]; } BvaPart;
typedef struct { char name[32]; float length_ms; int loop; } BvaAnim;
typedef struct { int anim, part, ch; float t, v; int ease; } BvaTrack;      /* ease: 0 linear 1 sine 2 step 3 ease_in 4 ease_out */
typedef struct { int schema, nparts, nanims, ntracks; BvaPart part[BVA_MAX_PARTS]; BvaAnim anim[BVA_MAX_ANIMS]; BvaTrack track[BVA_MAX_TRACKS]; } BvaRig;
typedef struct { float m[9]; float p[3]; float lo[3], hi[3]; float alpha; } BvaPose;   /* m row-major 3x3 world matrix, p world pivot, lo/hi the world AABB of the part's box */

static const char *const bva_ch_name[BVA_NCH] = { "pos_x", "pos_y", "pos_z", "rot_x", "rot_y", "rot_z", "scale_x", "scale_y", "scale_z", "alpha" };
static const char *const bva_ease_name[5] = { "linear", "sine", "step", "ease_in", "ease_out" };

static int bva_split(char *line, char *f[], int max) {      /* split on '|', trim; returns the field count */
    int n = 0; char *p = line;
    while (n < max) { while (*p == ' ' || *p == '\t') p++; f[n++] = p; char *q = strchr(p, '|'); if (!q) q = p + strlen(p); char *e = q; while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--; int more = (*q == '|'); *e = 0; if (!more) break; p = q + 1; }
    return n;
}
static int bva_triple(const char *s, float *o, const float *def) { return (sscanf(s, "%f %f %f", &o[0], &o[1], &o[2]) == 3) ? 1 : (def ? (memcpy(o, def, 3 * sizeof(float)), 1) : 0); }
static int bva_find_part(const BvaRig *r, const char *n) { for (int i = 0; i < r->nparts; i++) if (!strcmp(r->part[i].name, n)) return i; return -1; }
static int bva_find_anim(const BvaRig *r, const char *n) { for (int i = 0; i < r->nanims; i++) if (!strcmp(r->anim[i].name, n)) return i; return -1; }
static int bva_track_cmp(const void *a, const void *b) {
    const BvaTrack *x = a, *y = b; if (x->anim != y->anim) return x->anim - y->anim; if (x->part != y->part) return x->part - y->part; if (x->ch != y->ch) return x->ch - y->ch; return (x->t > y->t) - (x->t < y->t);
}

/* Parse the file text (modified in place). Returns 0 ok, else the 1-based line number of the first bad row (negated: -line). schema stays 1 when there is no SCHEMA row (an old colour-only rig file: nparts 0, nothing to pose). */
static int bva_parse(char *text, BvaRig *r) {
    memset(r, 0, sizeof *r); r->schema = 1; int ln = 0; char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        ln++; if (*line == '#') continue; char *f[16]; int n = bva_split(line, f, 16);
        if (!strcmp(f[0], "SCHEMA") && n >= 3 && !strcmp(f[1], "rig")) r->schema = atoi(f[2]);
        else if (!strcmp(f[0], "PART")) {
            if (n < 10 || r->nparts >= BVA_MAX_PARTS) return -ln; BvaPart *p = &r->part[r->nparts]; memset(p, 0, sizeof *p);
            snprintf(p->name, sizeof p->name, "%s", f[1]); if (!*p->name || bva_find_part(r, p->name) >= 0) return -ln;
            if (!strcmp(f[2], "-")) p->parent = -1; else { p->parent = bva_find_part(r, f[2]); if (p->parent < 0) return -ln; }
            snprintf(p->shape, sizeof p->shape, "%s", f[3]); p->res = atoi(f[4]); static const float one[3] = { 1, 1, 1 }, zero[3] = { 0, 0, 0 };
            if (!bva_triple(f[5], p->size, NULL) || !bva_triple(f[6], p->pivot, NULL) || !bva_triple(f[7], p->pos, NULL) || !bva_triple(f[8], p->rot, zero) || !bva_triple(f[9], p->scale, one)) return -ln;
            p->alpha = n > 10 ? atoi(f[10]) : 255; if (p->alpha < 0) p->alpha = 0; if (p->alpha > 255) p->alpha = 255; snprintf(p->material, sizeof p->material, "%s", n > 11 ? f[11] : "flat"); r->nparts++;
        } else if (!strcmp(f[0], "ANIM")) {
            if (n < 3 || r->nanims >= BVA_MAX_ANIMS) return -ln; BvaAnim *a = &r->anim[r->nanims]; snprintf(a->name, sizeof a->name, "%s", f[1]); a->length_ms = (float)atof(f[2]); if (a->length_ms <= 0) return -ln;
            a->loop = n > 3 && (!strcmp(f[3], "loop") || atoi(f[3]) == 1); r->nanims++;
        } else if (!strcmp(f[0], "TRACK")) {
            if (n < 6 || r->ntracks >= BVA_MAX_TRACKS) return -ln; BvaTrack *t = &r->track[r->ntracks]; t->anim = bva_find_anim(r, f[1]); t->part = bva_find_part(r, f[2]); t->ch = -1;
            for (int c = 0; c < BVA_NCH; c++) if (!strcmp(f[3], bva_ch_name[c])) t->ch = c;
            if (t->anim < 0 || t->part < 0 || t->ch < 0) return -ln; t->t = (float)atof(f[4]); t->v = (float)atof(f[5]); t->ease = 0;
            if (n > 6) { t->ease = -1; for (int e = 0; e < 5; e++) if (!strcmp(f[6], bva_ease_name[e])) t->ease = e; if (t->ease < 0) return -ln; }
            r->ntracks++;
        }       /* any other row type: ignored on purpose */
    }
    qsort(r->track, (size_t)r->ntracks, sizeof(BvaTrack), bva_track_cmp);
    return 0;
}

static float bva_ease(int e, float u) {
    switch (e) { case 1: return 0.5f * (1.f - cosf(3.14159265f * u)); case 2: return 0.f; case 3: return u * u; case 4: return 1.f - (1.f - u) * (1.f - u); default: return u; }
}
/* The value of one channel of one part at time t_ms: `base` when no track names it. */
static float bva_channel(const BvaRig *r, int anim, int part, int ch, float t_ms, float base) {
    if (anim < 0) return base; const BvaAnim *a = &r->anim[anim]; float t = t_ms;
    if (a->loop) { t = fmodf(t, a->length_ms); if (t < 0) t += a->length_ms; } else if (t > a->length_ms) t = a->length_ms; if (t < 0) t = 0;
    const BvaTrack *k[BVA_MAX_TRACKS]; int nk = 0;
    for (int i = 0; i < r->ntracks; i++) if (r->track[i].anim == anim && r->track[i].part == part && r->track[i].ch == ch) k[nk++] = &r->track[i];
    if (!nk) return base; if (nk == 1 || t <= k[0]->t) return k[0]->v;
    for (int i = 0; i < nk; i++) {
        float t1, v1; if (i + 1 < nk) { t1 = k[i + 1]->t; v1 = k[i + 1]->v; } else if (a->loop) { t1 = a->length_ms; v1 = k[0]->v; } else return k[i]->v;
        if (t >= k[i]->t && t < t1) return k[i]->v + (v1 - k[i]->v) * bva_ease(k[i]->ease, (t - k[i]->t) / (t1 - k[i]->t));
    }
    return k[nk - 1]->v;
}
static void bva_mul(const float *a, const float *b, float *o) { float t[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j]; memcpy(o, t, sizeof t); }
static void bva_euler(float rx, float ry, float rz, float *m) {      /* Rz*Ry*Rx, degrees */
    float d = 3.14159265f / 180.f, cx = cosf(rx * d), sx = sinf(rx * d), cy = cosf(ry * d), sy = sinf(ry * d), cz = cosf(rz * d), sz = sinf(rz * d);
    float X[9] = { 1, 0, 0, 0, cx, -sx, 0, sx, cx }, Y[9] = { cy, 0, sy, 0, 1, 0, -sy, 0, cy }, Z[9] = { cz, -sz, 0, sz, cz, 0, 0, 0, 1 }, t[9];
    bva_mul(Z, Y, t); bva_mul(t, X, m);
}
/* Evaluate every part at t_ms of animation `anim` (-1 = the rest pose). out[i] belongs to part i. */
static void bva_eval(const BvaRig *r, int anim, float t_ms, BvaPose *out) {
    for (int i = 0; i < r->nparts; i++) {
        const BvaPart *p = &r->part[i]; float v[BVA_NCH] = { p->pos[0], p->pos[1], p->pos[2], p->rot[0], p->rot[1], p->rot[2], p->scale[0], p->scale[1], p->scale[2], (float)p->alpha };
        for (int c = 0; c < BVA_NCH; c++) v[c] = bva_channel(r, anim, i, c, t_ms, v[c]);
        float R[9], S[9] = { v[6], 0, 0, 0, v[7], 0, 0, 0, v[8] }, L[9]; bva_euler(v[3], v[4], v[5], R); bva_mul(R, S, L);
        int pa = p->parent;
#ifdef BV_RIG_ANIM_MUTANT_NO_PARENT
        pa = -1;
#endif
        BvaPose *o = &out[i];
        if (pa < 0) { memcpy(o->m, L, sizeof L); memcpy(o->p, v, 3 * sizeof(float)); o->alpha = v[9]; }
        else { const BvaPose *q = &out[pa]; bva_mul(q->m, L, o->m);
            for (int k = 0; k < 3; k++) o->p[k] = q->p[k] + q->m[k * 3] * v[0] + q->m[k * 3 + 1] * v[1] + q->m[k * 3 + 2] * v[2];
            o->alpha = v[9] * q->alpha / 255.f; }
        for (int k = 0; k < 3; k++) { o->lo[k] = 1e30f; o->hi[k] = -1e30f; }
        for (int c = 0; c < 8; c++) { float cr[3] = { (c & 1) ? p->size[0] - p->pivot[0] : -p->pivot[0], (c & 2) ? p->size[1] - p->pivot[1] : -p->pivot[1], (c & 4) ? p->size[2] - p->pivot[2] : -p->pivot[2] };
            for (int k = 0; k < 3; k++) { float w = o->p[k] + o->m[k * 3] * cr[0] + o->m[k * 3 + 1] * cr[1] + o->m[k * 3 + 2] * cr[2]; if (w < o->lo[k]) o->lo[k] = w; if (w > o->hi[k]) o->hi[k] = w; } }
    }
}
#endif
