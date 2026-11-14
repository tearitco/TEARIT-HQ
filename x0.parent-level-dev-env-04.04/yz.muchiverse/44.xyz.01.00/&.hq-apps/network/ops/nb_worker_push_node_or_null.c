/* nb_worker_push_node_or_null.c - push_node_or_null helper (mechanical extract, single-TU include) */
static JSValue push_node_or_null(JSContext *ctx, NbNode *n) { return n ? push_node(ctx, n) : JS_NULL; };
