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
/* part_set 0 = not a rig: returns 0 boxes and writes nothing (every other board is unchanged). Else returns 6. */
static int bv_rig_boxes(int part_set, int direction, int pattern, int frame, float out[6][6]) {
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
    return 6;
}
/* RIG file rig_<name>.pdl beside the desk: `RIG | head|torso|arm|leg|face | r g b`. col[0..4] = head torso arm leg face; a missing part keeps the caller's fallback. Returns rows read. */
static int __attribute__((unused)) bv_rig_parse(const char *path, unsigned char col[5][3]) {
    static const char *nm[5] = { "head", "torso", "arm", "leg", "face" };
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char line[256]; int n = 0;
    while (fgets(line, sizeof line, f)) {
        char a[16], b[16]; int r, g, bl;
        if (sscanf(line, " %15[^| ] | %15[^| ] | %d %d %d", a, b, &r, &g, &bl) != 5 || strcmp(a, "RIG")) continue;
        for (int i = 0; i < 5; i++) if (!strcmp(b, nm[i])) { col[i][0] = (unsigned char)(r & 255); col[i][1] = (unsigned char)(g & 255); col[i][2] = (unsigned char)(bl & 255); n++; }
    }
    fclose(f); return n;
}
/* Cached per (dir, name), refreshed once a second: a frame is a few ms, a file open is not. fallback = the event row's r g b. */
static void __attribute__((unused)) bv_rig_colors(const char *dir, const char *charset, const int fb[3], unsigned char col[5][3]) {
    static struct { char key[4600]; time_t t; unsigned char col[5][3]; } c[8]; static int next = 0;
    char key[4600]; snprintf(key, sizeof key, "%s/%s", dir, charset); time_t now = time(NULL); int s = -1;
    for (int i = 0; i < 8; i++) if (!strcmp(c[i].key, key)) s = i;
    if (s < 0 || c[s].t != now) {
        if (s < 0) { s = next; next = (next + 1) & 7; snprintf(c[s].key, sizeof c[s].key, "%s", key); }
        for (int i = 0; i < 5; i++) { c[s].col[i][0] = (unsigned char)fb[0]; c[s].col[i][1] = (unsigned char)fb[1]; c[s].col[i][2] = (unsigned char)fb[2]; }
        char p[4700]; snprintf(p, sizeof p, "%s/rig_%s.pdl", dir, charset + 4); bv_rig_parse(p, c[s].col); c[s].t = now;
    }
    memcpy(col, c[s].col, sizeof c[s].col);
}
#endif
