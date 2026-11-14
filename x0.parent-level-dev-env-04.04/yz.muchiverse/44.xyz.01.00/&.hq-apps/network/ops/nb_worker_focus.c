/* nb_worker_focus.c - Element focus/blur helpers (mechanical extract, single-TU include) */
static JSValue nb_el_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (g_active_node != n) {
        if (g_active_node) {
            JSValue fb = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, fb, "type", JS_NewString(ctx, "blur"));
            JS_SetPropertyStr(ctx, fb, "bubbles", JS_NewBool(ctx, 0));
            JS_SetPropertyStr(ctx, fb, "cancelable", JS_NewBool(ctx, 0));
            dispatch_event(ctx, EVT_NODE, g_active_node, fb, 0);
            JS_FreeValue(ctx, fb);
        }
        g_active_node = n;
        JSValue doc = get_global_attr(ctx, "document");
        if (JS_IsObject(doc)) JS_SetPropertyStr(ctx, doc, "activeElement", push_node(ctx, n));
        JS_FreeValue(ctx, doc);
    }
    JSValue e = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, e, "type", JS_NewString(ctx, "focus"));
    JS_SetPropertyStr(ctx, e, "bubbles", JS_NewBool(ctx, 0));
    JS_SetPropertyStr(ctx, e, "cancelable", JS_NewBool(ctx, 0));
    dispatch_event(ctx, EVT_NODE, n, e, 0);
    JS_FreeValue(ctx, e);
    return JS_UNDEFINED;
}
static JSValue nb_el_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (g_active_node == n) {
        JSValue doc = get_global_attr(ctx, "document");
        if (JS_IsObject(doc)) JS_SetPropertyStr(ctx, doc, "activeElement", JS_NULL);
        JS_FreeValue(ctx, doc);
        g_active_node = NULL;
    }
    JSValue e = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, e, "type", JS_NewString(ctx, "blur"));
    JS_SetPropertyStr(ctx, e, "bubbles", JS_NewBool(ctx, 0));
    JS_SetPropertyStr(ctx, e, "cancelable", JS_NewBool(ctx, 0));
    dispatch_event(ctx, EVT_NODE, n, e, 0);
    JS_FreeValue(ctx, e);
    return JS_UNDEFINED;
}
