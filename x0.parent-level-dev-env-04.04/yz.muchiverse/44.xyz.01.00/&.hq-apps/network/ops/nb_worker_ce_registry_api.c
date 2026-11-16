/* nb_worker_ce_registry_api.c - custom elements registry API (mechanical extract, single-TU include) */
static int ce_valid_name(const char *name) {
    if (!name || !name[0] || !strchr(name, '-')) return 0;
    for (const char *p = name; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
            return 0;
    }
    return 1;
}

static void ce_registry_push_name(JSContext *ctx, JSValue reg, const char *name) {
    JSValue names = JS_GetPropertyStr(ctx, reg, "__names");
    if (!JS_IsObject(names)) { JS_FreeValue(ctx, names); names = JS_NewArray(ctx); }
    int have = 0;
    JSValue lenv = JS_GetPropertyStr(ctx, names, "length");
    int32_t len = 0;
    if (JS_IsNumber(lenv)) JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    for (int32_t i = 0; i < len && !have; i++) {
        JSValue nm = JS_GetPropertyUint32(ctx, names, (uint32_t)i);
        if (JS_IsString(nm)) {
            char *owned = NULL;
            const char *s = (owned = JS_ToCString(ctx, nm));
            if (s && !strcmp(s, name)) have = 1;
            JS_FreeCString(ctx, owned);
        }
        JS_FreeValue(ctx, nm);
    }
    if (!have) JS_SetPropertyUint32(ctx, names, (uint32_t)len, JS_NewString(ctx, name));
    JS_SetPropertyStr(ctx, reg, "__names", names);
}

static JSValue ce_define(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!ce_valid_name(name)) {
        JS_FreeCString(ctx, owned);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e))
            JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "SyntaxError: invalid custom element name"));
        return JS_Throw(ctx, e);
    }
    JSValue reg = ce_registry(ctx, 1);
    if (JS_IsObject(reg)) {
        JSValue ex = JS_GetPropertyStr(ctx, reg, name);
        int dup = JS_IsObject(ex);
        JS_FreeValue(ctx, ex);
        if (dup && g_trace_cb)
            fprintf(stderr, "CE|define redefined (last wins): %s\n", name);
        /* kevlar's .f9({disableElementRegistration}) + .OV() flow registers a
         * placeholder class FIRST and replaces it with the real lazy-loaded
         * class; last-define-wins (browsers would throw NotSupportedError). */
        if (argc > 1) {
            if (g_trace_cb) {
                size_t nl = 0;
                const char *nn = NULL;
                if (JS_IsFunction(ctx, argv[1]))
                    nn = JS_ToCStringLen(ctx, &nl, JS_GetPropertyStr(ctx, argv[1], "name"));
                fprintf(stderr, "CE|define %s as %s%s\n", name, nn ? nn : "?", dup ? " (redefine)" : "");
                if (nn) JS_FreeCString(ctx, nn);
            }
            JS_SetPropertyStr(ctx, reg, name, JS_DupValue(ctx, argv[1]));
            ce_registry_push_name(ctx, reg, name);
        }
    }
    JS_FreeValue(ctx, reg);
    /* upgrade every existing matching node (parse-time elements) */
    if (g_dom_root) {
        reg = ce_registry(ctx, 0);
        if (JS_IsObject(reg)) {
            for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
                ce_walk_upgrade(ctx, (NbNode *)c, reg, name);
            JS_FreeValue(ctx, reg);
        }
    }
    JS_FreeCString(ctx, owned);
    return JS_UNDEFINED;
}

static JSValue ce_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue reg = ce_registry(ctx, 0);
    JSValue r = JS_UNDEFINED;
    if (JS_IsObject(reg) && name) r = JS_GetPropertyStr(ctx, reg, name);
    JS_FreeValue(ctx, reg);
    JS_FreeCString(ctx, owned);
    return r;
}

static JSValue ce_when_def(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_promise_resolved(ctx, this_val, argc, argv);
}

static JSValue ce_upgrade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *root = g_dom_root;
    if (argc > 0 && JS_IsObject(argv[0])) {
        NbNode *n = get_node(ctx, argv[0]);
        if (n) root = n;
    }
    JSValue reg = ce_registry(ctx, 0);
    if (JS_IsObject(reg) && root) {
        JSValue names = JS_GetPropertyStr(ctx, reg, "__names");
        if (JS_IsObject(names)) {
            JSValue lenv = JS_GetPropertyStr(ctx, names, "length");
            int32_t len = 0;
            if (JS_IsNumber(lenv)) JS_ToInt32(ctx, &len, lenv);
            JS_FreeValue(ctx, lenv);
            for (int32_t i = 0; i < len; i++) {
                JSValue nm = JS_GetPropertyUint32(ctx, names, (uint32_t)i);
                char *owned = NULL;
                const char *s = JS_IsString(nm) ? (owned = JS_ToCString(ctx, nm)) : NULL;
                if (s) {
                    for (const NbNode *c = root->first_child; c; c = c->next_sibling)
                        ce_walk_upgrade(ctx, (NbNode *)c, reg, s);
                }
                JS_FreeCString(ctx, owned);
                JS_FreeValue(ctx, nm);
            }
        }
        JS_FreeValue(ctx, names);
        JS_FreeValue(ctx, reg);
    }
    return JS_UNDEFINED;
}

/* Node.hasChildNodes/normalize helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_state.c"
