/* nb_worker_inner_html.c - innerHTML get/set helpers (mechanical extract, single-TU include) */
static JSValue nb_el_innerHTML_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    SB b = {0, 0, 0};
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling) node_outer_html(c, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_innerHTML_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (n->tag && !strcmp(n->tag, "template")) {   /* <template>.innerHTML lands in content */
        NbNode *frag = nb_template_content_frag(ctx, this_val, n);
        if (frag) n = frag;
    }
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    NbNode *frag = nb_parse_html_nodes(v, strlen(v));
    NbNode *child = frag ? frag->first_child : NULL;
    clear_children(n);
    /* innerHTML replaces ALL content. A LOAD-tree element keeps its direct
     * text in n->text (the folded model), so clearing children alone left the
     * old page text behind and the assignment appeared to append. */
    free(n->text); n->text = NULL;
    if (child) {
        frag->first_child = frag->last_child = NULL;
        NbNode *cur = child;
        while (cur) {
            NbNode *nx = cur->next_sibling;
            cur->next_sibling = NULL;
            cur->parent = NULL;       /* let local_append set it */
            local_append(n, cur);
            cur = nx;
        }
    }
    nb_node_free(frag);
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
