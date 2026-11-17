/* nb_worker_push_node.c - DOM node wrapper builder (mechanical extract, single-TU include) */
static JSValue push_node(JSContext *ctx, NbNode *n) {
    int nidx = node_index(n);
    /* wrapper identity: one JS object per C node, held in the global
     * __nb_idmap object (the context is fresh per page). */
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue map = JS_GetPropertyStr(ctx, g, IDMAPNAME);
    if (!JS_IsObject(map)) {
        JS_FreeValue(ctx, map);
        map = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, g, IDMAPNAME, JS_DupValue(ctx, map));
    }
    JS_FreeValue(ctx, g);
    JSValue ex = JS_GetPropertyUint32(ctx, map, (uint32_t)nidx);
    if (JS_IsObject(ex)) {           /* existing wrapper */
        JS_FreeValue(ctx, map);
        return ex;
    }
    JS_FreeValue(ctx, ex);
    if (nb_is_doc_fragment_node(n)) {
        JS_FreeValue(ctx, map);
        return make_fragment_wrapper(ctx, n, "#document-fragment");
    }
    int is_comment = (n->tag && !strcmp(n->tag, "#comment"));
    JSValue el = JS_NewObject(ctx);
    /* DOM class prototype (install_dom_classes) so `el instanceof Element`
     * and Element.prototype.* resolve like a browser; falls back to a plain
     * object when the holder is absent (node mode / pre-install). */
    {
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue holder = JS_GetPropertyStr(ctx, g2, PROTONAME);
        JS_FreeValue(ctx, g2);
        if (JS_IsObject(holder)) {
            int which = (n->tag && n->tag[0] && !is_comment) ? PROTO_EL : PROTO_TEXT;
            JSValue p = JS_GetPropertyUint32(ctx, holder, (uint32_t)which);
            if (JS_IsObject(p)) JS_SetPrototype(ctx, el, p);
            JS_FreeValue(ctx, p);
        }
        JS_FreeValue(ctx, holder);
    }
    JS_SetPropertyStr(ctx, el, NODEKEY, JS_NewInt32(ctx, nidx));
    {
        const char *label = (n->tag && n->tag[0]) ? n->tag : "#text";
        /* nodeType (2026-09-20): kevlar's materializer gate is
         * BD(O)=_.qh(O)&&"nodeType" in O — elements without a nodeType
         * property fall through every Qkq branch and the render dies with
         * Error("Oc"). Browsers expose it on the prototype; our wrappers
         * are plain objects so an own data property is the equivalent.
         * #document reports 9, like a browser. */
        int nt = (n->tag && !strcmp(n->tag, "#document")) ? 9
               : ((n->tag && !strcmp(n->tag, "#document-fragment")) ? 11
               : (is_comment ? 8
               : ((n->tag && n->tag[0]) ? 1 : 3)));
        JS_SetPropertyStr(ctx, el, "nodeType", JS_NewInt32(ctx, nt));
        JS_SetPropertyStr(ctx, el, "nodeName", JS_NewString(ctx, label));
        if (n->tag && n->tag[0] && !is_comment)
            JS_SetPropertyStr(ctx, el, "tagName", JS_NewString(ctx, label));
    }

    JS_SetPropertyStr(ctx, el, "getAttribute", JS_NewCFunction(ctx, nb_el_getAttribute, "getAttribute", 1));
    JS_SetPropertyStr(ctx, el, "setAttribute", JS_NewCFunction(ctx, nb_el_setAttribute, "setAttribute", 2));
    JS_SetPropertyStr(ctx, el, "removeAttribute", JS_NewCFunction(ctx, nb_el_removeAttribute, "removeAttribute", 1));
    JS_SetPropertyStr(ctx, el, "hasAttribute", JS_NewCFunction(ctx, nb_el_hasAttribute, "hasAttribute", 1));
    JS_SetPropertyStr(ctx, el, "hasAttributes", JS_NewCFunction(ctx, nb_el_hasAttributes, "hasAttributes", 0));
    JS_SetPropertyStr(ctx, el, "getAttributeNames", JS_NewCFunction(ctx, nb_el_getAttributeNames, "getAttributeNames", 0));
    JS_SetPropertyStr(ctx, el, "toggleAttribute", JS_NewCFunction(ctx, nb_el_toggleAttribute, "toggleAttribute", 2));
    JS_SetPropertyStr(ctx, el, "appendChild", JS_NewCFunction(ctx, nb_el_appendChild, "appendChild", 1));
    JS_SetPropertyStr(ctx, el, "removeChild", JS_NewCFunction(ctx, nb_el_removeChild, "removeChild", 1));
    JS_SetPropertyStr(ctx, el, "insertBefore", JS_NewCFunction(ctx, nb_el_insertBefore, "insertBefore", 2));
    JS_SetPropertyStr(ctx, el, "replaceChild", JS_NewCFunction(ctx, nb_el_replaceChild, "replaceChild", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentHTML", JS_NewCFunction(ctx, nb_el_insertAdjacentHTML, "insertAdjacentHTML", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentText", JS_NewCFunction(ctx, nb_el_insertAdjacentText, "insertAdjacentText", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentElement", JS_NewCFunction(ctx, nb_el_insertAdjacentElement, "insertAdjacentElement", 2));
    JS_SetPropertyStr(ctx, el, "addEventListener", JS_NewCFunction(ctx, nb_el_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, el, "removeEventListener", JS_NewCFunction(ctx, nb_el_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, el, "dispatchEvent", JS_NewCFunction(ctx, nb_el_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, el, "contains", JS_NewCFunction(ctx, nb_el_contains, "contains", 1));
    JS_SetPropertyStr(ctx, el, "click", JS_NewCFunction(ctx, nb_el_click, "click", 0));
    JS_SetPropertyStr(ctx, el, "focus", JS_NewCFunction(ctx, nb_el_focus, "focus", 0));
    JS_SetPropertyStr(ctx, el, "blur", JS_NewCFunction(ctx, nb_el_blur, "blur", 0));
    /* CharacterData (Text/Comment) ChildNode methods. Element wrappers
     * inherit these from Element.prototype; text/comment wrappers need
     * them here because push_node does not install Element.prototype. */
    if (!n->tag || !n->tag[0] || is_comment) {
        JS_SetPropertyStr(ctx, el, "remove",      JS_NewCFunction(ctx, nb_el_remove_js, "remove", 0));
        JS_SetPropertyStr(ctx, el, "before",      JS_NewCFunction(ctx, nb_el_before_js, "before", 0));
        JS_SetPropertyStr(ctx, el, "after",       JS_NewCFunction(ctx, nb_el_after_js, "after", 0));
        JS_SetPropertyStr(ctx, el, "replaceWith", JS_NewCFunction(ctx, nb_el_replaceWith_js, "replaceWith", 0));
    }
    /* resource/URL attributes real bundles read directly off the element.
     * Browsers expose src/href as STRINGS even when the attribute is absent
     * (an inline <script>.src is ""), and youtube's global error reporter
     * does `script.src.indexOf("/debug-")` across getElementsByTagName
     * ("script") — undefined there throws. closure's module loader also does
     * `O.src ? O.src : O.getAttribute("href")` on <script id="base-js">. */
    if (n->tag) {
        if (!strcmp(n->tag, "img")) {
            JSAtom nm = JS_NewAtom(ctx, "src");
            JS_DefinePropertyGetSet(ctx, el, nm,
                JS_NewCFunction(ctx, nb_img_src_get, "get src", 0),
                JS_NewCFunction(ctx, nb_img_src_set, "set src", 1),
                JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "naturalWidth");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_naturalWidth_get, "get naturalWidth", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "naturalHeight");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_naturalHeight_get, "get naturalHeight", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "complete");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_complete_get, "get complete", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        } else {
            if (!strcmp(n->tag, "script")) {
                /* script.src setter: when set, fetch and execute the script (Cloudflare challenge) */
                JSAtom nm = JS_NewAtom(ctx, "src");
                JS_DefinePropertyGetSet(ctx, el, nm,
                    JS_NewCFunction(ctx, nb_script_src_get, "get src", 0),
                    JS_NewCFunction(ctx, nb_script_src_set, "set src", 1),
                    JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
                JS_FreeAtom(ctx, nm);
            } else {
                static const char *src_tags[] = { "iframe","input",
                    "source","track","embed","video","audio","frame", NULL };
                static const char *href_tags[] = { "a","link","area","base", NULL };
                for (int i = 0; src_tags[i]; i++)
                    if (!strcmp(n->tag, src_tags[i])) {
                        JS_SetPropertyStr(ctx, el, "src", JS_NewString(ctx, nb_attr_get(n, "src")));
                        break;
                    }
                for (int i = 0; href_tags[i]; i++)
                    if (!strcmp(n->tag, href_tags[i])) {
                        JS_SetPropertyStr(ctx, el, "href", JS_NewString(ctx, nb_attr_get(n, "href")));
                        break;
                    }
            }
        }
    }
    /* canvas 2D (2026-09-18): real bundles probe <canvas> via
     * createElementNS('...','canvas') and immediately call getContext('2d')
     * through a fillStyle/color parse. Minimal context: geometry/measure
     * stubs that return well-formed objects so parser/feature paths don't
     * throw; rasterization is out of scope (see NB-JS-ENGINE-ROADMAP). */
    if (n->tag && strcmp(n->tag, "canvas") == 0) {
        JS_SetPropertyStr(ctx, el, "getContext", JS_NewCFunction(ctx, nb_el_getContext, "getContext", 1));
        JS_SetPropertyStr(ctx, el, "toDataURL", JS_NewCFunction(ctx, nb_canvas_toDataURL, "toDataURL", 0));
    }
    if (n->tag && strcmp(n->tag, "template") == 0) {
        /* <template>.content = C-backed #document-fragment (2026-09-20).
         * kevlar does template.content.insertBefore(content.cloneNode(true),
         * firstChild) while wiring its css style tags; the old prelude JS
         * docfrag couldn't produce/accept C nodes and the insert threw
         * HierarchyRequestError at bundle line 16063. */
        (void)nb_template_content_frag(ctx, el, n);
    }

    /* el.on<type> = cb accessors; native magic carries the ONPROPS index
     * (QuickJS accessors carry no property name — the magic is the index). */
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom nm = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_el_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* read-only accessor properties: children, childNodes, parentNode, firstChild, nextSibling */
    {
        JSAtom nm = JS_NewAtom(ctx, "children");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_children, "children", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childNodes");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_childNodes, "childNodes", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "attributes");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_attributes, "attributes", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentNode");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_parentNode, "parentNode", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "firstChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_firstChild, "firstChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_nextSibling, "nextSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "lastChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_lastChild, "lastChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_previousSibling, "previousSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentElement");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_parentElement, "parentElement", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "firstElementChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_firstElementChild, "firstElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "lastElementChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_lastElementChild, "lastElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextElementSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_nextElementSibling, "nextElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousElementSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_previousElementSibling, "previousElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childElementCount");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_childElementCount, "childElementCount", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* accessors: id, className, textContent, innerHTML */
    {
        JSAtom nm = JS_NewAtom(ctx, "id");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_id_get, "get id", 0), JS_NewCFunction(ctx, nb_el_id_set, "set id", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "className");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_className_get, "get className", 0), JS_NewCFunction(ctx, nb_el_className_set, "set className", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_textContent_get, "get textContent", 0), JS_NewCFunction(ctx, nb_el_textContent_set, "set textContent", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_innerHTML_get, "get innerHTML", 0), JS_NewCFunction(ctx, nb_el_innerHTML_set, "set innerHTML", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung-2 remainder + rung 7: el.style — identity-cached snapshot of
     * the inline style attribute (reads mirror inline decls; JS writes
     * persist on the snapshot; not fed back into layout metrics). */
    {
        JSAtom nm = JS_NewAtom(ctx, "style");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_style_get, "get style", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung 7: layout-intent metrics (display:none-aware, CSS px from the
     * cascade, else 0 — see NB-JS-ENGINE-ROADMAP rung 7). */
    {
        JSAtom nm = JS_NewAtom(ctx, "offsetWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientWidth", 0, JS_CFUNC_generic_magic, 2), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientHeight", 0, JS_CFUNC_generic_magic, 3), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetParent");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_offset_parent, "get offsetParent", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollTop");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollTop", 0, JS_CFUNC_generic_magic, 1),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollTop", 1, JS_CFUNC_generic_magic, 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollLeft");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollLeft", 0, JS_CFUNC_generic_magic, 0),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollLeft", 1, JS_CFUNC_generic_magic, 0),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, el, "getBoundingClientRect",
                      JS_NewCFunction(ctx, nb_el_getBoundingClientRect, "getBoundingClientRect", 0));

/* rung-2 remainder: el.value get/set for form fields. */
      if (is_form_field(n->tag)) {
          JSAtom nm = JS_NewAtom(ctx, "value");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_el_value_get, "get value", 0), JS_NewCFunction(ctx, nb_el_value_set, "set value", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      }
      /* select/option state (2026-10-05): selectedIndex on the dropdown,
       * selected on the option. Both are registered per-node by tag so a
       * page's `typeof sel.selectedIndex` feature-detects honestly. */
      if (node_is_tag(n, "select")) {
          JSAtom nm = JS_NewAtom(ctx, "selectedIndex");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_index_get, "get selectedIndex", 0),
              JS_NewCFunction(ctx, nb_sel_index_set, "set selectedIndex", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          /* options/index are VALUE properties, not methods: registering
           * them with JS_SetPropertyStr made sel.options a function, and
           * sel.options.length then answered the function's arity (0). */
          nm = JS_NewAtom(ctx, "options");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_options, "get options", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          nm = JS_NewAtom(ctx, "length");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_length, "get length", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      } else if (node_is_tag(n, "option")) {
          JSAtom nm = JS_NewAtom(ctx, "selected");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_opt_selected_get, "get selected", 0),
              JS_NewCFunction(ctx, nb_opt_selected_set, "set selected", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          nm = JS_NewAtom(ctx, "index");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_opt_index, "get index", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      }

    /* classList */
    {
        JSValue cl = JS_NewObject(ctx);                /* classList */
        JS_SetPropertyStr(ctx, cl, NODEKEY, JS_NewInt32(ctx, nidx));
        JS_SetPropertyStr(ctx, cl, "add", JS_NewCFunction(ctx, nb_cl_add, "add", 1));
        JS_SetPropertyStr(ctx, cl, "remove", JS_NewCFunction(ctx, nb_cl_remove, "remove", 1));
        JS_SetPropertyStr(ctx, cl, "toggle", JS_NewCFunction(ctx, nb_cl_toggle, "toggle", 1));
        JS_SetPropertyStr(ctx, cl, "contains", JS_NewCFunction(ctx, nb_cl_contains, "contains", 1));
        JS_SetPropertyStr(ctx, cl, "item", JS_NewCFunction(ctx, nb_cl_item, "item", 1));
        {
            JSValue cls = get_global_attr(ctx, "DOMTokenList");
            JSValue proto = JS_GetPropertyStr(ctx, cls, "prototype");
            if (JS_IsObject(proto)) JS_SetPrototype(ctx, cl, proto);
            JS_FreeValue(ctx, proto);
            JS_FreeValue(ctx, cls);
        }
        JS_SetPropertyStr(ctx, el, "classList", cl);
    }
    /* dataset (DOMStringMap over data-* attributes) */
    JS_SetPropertyStr(ctx, el, "dataset", nb_ds_make(ctx, n, nidx));

    /* cache wrapper in the identity map so later push_node calls return the
     * same object (map originally FIXED a self-storing bug: the old code
     * stored the wrapper into itself -> identity broke). */
    JS_SetPropertyUint32(ctx, map, (uint32_t)nidx, JS_DupValue(ctx, el));
    JS_FreeValue(ctx, map);
    /* customElements upgrade-on-creation (2026-09-20): elements built at
     * runtime (createElement/appendChild/innerHTML after classes define) get
     * the registered class + lifecycle like a browser; hyphenated tags only. */
    if (n->tag && strchr(n->tag, '-'))
        ce_upgrade_node_if_registered(ctx, el, n);
    return el;
}
