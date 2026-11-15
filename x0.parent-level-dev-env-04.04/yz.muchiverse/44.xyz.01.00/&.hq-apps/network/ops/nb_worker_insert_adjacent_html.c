/* nb_worker_insert_adjacent_html.c - Element.insertAdjacentHTML (mechanical extract, single-TU include) */
static JSValue nb_el_insertAdjacentHTML(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int pos = (argc > 0) ? nb_ia_parse_pos(ctx, argv[0]) : -1;
    if (pos == -2) return JS_EXCEPTION;
    if (pos < 0) return nb_ia_syntax_error(ctx);
    if (!n) return JS_UNDEFINED;
    if (n->tag && !strcmp(n->tag, "template") &&
        (pos == NB_IA_AFTER_BEGIN || pos == NB_IA_BEFORE_END)) {
        NbNode *frag = nb_template_content_frag(ctx, this_val, n);
        if (frag) n = frag;
    }
    NbNode *parent = NULL, *ref = NULL;
    if (!nb_ia_place(n, pos, &parent, &ref)) return JS_UNDEFINED;
    char *html = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;
    if (!html) return JS_EXCEPTION;
    nb_ia_insert_html(ctx, parent, ref, html);
    JS_FreeCString(ctx, html);
    return JS_UNDEFINED;
}
