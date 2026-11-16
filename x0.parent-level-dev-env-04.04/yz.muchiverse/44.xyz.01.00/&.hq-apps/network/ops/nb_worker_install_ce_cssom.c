/* nb_worker_install_ce_cssom.c - customElements and CSSStyleSheet globals (mechanical extract, single-TU include) */
    /* customElements (2026-09-20: REAL define/get/whenDefined/upgrade so
     * kevlar's deferred registration path activates — see the CE section
     * below) + CSSStyleSheet (inert; feature detection only). */
    {
        JSValue ce = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ce, "define", JS_NewCFunction(ctx, ce_define, "define", 2));
        JS_SetPropertyStr(ctx, ce, "get", JS_NewCFunction(ctx, ce_get, "get", 1));
        JS_SetPropertyStr(ctx, ce, "whenDefined", JS_NewCFunction(ctx, ce_when_def, "whenDefined", 1));
        JS_SetPropertyStr(ctx, ce, "upgrade", JS_NewCFunction(ctx, ce_upgrade, "upgrade", 1));
        JS_SetPropertyStr(ctx, g, "customElements", ce);

        JSValue CSf = JS_NewCFunction2(ctx, class_noop, "CSSStyleSheet", 1, JS_CFUNC_constructor, 0);
        JSValue CSp = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, CSp, "replaceSync", JS_NewCFunction(ctx, class_noop, "replaceSync", 1));
        JS_SetPropertyStr(ctx, CSp, "replace", JS_NewCFunction(ctx, nb_promise_resolved, "replace", 1));
        JS_SetPropertyStr(ctx, CSp, "insertRule", JS_NewCFunction(ctx, class_noop, "insertRule", 2));
        JS_SetPropertyStr(ctx, CSp, "deleteRule", JS_NewCFunction(ctx, class_noop, "deleteRule", 1));
        JS_SetPropertyStr(ctx, CSf, "prototype", CSp);
        JS_SetPropertyStr(ctx, g, "CSSStyleSheet", CSf);
    }
