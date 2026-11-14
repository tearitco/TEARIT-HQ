/* nb_worker_gbcr.c - getBoundingClientRect helper (mechanical extract, single-TU include) */
static JSValue nb_el_getBoundingClientRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* Deliberately NOT guarded against a null/undefined `this`: a browser
     * throws "cannot read property 'getBoundingClientRect' of null" in
     * exactly that case, that error is what the CALLER wrote (it did not
     * check its own lookup), and swallowing it into a zeroed rect would hide
     * a page bug while looking like a fix. Removing this guard on 2026-11-02:
     * it was added earlier the same day against that error message, which
     * turned out to be correct browser behaviour rather than a gap here. */
    NbNode *n = get_this(ctx, this_val);
    double w = 0, h = 0, x = 0, y = 0;
    if (n && !layout_hidden_anc(n)) {
        NbCssStyle st;
        nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
        w = nb_box_w(n);
        h = st.has_height ? st.height : nb_content_h(n);
        layout_xy(n, &x, &y);
        /* Shift the whole tree down by the chrome the renderer puts above the
         * first content row. Uniform, so every relative offset below the
         * origin is untouched - see g_content_inset_top(). */
        y += g_content_inset_top();
        /* Viewport-relative: the page-driven scroll offset moves the rect,
         * so getBoundingClientRect (and the IntersectionObserver root
         * comparison built on it) tracks page scroll. */
        x -= g_scroll_x;
        y -= g_scroll_y;
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "x", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, o, "y", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, o, "top", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, o, "right", JS_NewFloat64(ctx, x + w));
    JS_SetPropertyStr(ctx, o, "bottom", JS_NewFloat64(ctx, y + h));
    JS_SetPropertyStr(ctx, o, "left", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, o, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    return o;
}
