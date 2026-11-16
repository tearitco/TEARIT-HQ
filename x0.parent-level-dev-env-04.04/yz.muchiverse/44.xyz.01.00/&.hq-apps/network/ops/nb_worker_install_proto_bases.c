/* nb_worker_install_proto_bases.c - Element.remove and HTMLElement/SVGElement/Text prototype bases (mechanical extract, single-TU include) */
    JS_SetPropertyStr(ctx, Ep, "remove",         JS_NewCFunction(ctx, nb_el_remove_js, "remove", 0));
    JSValue Hp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Hp, Ep);
    JS_SetPropertyStr(ctx, Hf, "prototype", JS_DupValue(ctx, Hp));
    JSValue Sp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Sp, Ep);
    JS_SetPropertyStr(ctx, Sf, "prototype", JS_DupValue(ctx, Sp));
    JSValue Tp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Tp, Np);
    JS_SetPropertyStr(ctx, Tf, "prototype", JS_DupValue(ctx, Tp));
