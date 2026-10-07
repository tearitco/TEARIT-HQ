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
/* 3D form: dz = levels above/below the origin (the hero can move up and down). */
static __attribute__((unused)) int bvr_has3(const BvRange *r, int dx, int dy, int dz) {
    return r->nr > 0 && mvr_matrix_allows3(r, dx, dy, dz);
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

/* ---- entity position: the page row first, state.txt as the fallback ------
 * The livedesk page file is the shared source of truth for where an entity is
 * (18.pc-hq/PAGE-FILE.md): a move written to the row shows in the desk and in
 * both pc-hq views on their next draw. So when the entity has a row on the open
 * page, x/y are read from and written to THAT row (khtpm_page_rows.c).
 *
 * z is not in the row. Following the desk (khtpm_entity.c read_entity_z), an
 * entity keeps its own z= in <entity dir>/desktop_pos.txt. With no page row
 * (a board-owned map, or nothing seeded yet) everything stays in
 * pieces/<id>/state.txt exactly as before. state.txt is also kept in step when
 * a row is written, so readers that have not been moved to the page yet do not
 * drift. */
#include "../../_shared-lib/khtpm_page_rows.c"
#include "../../_shared-lib/khtpm_locations.c"   /* loc_house_root(): one place that finds the house root */

static __attribute__((unused)) int bvr_house(const char *froot, char *out, size_t n) {
    out[0] = '\0';
    return loc_house_root(froot, out, n);   /* shared resolver (_shared-lib/khtpm_locations.c) */
}

/* 1 = x,y,z filled. *from_row (optional) = 1 when the page row supplied x/y. */
static __attribute__((unused)) int bvr_pos_get(const char *froot, const char *ent,
                                               int *x, int *y, int *z, int *from_row) {
    char house[4400], rowpath[4400], sp[4400];
    int cx, cy;
    if (from_row) *from_row = 0;
    snprintf(sp, sizeof(sp), "%s/pieces/%s/state.txt", froot, ent);
    if (bvr_house(froot, house, sizeof(house)) && pgr_get(house, ent, &cx, &cy, rowpath, sizeof(rowpath))) {
        char dp[4400];
        *x = cx; *y = cy;
        snprintf(dp, sizeof(dp), "%s/%s/desktop_pos.txt", house, rowpath);
        *z = bvr_kv_int(dp, "z", bvr_kv_int(sp, "pos_z", 0));
        if (from_row) *from_row = 1;
        return 1;
    }
    if (bvr_kv_int(sp, "pos_x", -9999) == -9999) return 0;
    *x = bvr_kv_int(sp, "pos_x", 0); *y = bvr_kv_int(sp, "pos_y", 0); *z = bvr_kv_int(sp, "pos_z", 0);
    return 1;
}

/* Look of the range finder, from an external pdl (house config style: pipe-
 * delimited rows, `STYLE | key | value`) so it can be tuned without a rebuild:
 *   <real project>/pieces/system/move_range_style.pdl
 *     STYLE | range_edge   | 0.03        thin-wire half-width in cells (smaller = thinner)
 *     STYLE | placer_edge  | 0.10        placer-selector wire half-width
 *     STYLE | range_color  | 255,220,40  range wire colour
 *     STYLE | range_dim    | 0.55        range colour multiplier (lower = fainter glow)
 *     STYLE | placer_color | 40,255,80   placer selector colour (the one that matters)
 * Missing file / key = the defaults below. */
typedef struct { float range_edge, placer_edge, range_dim; int range_rgb[3], placer_rgb[3]; } BvrStyle;
static __attribute__((unused)) void bvr_style(const char *real_root, BvrStyle *st) {
    char p[4400], line[256];
    st->range_edge = 0.03f; st->placer_edge = 0.10f; st->range_dim = 0.55f;
    st->range_rgb[0] = 255; st->range_rgb[1] = 220; st->range_rgb[2] = 40;
    st->placer_rgb[0] = 40; st->placer_rgb[1] = 255; st->placer_rgb[2] = 80;
    snprintf(p, sizeof(p), "%s/pieces/system/move_range_style.pdl", real_root);
    FILE *f = fopen(p, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *a, *b, *v;
        if (strncmp(line, "STYLE", 5) != 0) continue;
        if (!(a = strchr(line, '|')) || !(b = strchr(a + 1, '|'))) continue;
        v = b + 1;
        a++; while (*a == ' ') a++;
        if (!strncmp(a, "range_edge", 10)) st->range_edge = (float)atof(v);
        else if (!strncmp(a, "placer_edge", 11)) st->placer_edge = (float)atof(v);
        else if (!strncmp(a, "range_dim", 9)) st->range_dim = (float)atof(v);
        else if (!strncmp(a, "range_color", 11)) sscanf(v, " %d , %d , %d", &st->range_rgb[0], &st->range_rgb[1], &st->range_rgb[2]);
        else if (!strncmp(a, "placer_color", 12)) sscanf(v, " %d , %d , %d", &st->placer_rgb[0], &st->placer_rgb[1], &st->placer_rgb[2]);
    }
    fclose(f);
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
    if (ent[0] && bvr_pos_get(real_root, ent, x, y, z, NULL)) return 1;
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
    {
        BvRange rg; int ox = 0, oy = 0, oz = 0, ok = 1;
        if (bvr_load(real_root, &rg) && bvr_origin(real_root, &ox, &oy, &oz)) ok = bvr_has3(&rg, x - ox, y - oy, z - oz);
        snprintf(l1, n1, "move %s -> %s%d (%d,%d,%d)%s", ent[0] ? ent : "?", ref, y + 1, x, y, z, ok ? "" : "  OUT OF RANGE");
    }
    if (buf[0]) snprintf(l2, n2, "jump: %s_", buf);
    else snprintf(l2, n2, "arrows | z/x level | ref+Enter jump | Enter place | Esc");
}

/* pc-hq's event ledger is <project>/data/master_ledger.txt
 * (ts|turn|actor|action|details - the same file pc_menu_input.c's
 * ledger_append() writes). The desk's move_entity_tick appends each step to
 * world-manager's entities_live.ledger so other systems see the move; this is
 * pc-hq's equivalent: one "entity_move" line per step. It does NOT advance the
 * tick (ending the turn on a move is a game-design call, not a Move concern). */
static __attribute__((unused)) void bvr_ledger(const char *real_root, const char *ent, int x, int y, int z) {
    char p[4400], ts[64];
    time_t now = time(NULL);
    struct tm tmv;
    snprintf(p, sizeof(p), "%s/pieces/world_01/state.txt", real_root);
    int turn = bvr_kv_int(p, "tick", 0);
    snprintf(p, sizeof(p), "%s/data/master_ledger.txt", real_root);
    FILE *f = fopen(p, "a");
    if (!f) return;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tmv);
    fprintf(f, "%s|%d|%s|entity_move|x=%d,y=%d,z=%d\n", ts, turn, ent, x, y, z);
    fclose(f);
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

/* Write an entity's position: the page row (x/y) + its own z= when it has a
 * row, and always state.txt (kept in step). */
static __attribute__((unused)) void bvr_pos_set(const char *froot, const char *ent, int x, int y, int z) {
    char house[4400], rowpath[4400], sp[4400], dp[4400];
    int cx, cy;
    snprintf(sp, sizeof(sp), "%s/pieces/%s/state.txt", froot, ent);
    if (bvr_house(froot, house, sizeof(house)) && pgr_get(house, ent, &cx, &cy, rowpath, sizeof(rowpath))) {
        pgr_set_cell(house, ent, x, y);
        if (z >= 0) {
            snprintf(dp, sizeof(dp), "%s/%s/desktop_pos.txt", house, rowpath);
            bvr_kv_set(dp, "z", z);
        }
    }
    bvr_kv_set(sp, "pos_x", x); bvr_kv_set(sp, "pos_y", y);
    if (z >= 0) bvr_kv_set(sp, "pos_z", z);
}

static __attribute__((unused)) void bvr_plan(const char *real_root, const char *ent,
                                             int x0, int y0, int z0, int x1, int y1, int z1) {
    char p[4400], dir[4400];
    int xy[256][2], wp[256][3], nxy, dz = z1 - z0, adz = dz < 0 ? -dz : dz, n, i;
    snprintf(dir, sizeof(dir), "%s/pieces/%s", real_root, ent);
    nxy = mvr_path(x0, y0, x1, y1, 1, xy, 256);
    n = nxy > adz ? nxy : adz;
    if (n == 0) n = 1;                                   /* same cell: still apply it */
    for (i = 0; i < n; i++) {                            /* x/y and z advance together */
        int k = i < nxy ? i : nxy - 1;
        wp[i][0] = nxy > 0 ? xy[k][0] : x1;
        wp[i][1] = nxy > 0 ? xy[k][1] : y1;
        wp[i][2] = i + 1 < adz ? z0 + (dz > 0 ? i + 1 : -(i + 1)) : z1;
    }
    wp[n - 1][0] = x1; wp[n - 1][1] = y1; wp[n - 1][2] = z1;
    if (!mvr_queue_write3(dir, wp, n)) return;
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
    /* claim this tick FIRST (stamp), and move the cursor BEFORE the side
     * effects, so an overlapping dispatch cannot apply/log the same waypoint twice */
    if ((f = fopen(stamp_path, "w"))) { fprintf(f, "%lld\n", now); fclose(f); }
    snprintf(dir, sizeof(dir), "%s/pieces/%s", real_root, ent);
    int x, y, z, ln, moved = 0;
    if (mvr_queue_next(dir, &x, &y, &z, &ln)) {
        mvr_queue_advance(dir, ln);
        (void)sp;
        bvr_pos_set(real_root, ent, x, y, z);
        bvr_ledger(real_root, ent, x, y, z);
        moved = 1;
    } else {                      /* drained: same cleanup the desk's tick does */
        mvr_queue_clear(dir);
        unlink(ap);
    }
    if ((f = fopen(stamp_path, "w"))) { fprintf(f, "%lld\n", now); fclose(f); }
    return moved;
}

/* Bump the session's redraw marker (same file bv_menu_input's
 * bump_screen_changed() appends to). */
static __attribute__((unused)) void bvr_bump(const char *sess_root) {
    char p[4400];
    snprintf(p, sizeof(p), "%s/pieces/display/bv_screen_changed.txt", sess_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

/* Place: accept the placer's cell if it is on a '#' of the range around the
 * origin AND inside the board (bw/bh; 0 = unbounded) - the pc-hq equivalent of
 * the desk's snap-to-grid + clamp-to-screen in move_entity_init. Plans the
 * animated path, then closes the range finder. Returns 1 if the move was
 * accepted, 0 if rejected (range stays open). */
static __attribute__((unused)) int bvr_confirm(const char *froot, const char *sess_root, int bw, int bh) {
    BvRange rng;
    char pp[4400], jp[4400], ep[4400], ent[64] = "", sp[4400];
    if (!bvr_load(froot, &rng)) return 0;
    snprintf(pp, sizeof(pp), "%s/pieces/display/placer.txt", sess_root);
    snprintf(jp, sizeof(jp), "%s/pieces/display/move_jump.txt", sess_root);
    bvr_path(froot, "move_range_entity.txt", ep, sizeof(ep));
    FILE *f = fopen(ep, "r");
    if (f) { char l[128]; while (fgets(l, sizeof(l), f)) if (!strncmp(l, "entity=", 7)) { snprintf(ent, sizeof(ent), "%.63s", l + 7); ent[strcspn(ent, "\r\n")] = 0; } fclose(f); }
    if (!ent[0]) return 0;
    int tx = bvr_kv_int(pp, "x", 0), ty = bvr_kv_int(pp, "y", 0), tz = bvr_kv_int(pp, "z", 0);
    int ox = 0, oy = 0, oz = 0;
    bvr_origin(froot, &ox, &oy, &oz);
    if (tx < 0 || ty < 0 || (bw > 0 && tx >= bw) || (bh > 0 && ty >= bh)) return 0;   /* off the board */
    if (!bvr_has3(&rng, tx - ox, ty - oy, tz - oz)) return 0;                          /* out of range (x,y,z) */
    (void)sp;
    {
        int cx = tx, cy = ty, cz = tz;
        bvr_pos_get(froot, ent, &cx, &cy, &cz, NULL);   /* from the page row when it has one */
        bvr_plan(froot, ent, cx, cy, cz, tx, ty, tz);
    }
    bvr_close(froot);
    unlink(jp);
    bvr_kv_set(pp, "armed", 0);
    bvr_bump(sess_root);
    return 1;
}

/* A mouse click resolved to board cell (cx,cy) while the range finder is open.
 * Desk behaviour (tp_arm_placer_rmmv): a click selects the target cell; a
 * second click on the SAME cell places. Out-of-range clicks are ignored.
 * Returns 1 if the click was consumed by the range finder (caller must not arm
 * the plain placer), 0 if no range is open. z stays the entity's own level. */
static __attribute__((unused)) int bvr_click(const char *froot, const char *sess_root,
                                             int cx, int cy, int bw, int bh) {
    BvRange rng;
    char pp[4400];
    int ox = 0, oy = 0, oz = 0;
    if (!bvr_load(froot, &rng)) return 0;
    bvr_origin(froot, &ox, &oy, &oz);
    if (!bvr_has(&rng, cx - ox, cy - oy)) return 1;                       /* out of range: ignore */
    if (bw > 0 && cx >= bw) return 1;
    if (bh > 0 && cy >= bh) return 1;
    snprintf(pp, sizeof(pp), "%s/pieces/display/placer.txt", sess_root);
    if (bvr_kv_int(pp, "armed", 0) && bvr_kv_int(pp, "x", -1) == cx && bvr_kv_int(pp, "y", -1) == cy) {
        bvr_kv_set(pp, "z", oz);
        bvr_confirm(froot, sess_root, bw, bh);                            /* second click, same cell */
        return 1;
    }
    FILE *f = fopen(pp, "w");
    if (f) { fprintf(f, "armed=1\nx=%d\ny=%d\nz=%d\n", cx, cy, oz); fclose(f); }
    bvr_bump(sess_root);
    return 1;
}
#endif
