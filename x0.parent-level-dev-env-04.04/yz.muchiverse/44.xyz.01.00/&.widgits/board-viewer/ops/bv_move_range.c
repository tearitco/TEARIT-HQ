/* bv_move_range.c - TEXT-INCLUDED (not compiled on its own, not in
 * build.sh's list) by bv_render_3d.c, bv_render_2d.c and
 * bv_menu_input.c. Pure file helpers, no state - house convention for
 * pure multi-consumer logic (see the no-header-link-split rule).
 *
 * The pc-hq "Move range finder", mirroring the desk's placer range:
 *
 *   <project>/pieces/display/move_range_matrix.txt   '#' = in range,
 *       rows centred on the origin cell. PRESENT = the range finder is
 *       OPEN; ABSENT = nothing is drawn anywhere. This one file is the
 *       only switch. Written by move_entity_on_desk.sh (the "Move"
 *       verb), deleted by bvr_close() on Esc or a confirmed pick.
 *   <project>/pieces/display/move_range_entity.txt   entity=<id>, the
 *       piece Move was chosen for (so Enter knows who to relocate).
 *
 * Origin cell = the entity Move was chosen for (xelector only as a
 * fallback). Both renderers and the confirm step
 * use the SAME origin and the SAME bvr_has() test, so what you see is
 * what Enter accepts.
 *
 * 3D is the real thing, 2D is a render of it: see
 * @.apps/piececraft-hq/RENDER-STANDARD.md. */
#ifndef BV_MOVE_RANGE_C
#define BV_MOVE_RANGE_C
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../../_shared-lib/khtpm_grid_jump.c"   /* shared cell-ref parser (csv-hq <grid>, tp_arm_placer) */

/* The matrix, the in-range test, path planning and the waypoint queue are the
 * SHARED khtpm_move_range.c - the exact code the desk's placer and
 * move_entity_init/_tick use. This file is only pc-hq's glue: WHERE the files
 * live (the real project's pieces/) and HOW the result is applied (cells in
 * pieces/<id>/state.txt, stepped by bv_dispatch). */
#include "../../_shared-lib/khtpm_move_range.c"
typedef MvrMatrix BvRange;

static __attribute__((unused)) void bvr_path(const char *root, const char *name, char *out, size_t n) {
    snprintf(out, n, "%s/pieces/display/%s", root, name);
}

/* 1 = range finder open and matrix loaded; 0 = closed. */
static __attribute__((unused)) int bvr_load(const char *root, BvRange *r) {
    char p[4400];
    bvr_path(root, "move_range_matrix.txt", p, sizeof(p));
    return mvr_matrix_load(p, r);
}

/* dx,dy = cell offset from the origin cell. */
static __attribute__((unused)) int bvr_has(const BvRange *r, int dx, int dy) {
    return r->nr > 0 && mvr_matrix_allows(r, dx, dy);
}

/* Esc / confirmed pick: closes the range finder everywhere at once. */
static __attribute__((unused)) void bvr_close(const char *root) {
    char p[4400];
    bvr_path(root, "move_range_matrix.txt", p, sizeof(p)); unlink(p);
    bvr_path(root, "move_range_entity.txt", p, sizeof(p)); unlink(p);
}

/* Tiny kv reader (key=value lines) so this include stays standalone. */
static __attribute__((unused)) int bvr_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    char line[256]; size_t kl = strlen(key);
    int v = def;
    if (!f) return def;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, kl) && line[kl] == '=') { v = atoi(line + kl + 1); break; }
    fclose(f);
    return v;
}

/* Origin cell of the range: the ENTITY Move was chosen for (the hero you
 * activated) - the range is built around it, like the desk's placer is
 * built around the entity. Falls back to the xelector only when no entity
 * is recorded. Returns 1 and fills x,y,z, else 0. */
static __attribute__((unused)) int bvr_origin(const char *real_root, int *x, int *y, int *z) {
    char p[4400], ep[4400], ent[64] = "";
    bvr_path(real_root, "move_range_entity.txt", ep, sizeof(ep));
    FILE *f = fopen(ep, "r");
    if (f) { char l[128]; while (fgets(l, sizeof(l), f)) if (!strncmp(l, "entity=", 7)) { snprintf(ent, sizeof(ent), "%.63s", l + 7); ent[strcspn(ent, "\r\n")] = 0; } fclose(f); }
    if (ent[0]) {
        snprintf(p, sizeof(p), "%s/pieces/%s/state.txt", real_root, ent);
        if (bvr_kv_int(p, "pos_x", -9999) != -9999) {
            *x = bvr_kv_int(p, "pos_x", 0); *y = bvr_kv_int(p, "pos_y", 0); *z = bvr_kv_int(p, "pos_z", 0);
            return 1;
        }
    }
    snprintf(p, sizeof(p), "%s/pieces/xelector_01/state.txt", real_root);
    if (bvr_kv_int(p, "pos_x", -9999) == -9999) return 0;
    *x = bvr_kv_int(p, "pos_x", 0); *y = bvr_kv_int(p, "pos_y", 0); *z = bvr_kv_int(p, "pos_z", 0);
    return 1;
}

/* Move just opened: arm the green placer on the origin so the arrows
 * have something to move (the desk's tp_arm_placer does the same).
 * Idempotent; a no-op once armed. placer_path is the session's
 * pieces/display/placer.txt. */
static __attribute__((unused)) void bvr_arm_placer(const char *placer_path, const char *real_root) {
    int x, y, z;
    if (bvr_kv_int(placer_path, "armed", 0)) return;
    if (!bvr_origin(real_root, &x, &y, &z)) return;
    FILE *f = fopen(placer_path, "w");
    if (f) { fprintf(f, "armed=1\nx=%d\ny=%d\nz=%d\n", x, y, z); fclose(f); }
}

/* HUD label for the open range finder, mirroring the desk placer's pill:
 *   l1 = "move <entity> -> <ref> (x,y,z)"     l2 = "jump: c7_" or the key hints.
 * <ref> is the same column-letters + 1-based-row ref the jump accepts. */
static __attribute__((unused)) void bvr_label(const char *real_root, const char *sess_root,
                                              char *l1, size_t n1, char *l2, size_t n2) {
    char p[4400], ent[64] = "", ref[16] = "", buf[GJ_BUF_CAP] = "";
    bvr_path(real_root, "move_range_entity.txt", p, sizeof(p));
    FILE *f = fopen(p, "r");
    if (f) { char l[128]; while (fgets(l, sizeof(l), f)) if (!strncmp(l, "entity=", 7)) { snprintf(ent, sizeof(ent), "%.63s", l + 7); ent[strcspn(ent, "\r\n")] = 0; } fclose(f); }
    snprintf(p, sizeof(p), "%s/pieces/display/placer.txt", sess_root);
    int x = bvr_kv_int(p, "x", 0), y = bvr_kv_int(p, "y", 0), z = bvr_kv_int(p, "z", 0);
    gj_col_to_letters(x, ref, sizeof(ref));
    snprintf(p, sizeof(p), "%s/pieces/display/move_jump.txt", sess_root);
    f = fopen(p, "r");
    if (f) { if (fgets(buf, sizeof(buf), f)) buf[strcspn(buf, "\r\n")] = 0; fclose(f); }
    snprintf(l1, n1, "move %s -> %s%d (%d,%d,%d)", ent[0] ? ent : "?", ref, y + 1, x, y, z);
    if (buf[0]) snprintf(l2, n2, "jump: %s_", buf);
    else snprintf(l2, n2, "arrows move | ref+Enter jump | Enter place | Esc cancel");
}

/* ---- Move animation: the desk's waypoint queue, in cells ----------------
 * bvr_plan() writes <entity>/animation_queue.txt (+ .cursor) with the shared
 * planner (step 1 cell) and notes the active entity in
 * pieces/display/move_active.txt; bvr_step() - called every tick by
 * bv_dispatch - takes one waypoint per BVR_STEP_MS and applies it to the
 * entity's pieces/<id>/state.txt. On the desk the same queue files are drained
 * by move_entity_tick.+x into desktop_pos.txt. */
#define BVR_STEP_MS 90   /* same cadence as move_entity.pal's sleep 90000 */

/* Replace or append key=value in a kv file (small files only). */
static __attribute__((unused)) void bvr_kv_set(const char *path, const char *key, int val) {
    char lines[64][128]; int n = 0, found = 0; size_t kl = strlen(key);
    FILE *f = fopen(path, "r");
    if (f) { while (n < 64 && fgets(lines[n], sizeof(lines[0]), f)) n++; fclose(f); }
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) {
        if (!strncmp(lines[i], key, kl) && lines[i][kl] == '=') { fprintf(f, "%s=%d\n", key, val); found = 1; }
        else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%d\n", key, val);
    fclose(f);
}

static __attribute__((unused)) void bvr_plan(const char *real_root, const char *ent,
                                             int x0, int y0, int z0, int x1, int y1, int z1) {
    char p[4400], dir[4400];
    int pts[256][2], n;
    snprintf(dir, sizeof(dir), "%s/pieces/%s", real_root, ent);
    n = mvr_path(x0, y0, x1, y1, 1, pts, 256);
    if (n == 0) { pts[0][0] = x1; pts[0][1] = y1; n = 1; }   /* same cell: still apply z */
    if (!mvr_queue_write(dir, pts, n, z0, z1)) return;
    bvr_path(real_root, "move_active.txt", p, sizeof(p));
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "entity=%s\n", ent); fclose(f); }
}

/* One tick: if a queue is pending and BVR_STEP_MS has passed (stamp_path holds
 * the last step's monotonic ms), move the entity one waypoint. Returns 1 when
 * something moved (caller redraws), 0 otherwise. */
static __attribute__((unused)) int bvr_step(const char *real_root, const char *stamp_path) {
    char ap[4400], dir[4400], sp[4400], ent[64] = "";
    FILE *f;
    bvr_path(real_root, "move_active.txt", ap, sizeof(ap));
    if (!(f = fopen(ap, "r"))) return 0;
    { char l[128]; while (fgets(l, sizeof(l), f)) if (!strncmp(l, "entity=", 7)) { snprintf(ent, sizeof(ent), "%.63s", l + 7); ent[strcspn(ent, "\r\n")] = 0; } }
    fclose(f);
    if (!ent[0]) { unlink(ap); return 0; }
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long long now = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000, last = 0;
    if ((f = fopen(stamp_path, "r"))) { if (fscanf(f, "%lld", &last) != 1) last = 0; fclose(f); }
    if (now - last < BVR_STEP_MS) return 0;
    snprintf(dir, sizeof(dir), "%s/pieces/%s", real_root, ent);
    int x, y, z, ln, moved = 0;
    if (mvr_queue_next(dir, &x, &y, &z, &ln)) {
        snprintf(sp, sizeof(sp), "%s/state.txt", dir);
        bvr_kv_set(sp, "pos_x", x); bvr_kv_set(sp, "pos_y", y);
        if (z >= 0) bvr_kv_set(sp, "pos_z", z);
        mvr_queue_advance(dir, ln);
        moved = 1;
    } else {                      /* drained: same cleanup the desk's tick does */
        mvr_queue_clear(dir);
        unlink(ap);
    }
    if ((f = fopen(stamp_path, "w"))) { fprintf(f, "%lld\n", now); fclose(f); }
    return moved;
}
#endif
