/* nb_worker_classlist.c - classList native methods (mechanical extract, single-TU include) */
/* ---- classList natives (this = the classList object, shares \xffnode) ---- */
static JSValue nb_cl_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok || !*tok) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    if (!has_class(n, tok)) {
        SB b = {0, 0, 0};
        if (n->cls && *n->cls) { sb_put(&b, n->cls); sb_put(&b, " "); }
        sb_put(&b, tok);
        free(n->cls); n->cls = b.s;
    }
    JS_FreeCString(ctx, tk);
    return JS_UNDEFINED;
}
static JSValue nb_cl_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    if (!has_class(n, tok)) { JS_FreeCString(ctx, tk); return JS_UNDEFINED; }
    SB b = {0, 0, 0};
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) {
        if (strcmp(c, tok)) { if (b.len) sb_put(&b, " "); sb_put(&b, c); }
        c = strtok(NULL, " ");
    }
    free(n->cls); n->cls = b.s ? b.s : strdup("");
    JS_FreeCString(ctx, tk);
    return JS_UNDEFINED;
}
static JSValue nb_cl_toggle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !tok || !*tok) { JS_FreeCString(ctx, tk); return JS_NewBool(ctx, 0); }
    if (has_class(n, tok)) { nb_cl_remove(ctx, this_val, argc, argv); JS_FreeCString(ctx, tk); return JS_NewBool(ctx, 0); }
    nb_cl_add(ctx, this_val, argc, argv);
    JS_FreeCString(ctx, tk);
    return JS_NewBool(ctx, 1);
}
static JSValue nb_cl_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *tk = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (tk = JS_ToCString(ctx, argv[0])) : NULL;
    int r = n && tok ? has_class(n, tok) : 0;
    JS_FreeCString(ctx, tk);
    return JS_NewBool(ctx, r);
}
static JSValue nb_cl_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int32_t idx = 0;
    if (argc > 0) JS_ToInt32(ctx, &idx, argv[0]);
    if (!n || !n->cls || idx < 0) return JS_NULL;
    char copy[512]; size_t cl = strlen(n->cls); if (cl > 511) cl = 511;
    memcpy(copy, n->cls, cl); copy[cl] = 0;
    char *c = strtok(copy, " ");
    while (c) {
        if (!idx) { JSValue r = JS_NewString(ctx, c); return r; }
        idx--;
        c = strtok(NULL, " ");
    }
    return JS_NULL;
}
