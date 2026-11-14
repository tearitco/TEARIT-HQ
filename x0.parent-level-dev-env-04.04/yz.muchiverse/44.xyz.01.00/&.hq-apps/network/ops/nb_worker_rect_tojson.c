/* nb_worker_rect_tojson.c - DOMRect.toJSON helper (mechanical extract, single-TU include) */
static JSValue nb_rect_tojson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_DupValue(ctx, this_val);    /* this is the rect */
}
