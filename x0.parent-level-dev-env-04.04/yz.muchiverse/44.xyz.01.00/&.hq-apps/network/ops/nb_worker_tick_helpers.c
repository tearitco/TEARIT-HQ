/* nb_worker_tick_helpers.c - tick loop frame/wait helpers (mechanical extract, single-TU include) */
static void send_render_rows(const char *rows) {
    size_t rn = rows ? strlen(rows) : 0;
    if (!rn || rn >= RENDER_MAX) return;
    size_t pn = 7 + rn;                        /* "RENDER\n" + rows */
    char *pay = malloc(pn + 1);
    if (!pay) return;
    memcpy(pay, "RENDER\n", 7);
    memcpy(pay + 7, rows, rn);
    pay[pn] = 0;
    send_payload(pay, pn);
    free(pay);
}

/* How long the main loop may block on stdin before a tick is due. -1 when
 * ticking is off (the historical indefinite wait), else the gap to the
 * next scheduled timer, capped so a page cannot park the loop for long. */
static int tick_wait_ms(void) {
    if (g_tick_cap_ms <= 0) return -1;
    uint64_t due = timer_min_due();
    if (!due) return g_tick_cap_ms;            /* nothing scheduled */
    uint64_t now = now_ms();
    if (due <= now) return 0;                   /* already due: tick now */
    uint64_t gap = due - now;
    if (gap > (uint64_t)g_tick_cap_ms) gap = (uint64_t)g_tick_cap_ms;
    return (int)gap;
}

/* One tick: fire whatever is due, drain microtasks, push only if the rows
 * actually changed. Returns 1 if a frame went out. */
static int tick_slice(void) {
    if (!g_live_ctx) return 0;
    uint64_t now = now_ms();
    uint64_t due = timer_min_due();
    if (!due || due > now) return 0;            /* nothing due yet */
    run_due_timers(g_live_ctx, now);
    drain_jobs(g_live_ctx);
    if (g_pending_err) return 0;

    SB r = {0, 0, 0};
    dom_render_rows(&r);
    int pushed = 0;
    if (r.s && r.s[0]) {
        if (!g_last_push || strcmp(g_last_push, r.s) != 0) {
            send_render_rows(r.s);
            free(g_last_push);
            g_last_push = r.s;
            r.s = NULL;                        /* ownership moved */
            pushed = 1;
        }
    }
    free(r.s);
    return pushed;
}
