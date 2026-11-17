/* nb_worker_text_content.c - textContent get/set helpers (mechanical extract, single-TU include) */
static JSValue nb_el_textContent_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    /* Node.textContent: for a Document this is null, not the concatenated
     * text of the tree. DocumentFragment (nodeType 11) still concatenates.
     * Comment nodes return their data. */
    if (n && n->tag && !strcmp(n->tag, "#document")) return JS_NULL;
    if (n && n->tag && !strcmp(n->tag, "#comment"))
        return JS_NewString(ctx, n->text ? n->text : "");
    SB b = {0, 0, 0};
    node_text_content(n, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_textContent_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    /* Node.textContent: Document and DocumentType are a no-op; a Text node
     * (tag is NULL here) replaces its data; everything else replaces all
     * children with one real Text node, and an empty string adds none.
     * Only null is special-cased to "": undefined converts like any other
     * DOMString ("undefined"), as in a browser. */
    if (n->tag && !strcmp(n->tag, "#document")) return JS_UNDEFINED;
    if (!n->tag || !n->tag[0] || (n->tag && !strcmp(n->tag, "#comment"))) {   /* character-data node replaces its data */
        char *vl = NULL;
        const char *v = "";
        if (argc > 0 && !JS_IsNull(argv[0])) {
            v = (vl = JS_ToCString(ctx, argv[0]));
            if (!v) return JS_EXCEPTION;
        }
        free(n->text);
        n->text = strdup(v);
        JS_FreeCString(ctx, vl);
        nb_obs_recheck(ctx);
        return JS_UNDEFINED;
    }
    char *vl = NULL;
    const char *v = "";
    if (argc > 0 && !JS_IsNull(argv[0])) {
        v = (vl = JS_ToCString(ctx, argv[0]));
        if (!v) return JS_EXCEPTION;
    }
    clear_children(n);
    free(n->text);
    n->text = NULL;
    NbNode *tn = NULL;
    if (v && v[0]) {
        tn = calloc(1, sizeof(*tn));
        if (tn) {
            tn->text = strdup(v);
            local_append(n, tn);
        }
    }
    JS_FreeCString(ctx, vl);
    /* The added Text node is real, so a childList record reporting it is
     * true. Removed children are not reported: one record carries a single
     * removedNode and a consumer could retain it after its C node is gone.
     * Geometry may change (text length -> height) either way. */
    if (tn) nb_mutation_notify_childlist(ctx, n, tn, NULL, NULL, NULL);
    else nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
