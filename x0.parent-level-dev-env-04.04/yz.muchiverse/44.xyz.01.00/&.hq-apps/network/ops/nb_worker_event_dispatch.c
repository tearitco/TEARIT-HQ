/* nb_worker_event_dispatch.c - event dispatch helpers (mechanical extract, single-TU include) */
/* one dispatch level for a single node/window/document: exact evl registrations,
 * then the on-* callback. Each target scans the registers from scratch (an
 * entry's kind+node binds it to exactly one target), so bubbling reaches the
 * document/window levels without a shared cursor skipping their listeners. */
static void dispatch_level(JSContext *ctx, int kind, NbNode *node, JSValue ev,
                           const char *type, int *stopped) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != node) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != node) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
}
static int dispatch_event(JSContext *ctx, int kind, NbNode *node, JSValue ev, int bubbles) {
    JSValue tv = JS_GetPropertyStr(ctx, ev, "type");
    char *to = NULL;
    const char *type = JS_IsString(tv) ? (to = JS_ToCString(ctx, tv)) : NULL;
    JS_FreeValue(ctx, tv);
    if (!type) return 1;
    int stopped = 0;

    if (kind == EVT_NODE && node) {
        JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, node));
        JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, node));
        dispatch_level(ctx, EVT_NODE, node, ev, type, &stopped);
        if (!stopped && bubbles) {               /* bubble node->...->root->document->window */
            NbNode *a = node->parent;
            while (a) {
                if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
                JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, a));
                dispatch_level(ctx, EVT_NODE, a, ev, type, &stopped);
                if (stopped) break;
                a = a->parent;
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
                dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
                dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
            }
        }
    } else if (kind == EVT_DOC) {
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "document"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
        dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
        if (!stopped) {
            JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
            dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
        }
    } else {                                       /* EVT_WIN */
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "window"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
        dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
    }
    JS_FreeCString(ctx, to);
    JSValue dpv = JS_GetPropertyStr(ctx, ev, "defaultPrevented");
    int dp = JS_ToBool(ctx, dpv);
    if (dp < 0) dp = 0;
    JS_FreeValue(ctx, dpv);
    return !dp;
}
