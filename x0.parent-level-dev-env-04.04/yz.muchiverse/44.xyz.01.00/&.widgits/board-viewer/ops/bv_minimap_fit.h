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
#endif
