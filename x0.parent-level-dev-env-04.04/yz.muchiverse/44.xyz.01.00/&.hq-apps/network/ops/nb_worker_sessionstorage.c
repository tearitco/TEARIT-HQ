/* nb_worker_sessionstorage.c - sessionStorage implementation helpers (mechanical extract, single-TU include) */
static int ss_find(const char *k) {
    for (int i = 0; i < g_ss_count; i++) if (strcmp(g_ss[i].key, k) == 0) return i;
    return -1;
}
static JSValue nb_ss_getItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!k) { JS_FreeCString(ctx, ko); return JS_NULL; }
    int f = ss_find(k);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_NULL;
    return JS_NewString(ctx, g_ss[f].value);
}
static JSValue nb_ss_setItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL, *vo = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    const char *v = js_arg_str_any(ctx, argv, argc, 1, &vo);
    if (!k) { JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo); return JS_UNDEFINED; }
    int f = ss_find(k);
    if (f >= 0) {
        snprintf(g_ss[f].value, sizeof(g_ss[f].value), "%s", v);
    } else if (g_ss_count < ST_MAX_ENT) {
        StEnt *e = &g_ss[g_ss_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->key, sizeof(e->key), "%s", k);
        snprintf(e->value, sizeof(e->value), "%s", v);
    }
    JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
    return JS_UNDEFINED;
}
static JSValue nb_ss_removeItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *k = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!k) { JS_FreeCString(ctx, ko); return JS_UNDEFINED; }
    int f = ss_find(k);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_UNDEFINED;
    for (int i = f; i + 1 < g_ss_count; i++) g_ss[i] = g_ss[i + 1];
    g_ss_count--;
    return JS_UNDEFINED;
}
static JSValue nb_ss_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { g_ss_count = 0; return JS_UNDEFINED; }
static JSValue nb_ss_key(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int i = js_arg_index(ctx, argv, argc, 0);
    if (i < 0 || i >= g_ss_count) return JS_NULL;
    return JS_NewString(ctx, g_ss[i].key);
}
static JSValue nb_ss_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { return JS_NewInt32(ctx, g_ss_count); }
