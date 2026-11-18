/* nb_worker_cjs_read_file.c - CJS module file reader (mechanical extract, single-TU include) */
/* CLI-2: read a module file for require(). Returns the source string, or
 * undefined if the path is missing/unreadable (loader turns that into a
 * "Cannot find module" error). Same 512 kB cap as the page loader. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    JSValue r;
    if (p && p[0] && read_file(p, &s, &n)) {
        r = JS_NewStringLen(ctx, s, n);
        free(s);
    } else {
        r = JS_UNDEFINED;
    }
    JS_FreeCString(ctx, own);
    return r;
}
