/* nb_worker_c2d_strokerect.c - canvas 2D strokeRect implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_strokeRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    double x = nb_c2d_argd(ctx, argv, argc, 0, 0), y = nb_c2d_argd(ctx, argv, argc, 1, 0);
    double w = nb_c2d_argd(ctx, argv, argc, 2, 0), h = nb_c2d_argd(ctx, argv, argc, 3, 0);
    if (w < 0) { x += w; w = -w; } if (h < 0) { y += h; h = -h; }
    double lw = nb_ctx_num(ctx, this_val, "lineWidth", 1);
    if (lw < 1) lw = 1;
    unsigned char s[4];
    nb_ctx_fill_rgb(ctx, this_val, s);
    double ga = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    /* stroke uses strokeStyle; fall back to fill color when unset */
    nb_px_fill_rect_xf(cv, x, y, w, lw, s, ga);
    nb_px_fill_rect_xf(cv, x, y + h - lw, w, lw, s, ga);
    nb_px_fill_rect_xf(cv, x, y, lw, h, s, ga);
    nb_px_fill_rect_xf(cv, x + w - lw, y, lw, h, s, ga);
    return JS_UNDEFINED;
}
