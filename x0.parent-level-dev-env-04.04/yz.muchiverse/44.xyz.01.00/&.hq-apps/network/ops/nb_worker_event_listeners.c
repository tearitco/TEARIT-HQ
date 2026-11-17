/* nb_worker_event_listeners.c - event listener and basic event method bindings (mechanical extract, single-TU include) */
/* ---- events (EventTarget add/removeEventListener; dispatch is commit 8) ---- */
/* Fetch an attribute from the JS global object (returns owned value). */
static JSValue get_global_attr(JSContext *ctx, const char *name) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, g, name);
    JS_FreeValue(ctx, g);
    return v;
}
static void evl_add(JSContext *ctx, int kind, NbNode *n, JSValueConst typev, JSValueConst cb) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    if (!type || !type[0] || !JS_IsFunction(ctx, cb) || g_evl_count >= MAX_EVENTS) { JS_FreeCString(ctx, to); return; }
    int slot = g_evl_count++;
    g_evl[slot].kind = kind; g_evl[slot].node = n; g_evl[slot].slot = slot; g_evl[slot].active = 1;
    snprintf(g_evl[slot].type, sizeof(g_evl[slot].type), "%s", type);
    g_evl[slot].cb = JS_DupValue(ctx, cb);
    JS_FreeCString(ctx, to);
}
static void evl_del(JSContext *ctx, int kind, NbNode *n, JSValueConst typev) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    for (int i = 0; i < g_evl_count; i++)
        if (g_evl[i].active && g_evl[i].kind == kind && g_evl[i].node == n &&
            (!type || !type[0] || !strcmp(g_evl[i].type, type))) {
            g_evl[i].active = 0;
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
            break;
        }
    JS_FreeCString(ctx, to);
}
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_NODE, get_this(ctx, this_val),
            argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_NODE, get_this(ctx, this_val), argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_event_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue c;
    if (JS_IsObject(this_val) && !JS_IsException((c = JS_GetPropertyStr(ctx, this_val, "cancelable")))) {
        int can = JS_ToBool(ctx, c);
        if (can < 0) can = 0;
        JS_FreeValue(ctx, c);
        if (can) JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, 1));
    } else if (JS_IsObject(this_val)) JS_FreeValue(ctx, c);
    return JS_UNDEFINED;
}
static JSValue nb_event_stopPropagation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (JS_IsObject(this_val)) JS_SetPropertyStr(ctx, this_val, "propagationStopped", JS_NewBool(ctx, 1));
    return JS_UNDEFINED;
}
