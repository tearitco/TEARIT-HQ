/* nb_worker_c2d_clearrect.c - canvas 2D clearRect implementation (mechanical extract, single-TU include) */
static JSValue nb_c2d_clearRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv) return JS_UNDEFINED;
    if (cv) cv->dirty = 1;
    double x = nb_c2d_argd(ctx, argv, argc, 0, 0), y = nb_c2d_argd(ctx, argv, argc, 1, 0);
    double w = nb_c2d_argd(ctx, argv, argc, 2, 0), h = nb_c2d_argd(ctx, argv, argc, 3, 0);
    if (w < 0) { x += w; w = -w; } if (h < 0) { y += h; h = -h; }
    int x0 = (int)x, y0 = (int)y, x1 = (int)(x + w + 0.999), y1 = (int)(y + h + 0.999);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    for (int yy = y0; yy < y1; yy++)
        memset(cv->px + ((size_t)yy * cv->w + x0) * 4, 0, (size_t)(x1 - x0) * 4);
    return JS_UNDEFINED;
}
