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
 * Origin cell = the xelector (the range finder's cursor) when it
 * exists, else the entity itself. Both renderers and the confirm step
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
#include <unistd.h>

#define BVR_MAX 16
typedef struct { char rows[BVR_MAX][64]; int nr, nc; } BvRange;

static __attribute__((unused)) void bvr_path(const char *root, const char *name, char *out, size_t n) {
    snprintf(out, n, "%s/pieces/display/%s", root, name);
}

/* 1 = range finder open and matrix loaded; 0 = closed. */
static __attribute__((unused)) int bvr_load(const char *root, BvRange *r) {
    char p[4400];
    r->nr = r->nc = 0;
    bvr_path(root, "move_range_matrix.txt", p, sizeof(p));
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    while (r->nr < BVR_MAX && fgets(r->rows[r->nr], sizeof(r->rows[0]), f)) {
        r->rows[r->nr][strcspn(r->rows[r->nr], "\r\n")] = 0;
        if ((int)strlen(r->rows[r->nr]) > r->nc) r->nc = (int)strlen(r->rows[r->nr]);
        r->nr++;
    }
    fclose(f);
    return r->nr > 0;
}

/* dx,dy = cell offset from the origin cell. */
static __attribute__((unused)) int bvr_has(const BvRange *r, int dx, int dy) {
    int row = dy + r->nr / 2, col = dx + r->nc / 2;
    if (row < 0 || row >= r->nr || col < 0 || col >= r->nc) return 0;
    return r->rows[row][col] == '#';
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

/* Origin cell of the range: the xelector when it exists, else the
 * entity Move was chosen for. Returns 1 and fills x,y,z, else 0. */
static __attribute__((unused)) int bvr_origin(const char *real_root, int *x, int *y, int *z) {
    char p[4400];
    snprintf(p, sizeof(p), "%s/pieces/xelector_01/state.txt", real_root);
    if (bvr_kv_int(p, "pos_x", -9999) != -9999) {
        *x = bvr_kv_int(p, "pos_x", 0); *y = bvr_kv_int(p, "pos_y", 0); *z = bvr_kv_int(p, "pos_z", 0);
        return 1;
    }
    char ep[4400], ent[64] = "";
    bvr_path(real_root, "move_range_entity.txt", ep, sizeof(ep));
    FILE *f = fopen(ep, "r");
    if (f) { char l[128]; while (fgets(l, sizeof(l), f)) if (!strncmp(l, "entity=", 7)) { snprintf(ent, sizeof(ent), "%.63s", l + 7); ent[strcspn(ent, "\r\n")] = 0; } fclose(f); }
    if (!ent[0]) return 0;
    snprintf(p, sizeof(p), "%s/pieces/%s/state.txt", real_root, ent);
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
#endif
