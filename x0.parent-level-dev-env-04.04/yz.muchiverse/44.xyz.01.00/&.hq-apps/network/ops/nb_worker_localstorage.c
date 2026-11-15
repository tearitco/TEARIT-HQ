/* nb_worker_localstorage.c - localStorage implementation helpers (mechanical extract, single-TU include) */
static void ls_save_file(const StEnt *ents, int n) {
    if (!g_ls_path[0]) return;
    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", g_ls_path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) { *slash = 0; if (slash != dirbuf) mkdir_p(dirbuf); }
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        char k[256 * 3 + 1], v[4096 * 3 + 1];
        pct_encode(ents[i].key, k, sizeof(k));
        pct_encode(ents[i].value, v, sizeof(v));
        sb_put(&b, k); sb_put(&b, "\t"); sb_put(&b, v); sb_put(&b, "\n");
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_ls_path);
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (b.s && b.len) fwrite(b.s, 1, b.len, f);
        fclose(f);
        rename(tmp, g_ls_path);
    }
    free(b.s);
}

static int ls_find(int n, const char *key) {
    for (int i = 0; i < n; i++) if (strcmp(g_ls[i].key, key) == 0) return i;
    return -1;
}
static JSValue nb_ls_getItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); return JS_NULL; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_NULL;
    return JS_NewString(ctx, g_ls[f].value);
}
static JSValue nb_ls_setItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL, *vo = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    const char *val = js_arg_str_any(ctx, argv, argc, 1, &vo);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo); return JS_UNDEFINED; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    if (f >= 0) {
        snprintf(g_ls[f].value, sizeof(g_ls[f].value), "%s", val);
    } else if (n < ST_MAX_ENT) {
        StEnt *e = &g_ls[n++];
        memset(e, 0, sizeof(*e));
        snprintf(e->key, sizeof(e->key), "%s", key);
        snprintf(e->value, sizeof(e->value), "%s", val);
    } else {
        JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
        return JS_UNDEFINED;
    }
    ls_save_file(g_ls, n);
    JS_FreeCString(ctx, ko); JS_FreeCString(ctx, vo);
    return JS_UNDEFINED;
}
static JSValue nb_ls_removeItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *ko = NULL;
    const char *key = js_arg_str(ctx, argv, argc, 0, &ko);
    if (!g_ls_path_set) ls_jar_init();
    if (!key || !g_ls_path[0]) { JS_FreeCString(ctx, ko); return JS_UNDEFINED; }
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    int f = ls_find(n, key);
    JS_FreeCString(ctx, ko);
    if (f < 0) return JS_UNDEFINED;
    for (int i = f; i + 1 < n; i++) g_ls[i] = g_ls[i + 1];
    ls_save_file(g_ls, n - 1);
    return JS_UNDEFINED;
}
static JSValue nb_ls_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_ls_path_set) ls_jar_init();
    if (g_ls_path[0]) ls_save_file(g_ls, 0);
    return JS_UNDEFINED;
}
static JSValue nb_ls_key(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int i = js_arg_index(ctx, argv, argc, 0);
    if (!g_ls_path_set) ls_jar_init();
    if (!g_ls_path[0] || i < 0) return JS_NULL;
    int n = st_load_file(g_ls_path, g_ls, ST_MAX_ENT);
    if (i >= n) return JS_NULL;
    return JS_NewString(ctx, g_ls[i].key);
}
static JSValue nb_ls_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_ls_path_set) ls_jar_init();
    int n = g_ls_path[0] ? st_load_file(g_ls_path, g_ls, ST_MAX_ENT) : 0;
    return JS_NewInt32(ctx, n);
}
