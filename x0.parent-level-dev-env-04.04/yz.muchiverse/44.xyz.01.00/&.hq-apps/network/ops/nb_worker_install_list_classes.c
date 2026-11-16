/* nb_worker_install_list_classes.c - NodeList/HTMLCollection global installs (mechanical extract, single-TU include) */
        /* List classes (2026-11-09): NodeList/HTMLCollection prototypes
           carry item(); query results are arrays with these prototypes set.
           2026-11-13: add forEach and @@iterator (Array-like collection compat). */
        {
            JSValue Arr = JS_GetPropertyStr(ctx, g, "Array");
            JSValue ArrP = JS_GetPropertyStr(ctx, Arr, "prototype");
            JSValue arrForEach = JS_GetPropertyStr(ctx, ArrP, "forEach");
            JSValue Sym = JS_GetPropertyStr(ctx, g, "Symbol");
            JSValue symIter = JS_GetPropertyStr(ctx, Sym, "iterator");
            JSValue arrIter = JS_UNDEFINED;
            JSAtom iterAtom = JS_ATOM_NULL;
            if (JS_IsSymbol(symIter)) {
                iterAtom = JS_ValueToAtom(ctx, symIter);
                arrIter = JS_GetProperty(ctx, ArrP, iterAtom);
            }

            f = add_class(ctx, "NodeList", JS_UNDEFINED);
            p = JS_GetPropertyStr(ctx, f, "prototype");
            JS_SetPropertyStr(ctx, p, "constructor", JS_DupValue(ctx, f));
            JS_SetPropertyStr(ctx, p, "item", JS_NewCFunction(ctx, nb_list_item, "item", 1));
            if (JS_IsFunction(ctx, arrForEach))
                JS_SetPropertyStr(ctx, p, "forEach", JS_DupValue(ctx, arrForEach));
            if (iterAtom != JS_ATOM_NULL && JS_IsFunction(ctx, arrIter))
                JS_SetProperty(ctx, p, iterAtom, JS_DupValue(ctx, arrIter));
            JS_FreeValue(ctx, p);
            JS_SetPropertyStr(ctx, g, "NodeList", f);

            f = add_class(ctx, "HTMLCollection", JS_UNDEFINED);
            p = JS_GetPropertyStr(ctx, f, "prototype");
            JS_SetPropertyStr(ctx, p, "constructor", JS_DupValue(ctx, f));
            JS_SetPropertyStr(ctx, p, "item", JS_NewCFunction(ctx, nb_list_item, "item", 1));
            if (JS_IsFunction(ctx, arrForEach))
                JS_SetPropertyStr(ctx, p, "forEach", JS_DupValue(ctx, arrForEach));
            if (iterAtom != JS_ATOM_NULL && JS_IsFunction(ctx, arrIter))
                JS_SetProperty(ctx, p, iterAtom, JS_DupValue(ctx, arrIter));
            JS_FreeValue(ctx, p);
            JS_SetPropertyStr(ctx, g, "HTMLCollection", f);

            if (iterAtom != JS_ATOM_NULL) JS_FreeAtom(ctx, iterAtom);
            JS_FreeValue(ctx, arrIter);
            JS_FreeValue(ctx, symIter);
            JS_FreeValue(ctx, Sym);
            JS_FreeValue(ctx, arrForEach);
            JS_FreeValue(ctx, ArrP);
            JS_FreeValue(ctx, Arr);
        }
