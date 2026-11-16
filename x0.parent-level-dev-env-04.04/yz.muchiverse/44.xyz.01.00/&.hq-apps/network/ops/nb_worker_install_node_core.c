/* nb_worker_install_node_core.c - Node prototype core methods/getters install (mechanical extract, single-TU include) */
    /* Node-level methods live on Node.prototype: every element, text and
     * document node shares them. remove() is on Element (ChildNode) in the
     * spec; put it there, not here. */
    JS_SetPropertyStr(ctx, Np, "cloneNode",      JS_NewCFunction(ctx, nb_el_cloneNode, "cloneNode", 1));
    JS_SetPropertyStr(ctx, Np, "hasChildNodes",  JS_NewCFunction(ctx, nb_el_hasChildNodes, "hasChildNodes", 0));
    JS_SetPropertyStr(ctx, Np, "normalize",      JS_NewCFunction(ctx, nb_el_normalize, "normalize", 0));
    {
        JSAtom nm;
        nm = JS_NewAtom(ctx, "lastChild");
        JS_DefinePropertyGetSet(ctx, Np, nm, JS_NewCFunction(ctx, nb_el_lastChild, "get lastChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousSibling");
        JS_DefinePropertyGetSet(ctx, Np, nm, JS_NewCFunction(ctx, nb_el_previousSibling, "get previousSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentElement");
        JS_DefinePropertyGetSet(ctx, Np, nm, JS_NewCFunction(ctx, nb_el_parentElement, "get parentElement", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
