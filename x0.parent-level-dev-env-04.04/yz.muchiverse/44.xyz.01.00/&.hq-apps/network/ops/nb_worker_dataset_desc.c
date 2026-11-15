/* nb_worker_dataset_desc.c - dataset getOwnPropertyDescriptor trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_desc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = nb_ds_node(ctx, this_val);
    if (!n || argc < 2 || !JS_IsString(argv[1])) return JS_UNDEFINED;
    char *k = JS_ToCString(ctx, argv[1]);
    char kb[128];
    int ok = nb_ds_to_attr(k, kb, sizeof(kb));
    JS_FreeCString(ctx, k);
    if (!ok) return JS_UNDEFINED;
    const char *v = nb_attr_get(n, kb);
    if (!v && !attrs_has(n, kb)) return JS_UNDEFINED;
    JSValue d = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, d, "value", JS_NewString(ctx, v ? v : ""));
    JS_SetPropertyStr(ctx, d, "writable", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, d, "enumerable", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, d, "configurable", JS_NewBool(ctx, 1));
    return d;
}
