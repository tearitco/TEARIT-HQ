/* nb_worker_text_data.c - text data get/set helpers (mechanical extract, single-TU include) */
static JSValue nb_text_data_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, (n && n->text) ? n->text : "");
}
static JSValue nb_text_data_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = "";
    if (argc > 0 && JS_IsString(argv[0])) v = (vl = JS_ToCString(ctx, argv[0]));
    else if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) v = (vl = JS_ToCString(ctx, argv[0]));
    char *old = strdup(n->text ? n->text : "");
    free(n->text);
    n->text = strdup(v ? v : "");
    JS_FreeCString(ctx, vl);
    nb_mutation_notify_characterData(ctx, n, old);
    free(old);
    nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
