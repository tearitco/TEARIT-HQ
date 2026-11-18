/* nb_worker_run_event_loop.c - main event loop drain helper (mechanical extract, single-TU include) */
static int run_event_loop(JSContext *ctx) {
    /* The page's timer queue can keep this loop busy for minutes under the
     * resident worker (no JIT), and the manager guards its LOAD read with a
     * 3s stall watchdog. Emit a LIVE| beat every ~1.8s so a slow-but-alive
     * drain is never mistaken for a dead worker; RENDER still follows once
     * the loop is actually quiescent. */
    static uint64_t keep_last = 0;
    #define NB_KEEPALIVE_LIVE() do { \
        if (!g_cli) { \
            uint64_t _k = now_ms(); \
            if (_k - keep_last >= 1800) { \
                keep_last = _k; \
                char _b[96]; \
                int _n = snprintf(_b, sizeof(_b), "LIVE|drain|%u", g_invocations); \
                send_payload(_b, (size_t)_n); \
            } \
        } \
    } while (0)
    nb_watchdog_begin();   /* phase-1 backstop also covers timer/job callbacks */
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_DOC, NULL, "DOMContentLoaded");
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_WIN, NULL, "load");
    drain_jobs(ctx);
    uint64_t start = now_ms();
    for (int guard = 0; guard < 100000 && !g_pending_err; guard++) {
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        if (now_ms() - start > MAX_DRAIN_MS) break;   /* bound page_load wait */
        NB_KEEPALIVE_LIVE();
        uint64_t now = now_ms();
        int ran = run_due_timers(ctx, now);
        drain_jobs(ctx);
        if (!ran) {
            uint64_t m = timer_min_due();
            if (!m) break;                    /* nothing scheduled — quiescent */
            if (m <= now) continue;           /* due but callback skipped? re-drain */
            /* next timer in the future — sleep up to it (bounded by MAX_DRAIN_MS) */
            uint64_t d = m - now;
            if (d > 5) d = 5;
            struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
            nanosleep(&ts, NULL);
            continue;
        }
    }
    nb_watchdog_pause();
    nb_wire_search_proxy(ctx);
    return g_pending_err;
}
