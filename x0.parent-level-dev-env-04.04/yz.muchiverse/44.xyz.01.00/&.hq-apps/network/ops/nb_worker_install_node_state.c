/* nb_worker_install_node_state.c - Node isConnected/ownerDocument/nodeValue installs (mechanical extract, single-TU include) */
    {
        JSAtom nm = JS_NewAtom(ctx, "isConnected");
        JS_DefinePropertyGetSet(ctx, Np, nm,
            JS_NewCFunction(ctx, nb_node_isConnected, "get isConnected", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    {
        JSAtom nm = JS_NewAtom(ctx, "ownerDocument");
        JS_DefinePropertyGetSet(ctx, Np, nm,
            JS_NewCFunction(ctx, nb_node_ownerDocument, "get ownerDocument", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    {
        JSAtom nm = JS_NewAtom(ctx, "nodeValue");
        JS_DefinePropertyGetSet(ctx, Np, nm,
            JS_NewCFunction(ctx, nb_node_value_get, "get nodeValue", 0),
            JS_NewCFunction(ctx, nb_node_value_set, "set nodeValue", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
