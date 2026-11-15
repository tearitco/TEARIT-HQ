/* nb_worker_range_clone_contents.c - Range clone_contents implementation (mechanical extract, single-TU include) */
static NbNode *nbi_clone(const NbNode *n, int deep);
static void nb_clone_walk(NbNode *src, NbNode *dst, long ls, long le) {
    if (!src || !dst || ls >= le) return;
    long low = nb_linpos(src);
    long high = low + nb_txt_total(src);
    if (high <= ls || low >= le) return;
    int is_text = !src->tag || !src->tag[0];
    if (low >= ls && high <= le) {
        NbNode *c = nbi_clone(src, 1);
        if (!c) return;
        orphan_remove(c);
        local_append(dst, c);
        return;
    }
    if (is_text) {
        /* Partial text overlap inside a text node: cloned sliced text. */
        NbNode *c = calloc(1, sizeof(*c));
        if (!c) return;
        long own_s = low, own_e = high;
        long cs = ls > own_s ? ls : own_s;
        long ce = le < own_e ? le : own_e;
        if (ce > cs && src->text) {
            size_t n = (size_t)(ce - cs);
            c->text = malloc(n + 1);
            if (c->text) { memcpy(c->text, src->text + (cs - own_s), n); c->text[n] = 0; }
        }
        orphan_add(c);
        orphan_remove(c);
        local_append(dst, c);
        return;
    }
    if (src->tag && !strcmp(src->tag, "#document")) {
        for (NbNode *k = src->first_child; k; k = k->next_sibling)
            nb_clone_walk(k, dst, ls, le);
        return;
    }
    /* Partially selected element: shallow clone, trim own text, recurse. */
    NbNode *c = nbi_clone(src, 0);
    if (!c) return;
    orphan_remove(c);
    long own_s = low, own_e = low + (src->text ? (long)strlen(src->text) : 0);
    if (own_e <= ls || own_s >= le) {
        free(c->text); c->text = NULL;
    } else if (!(own_s >= ls && own_e <= le) && src->text) {
        long cs = ls > own_s ? ls : own_s;
        long ce = le < own_e ? le : own_e;
        size_t n = (size_t)(ce - cs);
        char *nt = malloc(n + 1);
        if (nt) {
            memcpy(nt, src->text + (cs - own_s), n);
            nt[n] = 0;
            free(c->text);
            c->text = nt;
        }
    }
    local_append(dst, c);
    for (NbNode *k = src->first_child; k; k = k->next_sibling)
        nb_clone_walk(k, c, ls, le);
}
static JSValue nb_range_clone_contents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    JSValue so_v = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
    JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
    JSValue eo_v = JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
    NbNode *s = get_node(ctx, sc);
    NbNode *e = get_node(ctx, ec);
    int32_t so = 0, eo = 0;
    JS_ToInt32(ctx, &so, so_v);
    JS_ToInt32(ctx, &eo, eo_v);
    /* Range contents live in a detached DocumentFragment, not a Document,
     * so Document-only semantics (e.g. textContent === null) do not apply. */
    NbNode *frag = calloc(1, sizeof(*frag));
    if (!frag) {
        JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so_v); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo_v);
        return JS_NULL;
    }
    frag->tag = strdup("#document-fragment");
    orphan_add(frag);
    if (s && e) {
        long ls = nb_endpoint_lin(s, so);
        long le = nb_endpoint_lin(e, eo);
        if (le > ls) {
            /* Common ancestor: project its contents only, not the ancestor
             * shell above it. Text ranges anchored in a single text node
             * clone as one sliced text node in the fragment. */
            NbNode *ca = NULL;
            for (NbNode *p = s; p && !ca; p = p->parent) {
                for (NbNode *q = e; q; q = q->parent) {
                    if (p == q) { ca = p; break; }
                }
            }
            if (!ca) ca = nb_top_root(s);
            if (!ca->tag || !ca->tag[0]) {
                long own_s = nb_linpos(ca);
                long cs = ls > own_s ? ls : own_s;
                long ce = le < own_s + nb_txt_total(ca) ? le : own_s + nb_txt_total(ca);
                if (ce > cs) {
                    NbNode *c = calloc(1, sizeof(*c));
                    if (c) {
                        size_t n = (size_t)(ce - cs);
                        c->text = malloc(n + 1);
                        if (c->text) { memcpy(c->text, ca->text + (cs - own_s), n); c->text[n] = 0; }
                        orphan_add(c);
                        orphan_remove(c);
                        local_append(frag, c);
                    }
                }
            } else {
                long own_s = nb_linpos(ca);
                long own_e = own_s + (ca->text ? (long)strlen(ca->text) : 0);
                if (ca->text && own_e > ls && own_s < le) {
                    long cs = ls > own_s ? ls : own_s;
                    long ce = le < own_e ? le : own_e;
                    if (ce > cs) {
                        NbNode *c = calloc(1, sizeof(*c));
                        if (c) {
                            size_t n = (size_t)(ce - cs);
                            c->text = malloc(n + 1);
                            if (c->text) { memcpy(c->text, ca->text + (cs - own_s), n); c->text[n] = 0; }
                            orphan_add(c);
                            orphan_remove(c);
                            local_append(frag, c);
                        }
                    }
                }
                for (NbNode *k = ca->first_child; k; k = k->next_sibling)
                    nb_clone_walk(k, frag, ls, le);
            }
        }
    }
    JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so_v); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo_v);
    return push_node(ctx, frag);
}
