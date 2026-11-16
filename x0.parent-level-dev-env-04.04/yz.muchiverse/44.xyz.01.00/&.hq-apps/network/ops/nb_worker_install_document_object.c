/* nb_worker_install_document_object.c - document object and timeline setup (mechanical extract, single-TU include) */
    /* the document object rides Document.prototype; web-animations reads
     * document.timeline at load, so give it a minimal one. */
    {
        JSValue doc = JS_GetPropertyStr(ctx, g, "document");
        if (JS_IsObject(doc)) {
            JS_SetPrototype(ctx, doc, Dp);
            JS_SetPropertyStr(ctx, doc, "nodeType", JS_NewInt32(ctx, 9));
            JSValue tl = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, tl, "currentTime", JS_NewFloat64(ctx, 0));
            JS_SetPropertyStr(ctx, tl, "getAnimations", JS_NewCFunction(ctx, class_noop, "getAnimations", 0));
            JS_SetPropertyStr(ctx, tl, "play", JS_NewCFunction(ctx, class_noop, "play", 1));
            JS_SetPropertyStr(ctx, tl, "reverse", JS_NewCFunction(ctx, class_noop, "reverse", 0));
            JS_SetPropertyStr(ctx, tl, "_play", JS_NewCFunction(ctx, class_noop, "_play", 1));
            JS_SetPropertyStr(ctx, doc, "timeline", tl);
        }
        JS_FreeValue(ctx, doc);
    }
