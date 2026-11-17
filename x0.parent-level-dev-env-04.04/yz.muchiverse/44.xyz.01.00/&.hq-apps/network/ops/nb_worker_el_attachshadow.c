/* nb_worker_el_attachshadow.c - Element.prototype.attachShadow implementation (mechanical extract, single-TU include) */
/* Element.prototype.attachShadow (C-backed ShadowRoot; el.shadowRoot). */
static JSValue nb_el_attachShadow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NULL;
    JSValue ex = JS_GetPropertyStr(ctx, this_val, "shadowRoot");
    if (JS_IsObject(ex)) { JSValue r = JS_DupValue(ctx, ex); JS_FreeValue(ctx, ex); return r; }
    JS_FreeValue(ctx, ex);
    NbNode *frag = calloc(1, sizeof(*frag));
    if (!frag) return JS_NULL;
    frag->tag = strdup("#shadow-root");
    orphan_add(frag);
    JSValue w = make_fragment_wrapper(ctx, frag, "#shadow-root");
    if (!JS_IsObject(w)) return JS_NULL;
    const char *mode = "open";
    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue mv = JS_GetPropertyStr(ctx, argv[0], "mode");
        char *mo = NULL;
        if (JS_IsString(mv)) { mo = JS_ToCString(ctx, mv); if (mo) mode = mo; }
        JS_FreeValue(ctx, mv);
        JS_FreeCString(ctx, mo);
    }
    JS_SetPropertyStr(ctx, w, "mode", JS_NewString(ctx, mode));
    JS_SetPropertyStr(ctx, w, "host", JS_DupValue(ctx, this_val));
    JS_SetPropertyStr(ctx, w, "nodeType", JS_NewInt32(ctx, 11));
    JS_SetPropertyStr(ctx, this_val, "shadowRoot", JS_DupValue(ctx, w));
    JS_SetPropertyStr(ctx, this_val, "__nb_shadow", JS_NewBool(ctx, 1));
    return w;
}