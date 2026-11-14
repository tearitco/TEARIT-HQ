/* nb_worker_dataset_node.c - dataset node lookup helper (mechanical extract, single-TU include) */
static NbNode *nb_ds_node(JSContext *ctx, JSValueConst h) {
    JSValue v = JS_GetPropertyStr(ctx, h, "__nb_ds_idx");
    int32_t i = -1;
    if (JS_ToInt32(ctx, &i, v) == 0 && i >= 0 && i < g_nodecount)
        return g_nodeindex[i];
    JS_FreeValue(ctx, v);
    return NULL;
}
