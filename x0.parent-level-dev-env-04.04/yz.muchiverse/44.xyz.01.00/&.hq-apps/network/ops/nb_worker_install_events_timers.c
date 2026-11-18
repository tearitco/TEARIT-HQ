/* nb_worker_install_events_timers.c - global event/timer/install helper (mechanical extract, single-TU include) */
static void install_events_timers(JSContext *ctx) {
    g_perf_mark_hook = nb_po_on_mark;
    JSValue doc = get_global_attr(ctx, "document");
    JS_SetPropertyStr(ctx, doc, "addEventListener",
        JS_NewCFunction(ctx, nb_doc_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "removeEventListener",
        JS_NewCFunction(ctx, nb_doc_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "dispatchEvent",
        JS_NewCFunction(ctx, nb_doc_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, doc, "contains",
        JS_NewCFunction(ctx, nb_doc_contains, "contains", 1));
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, doc, key,
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, doc);
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "__nb_sha1",   JS_NewCFunction(ctx, nb_sha1_native, "__nb_sha1", 1));
    JS_SetPropertyStr(ctx, g, "setTimeout",  JS_NewCFunction(ctx, nb_timer_setTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, g, "setInterval", JS_NewCFunction(ctx, nb_timer_setInterval, "setInterval", 2));
    JS_SetPropertyStr(ctx, g, "clearTimeout",  JS_NewCFunction(ctx, nb_timer_clearTimeout, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, g, "clearInterval", JS_NewCFunction(ctx, nb_timer_clearInterval, "clearInterval", 1));
    JS_SetPropertyStr(ctx, g, "queueMicrotask", JS_NewCFunction(ctx, nb_queueMicrotask, "queueMicrotask", 1));
    JS_SetPropertyStr(ctx, g, "requestAnimationFrame", JS_NewCFunction(ctx, nb_raf, "requestAnimationFrame", 1));
    /* window.innerWidth is a VALUE in every browser, not a function. The
     * first version registered a native function here and
     * ""+window.innerWidth came back as "function innerWidth() { [native
     * code] }" - a stringified function where a number belongs, which
     * silently corrupts every page that does arithmetic on it. */
    JS_SetPropertyStr(ctx, g, "innerWidth", JS_NewFloat64(ctx, g_viewport_px));
    JS_SetPropertyStr(ctx, g, "innerHeight", JS_NewFloat64(ctx, g_viewport_h > 0.0 ? g_viewport_h : 600.0));
    /* matchMedia: real evaluator (see nb_mq_*). The prelude used to install
     * a constant-false stub; that line is gone so this one is what pages see. */
    JS_SetPropertyStr(ctx, g, "matchMedia", JS_NewCFunction(ctx, nb_match_media, "matchMedia", 1));
    {
        JSAtom nm = JS_NewAtom(ctx, "scrollX");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get scrollX", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollY");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get scrollY", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        /* Legacy aliases: pageYOffset is what most lazy loaders still read. */
        nm = JS_NewAtom(ctx, "pageXOffset");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get pageXOffset", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "pageYOffset");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get pageYOffset", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, g, "scrollTo", JS_NewCFunction(ctx, nb_win_scrollTo, "scrollTo", 2));
    JS_SetPropertyStr(ctx, g, "scrollBy", JS_NewCFunction(ctx, nb_win_scrollBy, "scrollBy", 2));
    JS_SetPropertyStr(ctx, g, "scroll", JS_NewCFunction(ctx, nb_win_scrollTo, "scroll", 2));
    JS_SetPropertyStr(ctx, g, "getSelection", JS_NewCFunction(ctx, nb_win_getSelection, "getSelection", 0));
    JS_SetPropertyStr(ctx, g, "nbFetchSync", JS_NewCFunction(ctx, nb_fetch_sync, "nbFetchSync", 4));
    JS_SetPropertyStr(ctx, g, "addEventListener",    JS_NewCFunction(ctx, nb_win_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, g, "removeEventListener", JS_NewCFunction(ctx, nb_win_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, g, "dispatchEvent",       JS_NewCFunction(ctx, nb_win_dispatchEvent, "dispatchEvent", 1));
    /* ShadyDOM (webcomponents-lite.js) captures "native" methods by copying
     * property descriptors off prototypes (L(Window.prototype, ...)); our
     * natives are own props on window/elements, so the copy is empty and
     * window.__shady_native_addEventListener stays undefined → Ae() throws
     * "not a function" at init. Define the aliases on window directly. */
    JS_SetPropertyStr(ctx, g, "__shady_native_addEventListener",    JS_NewCFunction(ctx, nb_win_addEventListener, "__shady_native_addEventListener", 2));
    JS_SetPropertyStr(ctx, g, "__shady_native_removeEventListener", JS_NewCFunction(ctx, nb_win_removeEventListener, "__shady_native_removeEventListener", 2));
    JS_SetPropertyStr(ctx, g, "__shady_native_dispatchEvent",       JS_NewCFunction(ctx, nb_win_dispatchEvent, "__shady_native_dispatchEvent", 1));
    JS_SetPropertyStr(ctx,g,"atob",JS_NewCFunction(ctx,_nb_atob,"atob",1));
    JS_SetPropertyStr(ctx,g,"btoa",JS_NewCFunction(ctx,_nb_btoa,"btoa",1));
    { JSValue p=JS_GetPropertyStr(ctx,g,"performance"); if(!JS_IsObject(p)){ JS_FreeValue(ctx,p); p=JS_NewObject(ctx); JS_SetPropertyStr(ctx,g,"performance",JS_DupValue(ctx,p)); } JS_SetPropertyStr(ctx,p,"now",JS_NewCFunction(ctx,_nb_perfnow,"now",0)); JS_FreeValue(ctx,p); }
    { JSValue c=JS_NewObject(ctx); JS_SetPropertyStr(ctx,c,"getRandomValues",JS_NewCFunction(ctx,_nb_grv,"getRandomValues",1)); JS_SetPropertyStr(ctx,c,"randomUUID",JS_NewCFunction(ctx,_nb_uuid,"randomUUID",0)); JS_SetPropertyStr(ctx,g,"crypto",c); }
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, g, key,
            JS_NewCFunctionMagic(ctx, nb_win_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_win_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, g);
}
