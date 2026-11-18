/* nb_worker_cli_stdio.c - CLI stdio helpers (mechanical extract, single-TU include) */
/* ---- CLI-1: node-like runner (nbjs file.js [args...]) ---- */
static JSValue nb_cli_stdout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (g_out) { fputs(s ? s : "", g_out); fflush(g_out); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static JSValue nb_cli_stderr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (s) { fputs(s, stderr); fflush(stderr); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
/* console.error -> stderr (node parity; log/info/warn stay on stdout) */
static JSValue nb_cli_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        char *own = NULL;
        const char *s = (own = JS_ToCString(ctx, argv[i]));
        if (s) fputs(s, stderr);
        JS_FreeCString(ctx, own);
        if (i + 1 < argc) fputs(" ", stderr);
    }
    fputs("\n", stderr);
    fflush(stderr);
    return JS_UNDEFINED;
}
