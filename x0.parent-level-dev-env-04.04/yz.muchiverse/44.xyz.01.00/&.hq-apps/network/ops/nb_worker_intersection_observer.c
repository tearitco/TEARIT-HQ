/* nb_worker_intersection_observer.c - IntersectionObserver helpers (mechanical extract, single-TU include) */
#define NB_INT_MAX_TARGETS 64
typedef struct {
    NbNode *node;
    JSValue target_js;
    int last_intersecting;
    double last_ratio;
    double last_x, last_y, last_w, last_h;
} NbIntTarget;
typedef struct {
    JSValue self;
    JSValue callback;
    NbNode *root_node; /* NULL = page viewport */
    JSValue root_js;
    NbIntTarget targets[NB_INT_MAX_TARGETS];
    int target_count;
    int scheduled;
} NbIntObserver;
static NbIntObserver g_int_obs[NB_INT_MAX_OBS];
static int g_int_obs_count = 0;
static JSValue nb_el_getBoundingClientRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_intersection_observer_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue obj = class_noop(ctx, this_val, 0, argv);
    if (!JS_IsObject(obj)) return obj;
    if (argc > 0 && JS_IsFunction(ctx, argv[0]))
        JS_SetPropertyStr(ctx, obj, "__nb_int_callback", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, obj, "__nb_int_records", JS_NewArray(ctx));
    return obj;
}
static double nb_int_rect_field(JSContext *ctx, JSValueConst rect, const char *key) {
    JSValue v = JS_GetPropertyStr(ctx, rect, key);
    double d = 0;
    JS_ToFloat64(ctx, &d, v);
    JS_FreeValue(ctx, v);
    return d;
}
static int nb_int_find_observer(JSContext *ctx, JSValueConst self) {
    for (int i = 0; i < g_int_obs_count; i++)
        if (JS_StrictEq(ctx, g_int_obs[i].self, self)) return i;
    return -1;
}
static JSValue nb_int_records_for(JSContext *ctx, JSValue self) {
    JSValue recs = JS_GetPropertyStr(ctx, self, "__nb_int_records");
    if (!JS_IsArray(ctx, recs)) {
        JS_FreeValue(ctx, recs);
        recs = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, self, "__nb_int_records", JS_DupValue(ctx, recs));
    }
    return recs;
}
static void nb_int_flush(JSContext *ctx, NbIntObserver *o) {
    JSValue recs = nb_int_records_for(ctx, o->self);
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
    JS_SetPropertyStr(ctx, o->self, "__nb_int_records", JS_NewArray(ctx));
}
static JSValue nb_int_flush_job(JSContext *ctx, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;
    int oi = nb_int_find_observer(ctx, argv[0]);
    if (oi < 0) return JS_UNDEFINED;
    g_int_obs[oi].scheduled = 0;
    nb_int_flush(ctx, &g_int_obs[oi]);
    return JS_UNDEFINED;
}
static void nb_int_schedule(JSContext *ctx, NbIntObserver *o) {
    if (!o->scheduled) {
        o->scheduled = 1;
        JSValue jv[1] = { o->self };
        if (JS_EnqueueJob(ctx, nb_int_flush_job, 1, jv) < 0) {
            o->scheduled = 0;
            nb_int_flush(ctx, o);
        }
    }
}
static void nb_int_compute(JSContext *ctx, NbIntObserver *o, JSValue target_js,
                            double *x, double *y, double *w, double *h,
                            double *rx, double *ry, double *rr, double *rb,
                            int *intersecting, double *ratio) {
    JSValue rect = nb_el_getBoundingClientRect(ctx, target_js, 0, NULL);
    *x = nb_int_rect_field(ctx, rect, "x"); *y = nb_int_rect_field(ctx, rect, "y");
    *w = nb_int_rect_field(ctx, rect, "width"); *h = nb_int_rect_field(ctx, rect, "height");
    int have_root = 0;
    if (o->root_node && JS_IsObject(o->root_js)) {
        JSValue rrect = nb_el_getBoundingClientRect(ctx, o->root_js, 0, NULL);
        *rx = nb_int_rect_field(ctx, rrect, "x"); *ry = nb_int_rect_field(ctx, rrect, "y");
        *rr = *rx + nb_int_rect_field(ctx, rrect, "width");
        *rb = *ry + nb_int_rect_field(ctx, rrect, "height");
        have_root = 1;
        JS_FreeValue(ctx, rrect);
    }
    if (!have_root) {
        *rx = 0.0; *ry = 0.0;
        *rr = (g_viewport_px > 0.0) ? g_viewport_px : (double)NB_CONTENT_PX;
        *rb = (g_viewport_h > 0.0) ? g_viewport_h : 600.0;
    }
    double right = *x + *w, bottom = *y + *h;
    double ix = *x > *rx ? *x : *rx;
    double iy = *y > *ry ? *y : *ry;
    double iright = right < *rr ? right : *rr;
    double ibottom = bottom < *rb ? bottom : *rb;
    *intersecting = (iright > ix && ibottom > iy);
    double r0 = 0.0;
    if (*intersecting && *w > 0.0 && *h > 0.0)
        r0 = ((iright - ix) * (ibottom - iy)) / (*w * *h);
    if (r0 < 0.0) r0 = 0.0;
    if (r0 > 1.0) r0 = 1.0;
    *ratio = r0;
    JS_FreeValue(ctx, rect);
}
static JSValue nb_int_make_entry(JSContext *ctx, JSValue target_js,
                                 double rx, double ry, double rr, double rb,
                                 double x, double y, double w, double h,
                                 int intersecting, double ratio) {
    JSValue bounds = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, bounds, "x", JS_NewFloat64(ctx, rx));
    JS_SetPropertyStr(ctx, bounds, "y", JS_NewFloat64(ctx, ry));
    JS_SetPropertyStr(ctx, bounds, "width", JS_NewFloat64(ctx, rr - rx));
    JS_SetPropertyStr(ctx, bounds, "height", JS_NewFloat64(ctx, rb - ry));
    JS_SetPropertyStr(ctx, bounds, "top", JS_NewFloat64(ctx, ry));
    JS_SetPropertyStr(ctx, bounds, "left", JS_NewFloat64(ctx, rx));
    JS_SetPropertyStr(ctx, bounds, "right", JS_NewFloat64(ctx, rr));
    JS_SetPropertyStr(ctx, bounds, "bottom", JS_NewFloat64(ctx, rb));
    JS_SetPropertyStr(ctx, bounds, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    JSValue clipped = JS_NewObject(ctx);
    double cw = intersecting ? ((x + w) < rr ? (x + w) : rr) - (x > rx ? x : rx) : 0.0;
    double ch = intersecting ? ((y + h) < rb ? (y + h) : rb) - (y > ry ? y : ry) : 0.0;
    double ix = intersecting ? (x > rx ? x : rx) : 0.0;
    double iy = intersecting ? (y > ry ? y : ry) : 0.0;
    double iright = intersecting ? ((x + w) < rr ? (x + w) : rr) : 0.0;
    double ibottom = intersecting ? ((y + h) < rb ? (y + h) : rb) : 0.0;
    JS_SetPropertyStr(ctx, clipped, "x", JS_NewFloat64(ctx, ix));
    JS_SetPropertyStr(ctx, clipped, "y", JS_NewFloat64(ctx, iy));
    JS_SetPropertyStr(ctx, clipped, "width", JS_NewFloat64(ctx, cw));
    JS_SetPropertyStr(ctx, clipped, "height", JS_NewFloat64(ctx, ch));
    JS_SetPropertyStr(ctx, clipped, "top", JS_NewFloat64(ctx, iy));
    JS_SetPropertyStr(ctx, clipped, "left", JS_NewFloat64(ctx, ix));
    JS_SetPropertyStr(ctx, clipped, "right", JS_NewFloat64(ctx, iright));
    JS_SetPropertyStr(ctx, clipped, "bottom", JS_NewFloat64(ctx, ibottom));
    JS_SetPropertyStr(ctx, clipped, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    JSValue rect = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rect, "x", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, rect, "y", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, rect, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, rect, "height", JS_NewFloat64(ctx, h));
    JS_SetPropertyStr(ctx, rect, "top", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, rect, "right", JS_NewFloat64(ctx, x + w));
    JS_SetPropertyStr(ctx, rect, "bottom", JS_NewFloat64(ctx, y + h));
    JS_SetPropertyStr(ctx, rect, "left", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, rect, "toJSON", JS_NewCFunction(ctx, nb_rect_tojson, "toJSON", 0));
    JSValue entry = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, entry, "target", JS_DupValue(ctx, target_js));
    JS_SetPropertyStr(ctx, entry, "boundingClientRect", rect);
    JS_SetPropertyStr(ctx, entry, "intersectionRect", clipped);
    JS_SetPropertyStr(ctx, entry, "rootBounds", bounds);
    JS_SetPropertyStr(ctx, entry, "isIntersecting", JS_NewBool(ctx, intersecting));
    JS_SetPropertyStr(ctx, entry, "intersectionRatio", JS_NewFloat64(ctx, ratio));
    JS_SetPropertyStr(ctx, entry, "time", JS_NewInt32(ctx, 0));
    return entry;
}
static JSValue nb_intersection_observe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *target = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    JSValue cb = JS_GetPropertyStr(ctx, this_val, "__nb_int_callback");
    if (!target || !JS_IsFunction(ctx, cb)) {
        JS_FreeValue(ctx, cb);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: IntersectionObserver.observe requires a node and callback"));
        return JS_Throw(ctx, e);
    }
    JS_FreeValue(ctx, cb);
    int oi = nb_int_find_observer(ctx, this_val);
    if (oi < 0) {
        if (g_int_obs_count >= NB_INT_MAX_OBS) return JS_UNDEFINED;
        oi = g_int_obs_count++;
        memset(&g_int_obs[oi], 0, sizeof(g_int_obs[oi]));
        g_int_obs[oi].self = JS_DupValue(ctx, this_val);
        JSValue c2 = JS_GetPropertyStr(ctx, this_val, "__nb_int_callback");
        g_int_obs[oi].callback = c2;
        /* Capture root from options, if an element root was given. */
        JSValue opts = (argc > 1 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;
        if (JS_IsObject(opts)) {
            JSValue rv = JS_GetPropertyStr(ctx, opts, "root");
            if (JS_IsObject(rv)) {
                NbNode *rn = get_node(ctx, rv);
                if (rn) {
                    g_int_obs[oi].root_node = rn;
                    g_int_obs[oi].root_js = JS_DupValue(ctx, rv);
                }
            }
            JS_FreeValue(ctx, rv);
        }
    }
    NbIntObserver *o = &g_int_obs[oi];
    for (int ti = 0; ti < o->target_count; ti++)
        if (o->targets[ti].node == target) return JS_UNDEFINED; /* already observed */
    if (o->target_count >= NB_INT_MAX_TARGETS) return JS_UNDEFINED;
    NbIntTarget *t = &o->targets[o->target_count];
    memset(t, 0, sizeof(*t));
    t->node = target;
    t->target_js = JS_DupValue(ctx, argv[0]);
    double x, y, w, h, rx, ry, rr, rb, ratio;
    int intersecting;
    nb_int_compute(ctx, o, argv[0], &x, &y, &w, &h, &rx, &ry, &rr, &rb, &intersecting, &ratio);
    t->last_x = x; t->last_y = y; t->last_w = w; t->last_h = h;
    t->last_intersecting = intersecting; t->last_ratio = ratio;
    o->target_count++;
    JSValue recs = nb_int_records_for(ctx, o->self);
    JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
    uint32_t n = 0;
    JS_ToUint32(ctx, &n, lenv);
    JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, recs, n, nb_int_make_entry(ctx, argv[0], rx, ry, rr, rb, x, y, w, h, intersecting, ratio));
    JS_FreeValue(ctx, recs);
    nb_int_schedule(ctx, o);
    return JS_UNDEFINED;
}
static JSValue nb_intersection_unobserve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int oi = nb_int_find_observer(ctx, this_val);
    if (oi < 0 || argc < 1 || !JS_IsObject(argv[0])) return JS_UNDEFINED;
    NbNode *target = get_node(ctx, argv[0]);
    if (!target) return JS_UNDEFINED;
    NbIntObserver *o = &g_int_obs[oi];
    for (int ti = 0; ti < o->target_count; ti++) {
        if (o->targets[ti].node == target) {
            JS_FreeValue(ctx, o->targets[ti].target_js);
            memmove(&o->targets[ti], &o->targets[ti + 1], (size_t)(o->target_count - ti - 1) * sizeof(o->targets[0]));
            o->target_count--;
            break;
        }
    }
    /* Drop any pending entries for this target. */
    JSValue recs = nb_int_records_for(ctx, o->self);
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
    JS_SetPropertyStr(ctx, o->self, "__nb_int_records", kept);
    return JS_UNDEFINED;
}
static JSValue nb_intersection_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JS_SetPropertyStr(ctx, this_val, "__nb_int_records", JS_NewArray(ctx));
    int oi = nb_int_find_observer(ctx, this_val);
    if (oi < 0) return JS_UNDEFINED;
    JS_FreeValue(ctx, g_int_obs[oi].self);
    JS_FreeValue(ctx, g_int_obs[oi].callback);
    if (JS_IsObject(g_int_obs[oi].root_js)) JS_FreeValue(ctx, g_int_obs[oi].root_js);
    for (int ti = 0; ti < g_int_obs[oi].target_count; ti++)
        if (JS_IsObject(g_int_obs[oi].targets[ti].target_js)) JS_FreeValue(ctx, g_int_obs[oi].targets[ti].target_js);
    g_int_obs[oi].self = g_int_obs[oi].callback = JS_UNDEFINED;
    g_int_obs[oi].root_js = JS_UNDEFINED;
    memmove(&g_int_obs[oi], &g_int_obs[oi + 1], (size_t)(g_int_obs_count - oi - 1) * sizeof(g_int_obs[0]));
    g_int_obs_count--;
    return JS_UNDEFINED;
}
static JSValue nb_intersection_take_records(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue old = JS_GetPropertyStr(ctx, this_val, "__nb_int_records");
    if (!JS_IsArray(ctx, old)) {
        JS_FreeValue(ctx, old);
        old = JS_NewArray(ctx);
    }
    JS_SetPropertyStr(ctx, this_val, "__nb_int_records", JS_NewArray(ctx));
    return old;
}
static void nb_int_recheck(JSContext *ctx) {
    for (int oi = 0; oi < g_int_obs_count; oi++) {
        NbIntObserver *o = &g_int_obs[oi];
        if (JS_IsUndefined(o->self) || !JS_IsFunction(ctx, o->callback)) continue;
        for (int ti = 0; ti < o->target_count; ti++) {
            NbIntTarget *t = &o->targets[ti];
            double x, y, w, h, rx, ry, rr, rb, ratio;
            int intersecting;
            nb_int_compute(ctx, o, t->target_js, &x, &y, &w, &h, &rx, &ry, &rr, &rb, &intersecting, &ratio);
            double dr = ratio - t->last_ratio;
            if (dr < 0) dr = -dr;
            if (intersecting == t->last_intersecting && dr < 1e-9) continue;
            t->last_x = x; t->last_y = y; t->last_w = w; t->last_h = h;
            t->last_intersecting = intersecting; t->last_ratio = ratio;
            JSValue recs = nb_int_records_for(ctx, o->self);
            JSValue lenv = JS_GetPropertyStr(ctx, recs, "length");
            uint32_t n = 0;
            JS_ToUint32(ctx, &n, lenv);
            JS_FreeValue(ctx, lenv);
            JS_SetPropertyUint32(ctx, recs, n, nb_int_make_entry(ctx, t->target_js, rx, ry, rr, rb, x, y, w, h, intersecting, ratio));
            JS_FreeValue(ctx, recs);
            nb_int_schedule(ctx, o);
        }
    }
}

