/* nb_worker_install_el_nav.c - Element structural navigation getters install (mechanical extract, single-TU include) */
    /* Structural navigation (2026-11-11). Node-level accessors land on
     * Node.prototype, element-only ones on Element.prototype, matching where
     * a browser exposes them and what `in` probes expect. */
    {
        JSAtom nm;
        nm = JS_NewAtom(ctx, "firstElementChild");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_firstElementChild, "get firstElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "lastElementChild");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_lastElementChild, "get lastElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextElementSibling");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_nextElementSibling, "get nextElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousElementSibling");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_previousElementSibling, "get previousElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childElementCount");
        JS_DefinePropertyGetSet(ctx, Ep, nm, JS_NewCFunction(ctx, nb_el_childElementCount, "get childElementCount", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
