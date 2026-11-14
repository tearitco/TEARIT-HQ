/* nb_worker_click.c - element click/change synthetic event helpers (mechanical extract, single-TU include) */
static JSValue nb_el_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    JSValue Event = get_global_attr(ctx, "Event");
    if (!JS_IsFunction(ctx, Event)) { JS_FreeValue(ctx, Event); return JS_UNDEFINED; }
    JSValue opts = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, opts, "bubbles", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, opts, "cancelable", JS_NewBool(ctx, 1));
    JSValue cargv[2];
    cargv[0] = JS_NewString(ctx, "click");
    cargv[1] = opts;
    JSValue ev = JS_CallConstructor(ctx, Event, 2, cargv);
    JS_FreeValue(ctx, Event);
    JS_FreeValue(ctx, cargv[0]);
    JS_FreeValue(ctx, opts);
    if (JS_IsException(ev)) { JS_FreeValue(ctx, ev); return JS_UNDEFINED; }
    int r = dispatch_event(ctx, EVT_NODE, n, ev, 1);
    JS_FreeValue(ctx, ev);
    return JS_NewBool(ctx, r);
}
/* select/option state (2026-10-05): a script-driven selection is a user
 * interaction, so it fires `change` — the only event dropdown-driven pages
 * listen for. Same Event-constructor shape as nb_el_click() above; `change`
 * bubbles and is not cancelable. */
static void fire_change(JSContext *ctx, NbNode *n) {
    JSValue Event = get_global_attr(ctx, "Event");
    if (!JS_IsFunction(ctx, Event)) { JS_FreeValue(ctx, Event); return; }
    JSValue opts = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, opts, "bubbles", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, opts, "cancelable", JS_NewBool(ctx, 0));
    JSValue cargv[2];
    cargv[0] = JS_NewString(ctx, "change");
    cargv[1] = opts;
    JSValue ev = JS_CallConstructor(ctx, Event, 2, cargv);
    JS_FreeValue(ctx, Event);
    JS_FreeValue(ctx, cargv[0]);
    JS_FreeValue(ctx, opts);
    if (JS_IsException(ev)) { JS_FreeValue(ctx, ev); return; }
    dispatch_event(ctx, EVT_NODE, n, ev, 1);
    JS_FreeValue(ctx, ev);
}
