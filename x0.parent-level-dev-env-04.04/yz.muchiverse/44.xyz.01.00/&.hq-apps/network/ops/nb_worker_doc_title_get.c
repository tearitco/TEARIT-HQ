/* nb_worker_doc_title_get.c - document.title getter helper (mechanical extract, single-TU include) */
static JSValue nb_doc_title_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *doc = get_this(ctx, this_val);
    NbNode *te = doc ? find_tag_first(doc, "title") : NULL;
    if (!te) return JS_NewString(ctx, "");
    SB b = {0, 0, 0};
    node_text_content(te, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
