/* nb_worker_eval_budget.c - script eval budget helpers (mechanical extract, single-TU include) */
/* plan step 5: CPU budget for script eval. If page.js burns through
 * EVAL_BUDGET_SEC of CPU (while(true) {} and friends) SIGALRM fires while
 * the engine is running; the deadly default _exit kills the worker mid-eval, the
 * manager sees the socket close and respawns on the next LOAD. */
/* Evaluate src with a SIGALRM CPU budget. Returns 0 on success; on error
 * returns 1 and, if errbuf is non-NULL, formats the exception message into
 * it (the exception is always consumed/cleared). The completion value is
 * discarded — use peval_budget_value() when the result is wanted. */
static int peval_budget(JSContext *ctx, const char *src, size_t src_n,
                        char *errbuf, size_t errlen) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    nb_watchdog_pause();
    if (JS_IsException(r)) {
        if (errbuf && errlen) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            snprintf(errbuf, errlen, "%s", m);
        } else {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
/* Like peval_budget but returns the completion JSValue (owned; freed by the
 * caller — also on JS_EXCEPTION). No exception is consumed here. */
static JSValue peval_budget_value(JSContext *ctx, const char *src, size_t src_n) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    nb_watchdog_pause();
    return r;
}
/* Like peval_budget but uses JS_EVAL_TYPE_MODULE for ES module scripts.
 * Modules without imports work; imports need a module loader (not yet set). */
static int peval_budget_module(JSContext *ctx, const char *src, size_t src_n,
                               char *errbuf, size_t errlen) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<module>", JS_EVAL_TYPE_MODULE);
    nb_watchdog_pause();
    if (JS_IsException(r)) {
        if (errbuf && errlen) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            snprintf(errbuf, errlen, "%s", m);
        } else {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
