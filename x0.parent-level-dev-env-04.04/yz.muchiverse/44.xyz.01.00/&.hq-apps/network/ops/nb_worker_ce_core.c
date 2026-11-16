/* nb_worker_ce_core.c - custom elements core helpers (mechanical extract, single-TU include) */
#define CE_UPGRD "__nb_upgraded"

static JSValue ce_registry(JSContext *ctx, int create) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue ce = JS_GetPropertyStr(ctx, g, "customElements");
    JS_FreeValue(ctx, g);
    if (JS_IsObject(ce)) {
        JSValue r = JS_GetPropertyStr(ctx, ce, "__registry");
        if (!JS_IsObject(r)) {
            JS_FreeValue(ctx, r);
            r = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, ce, "__registry", JS_DupValue(ctx, r));
        }
        JS_FreeValue(ctx, ce);
        return r;
    }
    JS_FreeValue(ctx, ce);
    return JS_UNDEFINED;
}

static JSValue nb_js_void(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static void ce_call_lifecycle(JSContext *ctx, JSValue wrapper, const char *mname) {
    JSValue fn = JS_GetPropertyStr(ctx, wrapper, mname);
    if (JS_IsFunction(ctx, fn)) {
        JSValue r = JS_Call(ctx, fn, wrapper, 0, NULL);
        if (JS_IsException(r)) {
            char buf[1536];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            if (g_trace_cb) fprintf(stderr, "CE|%s: %s\n", mname, m ? m : buf);
        }
        JS_FreeValue(ctx, r);
    }
    JS_FreeValue(ctx, fn);
}

