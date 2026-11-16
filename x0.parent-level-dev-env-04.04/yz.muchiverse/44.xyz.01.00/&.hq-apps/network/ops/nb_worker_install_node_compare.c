/* nb_worker_install_node_compare.c - Node equality/root/position installs (mechanical extract, single-TU include) */
    JS_SetPropertyStr(ctx, Np, "isEqualNode",    JS_NewCFunction(ctx, nb_el_isEqualNode, "isEqualNode", 1));
    JS_SetPropertyStr(ctx, Np, "isSameNode",     JS_NewCFunction(ctx, nb_el_isSameNode, "isSameNode", 1));
    JS_SetPropertyStr(ctx, Np, "getRootNode",    JS_NewCFunction(ctx, nb_node_getRootNode, "getRootNode", 0));
    JS_SetPropertyStr(ctx, Np, "compareDocumentPosition", JS_NewCFunction(ctx, nb_node_compareDocumentPosition, "compareDocumentPosition", 1));
