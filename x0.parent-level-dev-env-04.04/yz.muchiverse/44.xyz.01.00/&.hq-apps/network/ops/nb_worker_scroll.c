/* nb_worker_scroll.c - window scroll helpers (mechanical extract, single-TU include) */
/* scrollIntoView: page-driven scroll (2026-11-09). The renderer owns the
 * native pane scroll and exposes no channel for a page to drive it, so the
 * offset is worker-local: window.scrollTo/scrollIntoView move g_scroll_y
 * (viewport-relative rects), and IntersectionObserver re-evaluates. Native
 * pane scroll is still not mirrored. */
static double nb_js_num(JSContext *ctx, JSValueConst v, double dflt) {
    double d = dflt;
    if (JS_IsNumber(v)) JS_ToFloat64(ctx, &d, v);
    else if (JS_IsString(v)) { char *s = JS_ToCString(ctx, v); if (s) { d = atof(s); JS_FreeCString(ctx, s); } }
    return d;
}
static void nb_set_scroll(double x, double y) {
    g_scroll_x = x < 0 ? 0 : x;
    g_scroll_y = y < 0 ? 0 : y;
}
static JSValue nb_win_scroll_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewFloat64(ctx, magic ? g_scroll_y : g_scroll_x);
}
static JSValue nb_win_scrollTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    double x = g_scroll_x, y = g_scroll_y;
    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSValue t = JS_GetPropertyStr(ctx, argv[0], "top");
        JSValue l = JS_GetPropertyStr(ctx, argv[0], "left");
        if (!JS_IsUndefined(t) && !JS_IsNull(t)) y = nb_js_num(ctx, t, y);
        if (!JS_IsUndefined(l) && !JS_IsNull(l)) x = nb_js_num(ctx, l, x);
        JS_FreeValue(ctx, t); JS_FreeValue(ctx, l);
    } else {
        if (argc > 0) x = nb_js_num(ctx, argv[0], x);
        if (argc > 1) y = nb_js_num(ctx, argv[1], y);
    }
    nb_set_scroll(x, y);
    nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
static JSValue nb_win_scrollBy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    double x = g_scroll_x, y = g_scroll_y;
    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSValue t = JS_GetPropertyStr(ctx, argv[0], "top");
        JSValue l = JS_GetPropertyStr(ctx, argv[0], "left");
        if (!JS_IsUndefined(t) && !JS_IsNull(t)) y += nb_js_num(ctx, t, 0);
        if (!JS_IsUndefined(l) && !JS_IsNull(l)) x += nb_js_num(ctx, l, 0);
        JS_FreeValue(ctx, t); JS_FreeValue(ctx, l);
    } else {
        if (argc > 0) x += nb_js_num(ctx, argv[0], 0);
        if (argc > 1) y += nb_js_num(ctx, argv[1], 0);
    }
    nb_set_scroll(x, y);
    nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
