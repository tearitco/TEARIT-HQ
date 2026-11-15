/* nb_worker_option_selected.c - option selected state helpers (mechanical extract, single-TU include) */
/* Explicit script-set state wins; with none set, fall back to the markup.
 * Returning "unset" and "false" as the same thing is fine here: a select
 * whose options all carry `selected` cannot express the difference in
 * markup either. */
static int opt_selected(JSContext *ctx, NbNode *opt) {
    JSValue w = push_node(ctx, opt);
    JSValue v = JS_GetPropertyStr(ctx, w, SELKEY);
    int r;
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        r = nb_attr_has(opt, "selected") ? 1 : 0;
    } else {
        r = JS_ToBool(ctx, v);
    }
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, w);
    return r;
}
static void opt_set_selected(JSContext *ctx, NbNode *opt, int on) {
    JSValue w = push_node(ctx, opt);
    JS_SetPropertyStr(ctx, w, SELKEY, JS_NewBool(ctx, on));
    JS_FreeValue(ctx, w);
}
