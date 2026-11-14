/* nb_worker_node_state.c - Node.hasChildNodes/normalize helpers (mechanical extract, single-TU include) */
/* ---- fragment helpers (ShadowRoot + <template>.content) ----
 * A fragment is a detached NbNode tagged "#shadow-root"/"#document-fragment"
 * whose wrapper reuses the element natives (appendChild/removeChild/
 * innerHTML/children/firstChild/... all read the C node behind the NODEKEY)
 * plus root-scoped query natives that walk the fragment's OWN subtree (the
 * document natives are hard-wired to g_dom_root). */
static JSValue nb_el_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    return JS_NewBool(ctx, n && n->first_child != NULL);
}
static int nb_is_text_data_node(const NbNode *n) {
    return n && (!n->tag || !n->tag[0]);
}
static void nb_normalize_node(NbNode *n) {
    if (!n) return;
    NbNode *c = n->first_child;
    while (c) {
        NbNode *next = c->next_sibling;
        if (nb_is_text_data_node(c)) {
            /* Merge adjacent text nodes into c, remove empty/merged ones. */
            if (!c->text || c->text[0] == '\0') {
                NbNode *nx = next;
                node_detach(c);
                orphan_add(c);
                c = nx;
                continue;
            }
            while (next && nb_is_text_data_node(next)) {
                NbNode *after = next->next_sibling;
                const char *t2 = next->text ? next->text : "";
                size_t l1 = strlen(c->text);
                size_t l2 = strlen(t2);
                char *merged = (char *)malloc(l1 + l2 + 1);
                if (merged) {
                    memcpy(merged, c->text, l1);
                    memcpy(merged + l1, t2, l2 + 1);
                    free(c->text);
                    c->text = merged;
                }
                node_detach(next);
                orphan_add(next);
                next = after;
            }
            c = next;
        } else {
            nb_normalize_node(c);
            c = next;
        }
    }
}
static JSValue nb_el_normalize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (n) nb_normalize_node(n);
    return JS_UNDEFINED;
}
