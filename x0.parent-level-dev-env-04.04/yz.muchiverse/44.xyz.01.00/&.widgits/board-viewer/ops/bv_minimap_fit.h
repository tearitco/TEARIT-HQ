/* The corner map's size, and where the camera sits on it.
 * A page that is one pixel per cell and still bigger than max_px is scaled
 * to fit. The eye stays at the same fraction of the page. It is clamped
 * only when it is actually outside that page. Integer cells are kept when
 * one cell is still at least one pixel and the whole page fits in max_px.
 * cellpx == 0 means the box is a fraction of the page, not N pixels per cell.
 */
#ifndef BV_MINIMAP_FIT_H
#define BV_MINIMAP_FIT_H
static void bv_minimap_box(int cells_w, int cells_h, int px_per_col, int max_px,
                           int *out_w, int *out_h, int *cellpx) {
    if (cells_w < 1) cells_w = 1;
    if (cells_h < 1) cells_h = 1;
    if (px_per_col < 1) px_per_col = 1;
    if (max_px < 8) max_px = 8;
    int cp = px_per_col;
    while (cp > 1 && (cells_w * cp > max_px || cells_h * cp > max_px)) cp--;
    if (cells_w * cp <= max_px && cells_h * cp <= max_px) {
        *cellpx = cp; *out_w = cells_w * cp; *out_h = cells_h * cp; return;
    }
    *cellpx = 0;
    if (cells_w >= cells_h) {
        *out_w = max_px;
        *out_h = (int)((double)max_px * (double)cells_h / (double)cells_w);
        if (*out_h < 1) *out_h = 1;
    } else {
        *out_h = max_px;
        *out_w = (int)((double)max_px * (double)cells_w / (double)cells_h);
        if (*out_w < 1) *out_w = 1;
    }
}
/* Pixel of a world position inside the box. Returns 1 when it was outside and got clamped. */
static int bv_minimap_dot(double cam, double origin, int cells, int span_px, int cellpx, int *px) {
    double t = (cellpx > 0) ? (cam - origin) * (double)cellpx
                            : (cam - origin) * (double)span_px / (double)(cells < 1 ? 1 : cells);
    int outside = 0;
    if (t < 0) { t = 0; outside = 1; }
    if (span_px < 1) span_px = 1;
    if (t > (double)(span_px - 1)) { t = (double)(span_px - 1); outside = 1; }
    *px = (int)t;
    return outside;
}
/* Which of a cell's two slots is its TOP face on the corner map (2026-11-25): the floor when one is set, else the wall. A wall-only cell (snow, 2026-11-16)
 * still uses its wall; a cell with a floor and a wall (Grok's earth desk puts stone in every wall slot) shows the floor, not the grey. 0 = empty cell.
 * BV_TOPFACE_OLD is the mutant the harness uses to prove the check can fail (the old wall-first rule). */
static int bv_cell_top_slot(int fl, int wl) {
#ifdef BV_TOPFACE_OLD
    return wl ? wl : fl;
#else
    return fl ? fl : wl;
#endif
}
/* The colour of one cell on the corner map = the average colour of its tile in the page's atlas (2026-11-16, owner: "why dont i see snow on mini map?" on
 * ark-age and live). ad70266e2 had replaced the per-cell colours with three flat ones (water, wall, floor), so snow, roads and sand could not show.
 * A cell's floor or wall id IS the atlas slot (slot % cols, slot / cols, no -1; 0 = no tile). Only pixels with alpha >= 128 count, so a tile that is mostly
 * transparent keeps the colour of its drawn part. Returns 1 and the colour, or 0 when the slot is off the atlas or has no opaque pixel (the caller keeps its
 * old flat colour then). Pure, no allocation: also compiled by minimap_fit_op for the harness. */
static int bv_minimap_tile_avg(const unsigned char *atlas, int aw, int ah, int cols, int tp, int slot, int *r, int *g, int *b) {
    if (!atlas || cols < 1 || tp < 1 || slot < 1) return 0;
    int ox = (slot % cols) * tp, oy = (slot / cols) * tp;
    if (ox + tp > aw || oy + tp > ah) return 0;
    long sr = 0, sg = 0, sb = 0, n = 0;
    for (int y = 0; y < tp; y++)
        for (int x = 0; x < tp; x++) {
            const unsigned char *p = atlas + ((size_t)(oy + y) * (size_t)aw + (size_t)(ox + x)) * 4;
            if (p[3] < 128) continue;
            sr += p[0]; sg += p[1]; sb += p[2]; n++;
        }
    if (n == 0) return 0;
    *r = (int)(sr / n); *g = (int)(sg / n); *b = (int)(sb / n);
    return 1;
}
/* Click on the corner map -> the page cell under it (2026-11-17, owner: "when i click on mini map, it moves my cam / xelector there on big map"). The inverse of how bv_draw_minimap paints:
 * cellpx > 0 means cell (col,row) of the shown window was painted at box_x + col * cellpx; cellpx == 0 means the box shows the window scaled to box_w x box_h, one sample per output pixel. (wx0, wy0, ww, wh) is the
 * window of the page that was painted (it includes minimap_margin and the minimap_follow window), page_w x page_h the real page. Returns 1 and the cell, or 0 when the pixel is outside the box or the cell lies outside
 * the page (the margin, or a follow window hanging over the edge). Pure: the harness compiles it through minimap_fit_op. */
static int bv_minimap_cell(int px, int py, int box_x, int box_y, int box_w, int box_h, int cellpx,
                           int wx0, int wy0, int ww, int wh, int page_w, int page_h, int *cx, int *cy) {
    if (box_w < 1 || box_h < 1 || px < box_x || py < box_y || px >= box_x + box_w || py >= box_y + box_h) return 0;
    int bx, by;
    if (cellpx > 0) { bx = wx0 + (px - box_x) / cellpx; by = wy0 + (py - box_y) / cellpx; }
    else { bx = wx0 + (int)((double)(px - box_x) * (double)ww / (double)box_w); by = wy0 + (int)((double)(py - box_y) * (double)wh / (double)box_h); }
    if (bx < 0 || by < 0 || bx >= page_w || by >= page_h) return 0;
    *cx = bx; *cy = by; return 1;
}
#endif
