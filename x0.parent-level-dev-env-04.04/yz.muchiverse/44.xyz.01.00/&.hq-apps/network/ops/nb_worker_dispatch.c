/* nb_worker_dispatch.c - element/document dispatchEvent helpers (mechanical extract, single-TU include) */
static JSValue nb_el_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewBool(ctx, 0);
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_NODE, n, ev, bubbles));
}
static JSValue nb_doc_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_DOC, NULL, ev, bubbles));
}
