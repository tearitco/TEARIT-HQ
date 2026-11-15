/* nb_worker_dataset_del.c - dataset delete trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_del(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = nb_ds_node(ctx, this_val);
    if (!n || argc < 2) return JS_UNDEFINED;
    char *k = NULL;
    const char *key = JS_IsString(argv[1]) ? (k = JS_ToCString(ctx, argv[1])) : NULL;
    if (!key) return JS_UNDEFINED;
    char kb[128];
    int ok = nb_ds_to_attr(key, kb, sizeof(kb));
    JS_FreeCString(ctx, k);
    if (!ok) return JS_UNDEFINED;
    { JSValue a[1] = { JS_NewString(ctx, kb) };
      JS_FreeValue(ctx, nb_el_removeAttribute(ctx, push_node_or_null(ctx, n), 1, a)); }
    return JS_NewBool(ctx, 1);
}
