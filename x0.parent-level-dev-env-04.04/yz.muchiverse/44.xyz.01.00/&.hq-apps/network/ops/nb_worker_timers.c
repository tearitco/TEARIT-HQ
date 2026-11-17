/* nb_worker_timers.c - timer schedule/clear bindings (mechanical extract, single-TU include) */
static int timer_schedule(JSContext *ctx, JSValueConst cb, long interval, int repeat) {
    if (!JS_IsFunction(ctx, cb) || g_timer_count >= MAX_TIMERS) return 0;
    int slot = g_timer_count++;
    g_timers[slot].id = g_next_id++;
    g_timers[slot].active = 1;
    g_timers[slot].interval = repeat ? (interval > 0 ? interval : 1) : 0;
    g_timers[slot].due = now_ms() + (uint64_t)(interval > 0 ? interval : 1);
    g_timers[slot].slot = slot;
    g_timers[slot].cb = JS_DupValue(ctx, cb);
    return g_timers[slot].id;
}
static JSValue nb_timer_setTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 0) ms = 0;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 0));
}
static JSValue nb_timer_setInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 1) ms = 1;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 1));
}
static void timer_clear(JSContext *ctx, int want_oneshot, JSValueConst idv) {
    if (!JS_IsNumber(idv)) return;
    double d;
    if (JS_ToFloat64(ctx, &d, idv) != 0) return;
    int id = (int)d;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active && g_timers[i].id == id &&
            (want_oneshot ? g_timers[i].interval == 0 : g_timers[i].interval > 0)) {
            g_timers[i].active = 0;
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
            break;
        }
}
static JSValue nb_timer_clearTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 1, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_timer_clearInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 0, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
