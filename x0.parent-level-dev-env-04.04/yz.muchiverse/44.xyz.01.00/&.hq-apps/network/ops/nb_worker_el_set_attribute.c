/* nb_worker_el_set_attribute.c - setAttribute helper (mechanical extract, single-TU include) */
static JSValue nb_el_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL, *vl = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    const char *val = (argc > 1 && JS_IsString(argv[1])) ? (vl = JS_ToCString(ctx, argv[1])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl); return JS_UNDEFINED; }
    if (!val) val = "";
    const char *old0 = nb_attr_get(n, name);
    char *old_value = old0 ? strdup(old0) : NULL;
    if (!strcasecmp(name, "id")) { free(n->id); n->id = strdup(val); }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = strdup(val); }
    char *na = attrs_set(n, name, val);
    free(n->attrs);
    n->attrs = na;
    nb_mutation_notify_attribute(ctx, n, name, old_value);
    free(old_value);
    JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
