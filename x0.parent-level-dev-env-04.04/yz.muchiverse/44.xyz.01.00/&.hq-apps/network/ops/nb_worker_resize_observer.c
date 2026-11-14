/* nb_worker_resize_observer.c - ResizeObserver and observer recheck helpers (mechanical extract, single-TU include) */
/* ---- ResizeObserver (2026-11-08, extended 2026-11-09) ----
 * Real initial entries from the worker's box geometry, delivered as a
 * queued microtask and batched per task. DOM-driven size changes
 * (childList / attribute edits, textContent / innerHTML) re-evaluate
 * observed targets and deliver change entries. It intentionally does not
 * claim pane-resize delivery: the worker knows the LOAD viewport size,
 * not a live pane-resize channel. */
#define NB_RO_MAX_OBS 32
#define NB_RO_MAX_TARGETS 64
typedef struct {
    NbNode *node;
    JSValue target_js;
    double last_w, last_h;
} NbRoTarget;
typedef struct {
    JSValue self;
    JSValue callback;
    NbRoTarget targets[NB_RO_MAX_TARGETS];
    int target_count;
    int scheduled;
} NbRoObserver;
static NbRoObserver g_ro_obs[NB_RO_MAX_OBS];
static int g_ro_obs_count = 0;
static JSValue nb_resize_observer_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue obj = class_noop(ctx, this_val, 0, argv);
    if (!JS_IsObject(obj)) return obj;
    if (argc > 0 && JS_IsFunction(ctx, argv[0]))
        JS_SetPropertyStr(ctx, obj, "__nb_resize_callback", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, obj, "__nb_ro_records", JS_NewArray(ctx));
    return obj;
}
static JSValue nb_resize_make_size(JSContext *ctx, double inline_size, double block_size) {
    JSValue size = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, size, "inlineSize", JS_NewFloat64(ctx, inline_size));
    JS_SetPropertyStr(ctx, size, "blockSize", JS_NewFloat64(ctx, block_size));
    JSValue list = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, list, 0, size);
    return list;
}
static int nb_ro_find_observer(JSContext *ctx, JSValueConst self) {
    for (int i = 0; i < g_ro_obs_count; i++)
        if (JS_StrictEq(ctx, g_ro_obs[i].self, self)) return i;
    return -1;
}
static JSValue nb_ro_records_for(JSContext *ctx, JSValue self) {
    JSValue recs = JS_GetPropertyStr(ctx, self, "__nb_ro_records");
    if (!JS_IsArray(ctx, recs)) {
        JS_FreeValue(ctx, recs);
        recs = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, self, "__nb_ro_records", JS_DupValue(ctx, recs));
    }
    return recs;
}
static void nb_ro_flush(JSContext *ctx, NbRoObserver *o) {
    JSValue recs = nb_ro_records_for(ctx, o->self);
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
    JS_SetPropertyStr(ctx, o->self, "__nb_ro_records", JS_NewArray(ctx));
}
static JSValue nb_ro_flush_job(JSContext *ctx, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    int oi = nb_ro_find_observer(ctx, argv[0]);
    if (oi < 0) return JS_UNDEFINED;
    g_ro_obs[oi].scheduled = 0;
    nb_ro_flush(ctx, &g_ro_obs[oi]);
    return JS_UNDEFINED;
}
static void nb_ro_schedule(JSContext *ctx, NbRoObserver *o) {
    if (!o->scheduled) {
        o->scheduled = 1;
        JSValue jv[1] = { o->self };
        if (JS_EnqueueJob(ctx, nb_ro_flush_job, 1, jv) < 0) {
            o->scheduled = 0;
            nb_ro_flush(ctx, o);
        }
    }
}
static JSValue nb_ro_make_entry(JSContext *ctx, JSValue target_js, JSValue rect, double x, double y, double w, double h) {
    JSValue content = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, content, "x", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, content, "y", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, content, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, content, "height", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, content, "top", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, content, "left", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, content, "right", JS_NewFloat64(ctx, x + w));
    JS_SetPropertyStr(ctx, content, "bottom", JS_NewFloat64(ctx, y + h));
    JS_SetPropertyStr(ctx, content, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    JS_FreeValue(ctx, rect);
    JSValue entry = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, entry, "target", JS_DupValue(ctx, target_js));
    JS_SetPropertyStr(ctx, entry, "contentRect", content);
    JS_SetPropertyStr(ctx, entry, "borderBoxSize", nb_resize_make_size(ctx, w, h));
    JS_SetPropertyStr(ctx, entry, "contentBoxSize", nb_resize_make_size(ctx, w, h));
    return entry;
}
static JSValue nb_resize_observe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *target = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    JSValue cb = JS_GetPropertyStr(ctx, this_val, "__nb_resize_callback");
    if (!target || !JS_IsFunction(ctx, cb)) {
        JS_FreeValue(ctx, cb);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: ResizeObserver.observe requires a node and callback"));
        return JS_Throw(ctx, e);
    }
    JS_FreeValue(ctx, cb);
    int oi = nb_ro_find_observer(ctx, this_val);
    if (oi < 0) {
        if (g_ro_obs_count >= NB_RO_MAX_OBS) return JS_UNDEFINED;
        oi = g_ro_obs_count++;
        memset(&g_ro_obs[oi], 0, sizeof(g_ro_obs[oi]));
        g_ro_obs[oi].self = JS_DupValue(ctx, this_val);
        g_ro_obs[oi].callback = JS_GetPropertyStr(ctx, this_val, "__nb_resize_callback");
    }
    NbRoObserver *o = &g_ro_obs[oi];
    for (int ti = 0; ti < o->target_count; ti++)
        if (o->targets[ti].node == target) return JS_UNDEFINED;
    if (o->target_count >= NB_RO_MAX_TARGETS) return JS_UNDEFINED;
    NbRoTarget *t = &o->targets[o->target_count];
    memset(t, 0, sizeof(*t));
    t->node = target;
    t->target_js = JS_DupValue(ctx, argv[0]);
    JSValue rect = nb_el_getBoundingClientRect(ctx, argv[0], 0, NULL);
    double x = nb_int_rect_field(ctx, rect, "x"), y = nb_int_rect_field(ctx, rect, "y");
    double w = nb_int_rect_field(ctx, rect, "width"), h = nb_int_rect_field(ctx, rect, "height");
    t->last_w = w; t->last_h = h;
    o->target_count++;
    JSValue recs = nb_ro_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, recs, n, nb_ro_make_entry(ctx, argv[0], rect, x, y, w, h));
    JS_FreeValue(ctx, recs);
    nb_ro_schedule(ctx, o);
    return JS_UNDEFINED;
}
static JSValue nb_resize_unobserve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int oi = nb_ro_find_observer(ctx, this_val);
    if (oi < 0 || argc < 1 || !JS_IsObject(argv[0])) return JS_UNDEFINED;
    NbNode *target = get_node(ctx, argv[0]);
    if (!target) return JS_UNDEFINED;
    NbRoObserver *o = &g_ro_obs[oi];
    for (int ti = 0; ti < o->target_count; ti++) {
        if (o->targets[ti].node == target) {
            JS_FreeValue(ctx, o->targets[ti].target_js);
            memmove(&o->targets[ti], &o->targets[ti + 1], (size_t)(o->target_count - ti - 1) * sizeof(o->targets[0]));
            o->target_count--;
            break;
        }
    }
    JSValue recs = nb_ro_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    JSValue kept = JS_NewArray(ctx);
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, recs, i);
        JSValue et = JS_GetPropertyStr(ctx, e, "target");
        int match = JS_StrictEq(ctx, et, argv[0]);
        JS_FreeValue(ctx, et);
        if (!match) JS_SetPropertyUint32(ctx, kept, k++, e);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, recs);
    JS_SetPropertyStr(ctx, o->self, "__nb_ro_records", kept);
    return JS_UNDEFINED;
}
static JSValue nb_resize_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JS_SetPropertyStr(ctx, this_val, "__nb_ro_records", JS_NewArray(ctx));
    int oi = nb_ro_find_observer(ctx, this_val);
    if (oi < 0) return JS_UNDEFINED;
    JS_FreeValue(ctx, g_ro_obs[oi].self);
    JS_FreeValue(ctx, g_ro_obs[oi].callback);
    for (int ti = 0; ti < g_ro_obs[oi].target_count; ti++)
        if (JS_IsObject(g_ro_obs[oi].targets[ti].target_js)) JS_FreeValue(ctx, g_ro_obs[oi].targets[ti].target_js);
    g_ro_obs[oi].self = g_ro_obs[oi].callback = JS_UNDEFINED;
    memmove(&g_ro_obs[oi], &g_ro_obs[oi + 1], (size_t)(g_ro_obs_count - oi - 1) * sizeof(g_ro_obs[0]));
    g_ro_obs_count--;
    return JS_UNDEFINED;
}
static JSValue nb_resize_take_records(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue old = JS_GetPropertyStr(ctx, this_val, "__nb_ro_records");
    if (!JS_IsArray(ctx, old)) {
        JS_FreeValue(ctx, old);
        old = JS_NewArray(ctx);
    }
    JS_SetPropertyStr(ctx, this_val, "__nb_ro_records", JS_NewArray(ctx));
    return old;
}
static void nb_ro_recheck(JSContext *ctx) {
    for (int oi = 0; oi < g_ro_obs_count; oi++) {
        NbRoObserver *o = &g_ro_obs[oi];
        if (JS_IsUndefined(o->self) || !JS_IsFunction(ctx, o->callback)) continue;
        for (int ti = 0; ti < o->target_count; ti++) {
            NbRoTarget *t = &o->targets[ti];
            JSValue rect = nb_el_getBoundingClientRect(ctx, t->target_js, 0, NULL);
            double x = nb_int_rect_field(ctx, rect, "x"), y = nb_int_rect_field(ctx, rect, "y");
            double w = nb_int_rect_field(ctx, rect, "width"), h = nb_int_rect_field(ctx, rect, "height");
            double dw = w - t->last_w, dh = h - t->last_h;
            if (dw < 0) dw = -dw;
            if (dh < 0) dh = -dh;
            if (dw < 1e-9 && dh < 1e-9) {
                JS_FreeValue(ctx, rect);
                continue;
            }
            t->last_w = w; t->last_h = h;
            JSValue recs = nb_ro_records_for(ctx, o->self);
            JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
            uint32_t n = 0;
            JS_ToUint32(ctx, &n, lenv);
            JS_FreeValue(ctx, lenv);
            JS_SetPropertyUint32(ctx, recs, n, nb_ro_make_entry(ctx, t->target_js, rect, x, y, w, h));
            JS_FreeValue(ctx, recs);
            nb_ro_schedule(ctx, o);
        }
    }
}
static void nb_obs_recheck(JSContext *ctx) {
    if (!ctx) return;
    nb_int_recheck(ctx);
    nb_ro_recheck(ctx);
}

