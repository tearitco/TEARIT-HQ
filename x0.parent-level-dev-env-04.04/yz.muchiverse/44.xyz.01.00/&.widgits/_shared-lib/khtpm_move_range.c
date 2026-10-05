/* khtpm_move_range.c - the PURE, shared part of Move's range finder.
 *
 * Text-included canonical helper (house convention for genuinely
 * multi-consumer, no-I/O-policy code - same family as khtpm_grid_jump.c):
 * no .so, no per-app copy, plain C + <stdio.h>/<string.h>/<stdlib.h>.
 *
 * WHY: the desk (tp_arm_placer_rmmv.c + move_entity_init/_tick) and pc-hq
 * (board-viewer's bv_move_range.c) used to carry two separate copies of the
 * same ideas. They consume the SAME DATA and each applies it its own way:
 *
 *   data (shared here)                     environment-specific (not here)
 *   -----------------------------------    ---------------------------------
 *   range matrix: '#' grid, centred on     desk: X11 overlay, pixel cells
 *     the origin; written by                pc-hq: 3D wire cells / 2D tiles
 *     tp_gen_range_matrix.+x
 *   path planning (straight, stepped)      desk: px step 8, desktop_pos.txt
 *                                           pc-hq: 1 cell, pieces/<id>/state.txt
 *   waypoint queue: animation_queue.txt    who ticks it: desk = move_entity.pal
 *     + .cursor (append-only ledger +       via move_entity_tick.+x; pc-hq =
 *     cursor, never rewritten)              the engine's bv_dispatch loop
 *
 * All functions are static + unused-tolerant so any includer can pull the
 * whole file in. Prefix: mvr_. */
#ifndef KHTPM_MOVE_RANGE_C
#define KHTPM_MOVE_RANGE_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__GNUC__)
#define MVR_UNUSED __attribute__((unused))
#else
#define MVR_UNUSED
#endif

/* ---- range matrix -------------------------------------------------------
 * Rows of '#' (in range) / '.' (out). Centred on the origin cell: the
 * matrix cell (nr/2, nc/2) is the origin. nr == 0 means "no matrix loaded". */
#define MVR_MAX 65
typedef struct { char rows[MVR_MAX][MVR_MAX]; int nr, nc; unsigned char depth[MVR_MAX][MVR_MAX]; } MvrMatrix;

/* depth[r][c] = how many z-levels deep a '#' cell reaches: Manhattan distance
 * to the nearest non-'#' cell (cells outside the matrix count as non-'#'), so
 * 0 for '.' and 1 for a '#' on the rim. For a diamond of radius R this is
 * R - (|dx|+|dy|) + 1, which makes the 3D range an octahedron: a cell is in
 * range at height dz iff depth > |dz|, i.e. |dx|+|dy|+|dz| <= R. Computed from
 * the matrix itself, so any shape the writer emits gets a consistent 3D form. */
MVR_UNUSED static void mvr_matrix_depth(MvrMatrix *m) {
    int r, c, changed = 1, guard = 0;
    for (r = 0; r < m->nr; r++)
        for (c = 0; c < MVR_MAX; c++) m->depth[r][c] = (c < m->nc && m->rows[r][c] == '#') ? 250 : 0;
    while (changed && guard++ < MVR_MAX * 2) {
        changed = 0;
        for (r = 0; r < m->nr; r++)
            for (c = 0; c < m->nc; c++) {
                int d, best;
                if (m->rows[r][c] != '#') continue;
                best = m->depth[r][c];
                d = (r > 0)        ? m->depth[r-1][c] : 0; if (d + 1 < best) best = d + 1;
                d = (r < m->nr-1)  ? m->depth[r+1][c] : 0; if (d + 1 < best) best = d + 1;
                d = (c > 0)        ? m->depth[r][c-1] : 0; if (d + 1 < best) best = d + 1;
                d = (c < m->nc-1)  ? m->depth[r][c+1] : 0; if (d + 1 < best) best = d + 1;
                if (best != m->depth[r][c]) { m->depth[r][c] = (unsigned char)best; changed = 1; }
            }
    }
}

/* Returns 1 if the file was read and holds at least one row. Blank lines are
 * skipped (not counted as rows); short rows are padded with '.'. */
MVR_UNUSED static int mvr_matrix_load(const char *path, MvrMatrix *m) {
    char line[MVR_MAX + 4];
    FILE *f;
    m->nr = m->nc = 0;
    if (!path || !path[0] || !(f = fopen(path, "r"))) return 0;
    while (m->nr < MVR_MAX && fgets(line, sizeof(line), f)) {
        int len = (int)strcspn(line, "\r\n"), i;
        if (len <= 0) continue;
        if (len > MVR_MAX) len = MVR_MAX;
        for (i = 0; i < len; i++) m->rows[m->nr][i] = line[i];
        for (; i < MVR_MAX; i++) m->rows[m->nr][i] = '.';
        if (len > m->nc) m->nc = len;
        m->nr++;
    }
    fclose(f);
    mvr_matrix_depth(m);
    return m->nr > 0;
}

/* Is the cell (dx,dy,dz) away from the origin in range? dz = levels above
 * (+) / below (-) the origin. An empty matrix allows everything (the caller's
 * own bounds are then the only restriction) - the desk placer's long-standing
 * "no matrix = no shape restriction" rule. The desk is flat, so it passes
 * dz = 0 through mvr_matrix_allows(). */
MVR_UNUSED static int mvr_matrix_allows3(const MvrMatrix *m, int dx, int dy, int dz) {
    int row, col, az = dz < 0 ? -dz : dz;
    if (m->nr <= 0) return 1;
    row = dy + m->nr / 2;
    col = dx + m->nc / 2;
    if (row < 0 || row >= m->nr || col < 0 || col >= m->nc) return 0;
    return m->rows[row][col] == '#' && m->depth[row][col] > az;
}
MVR_UNUSED static int mvr_matrix_allows(const MvrMatrix *m, int dx, int dy) {
    return mvr_matrix_allows3(m, dx, dy, 0);
}

/* ---- path planning -------------------------------------------------------
 * Straight path from (sx,sy) to (tx,ty): each tick both axes advance by
 * `step` toward the target (diagonal first, then straight), and the last
 * point is always exactly the target. Returns the point count (0 = already
 * there). out is out[max][2]. Same algorithm move_entity_init.c always used
 * (step 8 px there); pc-hq calls it with step 1 (cell). */
MVR_UNUSED static int mvr_path(int sx, int sy, int tx, int ty, int step, int (*out)[2], int max) {
    int n = 0, cx = sx, cy = sy;
    int dx = (tx > sx) ? step : (tx < sx) ? -step : 0;
    int dy = (ty > sy) ? step : (ty < sy) ? -step : 0;
    if (dx == 0 && dy == 0) return 0;
    while (n < max) {
        int rx = (dx == 0) ? (cx == tx) : ((dx > 0) ? (cx >= tx) : (cx <= tx));
        int ry = (dy == 0) ? (cy == ty) : ((dy > 0) ? (cy >= ty) : (cy <= ty));
        if (rx && ry) { out[n][0] = tx; out[n][1] = ty; n++; break; }
        if (cx != tx) cx += dx;
        if (cy != ty) cy += dy;
        out[n][0] = cx; out[n][1] = cy; n++;
    }
    return n;
}

/* ---- waypoint queue ------------------------------------------------------
 * <dir>/animation_queue.txt  one waypoint per line:  x=%d|y=%d[|z=%d]
 * <dir>/animation_queue.cursor  the number of lines already consumed.
 * Append-only ledger + cursor, the house idiom: nothing is ever rewritten
 * in place, a consumer just moves its cursor. The desk's format, unchanged
 * (a trailing |z= is ignored by readers that predate it). */
MVR_UNUSED static void mvr_queue_paths(const char *dir, char *q, size_t qn, char *c, size_t cn) {
    snprintf(q, qn, "%s/animation_queue.txt", dir);
    snprintf(c, cn, "%s/animation_queue.cursor", dir);
}

/* Write a fresh queue + cursor 0. z_mid/z_last < 0 omit the z field; the
 * last point gets z_last, every earlier one z_mid. Returns 1 on success. */
MVR_UNUSED static int mvr_queue_write(const char *dir, int (*xy)[2], int n, int z_mid, int z_last) {
    char q[4400], c[4400];
    FILE *f;
    int i;
    mvr_queue_paths(dir, q, sizeof(q), c, sizeof(c));
    if (!(f = fopen(q, "w"))) return 0;
    for (i = 0; i < n; i++) {
        int z = (i == n - 1) ? z_last : z_mid;
        if (z >= 0) fprintf(f, "x=%d|y=%d|z=%d\n", xy[i][0], xy[i][1], z);
        else        fprintf(f, "x=%d|y=%d\n", xy[i][0], xy[i][1]);
    }
    fclose(f);
    if ((f = fopen(c, "w"))) { fprintf(f, "0\n"); fclose(f); }
    return 1;
}

/* Same queue, but each waypoint carries its own z: xyz[n][3]. */
MVR_UNUSED static int mvr_queue_write3(const char *dir, int (*xyz)[3], int n) {
    char q[4400], c[4400];
    FILE *f;
    int i;
    mvr_queue_paths(dir, q, sizeof(q), c, sizeof(c));
    if (!(f = fopen(q, "w"))) return 0;
    for (i = 0; i < n; i++) fprintf(f, "x=%d|y=%d|z=%d\n", xyz[i][0], xyz[i][1], xyz[i][2]);
    fclose(f);
    if ((f = fopen(c, "w"))) { fprintf(f, "0\n"); fclose(f); }
    return 1;
}

/* Next unconsumed waypoint. Returns 1 and fills x,y (z = -1 if the line has
 * none) and *line_no (pass it to mvr_queue_advance once applied), or 0 when
 * the queue is missing/drained. Does NOT move the cursor. */
MVR_UNUSED static int mvr_queue_next(const char *dir, int *x, int *y, int *z, int *line_no) {
    char q[4400], c[4400], line[128];
    int cursor = 0, ln = 0;
    FILE *f;
    mvr_queue_paths(dir, q, sizeof(q), c, sizeof(c));
    if ((f = fopen(c, "r"))) { if (fscanf(f, "%d", &cursor) != 1) cursor = 0; fclose(f); }
    if (!(f = fopen(q, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) {
        int zz = -1;
        ln++;
        if (ln <= cursor) continue;
        if (sscanf(line, "x=%d|y=%d|z=%d", x, y, &zz) >= 2) { *z = zz; *line_no = ln; fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

MVR_UNUSED static void mvr_queue_advance(const char *dir, int line_no) {
    char q[4400], c[4400];
    FILE *f;
    mvr_queue_paths(dir, q, sizeof(q), c, sizeof(c));
    if ((f = fopen(c, "w"))) { fprintf(f, "%d\n", line_no); fclose(f); }
}

MVR_UNUSED static void mvr_queue_clear(const char *dir) {
    char q[4400], c[4400];
    mvr_queue_paths(dir, q, sizeof(q), c, sizeof(c));
    unlink(q);
    unlink(c);
}

#endif
