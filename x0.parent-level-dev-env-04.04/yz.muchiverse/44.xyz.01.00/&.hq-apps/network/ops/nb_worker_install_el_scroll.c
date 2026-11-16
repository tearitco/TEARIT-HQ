/* nb_worker_install_el_scroll.c - Element scroll properties install (mechanical extract, single-TU include) */
    {
        JSAtom nm = JS_NewAtom(ctx, "scrollTop");
        JS_DefinePropertyGetSet(ctx, Ep, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollTop", 0, JS_CFUNC_generic_magic, 1),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollTop", 1, JS_CFUNC_generic_magic, 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollLeft");
        JS_DefinePropertyGetSet(ctx, Ep, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollLeft", 0, JS_CFUNC_generic_magic, 0),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollLeft", 1, JS_CFUNC_generic_magic, 0),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollHeight");
        JS_DefinePropertyGetSet(ctx, Ep, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollWidth");
        JS_DefinePropertyGetSet(ctx, Ep, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
