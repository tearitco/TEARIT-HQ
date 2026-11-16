/* nb_worker_frag_wrapper.c - fragment wrapper construction (mechanical extract, single-TU include) */
static JSValue make_fragment_wrapper(JSContext *ctx, NbNode *frag, const char *nodeName) {
    int nidx = node_index(frag);
    if (nidx < 0) { orphan_add(frag); return JS_NULL; }
    JSValue w = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, w, NODEKEY, JS_NewInt32(ctx, nidx));
    JS_SetPropertyStr(ctx, w, "nodeType", JS_NewInt32(ctx, 11));
    JS_SetPropertyStr(ctx, w, "nodeName", JS_NewString(ctx, nodeName));
    {
        JSAtom nm = JS_NewAtom(ctx, "ownerDocument");
        JS_DefinePropertyGetSet(ctx, w, nm,
            JS_NewCFunction(ctx, nb_node_ownerDocument, "get ownerDocument", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, w, "appendChild", JS_NewCFunction(ctx, nb_el_appendChild, "appendChild", 1));
    JS_SetPropertyStr(ctx, w, "removeChild", JS_NewCFunction(ctx, nb_el_removeChild, "removeChild", 1));
    JS_SetPropertyStr(ctx, w, "insertBefore", JS_NewCFunction(ctx, nb_el_insertBefore, "insertBefore", 2));
    JS_SetPropertyStr(ctx, w, "replaceChild", JS_NewCFunction(ctx, nb_el_replaceChild, "replaceChild", 2));
    JS_SetPropertyStr(ctx, w, "querySelector", JS_NewCFunction(ctx, nb_frag_querySelector, "querySelector", 1));
    JS_SetPropertyStr(ctx, w, "querySelectorAll", JS_NewCFunction(ctx, nb_frag_querySelectorAll, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, w, "getElementById", JS_NewCFunction(ctx, nb_frag_getElementById, "getElementById", 1));
    JS_SetPropertyStr(ctx, w, "getElementsByTagName", JS_NewCFunction(ctx, nb_frag_getElementsByTagName, "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, w, "getElementsByClassName", JS_NewCFunction(ctx, nb_frag_getElementsByClassName, "getElementsByClassName", 1));
    JS_SetPropertyStr(ctx, w, "cloneNode", JS_NewCFunction(ctx, nb_frag_clone, "cloneNode", 1));
    JS_SetPropertyStr(ctx, w, "isEqualNode", JS_NewCFunction(ctx, nb_el_isEqualNode, "isEqualNode", 1));
    JS_SetPropertyStr(ctx, w, "isSameNode", JS_NewCFunction(ctx, nb_el_isSameNode, "isSameNode", 1));
    JS_SetPropertyStr(ctx, w, "getRootNode", JS_NewCFunction(ctx, nb_node_getRootNode, "getRootNode", 0));
    JS_SetPropertyStr(ctx, w, "compareDocumentPosition", JS_NewCFunction(ctx, nb_node_compareDocumentPosition, "compareDocumentPosition", 1));
    {
        JSAtom nm = JS_NewAtom(ctx, "isConnected");
        JS_DefinePropertyGetSet(ctx, w, nm,
            JS_NewCFunction(ctx, nb_node_isConnected, "get isConnected", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, w, "addEventListener", JS_NewCFunction(ctx, nb_el_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, w, "removeEventListener", JS_NewCFunction(ctx, nb_el_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, w, "dispatchEvent", JS_NewCFunction(ctx, nb_el_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, w, "hasChildNodes", JS_NewCFunction(ctx, nb_el_hasChildNodes, "hasChildNodes", 0));
    JS_SetPropertyStr(ctx, w, "normalize", JS_NewCFunction(ctx, nb_el_normalize, "normalize", 0));
    JS_SetPropertyStr(ctx, w, "contains", JS_NewCFunction(ctx, nb_el_contains, "contains", 1));
    JS_SetPropertyStr(ctx, w, "click", JS_NewCFunction(ctx, nb_el_click, "click", 0));
    {
        JSAtom nm;
#define FRAG_GET(nmstr, fn) do { nm = JS_NewAtom(ctx, nmstr); \
            JS_DefinePropertyGetSet(ctx, w, nm, JS_NewCFunction(ctx, fn, "get " nmstr, 0), JS_UNDEFINED, \
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE); JS_FreeAtom(ctx, nm); } while (0)
        FRAG_GET("children", nb_el_children);
        FRAG_GET("childNodes", nb_el_childNodes);
        FRAG_GET("firstChild", nb_el_firstChild);
        FRAG_GET("lastChild", nb_el_lastChild);
        FRAG_GET("parentNode", nb_el_parentNode);
        FRAG_GET("nextSibling", nb_el_nextSibling);
        FRAG_GET("previousSibling", nb_el_previousSibling);
        FRAG_GET("parentElement", nb_el_parentElement);
        FRAG_GET("firstElementChild", nb_el_firstElementChild);
        FRAG_GET("lastElementChild", nb_el_lastElementChild);
        FRAG_GET("nextElementSibling", nb_el_nextElementSibling);
        FRAG_GET("previousElementSibling", nb_el_previousElementSibling);
        FRAG_GET("childElementCount", nb_el_childElementCount);
#undef FRAG_GET
    }
    {
        JSAtom nm = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, w, nm,
            JS_NewCFunction(ctx, nb_el_textContent_get, "get textContent", 0),
            JS_NewCFunction(ctx, nb_el_textContent_set, "set textContent", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, w, "append", JS_NewCFunction(ctx, nb_el_append_js, "append", 0));
    JS_SetPropertyStr(ctx, w, "prepend", JS_NewCFunction(ctx, nb_el_prepend_js, "prepend", 0));
    JS_SetPropertyStr(ctx, w, "replaceChildren", JS_NewCFunction(ctx, nb_el_replaceChildren_js, "replaceChildren", 0));
    {
        JSAtom nm = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, w, nm,
            JS_NewCFunction(ctx, nb_el_innerHTML_get, "get innerHTML", 0),
            JS_NewCFunction(ctx, nb_el_innerHTML_set, "set innerHTML", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    /* Identity: one C fragment must yield one JS object, so a child's
     * parentNode IS the fragment the page holds. */
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue map = JS_GetPropertyStr(ctx, g, IDMAPNAME);
        if (!JS_IsObject(map)) {
            JS_FreeValue(ctx, map);
            map = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, g, IDMAPNAME, JS_DupValue(ctx, map));
        }
        JS_FreeValue(ctx, g);
        JS_SetPropertyUint32(ctx, map, (uint32_t)nidx, JS_DupValue(ctx, w));
        JS_FreeValue(ctx, map);
    }
    return w;
}

