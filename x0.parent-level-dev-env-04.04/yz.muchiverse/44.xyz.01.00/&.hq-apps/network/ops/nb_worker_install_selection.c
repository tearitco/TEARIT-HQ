/* nb_worker_install_selection.c - Selection global install (mechanical extract, single-TU include) */
        /* Selection (2026-11-09): range list with anchor/focus and toString. */
        f = JS_NewCFunction2(ctx, nb_sel_ctor, "Selection", 0, JS_CFUNC_constructor, 0);
        p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        {
            JSAtom nm = JS_NewAtom(ctx, "rangeCount");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_sel_get, "get rangeCount", 0, JS_CFUNC_generic_magic, 4), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "anchorNode");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_sel_get, "get anchorNode", 0, JS_CFUNC_generic_magic, 2), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "anchorOffset");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_sel_get, "get anchorOffset", 0, JS_CFUNC_generic_magic, 3), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "isCollapsed");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_sel_get, "get isCollapsed", 0, JS_CFUNC_generic_magic, 7), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, p, "addRange", JS_NewCFunction(ctx, nb_sel_add_range, "addRange", 1));
        JS_SetPropertyStr(ctx, p, "removeRange", JS_NewCFunction(ctx, nb_sel_remove_range, "removeRange", 1));
        JS_SetPropertyStr(ctx, p, "removeAllRanges", JS_NewCFunction(ctx, nb_sel_remove_all, "removeAllRanges", 0));
        JS_SetPropertyStr(ctx, p, "collapse", JS_NewCFunction(ctx, nb_sel_collapse, "collapse", 2));
        JS_SetPropertyStr(ctx, p, "getRangeAt", JS_NewCFunction(ctx, nb_sel_get_range, "getRangeAt", 1));
        JS_SetPropertyStr(ctx, p, "toString", JS_NewCFunction(ctx, nb_sel_to_string, "toString", 0));
        JS_SetPropertyStr(ctx, g, "Selection", f);
