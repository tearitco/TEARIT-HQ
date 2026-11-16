/* nb_worker_node_insert_ops.c - element insert/remove/replace operations (mechanical extract, single-TU include) */
/* The four variadic inserts. Order is the part that is easy to get wrong, so
 * each is stated and tested explicitly:
 *   append(a,b)  -> ..., a, b
 *   prepend(a,b) -> a, b, ...
 *   before(a,b)  -> a, b, ref
 *   after(a,b)   -> ref, a, b
 * All four walk the arguments FORWARD against an anchor captured before the
 * loop (the pre-existing first child, the receiver, or the receiver's
 * original next sibling). Reversing the walk instead would be the bug. */
static void nbi_do(JSContext *ctx, int argc, JSValueConst *argv, NbNode *self, int mode) {
    if (!self) return;
    NbNode *anchor = NULL;
    if (mode == NBI_APPEND || mode == NBI_PREPEND) {
        anchor = self->first_child;              /* NULL when empty -> append */
    } else {
        if (!self->parent) return;               /* detached: nothing to sit beside */
        anchor = (mode == NBI_BEFORE) ? self : self->next_sibling;
    }
    NbNode *into = (mode == NBI_APPEND || mode == NBI_PREPEND) ? self : self->parent;
    for (int i = 0; i < argc; i++) {
        NbNode *n = nbi_arg(ctx, argv[i], into);
        if (!n) continue;
        if (nb_is_doc_fragment_node(n)) {
            nb_insert_fragment_children(NULL, into, n, mode == NBI_APPEND ? NULL : anchor);
            orphan_add(n);
            continue;
        }
        local_insert_before(into, n, mode == NBI_APPEND ? NULL : anchor);
    }
}

static JSValue nb_el_append_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    nbi_do(ctx, argc, argv, get_this(ctx, this_val), NBI_APPEND);
    return JS_UNDEFINED;
}
static JSValue nb_el_prepend_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    nbi_do(ctx, argc, argv, get_this(ctx, this_val), NBI_PREPEND);
    return JS_UNDEFINED;
}
static JSValue nb_el_before_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    nbi_do(ctx, argc, argv, get_this(ctx, this_val), NBI_BEFORE);
    return JS_UNDEFINED;
}
static JSValue nb_el_after_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    nbi_do(ctx, argc, argv, get_this(ctx, this_val), NBI_AFTER);
    return JS_UNDEFINED;
}
static JSValue nb_el_replaceWith_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *self = get_this(ctx, this_val);
    nbi_do(ctx, argc, argv, self, NBI_BEFORE);   /* before(ref) then unlink ref */
    if (self) { node_detach(self); orphan_add(self); }
    return JS_UNDEFINED;
}
static JSValue nb_el_remove_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *self = get_this(ctx, this_val);
    /* orphan_add as well as detach: a detached node is no longer reachable
     * from g_dom_root, so without it teardown would never free it. A later
     * re-insertion takes it back off the list. */
    if (self) { node_detach(self); orphan_add(self); }
    return JS_UNDEFINED;
}
static JSValue nb_el_replaceChildren_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *self = get_this(ctx, this_val);
    if (!self) return JS_UNDEFINED;
    clear_children(self);
    /* Replace ALL children, including the folded direct text a LOAD-tree
     * element carries in n->text. */
    free(self->text); self->text = NULL;
    nbi_do(ctx, argc, argv, self, NBI_APPEND);
    return JS_UNDEFINED;
}
