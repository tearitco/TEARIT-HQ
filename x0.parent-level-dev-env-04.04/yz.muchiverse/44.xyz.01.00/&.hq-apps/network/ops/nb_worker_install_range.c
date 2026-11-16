/* nb_worker_install_range.c - Range global install (mechanical extract, single-TU include) */
        /* Range (2026-11-09): endpoints, boundary setters, select, collapse,
           clone, toString. */
        f = JS_NewCFunction2(ctx, nb_range_ctor, "Range", 0, JS_CFUNC_constructor, 0);
        p = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, f, "prototype", p);
        {
            JSAtom nm = JS_NewAtom(ctx, "startContainer");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get startContainer", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "startOffset");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get startOffset", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "endContainer");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get endContainer", 0, JS_CFUNC_generic_magic, 2), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "endOffset");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get endOffset", 0, JS_CFUNC_generic_magic, 3), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "collapsed");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get collapsed", 0, JS_CFUNC_generic_magic, 4), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "commonAncestorContainer");
            JS_DefinePropertyGetSet(ctx, p, nm, JS_NewCFunctionMagic(ctx, nb_range_get, "get commonAncestorContainer", 0, JS_CFUNC_generic_magic, 5), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        }
        JS_SetPropertyStr(ctx, p, "setStart", JS_NewCFunctionMagic(ctx, nb_range_set_m, "setStart", 2, JS_CFUNC_generic_magic, 0));
        JS_SetPropertyStr(ctx, p, "setEnd", JS_NewCFunctionMagic(ctx, nb_range_set_m, "setEnd", 2, JS_CFUNC_generic_magic, 1));
        JS_SetPropertyStr(ctx, p, "setStartBefore", JS_NewCFunctionMagic(ctx, nb_range_ba_m, "setStartBefore", 1, JS_CFUNC_generic_magic, 0));
        JS_SetPropertyStr(ctx, p, "setStartAfter", JS_NewCFunctionMagic(ctx, nb_range_ba_m, "setStartAfter", 1, JS_CFUNC_generic_magic, 1));
        JS_SetPropertyStr(ctx, p, "setEndBefore", JS_NewCFunctionMagic(ctx, nb_range_ba_m, "setEndBefore", 1, JS_CFUNC_generic_magic, 2));
        JS_SetPropertyStr(ctx, p, "setEndAfter", JS_NewCFunctionMagic(ctx, nb_range_ba_m, "setEndAfter", 1, JS_CFUNC_generic_magic, 3));
        JS_SetPropertyStr(ctx, p, "selectNode", JS_NewCFunction(ctx, nb_range_select_node, "selectNode", 1));
        JS_SetPropertyStr(ctx, p, "selectNodeContents", JS_NewCFunction(ctx, nb_range_select_contents, "selectNodeContents", 1));
        JS_SetPropertyStr(ctx, p, "collapse", JS_NewCFunction(ctx, nb_range_collapse, "collapse", 1));
        JS_SetPropertyStr(ctx, p, "cloneRange", JS_NewCFunction(ctx, nb_range_clone, "cloneRange", 0));
        JS_SetPropertyStr(ctx, p, "cloneContents", JS_NewCFunction(ctx, nb_range_clone_contents, "cloneContents", 0));
        JS_SetPropertyStr(ctx, p, "deleteContents", JS_NewCFunction(ctx, nb_range_delete_contents, "deleteContents", 0));
        JS_SetPropertyStr(ctx, p, "extractContents", JS_NewCFunction(ctx, nb_range_extract_contents, "extractContents", 0));
        JS_SetPropertyStr(ctx, p, "detach", JS_NewCFunction(ctx, nb_range_detach, "detach", 0));
        JS_SetPropertyStr(ctx, p, "toString", JS_NewCFunction(ctx, nb_range_to_string, "toString", 0));
        JS_SetPropertyStr(ctx, g, "Range", f);
