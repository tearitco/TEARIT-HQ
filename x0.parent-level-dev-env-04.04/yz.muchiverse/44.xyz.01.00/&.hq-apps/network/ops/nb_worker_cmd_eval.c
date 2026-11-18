/* nb_worker_cmd_eval.c - devtools EVAL command handler (mechanical extract, single-TU include) */
/* devtools console EVAL: run a <js> snippet against the resident page heap
 * (g_live_ctx). Source + result (+ any console.* noise) echo to the console
 * capture (g_out / NBW_CONSOLE); post-eval DOM rows are re-emitted so JS
 * mutations show in the browser; an eval-triggered navigation ships a NAV
 * frame. Budget-guarded like page scripts (peval_budget / EVAL_BUDGET_SEC). */
static void cmd_eval(const char *js) {
    if (!g_live_ctx) { send_status("STATUS err:no page loaded"); return; }
    if (g_out) { fprintf(g_out, ">%s\n", js); fflush(g_out); }
    JSValue rv = peval_budget_value(g_live_ctx, js, strlen(js));
    if (JS_IsException(rv)) {
        char tmp[512];
        const char *m = js_error_to_cstr(g_live_ctx, tmp, sizeof(tmp));
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "eval error");
        if (g_out) { fprintf(g_out, "!>%s\n", m ? m : "eval error"); fflush(g_out); }
        JS_FreeValue(g_live_ctx, rv);
        send_status(msg);
        return;
    }
    char disp[512];
    const char *r = js_display_cstr(g_live_ctx, rv, disp, sizeof(disp));
    if (g_out) { fprintf(g_out, "=>%s\n", r ? r : ""); fflush(g_out); }
    JS_FreeValue(g_live_ctx, rv);
    /* Microtasks (incl. MutationObserver delivery) drain before render,
     * as in a browser task boundary. */
    drain_jobs(g_live_ctx);

    /* re-emit post-eval RENDER rows so mutated DOM is reflected */
    SB rr = {0, 0, 0};
    dom_render_rows(&rr);
    if (rr.s && rr.s[0]) {
        size_t rn = strlen(rr.s);
        if (rn < RENDER_MAX) {
            char *pay = malloc(7 + rn + 1);
            if (pay) {
                memcpy(pay, "RENDER\n", 7);
                memcpy(pay + 7, rr.s, rn);
                pay[7 + rn] = 0;
                send_payload(pay, 7 + rn);
                free(pay);
            }
        }
    }
    free(rr.s);

    /* an eval can navigate too (e.g. location.href = "...") */
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                          g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay))
            send_payload(pay, (size_t)pn);
    }
    send_status("STATUS ok");
}
