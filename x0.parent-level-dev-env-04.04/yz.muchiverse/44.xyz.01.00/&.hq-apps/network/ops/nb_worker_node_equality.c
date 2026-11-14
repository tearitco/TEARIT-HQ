/* nb_worker_node_equality.c - Node.isEqualNode/isSameNode helpers (mechanical extract, single-TU include) */
static int nbi_streq(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}
static int nbi_equal(const NbNode *a, const NbNode *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (!nbi_streq(a->tag, b->tag) || !nbi_streq(a->id, b->id) ||
        !nbi_streq(a->cls, b->cls) || !nbi_streq(a->text, b->text) ||
        !nbi_streq(a->attrs, b->attrs)) return 0;
    const NbNode *x = a->first_child, *y = b->first_child;
    while (x && y) { if (!nbi_equal(x, y)) return 0; x = x->next_sibling; y = y->next_sibling; }
    return x == y;   /* both NULL, or different child counts */
}
static JSValue nb_el_isEqualNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *a = get_this(ctx, this_val);
    NbNode *b = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    return JS_NewBool(ctx, nbi_equal(a, b));
}
static JSValue nb_el_isSameNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *a = get_this(ctx, this_val);
    if (!a) return JS_FALSE;
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_FALSE;
    NbNode *b = get_node(ctx, argv[0]);
    return JS_NewBool(ctx, b != NULL && a == b);
}
