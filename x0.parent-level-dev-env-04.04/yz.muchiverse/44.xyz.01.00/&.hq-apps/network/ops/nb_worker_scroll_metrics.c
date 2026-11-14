/* nb_worker_scroll_metrics.c - element/window scroll metrics helpers (mechanical extract, single-TU include) */
/* ---- scroll metrics (2026-11-11) ----
 * window.scrollTo already moves g_scroll_x/g_scroll_y (worker-local; the
 * renderer owns the native pane scroll). This exposes the DOM surface pages
 * actually read: pageXOffset/pageYOffset, scrollingElement, and
 * scrollTop/scrollLeft/scrollHeight/scrollWidth.
 *
 * Heights come from the SAME box model the layout and scrollbar gutter
 * already use: documentElement.scrollHeight is the document height from
 * nb_doc_root_h, an element's scrollHeight is nb_content_h, and each is
 * raised to clientHeight as CSSOM requires. Widths report the client box:
 * this renderer wraps content instead of scrolling it sideways, so no
 * horizontal extent is claimed.
 *
 * Non-root scroll offsets are worker-local per-node values, like the window
 * offset. The renderer exposes no channel to scroll an inner pane, so the
 * value is what the page set and can read back; it is stored on the node so
 * a freed node cannot hand its value to a new node at the same address.
 * They are clamped at zero and not at the maximum: the maximum would need a
 * per-element overflow model this engine does not have. */
static int nb_live_root_scroller(NbNode *n) {
    if (!n || !n->tag || !g_dom_root) return 0;
    NbNode *html = find_tag_first(g_dom_root, "html");
    return html && n == html;
}
static double nb_client_dim(NbNode *n, int which) {
    if (!n) return 0.0;
    if (nb_live_root_scroller(n)) {
        if (which) return g_viewport_h > 0.0 ? g_viewport_h : 600.0;
        return g_col_px(n);
    }
    if (css_hidden(n)) return 0.0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    return which ? st.height : st.width;
}
static JSValue nb_el_scroll_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->tag) return JS_NewFloat64(ctx, 0.0);
    if (nb_live_root_scroller(n)) return JS_NewFloat64(ctx, magic ? g_scroll_y : g_scroll_x);
    return JS_NewFloat64(ctx, magic ? n->scroll_y : n->scroll_x);
}
static JSValue nb_el_scroll_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->tag) return JS_UNDEFINED;
    double v = nb_js_num(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, 0.0);
    if (!(v > 0.0)) v = 0.0;   /* negatives and NaN clamp to the top/left */
    if (nb_live_root_scroller(n)) {
        if (magic) nb_set_scroll(g_scroll_x, v);
        else nb_set_scroll(v, g_scroll_y);
        nb_obs_recheck(ctx);   /* viewport-relative rects just moved */
    } else if (magic) {
        n->scroll_y = v;
    } else {
        n->scroll_x = v;
    }
    return JS_UNDEFINED;
}
static JSValue nb_el_scroll_dim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->tag) return JS_NewFloat64(ctx, 0.0);
    /* display:none has no box in a browser, so no scroll extent either. */
    if (layout_hidden_anc(n)) return JS_NewFloat64(ctx, 0.0);
    if (!magic) return JS_NewFloat64(ctx, nb_client_dim(n, 0));
    double client = nb_client_dim(n, 1);
    double content;
    NbNode *html = g_dom_root ? find_tag_first(g_dom_root, "html") : NULL;
    if (html && n == html) content = nb_doc_root_h(n);
    else content = nb_content_h(n);
    return JS_NewFloat64(ctx, content > client ? content : client);
}
static JSValue nb_el_scrollIntoView(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    int align_top = 1; /* legacy boolean: true = block:start */
    char block[16] = "start";
    if (argc > 0 && JS_IsBool(argv[0])) {
        align_top = JS_ToBool(ctx, argv[0]);
        snprintf(block, sizeof(block), "%s", align_top ? "start" : "end");
    } else if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue b = JS_GetPropertyStr(ctx, argv[0], "block");
        if (JS_IsString(b)) {
            char *s = JS_ToCString(ctx, b);
            if (s) { snprintf(block, sizeof(block), "%s", s); JS_FreeCString(ctx, s); }
        }
        JS_FreeValue(ctx, b);
    }
    JSValue rectv = nb_el_getBoundingClientRect(ctx, this_val, 0, NULL);
    double rh = nb_int_rect_field(ctx, rectv, "height");
    double doc_y = nb_int_rect_field(ctx, rectv, "y") + g_scroll_y;
    JS_FreeValue(ctx, rectv);
    double vp = (g_viewport_h > 0.0) ? g_viewport_h : 600.0;
    double y = g_scroll_y;
    if (!strcmp(block, "center")) y = doc_y + rh / 2 - vp / 2;
    else if (!strcmp(block, "end")) y = doc_y + rh - vp;
    else if (!strcmp(block, "nearest")) {
        if (doc_y < g_scroll_y) y = doc_y;
        else if (doc_y + rh > g_scroll_y + vp) y = doc_y + rh - vp;
    } else y = doc_y; /* start */
    nb_set_scroll(g_scroll_x, y);
    nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
