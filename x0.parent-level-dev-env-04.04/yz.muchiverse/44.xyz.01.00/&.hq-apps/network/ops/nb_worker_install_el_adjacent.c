/* nb_worker_install_el_adjacent.c - Element insertAdjacent/outerHTML installs (mechanical extract, single-TU include) */
    JS_SetPropertyStr(ctx, Ep, "insertAdjacentHTML", JS_NewCFunction(ctx, nb_el_insertAdjacentHTML, "insertAdjacentHTML", 2));
    JS_SetPropertyStr(ctx, Ep, "insertAdjacentText", JS_NewCFunction(ctx, nb_el_insertAdjacentText, "insertAdjacentText", 2));
    JS_SetPropertyStr(ctx, Ep, "insertAdjacentElement", JS_NewCFunction(ctx, nb_el_insertAdjacentElement, "insertAdjacentElement", 2));
    {
        JSAtom nm = JS_NewAtom(ctx, "outerHTML");
        JS_DefinePropertyGetSet(ctx, Ep, nm,
            JS_NewCFunction(ctx, nb_el_outerHTML_get, "get outerHTML", 0),
            JS_NewCFunction(ctx, nb_el_outerHTML_set, "set outerHTML", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
