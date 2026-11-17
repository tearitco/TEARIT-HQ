/* nb_worker_css_computed_helpers.c - CSS computed style helpers (mechanical extract, single-TU include) */
static int css_hidden(const NbNode *n) {
    /* nb_css_resolve tolerates a NULL rule cache (a page with no stylesheet),
     * and inline display:none still has to count there: without this, the
     * probe's no-CSS fixtures reported a hidden box at its content size. */
    return nb_css_hidden(g_css, n);
}
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Return (as JSValue) an object with the modeled computed style of `n` plus a
 * getPropertyValue(). Missing display => "" (visible), opacity => "1". */
static JSValue push_computed_style(JSContext *ctx, NbNode *n) {
    JSValue o = JS_NewObject(ctx);
    NbCssStyle st;
    memset(&st, 0, sizeof(st));
    if (n) nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    JS_SetPropertyStr(ctx, o, "display", JS_NewString(ctx, st.display[0] ? st.display : ""));
    JS_SetPropertyStr(ctx, o, "visibility", JS_NewString(ctx, st.visibility));
    JS_SetPropertyStr(ctx, o, "opacity", JS_NewString(ctx, st.opacity[0] ? st.opacity : "1"));
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, st.width));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, st.height));
    JS_SetPropertyStr(ctx, o, "font-family", JS_NewString(ctx, st.font_family[0] ? st.font_family : ""));
    /* font-size is read as a CSS string (kevlar: fontSize.replace('px','')) */
    JS_SetPropertyStr(ctx, o, "fontSize", JS_NewString(ctx, "16px"));
    JS_SetPropertyStr(ctx, o, "getPropertyValue",
                      JS_NewCFunction(ctx, nb_css_getprop, "getPropertyValue", 1));
    return o;
}

static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!name) return JS_NewString(ctx, "");
    JSValue v = JS_GetPropertyStr(ctx, this_val, name);
    JS_FreeCString(ctx, name);
    return JS_ToString(ctx, v);
}

/* window.getComputedStyle(el) — registered globally as `__nb_ges` by the
 * resident worker (overriding install_host's minimal default); the host
 * prelude's getComputedStyle delegates here. */
static JSValue nb_ges_rich(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_node(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    return push_computed_style(ctx, n);
}
