/* nb_worker_invoke_cb0.c - 0-arg callback invocation helper (mechanical extract, single-TU include) */
static int invoke_cb0(JSContext *ctx, JSValue cb) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, cb, g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        /* diagnosis aid: NB_TRACE_CB=1 prints the FULL callback exception
         * (message + stack when NB_STACK=1) to stderr, even when the page's
         * own error handler swallows it into a truncated console line. */
        if (g_trace_cb) {
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            fprintf(stderr, "TRACE_CB| %s\n", m);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
