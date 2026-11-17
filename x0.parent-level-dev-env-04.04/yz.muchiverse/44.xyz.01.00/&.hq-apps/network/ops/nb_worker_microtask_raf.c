/* nb_worker_microtask_raf.c - microtask queue and rAF bindings (mechanical extract, single-TU include) */
/* A queueMicrotask() callback, executed by the QuickJS native job queue
 * (JS_EnqueueJob) inside run_event_loop's drain; `this` = globalThis. */
static JSValue microtask_job(JSContext *ctx, int argc, JSValueConst *argv) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, argv[0], g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        JS_FreeValue(ctx, r);
    } else {
        JS_FreeValue(ctx, r);
    }
    return JS_UNDEFINED;
}
static JSValue nb_queueMicrotask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        JS_ThrowTypeError(ctx, "queueMicrotask requires a function argument");
        return JS_EXCEPTION;
    }
    if (JS_EnqueueJob(ctx, microtask_job, 1, argv) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}
static JSValue nb_raf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc > 0 && JS_IsFunction(ctx, argv[0]) && g_raf_fires < MAX_RAF_FRAMES) {
        g_raf_fires++;
        timer_schedule(ctx, argv[0], RAF_MS, 0);
    }
    return JS_UNDEFINED;
}
