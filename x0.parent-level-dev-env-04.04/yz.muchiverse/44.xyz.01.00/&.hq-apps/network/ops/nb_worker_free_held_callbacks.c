/* nb_worker_free_held_callbacks.c - JS callback/observer slot cleanup (mechanical extract, single-TU include) */
static void free_held_callbacks(JSContext *ctx) {
    for (int i = 0; i < g_timer_count; i++)
        if (!JS_IsUndefined(g_timers[i].cb) && JS_IsFunction(ctx, g_timers[i].cb)) {
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_evl_count; i++)
        if (!JS_IsUndefined(g_evl[i].cb) && JS_IsFunction(ctx, g_evl[i].cb)) {
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_onprop_count; i++)
        if (!JS_IsUndefined(g_onprop[i].cb) && JS_IsFunction(ctx, g_onprop[i].cb)) {
            JS_FreeValue(ctx, g_onprop[i].cb);
            g_onprop[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_mut_obs_count; i++) {
        if (!JS_IsUndefined(g_mut_obs[i].self)) JS_FreeValue(ctx, g_mut_obs[i].self);
        if (!JS_IsUndefined(g_mut_obs[i].callback)) JS_FreeValue(ctx, g_mut_obs[i].callback);
        g_mut_obs[i].self = g_mut_obs[i].callback = JS_UNDEFINED;
        g_mut_obs[i].target_count = 0;
    }
    g_mut_obs_count = 0;
    for (int i = 0; i < g_int_obs_count; i++) {
        if (!JS_IsUndefined(g_int_obs[i].self)) JS_FreeValue(ctx, g_int_obs[i].self);
        if (!JS_IsUndefined(g_int_obs[i].callback)) JS_FreeValue(ctx, g_int_obs[i].callback);
        if (JS_IsObject(g_int_obs[i].root_js)) JS_FreeValue(ctx, g_int_obs[i].root_js);
        for (int ti = 0; ti < g_int_obs[i].target_count; ti++)
            if (JS_IsObject(g_int_obs[i].targets[ti].target_js)) JS_FreeValue(ctx, g_int_obs[i].targets[ti].target_js);
        g_int_obs[i].self = g_int_obs[i].callback = JS_UNDEFINED;
        g_int_obs[i].root_js = JS_UNDEFINED;
        g_int_obs[i].target_count = 0;
    }
    g_int_obs_count = 0;
    for (int i = 0; i < g_ro_obs_count; i++) {
        if (!JS_IsUndefined(g_ro_obs[i].self)) JS_FreeValue(ctx, g_ro_obs[i].self);
        if (!JS_IsUndefined(g_ro_obs[i].callback)) JS_FreeValue(ctx, g_ro_obs[i].callback);
        for (int ti = 0; ti < g_ro_obs[i].target_count; ti++)
            if (JS_IsObject(g_ro_obs[i].targets[ti].target_js)) JS_FreeValue(ctx, g_ro_obs[i].targets[ti].target_js);
        g_ro_obs[i].self = g_ro_obs[i].callback = JS_UNDEFINED;
        g_ro_obs[i].target_count = 0;
    }
    g_ro_obs_count = 0;
    for (int i = 0; i < g_po_obs_count; i++) {
        if (!JS_IsUndefined(g_po_obs[i].self)) JS_FreeValue(ctx, g_po_obs[i].self);
        if (!JS_IsUndefined(g_po_obs[i].callback)) JS_FreeValue(ctx, g_po_obs[i].callback);
        g_po_obs[i].self = g_po_obs[i].callback = JS_UNDEFINED;
    }
    g_po_obs_count = 0;
}
