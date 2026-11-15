/* nb_worker_dataset_make.c - dataset Proxy factory helper (mechanical extract, single-TU include) */
static JSValue nb_ds_make(JSContext *ctx, NbNode *n, int nidx) {
    JSValue h = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, h, "__nb_ds_idx", JS_NewInt32(ctx, nidx));
    JS_SetPropertyStr(ctx, h, "get", JS_NewCFunction(ctx, nb_ds_get, "get", 3));
    JS_SetPropertyStr(ctx, h, "set", JS_NewCFunction(ctx, nb_ds_set, "set", 3));
    JS_SetPropertyStr(ctx, h, "deleteProperty", JS_NewCFunction(ctx, nb_ds_del, "deleteProperty", 2));
    JS_SetPropertyStr(ctx, h, "has", JS_NewCFunction(ctx, nb_ds_has, "has", 2));
    JS_SetPropertyStr(ctx, h, "ownKeys", JS_NewCFunction(ctx, nb_ds_own_keys, "ownKeys", 1));
    JS_SetPropertyStr(ctx, h, "getOwnPropertyDescriptor", JS_NewCFunction(ctx, nb_ds_desc, "getOwnPropertyDescriptor", 2));
    JSValue target = JS_NewObject(ctx);
    JSValue proxy_ctor = get_global_attr(ctx, "Proxy");
    JSValue args[2] = { target, h };
    JSValue px = JS_IsFunction(ctx, proxy_ctor) ? JS_CallConstructor(ctx, proxy_ctor, 2, args) : JS_UNDEFINED;
    if (!JS_IsObject(px) || JS_IsException(px)) {
        JS_FreeValue(ctx, px);
        JS_FreeValue(ctx, proxy_ctor);
        JS_FreeValue(ctx, h);
        JS_FreeValue(ctx, target);
        return JS_NewObject(ctx); /* degraded: no Proxy available */
    }
    JS_FreeValue(ctx, proxy_ctor);
    JS_FreeValue(ctx, h);
    JS_FreeValue(ctx, target);
    return px;
}
