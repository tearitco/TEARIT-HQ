/* nb_worker_dataset_set.c - dataset set trap helper (mechanical extract, single-TU include) */
static JSValue nb_ds_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = nb_ds_node(ctx, this_val);
    if (!n || argc < 3) return JS_UNDEFINED;
    char *k = NULL, *vs = NULL;
    const char *key = JS_IsString(argv[1]) ? (k = JS_ToCString(ctx, argv[1])) : NULL;
    const char *val = JS_IsString(argv[2]) ? (vs = JS_ToCString(ctx, argv[2])) : NULL;
    if (!key) return JS_UNDEFINED;
    char kb[128];
    int ok = nb_ds_to_attr(key, kb, sizeof(kb));
    JS_FreeCString(ctx, k);
    if (!ok) return JS_UNDEFINED;
    char *kb2 = strdup(kb);
    if (!vs) { JSValue tv = JS_ToString(ctx, argv[2]); if (JS_IsString(tv)) vs = JS_ToCString(ctx, tv); JS_FreeValue(ctx, tv); }
    JS_FreeCString(ctx, NULL);
    if (!vs) { free(kb2); return JS_UNDEFINED; }
    { JSValue a[2] = { JS_NewString(ctx, kb2), JS_NewString(ctx, vs) };
      JS_FreeValue(ctx, nb_el_setAttribute(ctx, push_node_or_null(ctx, n), 2, a)); }
    JS_FreeCString(ctx, vs);
    free(kb2);
    return JS_NewBool(ctx, 1);
}
