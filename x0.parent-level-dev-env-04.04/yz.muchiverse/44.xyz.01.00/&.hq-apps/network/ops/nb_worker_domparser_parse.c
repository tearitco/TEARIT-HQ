/* nb_worker_domparser_parse.c - DOMParser.parseFromString (mechanical extract, single-TU include) */
static JSValue nb_domparser_parse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    char *str = NULL;
    const char *s = (argc > 0 && JS_IsString(argv[0])) ? (str = JS_ToCString(ctx, argv[0])) : NULL;
    if (!s) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: DOMParser.parseFromString requires a string"));
        return JS_Throw(ctx, e);
    }
    char *mime = NULL;
    const char *m = "text/html";
    if (argc > 1 && JS_IsString(argv[1])) m = mime = JS_ToCString(ctx, argv[1]);
    static const char *kOk[] = { "text/html", "application/xhtml+xml", "text/xml", "application/xml", "image/svg+xml", NULL };
    int ok = 0;
    for (int i = 0; kOk[i]; i++) if (!strcasecmp(m, kOk[i])) { ok = 1; break; }
    if (!ok) {
        JS_FreeCString(ctx, mime);
        JS_FreeCString(ctx, str);
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "TypeError: DOMParser: unsupported MIME type"));
        return JS_Throw(ctx, e);
    }
    char ctype[64];
    snprintf(ctype, sizeof(ctype), "%s", m);
    JS_FreeCString(ctx, mime);
    NbNode *root = nb_parse_html_nodes(s, strlen(s));
    JS_FreeCString(ctx, str);
    if (!root) {
        JSValue e = JS_NewError(ctx);
        if (!JS_IsException(e)) JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "DOMParser: parse failed"));
        return JS_Throw(ctx, e);
    }
    orphan_add(root);
    JSValue doc = push_node(ctx, root);
    /* Scoped document surface for this parsed tree only. */
    nb_install_doc_factories(ctx, doc);
    nb_install_doc_metadata(ctx, doc, ctype, "about:blank");
    {
        JSAtom nm = JS_NewAtom(ctx, "title");
        JS_DefinePropertyGetSet(ctx, doc, nm,
            JS_NewCFunction(ctx, nb_doc_title_get, "get title", 0),
            JS_NewCFunction(ctx, nb_doc_title_set, "set title", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, doc, "getElementById", JS_NewCFunction(ctx, nb_parsed_getElementById, "getElementById", 1));
    JS_SetPropertyStr(ctx, doc, "createRange", JS_NewCFunction(ctx, nb_dom_createRange, "createRange", 0));
    JS_SetPropertyStr(ctx, doc, "getSelection", JS_NewCFunction(ctx, nb_win_getSelection, "getSelection", 0));
    {
        JSAtom nm = JS_NewAtom(ctx, "scripts");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_doc_scripts, "get scripts", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "images");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_doc_images, "get images", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "links");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_doc_links, "get links", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "forms");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_doc_forms, "get forms", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "anchors");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_doc_linksA, "get anchors", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    {
        JSAtom nm = JS_NewAtom(ctx, "documentElement");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_parsed_documentElement, "get documentElement", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollingElement");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_parsed_documentElement, "get scrollingElement", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "body");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_parsed_body, "get body", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "head");
        JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_parsed_head, "get head", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    return doc;
}
