/* nb_worker_install_dom_tail.c - install_dom_classes tail and cleanup (mechanical extract, single-TU include) */
        f = add_class(ctx, "NamedNodeMap", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "constructor", JS_DupValue(ctx, f));
        JS_SetPropertyStr(ctx, p, "item", JS_NewCFunction(ctx, nb_list_item, "item", 1));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "NamedNodeMap", f);
        f = add_class(ctx, "MutationRecord", JS_UNDEFINED);
        p = JS_GetPropertyStr(ctx, f, "prototype");
        JS_SetPropertyStr(ctx, p, "constructor", JS_DupValue(ctx, f));
        JS_FreeValue(ctx, p);
        JS_SetPropertyStr(ctx, g, "MutationRecord", f);
        JS_SetPropertyStr(ctx, g, "Image", JS_NewCFunction2(ctx, nb_image_ctor, "Image", 0, JS_CFUNC_constructor, 0));
    }

    JS_FreeValue(ctx, g);
    /* the constructor function refs were consumed by JS_SetPropertyStr on the
     * global; the prototype objects are still ours (dups chain/holder). */
    JS_FreeValue(ctx, Ap);
    JS_FreeValue(ctx, Dp);
    JS_FreeValue(ctx, Tp);
    JS_FreeValue(ctx, Sp);
    JS_FreeValue(ctx, Hp);
    JS_FreeValue(ctx, Ep);
    JS_FreeValue(ctx, Np);
    JS_FreeValue(ctx, ETp);
}
