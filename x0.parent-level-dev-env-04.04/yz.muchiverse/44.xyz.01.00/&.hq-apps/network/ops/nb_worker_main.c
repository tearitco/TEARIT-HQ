/* nb_worker_main.c - worker main entry and command loop (mechanical extract, single-TU include) */
int main(int argc, char **argv) {
    nb_crash_install();
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */
    if (getenv("NB_TRACE_CB")) g_trace_cb = 1;   /* full callback exceptions to stderr */
    {
        const char *nbw_out = getenv("NBW_CONSOLE");
        if (nbw_out && nbw_out[0]) {
            /* append ("ab") — the manager keeps a live console panel across
             * worker respawns and trims the file itself (trim_tail_file) */
            g_out = fopen(nbw_out, "ab");
            if (g_out) setvbuf(g_out, NULL, _IOLBF, 0);   /* console capture (debug/tests) */
        }
    }

    /* duk -i / --interactive: force the REPL even when stdin is piped
     * (bare `duk` also reaches it on a tty via the isatty check below). */
    if (argc > 1 && (strcmp(argv[1], "-i") == 0 || strcmp(argv[1], "--interactive") == 0))
        return repl_main();

    if (argc > 1) {
        /* duk --browser page.js [fetch.dom] — the released DOM page runner
         * (full DOM engine + render-back). duk file.js — node mode, no
         * browser globals, process/console + require()/fs host. Either way:
         * exit 0 clean / 1 thrown error / 2 usage. No RPC framing, no khtpm
         * dependency. */
        if (strcmp(argv[1], "--browser") == 0 || strcmp(argv[1], "-b") == 0)
            return browser_cli_main(argc - 1, argv + 1);
        return cli_main(argc, argv);
    }

    /* bare `duk` with a terminal as stdin: interactive REPL instead of the
     * framed daemon (the manager spawns over a socketpair -> not a tty). */
    if (isatty(STDIN_FILENO)) return repl_main();

    for (;;) {
        /* A bounded wait is what lets a page keep running between inputs.
         * NB_RECV_SHUTDOWN still ends the loop exactly as before; a budget
         * timeout is not an exit, it is a tick that is due. */
        int why = NB_RECV_SHUTDOWN;
        if (!recv_frame_ex(tick_wait_ms(), &why)) {
            if (why == NB_RECV_SHUTDOWN) break;
            tick_slice();
            continue;
        }

        char *f[8];           split_lines(f);
        const char *cmd = f[0] ? f[0] : "";
        if (strcmp(cmd, "QUIT") == 0) {
            live_teardown();
            break;
        } else if (strcmp(cmd, "LOAD") == 0) {
            g_title[0] = 0; g_href[0] = 0;
            g_style_css[0] = 0;
            if (f[1]) snprintf(g_page_js, sizeof(g_page_js), "%s", f[1]);
            if (f[2]) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", f[2]);
            if (f[3]) snprintf(g_href, sizeof(g_href), "%s", f[3]);
            fprintf(stderr, "DEBUG: LOAD received, g_href = '%s'\n", g_href);
            if (f[4]) snprintf(g_title, sizeof(g_title), "%s", f[4]);
            if (f[5]) snprintf(g_style_css, sizeof(g_style_css), "%s", f[5]);
            /* f[6] = the viewport width this page will be laid out in.
             *
             * Until now the box tree assumed a fixed 640px column while
             * the renderer actually paints the content pane at 389px, so
             * every width the worker reported was wrong by ~250px (G2.2).
             * The manager does not know the pane width yet - it is the
             * RENDERER that lays the pane out - so this field exists and
             * is honoured, and nothing sends it yet. When it is sent, the
             * guess becomes a fact; until then behaviour is unchanged,
             * which is deliberate: a wrong-but-supplied number would be
             * worse than an obviously-default one.
             *
             * Absent or unusable (<= 0) leaves the current value alone
             * rather than resetting to the default mid-session. */
            if (f[6] && atof(f[6]) > 0) g_viewport_px = atof(f[6]);
            /* Field 7: the pane's content height. Width alone cannot say
             * whether the column scrolls, and a scrolling column is 8px
             * narrower once the scrollbar is drawn - see g_col_px(). */
            g_viewport_h = (f[7] && atof(f[7]) > 0) ? atof(f[7]) : 0.0;
            /* The page title, which the manager projects into the content
             * scrolllist as a real row. Without it the worker's origin sits
             * 30px above where the renderer paints the first paragraph. */
            g_page_title[0] = '\0';
            if (f[4]) {
                size_t tl = strlen(f[4]);
                if (tl >= sizeof(g_page_title)) tl = sizeof(g_page_title) - 1;
                memcpy(g_page_title, f[4], tl);
                g_page_title[tl] = '\0';
            }
            /* New page, so the gutter is an open question again. Leaving the
             * previous page's verdict in place would carry a scrollbar
             * decision across a navigation onto a page that may not scroll. */
            g_gutter_state = -1;
            free(g_last_push); g_last_push = NULL;   /* rows are per-page */
            run_page();
            if (g_module_page_loaded) {
                g_module_page_loaded = 0;
                break;  /* exit worker so manager respawns fresh worker */
            }
        } else if (strcmp(cmd, "TICK") == 0) {
            /* TICK\n<ms>\n - enable periodic ticking with stdin waits capped
             * at <ms>. TICK\n0 disables it, restoring the historical
             * indefinite wait. Opt-in: until the manager side of the push
             * protocol is ready, the resident worker must not emit frames
             * nobody asked for. A new page starts ticking only if it was
             * already on - this flag is the driver's, not the page's. */
            int want = f[1] ? atoi(f[1]) : 0;
            if (want < 0) want = 0;
            g_tick_cap_ms = want;
            free(g_last_push); g_last_push = NULL;
            send_status(want ? "STATUS ok" : "STATUS err:TICK wants ms >= 1");
        } else if (strcmp(cmd, "EVAL") == 0) {
            /* devtools console: eval:<js> — js is field 1 (address-bar single
             * line; embedded '\n' is split out by split_lines, fine for REPL) */
            cmd_eval(f[1] ? f[1] : "");
        } else if (strcmp(cmd, "EVENT") == 0) {
            cmd_event(f[1] ? f[1] : "", f[2] ? f[2] : "click");
        } else {
            send_status("STATUS err:unknown command");
        }
    }
    return 0;
}
