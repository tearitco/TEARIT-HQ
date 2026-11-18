/* nb_worker_cli_process.c - CLI process helpers (mechanical extract, single-TU include) */
static JSValue nb_cli_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int code = 0;
    if (argc > 0 && JS_IsNumber(argv[0])) {
        double d;
        if (JS_ToFloat64(ctx, &d, argv[0]) == 0) code = (int)d;
    }
    if (g_out) fflush(g_out);
    exit(code);
    return JS_UNDEFINED;
}
static JSValue nb_cli_cwd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) return JS_NewString(ctx, buf);
    return JS_NewString(ctx, "/");
}
