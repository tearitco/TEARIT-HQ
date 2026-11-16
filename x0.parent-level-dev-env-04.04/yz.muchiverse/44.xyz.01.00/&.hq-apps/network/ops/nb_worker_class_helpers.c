/* nb_worker_class_helpers.c - DOM class constructor helpers (mechanical extract, single-TU include) */
/* construct an inert constructor whose .prototype chains off `parent`
 * (or Object.prototype when parent is not an object). Returns an owned
 * function object; calling it yields undefined (see class_noop). */
static JSValue add_class(JSContext *ctx, const char *name, JSValueConst parent) {
    JSValue f = JS_NewCFunction2(ctx, class_noop, name, 1, JS_CFUNC_constructor, 0);
    JSValue p = JS_NewObject(ctx);
    if (JS_IsObject(parent)) JS_SetPrototype(ctx, p, parent);
    JS_SetPropertyStr(ctx, f, "prototype", p);
    return f;
}
/* register a constructor global only if the name is still free — the prelude
 * already defines Event (nb_el_click builds clicks through it) and URL-family
 * globals; clobbering them breaks dispatch and parsing. */
static void add_global_class(JSContext *ctx, JSValueConst g, const char *name, JSValueConst parent) {
    JSValue ex = JS_GetPropertyStr(ctx, g, name);
    int present = !JS_IsUndefined(ex);
    JS_FreeValue(ctx, ex);
    if (present) return;
    JS_SetPropertyStr(ctx, g, name, add_class(ctx, name, parent));
}
