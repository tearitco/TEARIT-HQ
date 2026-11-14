/* nb_worker_id_class.c - id/className property helpers (mechanical extract, single-TU include) */
static JSValue nb_el_id_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, n && n->id ? n->id : "");
}
static JSValue nb_el_id_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (n) {
        free(n->id); n->id = strdup(v);
        /* The attribute blob is what innerHTML/outerHTML serialize, so the
         * property must move it too - not just the selector field. */
        char *na = attrs_set(n, "id", v);
        free(n->attrs); n->attrs = na;
    }
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
static JSValue nb_el_className_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, n && n->cls ? n->cls : "");
}
static JSValue nb_el_className_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (n) {
        free(n->cls); n->cls = strdup(v);
        char *na = attrs_set(n, "class", v);
        free(n->attrs); n->attrs = na;
    }
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
