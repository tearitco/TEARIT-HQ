/* nb_worker_install_domtokenlist.c - DOMTokenList global install (mechanical extract, single-TU include) */
        f = add_class(ctx, "DOMTokenList", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "constructor", JS_DupValue(ctx, f));
        JS_SetPropertyStr(ctx, p, "item", JS_NewCFunction(ctx, nb_cl_item, "item", 1));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "DOMTokenList", f);
