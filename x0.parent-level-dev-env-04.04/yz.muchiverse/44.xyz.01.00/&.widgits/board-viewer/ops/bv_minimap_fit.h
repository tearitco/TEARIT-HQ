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
#endif
