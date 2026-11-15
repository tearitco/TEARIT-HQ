/* nb_worker_dataset_has.c - dataset has trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = nb_ds_node(ctx, this_val);
    if (!n || argc < 2 || !JS_IsString(argv[1])) return JS_FALSE;
    char *k = JS_ToCString(ctx, argv[1]);
    char kb[128];
    int ok = nb_ds_to_attr(k, kb, sizeof(kb));
    JS_FreeCString(ctx, k);
    return ok && attrs_has(n, kb) ? JS_TRUE : JS_FALSE;
}
