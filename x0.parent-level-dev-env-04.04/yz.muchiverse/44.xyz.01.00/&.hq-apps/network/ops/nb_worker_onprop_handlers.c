/* nb_worker_onprop_handlers.c - on-* property handler helpers and bindings (mechanical extract, single-TU include) */
/* ---- on-* handlers: el.onclick = fn (C-side registry, per kind+node+type).
 * The wrapper objects carry a generic accessor per key; magic identifies the
 * ONPROPS key (native signature gains `int magic`). */
static int onprop_find(int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_onprop_count; i++) {
        if (!g_onprop[i].active) continue;
        if (g_onprop[i].kind != kind || g_onprop[i].node != n) continue;
        if (strcmp(g_onprop[i].type, type)) continue;
        return i;
    }
    return -1;
}
static const char *onprop_type_from_magic(int magic) {
    if (magic < 0 || !ONPROPS[magic]) return NULL;
    return ONPROPS[magic];
}
static int onprop_set_core(JSContext *ctx, int kind, NbNode *n, int magic, JSValueConst cb) {
    const char *type = onprop_type_from_magic(magic);
    if (!type || !JS_IsFunction(ctx, cb)) return 0;
    int i = onprop_find(kind, n, type);
    if (i < 0) {
        if (g_onprop_count >= MAX_ONPROPS) return 0;
        i = g_onprop_count++;
        g_onprop[i].kind = kind; g_onprop[i].node = n;
        snprintf(g_onprop[i].type, sizeof(g_onprop[i].type), "%s", type);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    } else {
        JS_FreeValue(ctx, g_onprop[i].cb);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    }
    g_onprop[i].active = 1;
    g_onprop[i].slot = i;
    return 0;
}
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_NODE, get_this(ctx, this_val), magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_DOC, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_WIN, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_onprop_get_core(JSContext *ctx, int kind, NbNode *n, int magic) {
    const char *type = onprop_type_from_magic(magic);
    if (!type) return JS_UNDEFINED;
    int i = onprop_find(kind, n, type);
    if (i < 0 || !g_onprop[i].active) return JS_UNDEFINED;
    return JS_DupValue(ctx, g_onprop[i].cb);
}
static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_NODE, get_this(ctx, this_val), magic);
}
static JSValue nb_doc_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_DOC, NULL, magic);
}
static JSValue nb_win_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_WIN, NULL, magic);
}
