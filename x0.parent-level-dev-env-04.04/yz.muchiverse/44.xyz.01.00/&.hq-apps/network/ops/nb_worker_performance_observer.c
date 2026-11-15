/* nb_worker_performance_observer.c - PerformanceObserver mark delivery (mechanical extract, single-TU include) */
/* ---- PerformanceObserver: mark delivery (2026-11-09) ----
 * The prior class was an inert plain stub: observe() did not exist, so
 * web-vitals-style code crashed on `new PerformanceObserver(cb)`. This
 * delivers real entries for User Timing marks (the only entry type this
 * engine produces) on a queued microtask, with buffered replay of marks
 * made before observe(). Other entryTypes are accepted but simply never
 * match; metric receivers checking for absent LCP/CLS data treat empty
 * entries honestly. */
#define NB_PO_MAX_OBS 32
#define NB_PO_MAX_TYPES 8
typedef struct {
    JSValue self;
    JSValue callback;
    char types[NB_PO_MAX_TYPES][32];
    int type_count;
    int uses_mark;
    int consumed; /* index into g_perf_mark_n already queued for this observer */
    int scheduled;
} NbPoObserver;
static NbPoObserver g_po_obs[NB_PO_MAX_OBS];
static int g_po_obs_count = 0;
static JSValue nb_po_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue obj = class_noop(ctx, this_val, 0, argv);
    if (!JS_IsObject(obj)) return obj;
    if (argc > 0 && JS_IsFunction(ctx, argv[0]))
        JS_SetPropertyStr(ctx, obj, "__nb_po_callback", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, obj, "__nb_po_records", JS_NewArray(ctx));
    return obj;
}
static int nb_po_find_observer(JSContext *ctx, JSValueConst self) {
    for (int i = 0; i < g_po_obs_count; i++)
        if (JS_StrictEq(ctx, g_po_obs[i].self, self)) return i;
    return -1;
}
static JSValue nb_po_records_for(JSContext *ctx, JSValue self) {
    JSValue recs = JS_GetPropertyStr(ctx, self, "__nb_po_records");
    if (!JS_IsArray(ctx, recs)) {
        JS_FreeValue(ctx, recs);
        recs = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, self, "__nb_po_records", JS_DupValue(ctx, recs));
    }
    return recs;
}
static void nb_po_push_mark(JSContext *ctx, NbPoObserver *o, int idx) {
    if (idx < 0 || idx >= g_perf_mark_n) return;
    JSValue recs = nb_po_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    JSValue e = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, e, "name", JS_NewString(ctx, g_perf_mark_name[idx]));
    JS_SetPropertyStr(ctx, e, "entryType", JS_NewString(ctx, "mark"));
    JS_SetPropertyStr(ctx, e, "startTime", JS_NewFloat64(ctx, g_perf_mark_t[idx]));
    JS_SetPropertyStr(ctx, e, "duration", JS_NewFloat64(ctx, 0));
    JS_SetPropertyUint32(ctx, recs, n, e);
    JS_FreeValue(ctx, recs);
}
static void nb_po_flush(JSContext *ctx, NbPoObserver *o) {
    JSValue recs = nb_po_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    if (n == 0 || !JS_IsFunction(ctx, o->callback)) {
        JS_FreeValue(ctx, recs);
        return;
    }
    JSValue recarg = JS_DupValue(ctx, recs);
    JSValue selfarg = JS_DupValue(ctx, o->self);
    JSValue av[2] = { recarg, selfarg };
    JSValue r = JS_Call(ctx, o->callback, o->self, 2, av);
    if (JS_IsException(r)) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, r);
    JS_FreeValue(ctx, recarg);
    JS_FreeValue(ctx, selfarg);
    JS_FreeValue(ctx, recs);
    JS_SetPropertyStr(ctx, o->self, "__nb_po_records", JS_NewArray(ctx));
}
static JSValue nb_po_flush_job(JSContext *ctx, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    int oi = nb_po_find_observer(ctx, argv[0]);
    if (oi < 0) return JS_UNDEFINED;
    g_po_obs[oi].scheduled = 0;
    nb_po_flush(ctx, &g_po_obs[oi]);
    return JS_UNDEFINED;
}
static void nb_po_schedule(JSContext *ctx, NbPoObserver *o) {
    if (!o->scheduled) {
        o->scheduled = 1;
        JSValue jv[1] = { o->self };
        if (JS_EnqueueJob(ctx, nb_po_flush_job, 1, jv) < 0) {
            o->scheduled = 0;
            nb_po_flush(ctx, o);
        }
    }
}
static void nb_po_on_mark(JSContext *ctx) {
    for (int oi = 0; oi < g_po_obs_count; oi++) {
        NbPoObserver *o = &g_po_obs[oi];
        if (JS_IsUndefined(o->self) || !o->uses_mark) continue;
        for (int mi = o->consumed; mi < g_perf_mark_n; mi++)
            nb_po_push_mark(ctx, o, mi);
        if (g_perf_mark_n > o->consumed) {
            o->consumed = g_perf_mark_n;
            nb_po_schedule(ctx, o);
        }
    }
}
static JSValue nb_po_observe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0])) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: PerformanceObserver.observe requires an options object"));
        return JS_Throw(ctx, e);
    }
    JSValue opts = argv[0];
    JSValue cb = JS_GetPropertyStr(ctx, this_val, "__nb_po_callback");
    if (!JS_IsFunction(ctx, cb)) {
        JS_FreeValue(ctx, cb);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: PerformanceObserver requires a callback"));
        return JS_Throw(ctx, e);
    }
    JS_FreeValue(ctx, cb);
    int oi = nb_po_find_observer(ctx, this_val);
    if (oi < 0) {
        if (g_po_obs_count >= NB_PO_MAX_OBS) return JS_UNDEFINED;
        oi = g_po_obs_count++;
        memset(&g_po_obs[oi], 0, sizeof(g_po_obs[oi]));
        g_po_obs[oi].self = JS_DupValue(ctx, this_val);
        g_po_obs[oi].callback = JS_GetPropertyStr(ctx, this_val, "__nb_po_callback");
        g_po_obs[oi].consumed = g_perf_mark_n;
    }
    NbPoObserver *o = &g_po_obs[oi];
    JSValue types = JS_GetPropertyStr(ctx, opts, "entryTypes");
    int has_type = 0;
    if (JS_IsArray(ctx, types)) {
        JSValue lenv = JS_GetPropertyStr(ctx, types, "length");
        uint32_t n = 0;
        JS_ToUint32(ctx, &n, lenv);
        JS_FreeValue(ctx, lenv);
        for (uint32_t i = 0; i < n; i++) {
            JSValue v = JS_GetPropertyUint32(ctx, types, i);
            if (JS_IsString(v)) {
                char *s = JS_ToCString(ctx, v);
                if (s) {
                    int seen = 0;
                    for (int k = 0; k < o->type_count; k++) if (!strcmp(o->types[k], s)) seen = 1;
                    if (!strcmp(s, "mark")) o->uses_mark = 1;
                    if (!seen && o->type_count < NB_PO_MAX_TYPES) {
                        snprintf(o->types[o->type_count], sizeof(o->types[0]), "%s", s);
                        o->type_count++;
                    }
                    has_type = 1;
                }
                JS_FreeCString(ctx, s);
            }
            JS_FreeValue(ctx, v);
        }
    }
    JS_FreeValue(ctx, types);
    if (!has_type) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: PerformanceObserver.observe requires entryTypes"));
        return JS_Throw(ctx, e);
    }
    JSValue buffered_v = JS_GetPropertyStr(ctx, opts, "buffered");
    int buffered = JS_ToBool(ctx, buffered_v);
    JS_FreeValue(ctx, buffered_v);
    if (buffered && o->uses_mark) {
        for (int mi = 0; mi < g_perf_mark_n; mi++) nb_po_push_mark(ctx, o, mi);
        o->consumed = g_perf_mark_n;
        nb_po_schedule(ctx, o);
    }
    return JS_UNDEFINED;
}
static JSValue nb_po_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JS_SetPropertyStr(ctx, this_val, "__nb_po_records", JS_NewArray(ctx));
    int oi = nb_po_find_observer(ctx, this_val);
    if (oi < 0) return JS_UNDEFINED;
    JS_FreeValue(ctx, g_po_obs[oi].self);
    JS_FreeValue(ctx, g_po_obs[oi].callback);
    g_po_obs[oi].self = g_po_obs[oi].callback = JS_UNDEFINED;
    memmove(&g_po_obs[oi], &g_po_obs[oi + 1], (size_t)(g_po_obs_count - oi - 1) * sizeof(g_po_obs[0]));
    g_po_obs_count--;
    return JS_UNDEFINED;
}
static JSValue nb_po_take_records(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue old = JS_GetPropertyStr(ctx, this_val, "__nb_po_records");
    if (!JS_IsArray(ctx, old)) {
        JS_FreeValue(ctx, old);
        old = JS_NewArray(ctx);
    }
    JS_SetPropertyStr(ctx, this_val, "__nb_po_records", JS_NewArray(ctx));
    return old;
}
