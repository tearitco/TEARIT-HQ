/* nb_worker_insert_adjacent_text.c - Element.insertAdjacentText (mechanical extract, single-TU include) */
static JSValue nb_el_insertAdjacentText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int pos = (argc > 0) ? nb_ia_parse_pos(ctx, argv[0]) : -1;
    if (pos == -2) return JS_EXCEPTION;
    if (pos < 0) return nb_ia_syntax_error(ctx);
    if (!n) return JS_UNDEFINED;
    NbNode *parent = NULL, *ref = NULL;
    if (!nb_ia_place(n, pos, &parent, &ref)) return JS_UNDEFINED;
    char *text = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;
    if (!text) return JS_EXCEPTION;
    NbNode *t = calloc(1, sizeof(*t));
    if (t) {
        t->text = strdup(text);
        orphan_add(t);
        nb_ia_attach(ctx, parent, ref, t);
    }
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}
