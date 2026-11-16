/* nb_worker_install_dom_exports.c - Document/Animation prototypes and global DOM exports (mechanical extract, single-TU include) */
    JSValue Dp = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Dp, Np);
    JS_SetPropertyStr(ctx, Df, "prototype", JS_DupValue(ctx, Dp));
    JSValue Ap = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, Af, "prototype", JS_DupValue(ctx, Ap));

    JSValue g = JS_GetGlobalObject(ctx);
    JSValue holder = JS_NewObject(ctx);
    JS_SetPropertyUint32(ctx, holder, PROTO_EL, JS_DupValue(ctx, Hp));
    JS_SetPropertyUint32(ctx, holder, PROTO_TEXT, JS_DupValue(ctx, Tp));
    JS_SetPropertyUint32(ctx, holder, PROTO_DOC, JS_DupValue(ctx, Dp));
    JS_SetPropertyStr(ctx, g, PROTONAME, holder);

    JS_SetPropertyStr(ctx, g, "EventTarget", ETf);
    JS_SetPropertyStr(ctx, g, "Node", Nf);
    JS_SetPropertyStr(ctx, g, "Element", Ef);
    JS_SetPropertyStr(ctx, g, "HTMLElement", Hf);
    JS_SetPropertyStr(ctx, g, "SVGElement", Sf);
    JS_SetPropertyStr(ctx, g, "Text", Tf);
    JS_SetPropertyStr(ctx, g, "Document", Df);
    JS_SetPropertyStr(ctx, g, "Animation", Af);
