/* nb_worker_timer_loop_helpers.c - timer due/drain/run helpers (mechanical extract, single-TU include) */
static uint64_t timer_min_due(void) {
    uint64_t m = 0; int have = 0;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active) { if (!have || g_timers[i].due < m) { m = g_timers[i].due; have = 1; } }
    return have ? m : 0;
}
/* Drain the native job queue (promise reactions + queueMicrotask callbacks). */
static void drain_jobs(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *jctx;
    while (JS_IsJobPending(rt)) {
        if (g_pending_err) return;
        if (JS_ExecutePendingJob(rt, &jctx) < 0) {
            if (jctx && !g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(jctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            if (g_trace_cb && jctx) {
                char buf[512];
                const char *m = js_error_to_cstr(jctx, buf, sizeof(buf));
                fprintf(stderr, "TRACE_CB| %s\n", m);
            }
        }
    }
}
static int run_due_timers(JSContext *ctx, uint64_t now) {
    int ran = 0;
    for (int i = 0; i < g_timer_count; i++) {
        if (!g_timers[i].active || g_timers[i].due > now) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        long iv = g_timers[i].interval;
        if (invoke_cb0(ctx, g_timers[i].cb)) { g_invocations++; break; }
        g_invocations++; ran = 1;
        if (iv > 0) g_timers[i].due = now + (uint64_t)iv;   /* repeating — re-arm */
        else {
            g_timers[i].active = 0;                          /* oneshot */
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    }
    return ran;
}
