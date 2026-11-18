/* nb_worker_repl_main.c - interactive REPL entry (mechanical extract, single-TU include) */
static int repl_main(void) {
    JSRuntime *rt = JS_NewRuntime();
    nb_watchdog_install(rt);
    if (!rt) return 1;
    JSContext *ctx = rt ? JS_NewContext(rt) : NULL;
    if (!ctx) { if (rt) JS_FreeRuntime(rt); return 1; }
    g_cli = 1; g_cli_log = 1;
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);
    install_host(ctx);
    (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);
    /* CLI-2/CLI-3 in the REPL too: require() + fs work line-by-line,
     * sharing the browser prelude above (window/document still present). */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
    nb_install_fs(ctx);
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "err:%s\n", errbuf[0] ? errbuf : "loader error");
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    char cwd_buf[PATH_MAX];
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) snprintf(cwd_buf, sizeof(cwd_buf), ".");
    JSValue av[2];
    av[0] = JS_NewString(ctx, cwd_buf);
    av[1] = JS_NewString(ctx, "[repl]");
    JSValue g0 = JS_GetGlobalObject(ctx);
    nb_watchdog_begin();
    JSValue rr = JS_Call(ctx, install, g0, 2, av);
    JS_FreeValue(ctx, g0);
    JS_FreeValue(ctx, install);
    nb_watchdog_pause();
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "err:%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JS_FreeValue(ctx, rr);
    static const char empty_html[] = "<html><body></body></html>";
    g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    boot_install_safe(ctx);

    int tty_out = isatty(STDOUT_FILENO);
    char line[8192];
    if (tty_out) { printf("nbjs (duk) — type JS; Ctrl-D or 'exit' to quit\n"); fflush(stdout); }
    for (;;) {
        if (tty_out) { fputs("> ", stdout); fflush(stdout); }
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t ln = strlen(line);
        while (ln && (line[ln - 1] == '\n' || line[ln - 1] == '\r')) line[--ln] = 0;
        if (!ln) continue;
        if (!strcmp(line, "exit") || !strcmp(line, "quit") || !strcmp(line, ".exit")) break;

        /* don't leak listeners/timers across lines */
        free_held_callbacks(ctx);
g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
    g_search_wired = 0;
        g_invocations = 0; g_raf_fires = 0;
        g_pending_err = 0; g_pending_errmsg[0] = 0;

        /* CLI-4: single-line ESM (import/export) → CJS in the REPL too.
         * Multi-line import/export statements are out of scope here. */
        JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
        if (JS_IsFunction(ctx, prep)) {
            JSValue av2[1]; av2[0] = JS_NewString(ctx, line);
            JSValue g1 = JS_GetGlobalObject(ctx);
            JSValue rp = JS_Call(ctx, prep, g1, 1, av2);
            JS_FreeValue(ctx, g1);
            JS_FreeValue(ctx, av2[0]);
            if (!JS_IsException(rp)) {
                if (JS_IsString(rp)) {
                    size_t sl;
                    char *ps = JS_ToCStringLen(ctx, &sl, rp);
                    if (ps) {
                        if (sl < sizeof(line)) { memcpy(line, ps, sl); line[sl] = 0; }
                        JS_FreeCString(ctx, ps);
                    }
                }
            } else {
                JSValue e = JS_GetException(ctx);
                JS_FreeValue(ctx, e);
            }
            JS_FreeValue(ctx, rp);
        }
        JS_FreeValue(ctx, prep);

        JSValue rv = peval_budget_value(ctx, line, strlen(line));
        if (JS_IsException(rv)) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            printf("err:%s\n", m ? m : "script error");
        } else {
            if (!JS_IsUndefined(rv)) {
                char disp[512];
                const char *s = js_display_cstr(ctx, rv, disp, sizeof(disp));
                printf("%s\n", s ? s : "");
            }
        }
        JS_FreeValue(ctx, rv);
        fflush(stdout);

        /* drain jobs + timers (no lifecycle events), CPU-bounded */
        nb_watchdog_begin();
        uint64_t start = now_ms();
        for (int guard = 0; guard < 10000 && !g_pending_err; guard++) {
            if (now_ms() - start > 200) break;
            uint64_t now = now_ms();
            int ran = run_due_timers(ctx, now);
            drain_jobs(ctx);
            if (!ran) {
                uint64_t m = timer_min_due();
                if (!m) break;
                if (m <= now) continue;
                uint64_t d = m - now; if (d > 5) d = 5;
                struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
                nanosleep(&ts, NULL);
            }
        }
        nb_watchdog_pause();
    }
    free_held_callbacks(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    dom_teardown();
    return 0;
}
