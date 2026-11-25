#ifndef BV_RIG_H
#define BV_RIG_H
/* Box-limb rig (BOOK 16.game/BOX-LIMB-RIG-FOR-WALKING-CHARACTERS-2026-11-25.md). A map event whose charset name starts with `rig:` is drawn as six axis-aligned boxes
 * (head, torso, right arm, left arm, right leg, left leg) instead of one extruded sprite. Pure functions; the harness compiles this header alone (bv_rig_op.c).
 *
 * Boxes cannot rotate in this raymarcher, so a swing is NOT an angle: the limb box is OFFSET along the facing axis, stepped (a walk is two poses, a swing is two poses).
 * Sizes are Minecraft's, in 1/16: head 8x8x8, torso 8x12x4, arms 4x12x4, legs 4x12x4; 32/16 = 2 blocks, scaled by BV_RIG_SCALE so the figure stands 1.8 blocks.
 * Output: out[i] = {x0,y0,z0,x1,y1,z1} in block units, x/z relative to the cell centre (x east, z south = board row), y up from the feet.
 * Order: 0 head, 1 torso, 2 right arm, 3 left arm, 4 right leg, 5 left leg. Left/right are the FIGURE's.
 * direction: 2 south, 4 west, 6 east, 8 north (RPG Maker numpad); 0 south, 1 west, 3 north also accepted (2 is south in both readings).
 * pattern: 0 idle, 1/2 walk (pose = (frame + (pattern==2)) & 1), 3 jump, 4 mine / swing (right arm), 5 place (both arms), anything else idle. frame: any counter. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define BV_RIG_SCALE (0.9f / 16.0f)
static int bv_rig_is(const char *charset) { return charset && !strncmp(charset, "rig:", 4) && charset[4]; }
static void bv_rig_facing(int direction, int *fx, int *fz) {
    switch (direction) { case 4: case 1: *fx = -1; *fz = 0; break; case 6: *fx = 1; *fz = 0; break; case 8: case 3: *fx = 0; *fz = -1; break; default: *fx = 0; *fz = 1; break; }
}
/* Extra parts, only when the RIG file has the row (mask bits: 1 hair, 2 sleeve, 4 shoe). Colour index per part (ci): 0 head 1 torso 2 arm 3 leg 4 face 5 hair 6 sleeve 7 shoe.
 * Order: 0..5 as above, then hair cap (top of the head) + hair back (slab behind the head), sleeves (upper half of each arm), shoes (bottom quarter of each leg). Total <= 12. */
#define BV_RIG_MAX 20
#define BV_RIG_HAIR 1
#define BV_RIG_SLEEVE 2
#define BV_RIG_SHOE 4
#define BV_RIG_FACE 8      /* RIG row `eyes`: eyes, brows and mouth as thin boxes on the front of the head */
static void bv_rig_put(int rx, int rz, int fx, int fz, float lat, float fwd, float hl, float hf, float ya, float yb, float *o) {
    float cx = (rx * lat + fx * fwd) * BV_RIG_SCALE, cz = (rz * lat + fz * fwd) * BV_RIG_SCALE;
    float ex = ((rx < 0 ? -rx : rx) * hl + (fx < 0 ? -fx : fx) * hf) * BV_RIG_SCALE, ez = ((rz < 0 ? -rz : rz) * hl + (fz < 0 ? -fz : fz) * hf) * BV_RIG_SCALE;
    o[0] = cx - ex; o[1] = ya * BV_RIG_SCALE; o[2] = cz - ez; o[3] = cx + ex; o[4] = yb * BV_RIG_SCALE; o[5] = cz + ez;
}
/* part_set 0 = not a rig: returns 0 boxes and writes nothing (every other board is unchanged). Else returns 6 + the extra parts the mask asks for (<= BV_RIG_MAX). */
static int bv_rig_boxes_ex(int part_set, int mask, int direction, int pattern, int frame, float out[BV_RIG_MAX][6], int ci[BV_RIG_MAX]) {
    if (!part_set) return 0;
    int fx, fz; bv_rig_facing(direction, &fx, &fz);
    int rx = -fz, rz = fx;                                   /* the figure's right hand: south -> west, east -> south */
    int pose = frame & 1;
    /* per part, in 1/16: lateral centre (+ = figure's right), forward centre, y0, lateral half, forward half, y1 */
    float lat[6] = { 0, 0, -6, 6, -2, 2 }, fwd[6] = { 0, 0, 0, 0, 0, 0 };
    float y0[6] = { 24, 12, 12, 12, 0, 0 }, y1[6] = { 32, 24, 24, 24, 12, 12 };
    float hl[6] = { 4, 4, 2, 2, 2, 2 }, hf[6] = { 4, 2, 2, 2, 2, 2 };
    lat[2] = 6; lat[3] = -6;                                 /* +lat = figure's right: arm 2 is the RIGHT arm */
    lat[4] = 2; lat[5] = -2;
    if (pattern == 1 || pattern == 2) {
        int sw = (((frame + (pattern == 2)) & 1) ? -1 : 1);  /* legs one way, arms the other */
        fwd[4] = 3.f * sw; fwd[5] = -3.f * sw; fwd[2] = -3.f * sw; fwd[3] = 3.f * sw;
        if (sw < 0) for (int i = 0; i < 4; i++) { y0[i] += 1; y1[i] += 1; }   /* body bob 1/16 on the second pose */
    } else if (pattern == 3) {                               /* jump: whole body 4 up, legs together, arms raised beside the head */
        for (int i = 0; i < 6; i++) { y0[i] += 4; y1[i] += 4; }
        lat[4] = 1; lat[5] = -1; y0[2] += 12; y1[2] += 12; y0[3] += 12; y1[3] += 12;
    } else if (pattern == 4) {                               /* mine / swing: right arm forward and up, two poses by frame */
        fwd[2] = pose ? 7.f : 5.f; y0[2] += pose ? 4 : 2; y1[2] += pose ? 4 : 2;
    } else if (pattern == 5) {                               /* place: both arms forward */
        fwd[2] = fwd[3] = 6.f; y0[2] += 2; y1[2] += 2; y0[3] += 2; y1[3] += 2;
    }
    for (int i = 0; i < 6; i++) {
        float cx = (rx * lat[i] + fx * fwd[i]) * BV_RIG_SCALE, cz = (rz * lat[i] + fz * fwd[i]) * BV_RIG_SCALE;
        float ex = ((rx < 0 ? -rx : rx) * hl[i] + (fx < 0 ? -fx : fx) * hf[i]) * BV_RIG_SCALE;
        float ez = ((rz < 0 ? -rz : rz) * hl[i] + (fz < 0 ? -fz : fz) * hf[i]) * BV_RIG_SCALE;
        out[i][0] = cx - ex; out[i][1] = y0[i] * BV_RIG_SCALE; out[i][2] = cz - ez;
        out[i][3] = cx + ex; out[i][4] = y1[i] * BV_RIG_SCALE; out[i][5] = cz + ez;
    }
#ifdef BV_RIG_MUTANT_LEGS_IN_PHASE      /* the harness mutant: both legs swing the same way (must turn the walk check red) */
    if (pattern == 1 || pattern == 2) { out[5][2] = out[4][2]; out[5][5] = out[4][5]; out[5][0] = out[4][0]; out[5][3] = out[4][3]; }
#endif
    static const int base_ci[6] = { 0, 1, 2, 2, 3, 3 };
    for (int i = 0; i < 6; i++) ci[i] = base_ci[i];
    int n = 6;
    if (mask & BV_RIG_HAIR) {                                /* cap: the top 2/16 of the head, a hair wider; back: a slab behind the head down to the cap */
        bv_rig_put(rx, rz, fx, fz, lat[0], fwd[0], hl[0] + 0.5f, hf[0] + 0.5f, y1[0] - 2.f, y1[0] + 0.5f, out[n]); ci[n++] = 5;
        bv_rig_put(rx, rz, fx, fz, lat[0], fwd[0] - hf[0] + 0.25f, hl[0] + 0.5f, 0.75f, y0[0], y1[0] - 1.5f, out[n]); ci[n++] = 5;
    }
    if (mask & BV_RIG_SLEEVE) for (int i = 2; i < 4; i++) { bv_rig_put(rx, rz, fx, fz, lat[i], fwd[i], hl[i] + 0.3f, hf[i] + 0.3f, (y0[i] + y1[i]) / 2.f, y1[i] + 0.2f, out[n]); ci[n++] = 6; }
    if (mask & BV_RIG_SHOE) for (int i = 4; i < 6; i++) { bv_rig_put(rx, rz, fx, fz, lat[i], fwd[i], hl[i] + 0.3f, hf[i] + 0.3f, y0[i] - 0.1f, y0[i] + 3.f, out[n]); ci[n++] = 7; }
    if (mask & BV_RIG_FACE) {                               /* face on the FRONT of the head (units 1/16): eyes = sclera outside + iris inside, brows in the hair colour, a darker mouth. Thin boxes, no texture. */
        float fz0 = fwd[0] + hf[0] + 0.12f, hd = 0.15f, yb = y0[0];
#ifdef BV_RIG_MUTANT_FACE_BEHIND      /* the harness mutant: the face is drawn on the BACK of the head (must turn the in_front check red) */
        fz0 = fwd[0] - hf[0] - 0.12f;
#endif
        for (int s = -1; s <= 1; s += 2) {
            bv_rig_put(rx, rz, fx, fz, lat[0] + s * 2.5f, fz0, 0.5f, hd, yb + 3.0f, yb + 4.0f, out[n]); ci[n++] = 9;
            bv_rig_put(rx, rz, fx, fz, lat[0] + s * 1.5f, fz0 + 0.05f, 0.5f, hd, yb + 3.0f, yb + 4.0f, out[n]); ci[n++] = 8;
            bv_rig_put(rx, rz, fx, fz, lat[0] + s * 2.2f, fz0, 1.3f, hd, yb + 4.5f, yb + 5.1f, out[n]); ci[n++] = 5;
        }
        bv_rig_put(rx, rz, fx, fz, lat[0], fz0, 1.5f, hd, yb + 1.0f, yb + 1.8f, out[n]); ci[n++] = 10;
    }
    return n;
}
/* the original 6-box call (old files, the bv_rig harness) */
static int __attribute__((unused)) bv_rig_boxes(int part_set, int direction, int pattern, int frame, float out[6][6]) {
    float o[BV_RIG_MAX][6]; int ci[BV_RIG_MAX];
    int n = bv_rig_boxes_ex(part_set, 0, direction, pattern, frame, o, ci);
    for (int i = 0; i < n; i++) memcpy(out[i], o[i], sizeof o[i]);
    return n;
}
/* RIG file rig_<name>.pdl beside the desk: `RIG | head|torso|arm|leg|face|hair|sleeve|shoe | r g b`. col[0..7] in that order; *mask gets bits 1 hair, 2 sleeve, 4 shoe for the rows present;
 * a missing part keeps the caller's fallback. Returns rows read. */
static int __attribute__((unused)) bv_rig_parse(const char *path, unsigned char col[12][3], int *mask) {
    static const char *nm[9] = { "head", "torso", "arm", "leg", "face", "hair", "sleeve", "shoe", "eyes" };
    if (mask) *mask = 0;
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[256]; int n = 0;
    while (fgets(line, sizeof line, f)) {
        char a[16], b[16]; int r, g, bl;
        if (sscanf(line, " %15[^| ] | %15[^| ] | %d %d %d", a, b, &r, &g, &bl) != 5 || strcmp(a, "RIG")) continue;
        for (int i = 0; i < 9; i++) if (!strcmp(b, nm[i])) { col[i][0] = (unsigned char)(r & 255); col[i][1] = (unsigned char)(g & 255); col[i][2] = (unsigned char)(bl & 255); n++; if (mask && i >= 5) *mask |= 1 << (i - 5); }
    }
    fclose(f); return n;
}
/* Cached per (dir, name), refreshed once a second: a frame is a few ms, a file open is not. fallback = the event row's r g b. */
static void __attribute__((unused)) bv_rig_colors(const char *dir, const char *charset, const int fb[3], unsigned char col[12][3], int *mask) {
    static struct { char key[4600]; time_t t; unsigned char col[12][3]; int mask; } c[8]; static int next = 0;
    char key[4600]; snprintf(key, sizeof key, "%s/%s", dir, charset); time_t now = time(NULL); int s = -1;
    for (int i = 0; i < 8; i++) if (!strcmp(c[i].key, key)) s = i;
    if (s < 0 || c[s].t != now) {
        if (s < 0) { s = next; next = (next + 1) & 7; snprintf(c[s].key, sizeof c[s].key, "%s", key); }
        for (int i = 0; i < 12; i++) { c[s].col[i][0] = (unsigned char)fb[0]; c[s].col[i][1] = (unsigned char)fb[1]; c[s].col[i][2] = (unsigned char)fb[2]; }
        char p[4700]; snprintf(p, sizeof p, "%s/rig_%s.pdl", dir, charset + 4); bv_rig_parse(p, c[s].col, &c[s].mask); c[s].t = now;
        for (int k = 0; k < 3; k++) { c[s].col[9][k] = 240; c[s].col[10][k] = (unsigned char)(c[s].col[0][k] * 60 / 100); }      /* sclera white, mouth = skin darkened */
    }
    memcpy(col, c[s].col, sizeof c[s].col); if (mask) *mask = c[s].mask;
}
/* ---- SKIN: voxel models for the limbs (owner 2026-11-25: "didn't we design so we could scale smaller voxels to render? ... more than one colour as a pattern of the sprite ... create it before render").
 * rig_<name>.skin.csv (written by rig_from_charset_op: `part,face,u,v,r,g,b`) gives every surface voxel of every limb a colour. Each limb is ONE box (one slot of the box list) carrying a dense voxel model
 * (the shader maps the grid onto the box, so the box size IS the voxel scale: 8x8x8 voxels in a 0.45 block head = Minecraft's 1/16 pixels). Parts in box order: head, torso, rarm, larm, rleg, lleg; faces F B L R T D.
 * A model is built once per (skin, part, facing) and cached; a skin file is re-read at most once a second. Pure functions: the harness compiles this header alone. */
#include <math.h>
typedef struct { unsigned char c[6][6][12][8][3]; int rows; } BvRigSkin;
static const int BV_RIG_PD[6][3] = { { 8, 8, 8 }, { 8, 12, 4 }, { 4, 12, 4 }, { 4, 12, 4 }, { 4, 12, 4 }, { 4, 12, 4 } };   /* w, h, d */
static int __attribute__((unused)) bv_rig_skin_parse(const char *path, BvRigSkin *s) {
    static const char *pn[6] = { "head", "torso", "rarm", "larm", "rleg", "lleg" }; static const char fn[7] = "FBLRTD";
    memset(s, 0, sizeof *s); FILE *f = fopen(path, "r"); if (!f) return 0; char line[128]; int n = 0;
    while (fgets(line, sizeof line, f)) { char p[16], fc; int u, v, r, g, b; if (sscanf(line, " %15[^,],%c,%d,%d,%d,%d,%d", p, &fc, &u, &v, &r, &g, &b) != 7) continue;
        int pi = -1, fi = -1; for (int i = 0; i < 6; i++) if (!strcmp(p, pn[i])) pi = i; for (int i = 0; i < 6; i++) if (fc == fn[i]) fi = i;
        if (pi < 0 || fi < 0 || u < 0 || u >= 8 || v < 0 || v >= 12) continue;
        s->c[pi][fi][v][u][0] = (unsigned char)(r & 255); s->c[pi][fi][v][u][1] = (unsigned char)(g & 255); s->c[pi][fi][v][u][2] = (unsigned char)(b & 255); n++; }
    fclose(f); s->rows = n; return n;
}
/* the dense grid of one limb for one facing, compact: out[((z * dim[1] + y) * dim[0] + x) * 4] = r g b a (a 255 = solid, 0 = empty). The figure's frame: wi runs from the figure's RIGHT to its LEFT, di from the FRONT to the BACK,
 * vi from the top; x east, z south, y up. out must hold 16 * 16 * 16 * 4 bytes. Returns the voxel count. */
static int __attribute__((unused)) bv_rig_skin_build(const BvRigSkin *s, int part, int fx, int fz, int dim[3], unsigned char *out) {
    int w = BV_RIG_PD[part][0], h = BV_RIG_PD[part][1], d = BV_RIG_PD[part][2], rx = -fz, rz = fx, GX = fx ? d : w, GZ = fx ? w : d, n = 0;
    dim[0] = GX; dim[1] = h; dim[2] = GZ; memset(out, 0, (size_t)GX * (size_t)h * (size_t)GZ * 4);
    for (int vi = 0; vi < h; vi++) for (int di = 0; di < d; di++) for (int wi = 0; wi < w; wi++) {
        if (!(wi == 0 || wi == w - 1 || di == 0 || di == d - 1 || vi == 0 || vi == h - 1)) continue;            /* a closed shell is enough: the ray meets the surface first */
        int fc, u, v;
        if (di == 0) { fc = 0; u = wi; v = vi; } else if (di == d - 1) { fc = 1; u = w - 1 - wi; v = vi; }
        else if (wi == w - 1) { fc = 2; u = di; v = vi; } else if (wi == 0) { fc = 3; u = d - 1 - di; v = vi; }
        else { fc = vi == 0 ? 4 : 5; u = wi; v = di; }
        float lat = (float)wi - (w - 1) / 2.f, fwd = (d - 1) / 2.f - (float)di, wx = -rx * lat + fx * fwd, wz = -rz * lat + fz * fwd;
        int gx = (int)floorf(wx + (GX - 1) / 2.f + 0.5f), gz = (int)floorf(wz + (GZ - 1) / 2.f + 0.5f), gy = h - 1 - vi; if (gx < 0 || gx >= GX || gz < 0 || gz >= GZ) continue;
        unsigned char *o = out + (((size_t)gz * (size_t)h + (size_t)gy) * (size_t)GX + (size_t)gx) * 4; const unsigned char *c = s->c[part][fc][v][u];
        o[0] = c[0]; o[1] = c[1]; o[2] = c[2]; o[3] = 255; n++; }
    return n;
}
/* Cached per (dir, name): the parsed skin and its 6 x 4 models, refreshed once a second. Returns NULL when there is no rig_<name>.skin.csv (the caller keeps the flat-colour rig). */
typedef struct { int dim[3], built; unsigned char vox[16 * 16 * 16 * 4]; } BvRigModel;
static const BvRigSkin * __attribute__((unused)) bv_rig_skin_get(const char *dir, const char *charset, int fx, int fz, int part, const unsigned char **vox, int dim[3]) {
    static struct { char key[4600]; time_t t; int has; BvRigSkin sk; BvRigModel m[6][4]; } c[4]; static int next = 0;
    char key[4600]; snprintf(key, sizeof key, "%s/%s", dir, charset); time_t now = time(NULL); int s = -1;
    for (int i = 0; i < 4; i++) if (!strcmp(c[i].key, key)) s = i;
    if (s < 0 || c[s].t != now) {
        if (s < 0) { s = next; next = (next + 1) & 3; snprintf(c[s].key, sizeof c[s].key, "%s", key); c[s].has = 0; c[s].t = 0; }
        char p[4700]; snprintf(p, sizeof p, "%s/rig_%s.skin.csv", dir, charset + 4); BvRigSkin t; int had = c[s].has; int n = bv_rig_skin_parse(p, &t);
        if (n > 0 && (!had || memcmp(&t, &c[s].sk, sizeof t))) { c[s].sk = t; c[s].has = 1; for (int a = 0; a < 6; a++) for (int b = 0; b < 4; b++) c[s].m[a][b].built = 0; } else if (n <= 0) c[s].has = 0;
        c[s].t = now; }
    if (!c[s].has) return NULL;
    int fi = fx == 1 ? 3 : fx == -1 ? 1 : fz == -1 ? 2 : 0; BvRigModel *m = &c[s].m[part][fi];
    if (!m->built) { bv_rig_skin_build(&c[s].sk, part, fx, fz, m->dim, m->vox); m->built = 1; }
    *vox = m->vox; dim[0] = m->dim[0]; dim[1] = m->dim[1]; dim[2] = m->dim[2]; return &c[s].sk;
}
#endif
