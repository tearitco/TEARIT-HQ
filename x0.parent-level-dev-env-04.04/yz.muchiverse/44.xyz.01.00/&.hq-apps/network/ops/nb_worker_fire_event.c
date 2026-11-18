/* nb_worker_fire_event.c - legacy/simple event firing helper (mechanical extract, single-TU include) */
static void fire_event(JSContext *ctx, int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != n) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        if (kind == EVT_NODE && n) JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, n));
        else JS_SetPropertyStr(ctx, ev, "target",
             get_global_attr(ctx, kind == EVT_WIN ? "window" : "document"));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
    /* lifecycle on-* props: window.onload, document.onDOMContentLoaded (commit 8) */
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != n) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
}
