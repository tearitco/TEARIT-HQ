/* nb_worker_run_page.c - page load/run helper (mechanical extract, single-TU include) */
static void run_page(void) {
    /* phase-2 (commit 7): per-page event/timer/microtask state. The previous
     * LOAD's held callbacks belong to the old heap — release them FIRST, then
     * reset the counters so live_teardown()'s free_held_callbacks is a no-op. */
    if (g_live_ctx) free_held_callbacks(g_live_ctx);
    g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
    g_next_id = 1; g_invocations = 0; g_raf_fires = 0;
    g_pending_err = 0; g_pending_errmsg[0] = 0;
    g_scroll_x = 0; g_scroll_y = 0;

    /* rung-6 slice 2: only the daemon (manager) can act on navigation. */
    g_nav_kind[0] = 0; g_nav_url[0] = 0; g_nav_count = 1;
    g_nav_emit = !g_cli;

    /* rung-6: sessionStorage is per-LOAD — fresh session for this page. */
    g_ss_count = 0;

    /* devtools console EVAL: a previous LOAD may still have its resident
     * heap + DOM alive; tear it down before building this page. */
    live_teardown();

    g_dom_root = NULL;
    g_orphans = NULL;
    g_css = NULL;
    node_index_reset();

    /* build the DOM tree from the manager's fetch.dom (if present) */
    fprintf(stderr, "DEBUG: g_fetch_dom = '%s'\n", g_fetch_dom);
    if (g_fetch_dom[0]) {
        FILE *df = fopen(g_fetch_dom, "rb");
        if (!df) {
            /* SILENT BEFORE (2026-10-13): a fetch.dom the worker cannot
             * open left g_dom_root NULL and the page rendered as its
             * title only, with STATUS ok. */
            char m[900];
            snprintf(m, sizeof(m),
                     "DOM: cannot open fetch.dom (%s) - the page will render as its title only",
                     g_fetch_dom);
            send_worker_error(m);
        } else {
            fprintf(stderr, "DEBUG: loading DOM from fetch.dom\n");
            g_dom_root = nb_dom_load(df);
            fclose(df);
            fprintf(stderr, "DEBUG: nb_dom_load returned %p\n", (void*)g_dom_root);
            if (!g_dom_root) {
                /* Same silence, same consequence. nb_dom_load() reads the
                 * nb_serialize() wire format, not HTML - anything else
                 * (an HTML file, a stray comment at the top) returns
                 * NULL, which until now looked exactly like a page with
                 * no content. */
                send_worker_error("DOM: fetch.dom did not deserialize "
                                  "(nb_dom_load returned NULL; the file must be "
                                  "nb_serialize output, not HTML) - the page will "
                                  "render as its title only");
            } else {
                fprintf(stderr, "DEBUG: DOM loaded, root tag: %s, children: %d\n", g_dom_root->tag ? g_dom_root->tag : "NULL", g_dom_root->first_child ? 1 : 0);
                if (g_dom_root->first_child) {
                    for (NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
                        fprintf(stderr, "DEBUG: child tag: %s\n", c->tag ? c->tag : "NULL");
                    }
                }
            }
        }
    } else if (g_cli) {
        /* standalone nbjs: give the page an empty document/body */
        static const char empty_html[] = "<html><body></body></html>";
        g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    }

    /* rung 7: parse the LOAD-delivered stylesheet into a rule cache.
     * nb_css_parse accepts empty files (nr==0), so no special casing
     * needed when there are no inline <style> blocks. */
    if (g_style_css[0]) {
        char *sc = NULL; size_t sn = 0;
        if (read_file(g_style_css, &sc, &sn)) {
            /* G2.5 (2026-11-04): parse WITH the viewport the page was laid
             * out in, so an @media block whose query matches becomes real
             * rules. g_viewport_px is set by the LOAD handler (field 6, the
             * pane's content width) before run_page() is called, and it is
             * the same value window.innerWidth reports - one viewport, one
             * number, so a page's own JS and its CSS cannot disagree about
             * how wide it is. viewport 0 (unmeasured) keeps the old
             * skip-every-@media behaviour. */
            g_css = nb_css_parse_vp(sc, sn, (int)g_viewport_px);
            free(sc);
        }
    }

    /* Read page.js FIRST to detect modules BEFORE creating runtime. */
    char *src = NULL;
    size_t src_n = 0;
    if (!read_file_big(g_page_js, &src, &src_n)) {
        send_status("STATUS err:cannot read page.js");
        live_teardown();
        return;
    }
    if (src_n == 0) { free(src); send_status("STATUS ok"); return; }

    /* Detect if page has any module scripts (by checking for marker). */
    int has_module = 0;
    if (src_n >= 15 && memcmp(src, "/*nbjs-module*/", 15) == 0) has_module = 1;
    else {
        const char *p = src;
        for (; p + 15 < src + src_n; p++) {
            if (p[0] == '/' && p[1] == '*' && memcmp(p, "/*nbjs-module*/", 15) == 0) {
                has_module = 1; break;
            }
        }
    }

    if (has_module) {
        /* Module page: use a completely isolated, non-resident runtime.
         * Module evaluation corrupts the runtime (GC list not empty), so we
         * create a fresh runtime, run the page, and free it immediately.
         * No resident context = no EVAL support for module pages. */
        JSRuntime *rt = JS_NewRuntime();
        nb_watchdog_install(rt);
        if (!rt) { free(src); send_status("STATUS err:heap"); return; }
        JSContext *ctx = JS_NewContext(rt);
        if (!ctx) { JS_FreeRuntime(rt); free(src); send_status("STATUS err:heap"); return; }
        install_host(ctx);

        /* rung 7: the worker's CSS resolve wins over install_host's
         * minimal __nb_ges default (the prelude reads __nb_ges at call time). */
        JSValue ge = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, ge, "__nb_ges",
                          JS_NewCFunction(ctx, nb_ges_rich, "__nb_ges", 1));
        JS_FreeValue(ctx, ge);

        /* rung-6 prelude: swallow its own failure, page continues */
        (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);

        boot_install_safe(ctx);

        /* For module pages in isolated runtime: strip 'export ' and module markers,
         * evaluate as classic script to avoid JS_EVAL_TYPE_MODULE GC corruption. */
        {
            char *buf = malloc(src_n + 1);
            if (buf) {
                size_t j = 0;
                for (size_t i = 0; i < src_n; ) {
                    if (i + 7 <= src_n && memcmp(src + i, "export ", 7) == 0) {
                        i += 7;
                    } else if (i + 6 <= src_n && memcmp(src + i, "export", 6) == 0 &&
                               (i+6 == src_n || isspace((unsigned char)src[i+6]))) {
                        i += 6;
                    } else if (i + 15 <= src_n && memcmp(src + i, "/*nbjs-module*/", 15) == 0) {
                        i += 15;
                        /* skip optional whitespace/newline after marker */
                        while (i < src_n && (src[i] == '\n' || src[i] == '\r' || src[i] == ' ' || src[i] == '\t'))
                            i++;
                    } else {
                        buf[j++] = src[i++];
                    }
                }
                buf[j] = 0;
                run_scripts_slices(ctx, buf, j);
                free(buf);
            } else {
                run_scripts_slices(ctx, src, src_n);
            }
        }
        free(src);

        /* phase-2: lifecycle + event loop — timers/microtasks now fire */
        int ev_err = run_event_loop(ctx);
        if (ev_err) {
            char msg[1100];
            snprintf(msg, sizeof(msg), "STATUS err:%s",
                     g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
            send_status(msg);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            return;
        }

        /* Emit RENDER rows from the post-JS DOM, then STATUS. */
        SB r = {0, 0, 0};
        dom_render_rows(&r);
        if (r.s && r.s[0]) {
            size_t rn = strlen(r.s);
            char *pay = NULL;
            size_t pn = 0;
            if (rn < RENDER_MAX) {
                pn = 7 + rn;
                pay = malloc(pn + 1);
                if (pay) {
                    memcpy(pay, "RENDER\n", 7);
                    memcpy(pay + 7, r.s, rn);
                    pay[pn] = 0;
                    send_payload(pay, pn);
                }
            }
            free(pay);
        }
        free(r.s);

        /* Navigation handling */
        if (g_nav_emit && g_nav_kind[0]) {
            char pay[4600];
            int pn = 0;
            if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                              g_nav_count > 0 ? g_nav_count : 1);
            else
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
            if (pn > 0 && pn < (int)sizeof(pay)) {
                char *ppay = malloc((size_t)pn + 1);
                if (ppay) {
                    memcpy(ppay, pay, (size_t)pn); ppay[pn] = 0;
                    send_payload(ppay, (size_t)pn);
                    free(ppay);
                }
            }
        }

        /* Free the isolated runtime immediately - no resident context */
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        g_module_page_loaded = 1;
        send_status("STATUS ok (no EVAL for module pages)");
    } else {
        /* Classic page: use the standard resident runtime for EVAL support. */
        JSRuntime *rt = JS_NewRuntime();
        nb_watchdog_install(rt);
        if (!rt) { free(src); live_teardown(); send_status("STATUS err:heap"); return; }
        JSContext *ctx = JS_NewContext(rt);
        if (!ctx) { JS_FreeRuntime(rt); free(src); live_teardown(); send_status("STATUS err:heap"); return; }
        g_live_rt = rt;
        g_live_ctx = ctx;   /* resident from here on — kept alive for EVAL */
        install_host(ctx);

        /* rung 7: the resident worker's CSS resolve wins over install_host's
         * minimal __nb_ges default (the prelude reads __nb_ges at call time). */
        JSValue ge = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, ge, "__nb_ges",
                          JS_NewCFunction(ctx, nb_ges_rich, "__nb_ges", 1));
        JS_FreeValue(ctx, ge);

        /* rung-6 prelude: swallow its own failure, page continues */
        (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);

        boot_install_safe(ctx);

        run_scripts_slices(ctx, src, src_n);
        free(src);

        /* phase-2 (commit 7): lifecycle + event loop — timers/microtasks now fire */
        int ev_err = run_event_loop(ctx);
        if (ev_err) {
            char msg[1100];
            snprintf(msg, sizeof(msg), "STATUS err:%s",
                     g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
            send_status(msg);
            live_teardown();
            return;
        }

        /* step 4: emit RENDER rows from the post-JS DOM, then STATUS. The
         * manager merges these into page.state.txt so JS mutations show. */
        SB r = {0, 0, 0};
        dom_render_rows(&r);
        if (r.s && r.s[0]) {
            size_t rn = strlen(r.s);
            char *pay = NULL;
            size_t pn = 0;
            if (rn < RENDER_MAX) {
                pn = 7 + rn;                       /* "RENDER\n" + rows */
                pay = malloc(pn + 1);
                if (pay) {
                    memcpy(pay, "RENDER\n", 7);
                    memcpy(pay + 7, r.s, rn);
                    pay[pn] = 0;
                    send_payload(pay, pn);
                }
            }
            free(pay);
        }
        free(r.s);

        /* rung-6 slice 2: if the page asked to navigate, ship a NAV frame so the
         * manager follows it through its own fetch/stack machinery (manager
         * worker_load captures it, the main loop consumes it next tick). */
        if (g_nav_emit && g_nav_kind[0]) {
            char pay[4600];
            int pn = 0;
            if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')  /* BACK/FORWARD: step count */
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                              g_nav_count > 0 ? g_nav_count : 1);
            else
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
            if (pn > 0 && pn < (int)sizeof(pay)) {
                char *ppay = malloc((size_t)pn + 1);
                if (ppay) {
                    memcpy(ppay, pay, (size_t)pn); ppay[pn] = 0;
                    send_payload(ppay, (size_t)pn);
                    free(ppay);
                }
            }
        }

        /* devtools console EVAL: keep the heap + DOM tree resident so an
         * eval:<js> snippet can run against this page context (see cmd_eval);
         * live_teardown() reclaims it at the next LOAD / QUIT / error. */
        send_status("STATUS ok");
    }
}
