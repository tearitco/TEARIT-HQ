/* nb_worker_install_node_constants.c - Node static type constants (mechanical extract, single-TU include) */
    /* Node static constants (2026-09-20): kevlar's wrz() compares
     * O.nodeType === Node.ELEMENT_NODE when deciding to wrap a lone node into
     * an array before materialization. Without these the comparison is
     * false-on-undefined and rendered elements are silently dropped. */
    JS_SetPropertyStr(ctx, Nf, "ELEMENT_NODE", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, Nf, "TEXT_NODE", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, Nf, "DOCUMENT_NODE", JS_NewInt32(ctx, 9));
    JS_SetPropertyStr(ctx, Nf, "DOCUMENT_FRAGMENT_NODE", JS_NewInt32(ctx, 11));
    JS_SetPropertyStr(ctx, Nf, "ATTRIBUTE_NODE", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, Nf, "COMMENT_NODE", JS_NewInt32(ctx, 8));
    JS_SetPropertyStr(ctx, Nf, "DOCUMENT_TYPE_NODE", JS_NewInt32(ctx, 10));
