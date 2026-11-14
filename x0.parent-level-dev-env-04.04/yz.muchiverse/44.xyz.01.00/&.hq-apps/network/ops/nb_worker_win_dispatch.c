/* nb_worker_win_dispatch.c - window.dispatchEvent helper (mechanical extract, single-TU include) */
static JSValue nb_win_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ev = argc > 0 ? argv[0] : JS_UNDEFINED;
    JSValue bv = JS_GetPropertyStr(ctx, ev, "bubbles");
    int bubbles = JS_ToBool(ctx, bv);
    if (bubbles < 0) bubbles = 0;
    JS_FreeValue(ctx, bv);
    return JS_NewBool(ctx, dispatch_event(ctx, EVT_WIN, NULL, ev, bubbles));
}
