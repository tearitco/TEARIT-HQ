/* nb_worker_doc_title_set.c - document.title setter (mechanical extract, single-TU include) */
static JSValue nb_doc_title_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *doc = get_this(ctx, this_val);
    if (!doc) return JS_UNDEFINED;
    char *vl = NULL;
    /* Not LegacyNullToEmptyString: a Document title converts like any
     * DOMString, so null -> "null" and undefined -> "undefined". */
    const char *v = (argc > 0) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (!v) return JS_EXCEPTION;
    NbNode *te = find_tag_first(doc, "title");
    if (!te) {
        NbNode *head = find_tag_first(doc, "head");
        NbNode *parent = head;
        if (!head) {
            NbNode *root = NULL;
            for (const NbNode *c = doc->first_child; c; c = c->next_sibling)
                if (c->tag && c->tag[0]) { root = (NbNode *)c; break; }
            if (root && !strcasecmp(root->tag, "html")) {
                head = calloc(1, sizeof(*head));
                if (!head) { JS_FreeCString(ctx, vl); return JS_UNDEFINED; }
                head->tag = strdup("head");
                NbNode *next = root->first_child;
                local_insert_before(root, head, next);
                nb_mutation_notify_childlist(ctx, root, head, NULL, NULL, next);
                parent = head;
            } else {
                /* Fragment-like parse ("<div>x</div>"): there is no html
                 * root to hold a head. Attach the title to the document
                 * itself; synthesizing head inside the div would corrupt the
                 * parsed fragment's content. */
                parent = doc;
            }
        }
        te = calloc(1, sizeof(*te));
        if (!te) { JS_FreeCString(ctx, vl); return JS_UNDEFINED; }
        te->tag = strdup("title");
        local_append(parent, te);
        nb_mutation_notify_childlist(ctx, parent, te, NULL, NULL, NULL);
    }
    /* Replace the title's text on both parse shapes: the folded fetch path
     * keeps it on ->text, the tree path as a #text child. */
    clear_children(te);
    free(te->text);
    te->text = NULL;
    NbNode *tn = NULL;
    if (v[0]) {
        tn = calloc(1, sizeof(*tn));
        if (tn) { tn->text = strdup(v); local_append(te, tn); }
    }
    if (tn) nb_mutation_notify_childlist(ctx, te, tn, NULL, NULL, NULL);
    else nb_obs_recheck(ctx);
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
