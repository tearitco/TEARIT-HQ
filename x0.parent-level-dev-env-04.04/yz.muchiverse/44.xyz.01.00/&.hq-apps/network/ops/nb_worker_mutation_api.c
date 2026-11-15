/* nb_worker_mutation_api.c - MutationObserver public API methods (mechanical extract, single-TU include) */
static JSValue nb_mutation_observer_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue obj = class_noop(ctx, this_val, 0, argv);
    if (!JS_IsObject(obj)) return obj;
    if (argc > 0 && JS_IsFunction(ctx, argv[0]))
        JS_SetPropertyStr(ctx, obj, "__nb_mut_callback", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, obj, "__nb_mut_records", JS_NewArray(ctx));
    return obj;
}
static JSValue nb_mutation_observe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *target = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    JSValue cb = JS_GetPropertyStr(ctx, this_val, "__nb_mut_callback");
    if (!target || !JS_IsFunction(ctx, cb)) {
        JS_FreeValue(ctx, cb);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: MutationObserver.observe requires a node and callback"));
        return JS_Throw(ctx, e);
    }
    JSValue opts = (argc > 1 && JS_IsObject(argv[1])) ? JS_DupValue(ctx, argv[1]) : JS_UNDEFINED;
    int child_list = nb_mut_option_bool(ctx, opts, "childList");
    int subtree = nb_mut_option_bool(ctx, opts, "subtree");
    int attributes = nb_mut_option_bool(ctx, opts, "attributes");
    int character_data = nb_mut_option_bool(ctx, opts, "characterData");
    if (!child_list && !attributes && !character_data) {
        JS_FreeValue(ctx, opts);
        JS_FreeValue(ctx, cb);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: an observer must observe at least one of childList, attributes, or characterData"));
        return JS_Throw(ctx, e);
    }
    int oi = nb_mut_find_observer(ctx, this_val);
    if (oi < 0) {
        if (g_mut_obs_count >= NB_MUT_MAX_OBSERVERS) {
            JS_FreeValue(ctx, opts);
            JS_FreeValue(ctx, cb);
            return JS_UNDEFINED;
        }
        oi = g_mut_obs_count++;
        memset(&g_mut_obs[oi], 0, sizeof(g_mut_obs[oi]));
        g_mut_obs[oi].self = JS_DupValue(ctx, this_val);
        g_mut_obs[oi].callback = JS_DupValue(ctx, cb);
    }
    JS_FreeValue(ctx, cb);
    NbMutObserver *o = &g_mut_obs[oi];
    if (o->target_count >= NB_MUT_MAX_TARGETS) { JS_FreeValue(ctx, opts); return JS_UNDEFINED; }
    NbMutTarget *t = &o->targets[o->target_count++];
    memset(t, 0, sizeof(*t));
    t->target = target;
    t->child_list = child_list;
    t->subtree = subtree;
    t->attributes = attributes;
    t->character_data = character_data;
    if (attributes && JS_IsObject(opts)) {
        JSValue names = JS_GetPropertyStr(ctx, opts, "attributeFilter");
        if (JS_IsArray(ctx, names)) {
            JSValue lenv = JS_GetPropertyStr(ctx, names, "length");
            uint32_t n = 0;
            JS_ToUint32(ctx, &n, lenv);
            JS_FreeValue(ctx, lenv);
            if (n > NB_MUT_MAX_ATTRS) n = NB_MUT_MAX_ATTRS;
            for (uint32_t i = 0; i < n; i++) {
                JSValue v = JS_GetPropertyUint32(ctx, names, i);
                if (JS_IsString(v)) {
                    char *s = JS_ToCString(ctx, v);
                    if (s) { snprintf(t->attrs[t->attr_count], sizeof(t->attrs[0]), "%s", s); t->attr_count++; }
                    JS_FreeCString(ctx, s);
                }
                JS_FreeValue(ctx, v);
            }
        }
        JS_FreeValue(ctx, names);
    }
    JS_FreeValue(ctx, opts);
    return JS_UNDEFINED;
}
static JSValue nb_mutation_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    int oi = nb_mut_find_observer(ctx, this_val);
    /* Spec: disconnect empties the pending record queue. */
    JS_SetPropertyStr(ctx, this_val, "__nb_mut_records", JS_NewArray(ctx));
    if (oi < 0) return JS_UNDEFINED;
    JS_FreeValue(ctx, g_mut_obs[oi].self);
    JS_FreeValue(ctx, g_mut_obs[oi].callback);
    g_mut_obs[oi].self = g_mut_obs[oi].callback = JS_UNDEFINED;
    memmove(&g_mut_obs[oi], &g_mut_obs[oi + 1], (size_t)(g_mut_obs_count - oi - 1) * sizeof(g_mut_obs[0]));
    g_mut_obs_count--;
    return JS_UNDEFINED;
}
static JSValue nb_mutation_take_records(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue old = JS_GetPropertyStr(ctx, this_val, "__nb_mut_records");
    if (!JS_IsArray(ctx, old)) {
        JS_FreeValue(ctx, old);
        old = JS_NewArray(ctx);
    }
    JS_SetPropertyStr(ctx, this_val, "__nb_mut_records", JS_NewArray(ctx));
    return old;
}
