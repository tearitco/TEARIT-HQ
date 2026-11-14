/* nb_worker_rects.c - getClientRects helper (mechanical extract, single-TU include) */
/* The box tree already models one rect per element, so getClientRects()
 * answers with that single-box list rather than an empty one - a page that
 * counts boxes would otherwise conclude the element is invisible.
 * scrollIntoView is a no-op: the renderer owns scrolling and exposes no
 * channel for a page to drive it, and returning quietly is what keeps a
 * page's call from throwing. */
static JSValue nb_el_getClientRects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, nb_el_getBoundingClientRect(ctx, this_val, 0, NULL));
    JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, 1));
    return arr;
}
