/* nb_worker_value_get_set.c - element value get/set and fire_change decl (mechanical extract, single-TU include) */
static JSValue nb_el_value_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewString(ctx, "");
    /* a select reports its SELECTED option's value */
    if (node_is_tag(n, "select")) {
        char buf[512];
        option_value(sel_selected(ctx, n), buf, sizeof buf);
        return JS_NewString(ctx, buf);
    }
    JSValue v0 = JS_GetPropertyStr(ctx, this_val, "\xffvalue");
    if (JS_IsString(v0)) return v0;
    JS_FreeValue(ctx, v0);
    /* an option with no value attribute IS its text (HTML) — the generic
     * path below answered "" for those, so a page reading opt.value saw an
     * empty string and a select full of text-only options looked unset. */
    if (node_is_tag(n, "option")) {
        char buf[512];
        option_value(n, buf, sizeof buf);
        return JS_NewString(ctx, buf);
    }
    const char *v = nb_attr_get(n, "value");
    if (v && v[0]) return JS_NewString(ctx, v);
    return JS_NewString(ctx, "");
}
/* Selecting an option is a user interaction, so it fires `change` — the
 * only event dropdown-driven pages listen for. Defined next to
 * nb_el_click(), where the Event constructor and dispatch_event live. */
static void fire_change(JSContext *ctx, NbNode *n);
static JSValue nb_el_value_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    if (node_is_tag(n, "select")) {
        /* select the FIRST option whose value matches, deselect the rest.
         * An unmatched value leaves the selection alone, as HTML does —
         * silently selecting nothing would look like the dropdown worked. */
        int i = 0, hit = -1;
        char buf[512];
        for (NbNode *c = n->first_child; c && hit < 0; c = c->next_sibling) {
            if (!node_is_tag(c, "option")) continue;
            option_value(c, buf, sizeof buf);
            if (!strcmp(buf, v)) hit = i;
            i++;
        }
        if (hit >= 0) {
            sel_select_index(ctx, n, hit);
            JS_FreeCString(ctx, vl);
            fire_change(ctx, n);
            return JS_UNDEFINED;
        }
        JS_FreeCString(ctx, vl);
        return JS_UNDEFINED;
    }
    JS_SetPropertyStr(ctx, this_val, "\xffvalue", JS_NewString(ctx, v));
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
