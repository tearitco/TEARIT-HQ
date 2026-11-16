/* nb_worker_install_el_methods.c - Element prototype methods install (mechanical extract, single-TU include) */
    /* Element methods (2026-11-02). Installed on Element.prototype rather
     * than per wrapper so elements made later by createElement get them too,
     * which is how a browser behaves. See the block above for what these do
     * and what is deliberately not-to-spec. */
    JS_SetPropertyStr(ctx, Ep, "matches",        JS_NewCFunction(ctx, nb_el_matches, "matches", 1));
    {
        JSAtom nm = JS_NewAtom(ctx, "localName");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_localName, "get localName", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, Ep, "closest",        JS_NewCFunction(ctx, nb_el_closest, "closest", 1));
    JS_SetPropertyStr(ctx, Ep, "querySelector",  JS_NewCFunction(ctx, nb_el_querySelector, "querySelector", 1));
    JS_SetPropertyStr(ctx, Ep, "querySelectorAll", JS_NewCFunction(ctx, nb_el_querySelectorAll, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, Ep, "getElementsByTagName",   JS_NewCFunction(ctx, nb_el_getElementsByTagName, "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, Ep, "getElementsByClassName", JS_NewCFunction(ctx, nb_el_getElementsByClassName, "getElementsByClassName", 1));
    JS_SetPropertyStr(ctx, Ep, "prepend",        JS_NewCFunction(ctx, nb_el_prepend_js, "prepend", 0));
    JS_SetPropertyStr(ctx, Ep, "append",         JS_NewCFunction(ctx, nb_el_append_js, "append", 0));
    JS_SetPropertyStr(ctx, Ep, "before",         JS_NewCFunction(ctx, nb_el_before_js, "before", 0));
    JS_SetPropertyStr(ctx, Ep, "after",          JS_NewCFunction(ctx, nb_el_after_js, "after", 0));
    JS_SetPropertyStr(ctx, Ep, "replaceWith",    JS_NewCFunction(ctx, nb_el_replaceWith_js, "replaceWith", 0));
    JS_SetPropertyStr(ctx, Ep, "replaceChildren", JS_NewCFunction(ctx, nb_el_replaceChildren_js, "replaceChildren", 0));
    JS_SetPropertyStr(ctx, Ep, "getClientRects", JS_NewCFunction(ctx, nb_el_getClientRects, "getClientRects", 0));
    JS_SetPropertyStr(ctx, Ep, "scrollIntoView", JS_NewCFunction(ctx, nb_el_scrollIntoView, "scrollIntoView", 0));
