/* nb_worker_dom_document.c - document/DOM factory helpers (mechanical extract, single-TU include) */
static void nb_set_owner_if_doc(JSContext *ctx, JSValue wrapper, JSValueConst creator);

static JSValue nb_dom_createElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->tag = strdup(tag);
    for (char *t = n->tag; *t; t++) *t = (char)((*t >= 'A' && *t <= 'Z') ? *t + 32 : *t);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    JSValue w = push_node(ctx, n);
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}
static JSValue nb_dom_createElementNS(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* Our element tree is flat (no namespace tracking), so a namespace +
     * qualified name collapses to the plain tag — same node the browser
     * would build for e.g. an SVG <svg>. Matches createElement semantics;
     * real bundles (web-animations-lite calls createElementNS during load)
     * only need the returned element to exist and accept method calls. */
    char *owned = NULL;
    const char *tag = (argc > 1 && JS_IsString(argv[1])) ? (owned = JS_ToCString(ctx, argv[1])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->tag = strdup(tag);
    for (char *t = n->tag; *t; t++) *t = (char)((*t >= 'A' && *t <= 'Z') ? *t + 32 : *t);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    JSValue w = push_node(ctx, n);
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}
/* document.implementation (row-31): webcomponents-lite.js does
 * `document.implementation.createHTMLDocument("inert")` at init and uses
 * the result as a scratch document to parse HTML strings via innerHTML
 * (`Sd.createElement(...); d.innerHTML = b`). Our scratch doc reuses the
 * real createElement/createElementNS (orphan nodes) and advertises the
 * HTML namespace so ShadyDOM's namespace branch takes createElement. */
static JSValue make_fake_doc(JSContext *ctx) {
    JSValue d = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, d, "namespaceURI", JS_NewString(ctx, "http://www.w3.org/1999/xhtml"));
    JS_SetPropertyStr(ctx, d, "contentType", JS_NewString(ctx, "text/html"));
    JS_SetPropertyStr(ctx, d, "createElement", JS_NewCFunction(ctx, nb_dom_createElement, "createElement", 1));
    JS_SetPropertyStr(ctx, d, "createElementNS", JS_NewCFunction(ctx, nb_dom_createElementNS, "createElementNS", 2));
    return d;
}
static JSValue nb_impl_createHTMLDocument(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return make_fake_doc(ctx);
}
static JSValue nb_impl_createDocument(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return make_fake_doc(ctx);
}
static JSValue nb_impl_createDocumentType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue t = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, t, "name", (argc > 0) ? JS_DupValue(ctx, argv[0]) : JS_NewString(ctx, "html"));
    JS_SetPropertyStr(ctx, t, "publicId", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, t, "systemId", JS_NewString(ctx, ""));
    return t;
}
static JSValue nb_impl_hasFeature(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewBool(ctx, 1);
}
static JSValue nb_dom_documentElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "html") : NULL;
    if (el) return push_node(ctx, el);
    return JS_NULL;
}
static JSValue nb_doc_tagged(JSContext *ctx, JSValueConst this_val, const char *tag) {
    NbNode *n = get_this(ctx, this_val);
    if (n) while (n->parent) n = n->parent;
    JSValue arr = JS_NewArray(ctx);
    if (n) {
        int i = 0;
        for (const NbNode *c = n->first_child; c; c = c->next_sibling)
            collect_tag_into(ctx, (NbNode *)c, tag, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
static JSValue nb_doc_scripts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_doc_tagged(ctx, this_val, "script");
}
static JSValue nb_doc_images(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_doc_tagged(ctx, this_val, "img");
}
static JSValue nb_doc_links(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_doc_tagged(ctx, this_val, "link");
}
static JSValue nb_doc_forms(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_doc_tagged(ctx, this_val, "form");
}
static JSValue nb_doc_linksA(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_doc_tagged(ctx, this_val, "a");
}
static JSValue nb_dom_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *el = g_dom_root ? find_tag_first(g_dom_root, "body") : NULL;
    if (el) return push_node(ctx, el);
    return JS_NULL;
}
/* rung-2 remainder: document.createTextNode / getElementsByClassName /
 * document.head. The parser skips <head> wholesale, so browsers' implicit
 * empty <head> is created on first access (stays out of the render path). */
static JSValue nb_dom_createTextNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : "";
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_NULL; }
    n->text = strdup(v);
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    JSValue w = push_node(ctx, n);
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}
static void collect_cls_into(JSContext *ctx, NbNode *n, const char *tok, JSValue arr, int *i) {
    if (!n) return;
    if (n->tag && n->tag[0] && has_class(n, tok)) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_cls_into(ctx, (NbNode *)c, tok, arr, i);
}
static JSValue nb_dom_getElementsByClassName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tok = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root && tok && *tok) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            collect_cls_into(ctx, (NbNode *)c, tok, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
static JSValue nb_dom_head(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_dom_root) return JS_NULL;
    NbNode *head = find_tag_first(g_dom_root, "head");
    if (!head) {
        head = calloc(1, sizeof(*head));
        if (!head) return JS_NULL;
        head->tag = strdup("head");
        NbNode *html = find_tag_first(g_dom_root, "html");
        if (html) local_insert_before(html, head, html->first_child);
        else local_insert_before(g_dom_root, head, NULL);
    }
    return push_node(ctx, head);
}

/* document.createEvent — create a synthetic event object */
static JSValue nb_dom_createEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsString(argv[0])) return JS_UNDEFINED;
    const char *type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_UNDEFINED;
    JSValue Event = get_global_attr(ctx, "Event");
    if (!JS_IsFunction(ctx, Event)) { JS_FreeValue(ctx, Event); JS_FreeCString(ctx, type); return JS_UNDEFINED; }
    JSValue ev = JS_CallConstructor(ctx, Event, 1, argv);
    JS_FreeValue(ctx, Event);
    JS_FreeCString(ctx, type);
    return ev;
}

