/* nb_worker_frag_template.c - template content fragment helper (mechanical extract, single-TU include) */
/* <template>.innerHTML / appendChild land in template.content, which is a
 * C-backed #document-fragment wrapper (overrides the prelude's inert JS
 * docfrag so stamping can read firstChild/cloneNode/querySelector). */
static NbNode *nb_template_content_frag(JSContext *ctx, JSValue el, NbNode *n) {
    if (!n || !n->tag || strcmp(n->tag, "template") != 0) return NULL;
    JSValue ex = JS_GetPropertyStr(ctx, el, "content");
    NbNode *frag = JS_IsObject(ex) ? get_node(ctx, ex) : NULL;
    if (!frag) {
        frag = calloc(1, sizeof(*frag));
        if (frag) {
            frag->tag = strdup("#document-fragment");
            orphan_add(frag);
            JSValue w = make_fragment_wrapper(ctx, frag, "#document-fragment");
            if (JS_IsObject(w)) JS_SetPropertyStr(ctx, el, "content", w);
        }
    }
    JS_FreeValue(ctx, ex);
    return frag;
}