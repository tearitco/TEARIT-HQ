/* nb_worker_install_dom.c - DOM installation helper (mechanical extract, single-TU include) */
static void install_dom(JSContext *ctx) {
    install_dom_classes(ctx);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, g, "document");
    if (JS_IsObject(doc)) {
        JS_SetPropertyStr(ctx, doc, "getElementById", JS_NewCFunction(ctx, nb_dom_getElementById, "getElementById", 1));
        JS_SetPropertyStr(ctx, doc, "getElementsByTagName", JS_NewCFunction(ctx, nb_dom_getElementsByTagName, "getElementsByTagName", 1));
        JS_SetPropertyStr(ctx, doc, "querySelector", JS_NewCFunction(ctx, nb_dom_querySelector, "querySelector", 1));
        JS_SetPropertyStr(ctx, doc, "querySelectorAll", JS_NewCFunction(ctx, nb_dom_querySelectorAll, "querySelectorAll", 1));
        JS_SetPropertyStr(ctx, doc, "createTreeWalker", JS_NewCFunction(ctx, nb_doc_createTreeWalker, "createTreeWalker", 1));
        JS_SetPropertyStr(ctx, doc, "createRange", JS_NewCFunction(ctx, nb_dom_createRange, "createRange", 0));
        JS_SetPropertyStr(ctx, doc, "getSelection", JS_NewCFunction(ctx, nb_win_getSelection, "getSelection", 0));
        nb_install_doc_factories(ctx, doc);
        nb_install_doc_metadata(ctx, doc, "text/html", g_href);
        JS_SetPropertyStr(ctx, doc, "ownerDocument", JS_NULL);
        JS_SetPropertyStr(ctx, g, "DocumentFragment", JS_NewCFunction2(ctx, nb_dom_createDocumentFragment, "DocumentFragment", 0, JS_CFUNC_constructor, 0));
        {
            JSAtom nm = JS_NewAtom(ctx, "textContent");
            JS_DefinePropertyGetSet(ctx, doc, nm,
                JS_NewCFunction(ctx, nb_live_doc_textContent_get, "get textContent", 0),
                JS_NewCFunction(ctx, nb_live_doc_textContent_set, "set textContent", 1),
                JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, doc, "getElementsByClassName", JS_NewCFunction(ctx, nb_dom_getElementsByClassName, "getElementsByClassName", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "documentElement");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_documentElement, "get documentElement", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            /* Standards-mode scrolling element is the root element. */
            nm = JS_NewAtom(ctx, "scrollingElement");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_documentElement, "get scrollingElement", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "scripts");
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
            JSAtom nm = JS_NewAtom(ctx, "body");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_body, "get body", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        {
            JSAtom nm = JS_NewAtom(ctx, "head");
            JS_DefinePropertyGetSet(ctx, doc, nm, JS_NewCFunction(ctx, nb_dom_head, "get head", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        /* rung-6: document.cookie — file-backed jar. The prelude's empty-jar
         * stub is replaced by real C natives (survive across LOADs because the
         * jar is on disk; each LOAD runs a fresh heap). */
        {
            JSAtom nm = JS_NewAtom(ctx, "cookie");
            JS_DefinePropertyGetSet(ctx, doc, nm,
                JS_NewCFunction(ctx, nb_dom_cookie_get, "get cookie", 0),
                JS_NewCFunction(ctx, nb_dom_cookie_set, "set cookie", 1),
                JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, doc, "referrer", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, doc, "domain", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, doc, "visibilityState", JS_NewString(ctx, "visible"));
        JS_SetPropertyStr(ctx, doc, "hidden", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, doc, "readyState", JS_NewString(ctx, "complete"));
        JS_SetPropertyStr(ctx, doc, "currentScript", JS_NULL);
        JS_SetPropertyStr(ctx, doc, "createEvent",
                          JS_NewCFunction(ctx, nb_dom_createEvent, "createEvent", 1));
        JS_SetPropertyStr(ctx, doc, "addEventListener",
                          JS_NewCFunction(ctx, nb_doc_addEventListener, "addEventListener", 2));
        JS_SetPropertyStr(ctx, doc, "removeEventListener",
                          JS_NewCFunction(ctx, nb_doc_removeEventListener, "removeEventListener", 2));
        JS_SetPropertyStr(ctx, doc, "dispatchEvent",
                          JS_NewCFunction(ctx, nb_doc_dispatchEvent, "dispatchEvent", 1));
    }
    JS_FreeValue(ctx, doc);

    /* rung 6: localStorage/sessionStorage — the prelude's twin no-op stubs
     * become two real objects: localStorage (disk jar, survives LOADs) and
     * sessionStorage (in-memory, cleared per LOAD). Fresh objects are created
     * here and replace the globals (redefining a non-configurable stub prop
     * would throw). Both share the shape getItem/setItem/removeItem/clear/key
     * + a length getter. */
    {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "getItem", JS_NewCFunction(ctx, nb_ls_getItem, "getItem", 1));
        JS_SetPropertyStr(ctx, obj, "setItem", JS_NewCFunction(ctx, nb_ls_setItem, "setItem", 2));
        JS_SetPropertyStr(ctx, obj, "removeItem", JS_NewCFunction(ctx, nb_ls_removeItem, "removeItem", 1));
        JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx, nb_ls_clear, "clear", 0));
        JS_SetPropertyStr(ctx, obj, "key", JS_NewCFunction(ctx, nb_ls_key, "key", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "length");
            JS_DefinePropertyGetSet(ctx, obj, nm, JS_NewCFunction(ctx, nb_ls_length, "get length", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, g, "localStorage", obj);
    }
    {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "getItem", JS_NewCFunction(ctx, nb_ss_getItem, "getItem", 1));
        JS_SetPropertyStr(ctx, obj, "setItem", JS_NewCFunction(ctx, nb_ss_setItem, "setItem", 2));
        JS_SetPropertyStr(ctx, obj, "removeItem", JS_NewCFunction(ctx, nb_ss_removeItem, "removeItem", 1));
        JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx, nb_ss_clear, "clear", 0));
        JS_SetPropertyStr(ctx, obj, "key", JS_NewCFunction(ctx, nb_ss_key, "key", 1));
        {
            JSAtom nm = JS_NewAtom(ctx, "length");
            JS_DefinePropertyGetSet(ctx, obj, nm, JS_NewCFunction(ctx, nb_ss_length, "get length", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, g, "sessionStorage", obj);
    }
    JS_FreeValue(ctx, g);
}

