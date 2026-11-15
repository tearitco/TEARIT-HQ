/* nb_worker_dataset_get.c - dataset get trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc;
    NbNode *n = nb_ds_node(ctx, this_val);
    if (!n || argc < 2) return JS_UNDEFINED;
    char *k = NULL;
    const char *key = JS_IsString(argv[1]) ? (k = JS_ToCString(ctx, argv[1])) : NULL;
    if (!key) return JS_UNDEFINED;
    if (!strcmp(key, "Symbol.toStringTag")) { JS_FreeCString(ctx, k); return JS_NewString(ctx, "DOMStringMap"); }
    char kb[128];
    int ok = nb_ds_to_attr(key, kb, sizeof(kb));
    JS_FreeCString(ctx, k);
    if (!ok) return JS_UNDEFINED;
    if (!attrs_has(n, kb)) return JS_UNDEFINED;
    const char *v = nb_attr_get(n, kb);
    return JS_NewString(ctx, v ? v : "");
}
