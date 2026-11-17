/* nb_worker_script_src_get.c - script src getter binding (mechanical extract, single-TU include) */
static JSValue nb_script_src_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    const char *s = script_get_src(n);
    return JS_NewString(ctx, s ? s : "");
}
