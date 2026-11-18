/* nb_worker_cmd_event.c - EVENT RPC command handler (mechanical extract, single-TU include) */
/* Commit 8 (Rung 6 slice 1): EVENT RPC — user click in window reaches scripted el.
 * Manager -> worker: EVENT\n<selector>\n<type>  (type defaults to click, bubbles+cancelable).
 * Dispatch is via JS (document.querySelector + new Event) so on-* and bubbling reuse the
 * existing dispatch_event path. After dispatch we drain microtasks/timers and re-emit
 * RENDER so mutations show, same as EVAL. */
static void cmd_event(const char *selector, const char *type) {
    if (!g_live_ctx) { send_status("STATUS err:no page loaded"); return; }
    if (!selector || !selector[0]) { send_status("STATUS err:empty selector"); return; }
    const char *evtype = (type && type[0]) ? type : "click";
    JSContext *ctx = g_live_ctx;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__nb_event_selector", JS_NewString(ctx, selector));
    JS_SetPropertyStr(ctx, global, "__nb_event_type", JS_NewString(ctx, evtype));
    JS_FreeValue(ctx, global);
    const char *js = "var __nb_el=document.querySelector(__nb_event_selector);"
                     "if(!__nb_el) throw new Error('not found:'+__nb_event_selector);"
                     "var __nb_ev=new Event(__nb_event_type,{bubbles:true,cancelable:true});"
                     "__nb_el.dispatchEvent(__nb_ev); __nb_ev.type;";
    JSValue rv = peval_budget_value(ctx, js, strlen(js));
    JSValue g2 = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g2, "__nb_event_selector", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, g2, "__nb_event_type", JS_UNDEFINED);
    JS_FreeValue(ctx, g2);
    if (JS_IsException(rv)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "event error");
        JS_FreeValue(ctx, rv);
        send_status(msg);
        return;
    }
    JS_FreeValue(ctx, rv);
    drain_jobs(ctx);
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    run_due_timers(ctx, now);
    drain_jobs(ctx);
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
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind, g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay))
            send_payload(pay, (size_t)pn);
    }
    send_status("STATUS ok");
}
