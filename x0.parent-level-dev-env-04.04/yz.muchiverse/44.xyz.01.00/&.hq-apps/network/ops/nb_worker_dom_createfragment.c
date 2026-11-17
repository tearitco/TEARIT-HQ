/* nb_worker_dom_createfragment.c - DocumentFragment factory (mechanical extract, single-TU include) */
static JSValue nb_dom_createDocumentFragment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *frag = calloc(1, sizeof(*frag));
    if (!frag) return JS_ThrowOutOfMemory(ctx);
    frag->tag = strdup("#document-fragment");
    orphan_add(frag);
    JSValue w = make_fragment_wrapper(ctx, frag, "#document-fragment");
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}
