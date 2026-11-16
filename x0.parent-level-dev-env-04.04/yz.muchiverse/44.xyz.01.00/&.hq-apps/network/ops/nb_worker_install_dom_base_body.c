/* nb_worker_install_dom_base_body.c - DOM class hierarchy base setup body (mechanical extract, single-TU include) */
    JSValue ETf = JS_NewCFunction2(ctx, class_noop, "EventTarget", 1, JS_CFUNC_constructor, 0);
    JSValue Nf = JS_NewCFunction2(ctx, class_noop, "Node", 1, JS_CFUNC_constructor, 0);
    JSValue Ef = JS_NewCFunction2(ctx, class_noop, "Element", 1, JS_CFUNC_constructor, 0);
    JSValue Hf = JS_NewCFunction2(ctx, class_noop, "HTMLElement", 1, JS_CFUNC_constructor, 0);
    JSValue Sf = JS_NewCFunction2(ctx, class_noop, "SVGElement", 1, JS_CFUNC_constructor, 0);
    JSValue Tf = JS_NewCFunction2(ctx, class_noop, "Text", 1, JS_CFUNC_constructor, 0);
    JSValue Df = JS_NewCFunction2(ctx, class_noop, "Document", 1, JS_CFUNC_constructor, 0);
    JSValue Af = JS_NewCFunction2(ctx, class_noop, "Animation", 1, JS_CFUNC_constructor, 0);

    JSValue ETp = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ETf, "prototype", JS_DupValue(ctx, ETp));
    JSValue Np = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Np, ETp);
    JS_SetPropertyStr(ctx, Nf, "prototype", JS_DupValue(ctx, Np));
    /* Node.DOCUMENT_POSITION_* constants */
    {
        const struct { const char *name; int v; } posc[] = {
            {"DOCUMENT_POSITION_DISCONNECTED", 1},
            {"DOCUMENT_POSITION_PRECEDING", 2},
            {"DOCUMENT_POSITION_FOLLOWING", 4},
            {"DOCUMENT_POSITION_CONTAINS", 8},
            {"DOCUMENT_POSITION_CONTAINED_BY", 16},
            {"DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", 32},
        };
        for (size_t i = 0; i < sizeof(posc)/sizeof(posc[0]); i++) {
            JS_SetPropertyStr(ctx, Nf, posc[i].name, JS_NewInt32(ctx, posc[i].v));
            JS_SetPropertyStr(ctx, Np, posc[i].name, JS_NewInt32(ctx, posc[i].v));
        }
    }
    JSValue Ep = JS_NewObject(ctx);
    JS_SetPrototype(ctx, Ep, Np);
    JS_SetPropertyStr(ctx, Ef, "prototype", JS_DupValue(ctx, Ep));
    /* Element.prototype.attachShadow (2026-09-20): C-backed ShadowRoot, see
     * the customElements section. Native path so kevlar can stamp after
     * upgrade. */
    JS_SetPropertyStr(ctx, Ep, "attachShadow", JS_NewCFunction(ctx, nb_el_attachShadow, "attachShadow", 1));
