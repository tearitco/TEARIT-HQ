/* nb_worker_el_toggle_attribute.c - toggleAttribute helper (mechanical extract, single-TU include) */
static JSValue nb_el_toggleAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, nm); return JS_FALSE; }
    int present = attrs_has(n, name);
    int force_present = 0;
    int has_force = (argc > 1);
    if (has_force) force_present = JS_ToBool(ctx, argv[1]);
    int should_set = has_force ? force_present : !present;
    if (should_set) {
        if (!present) {
            JSValue a[2]; a[0] = JS_DupValue(ctx, argv[0]); a[1] = JS_NewString(ctx, "");
            JS_FreeValue(ctx, nb_el_setAttribute(ctx, this_val, 2, a));
            JS_FreeValue(ctx, a[0]); JS_FreeValue(ctx, a[1]);
        }
    } else {
        if (present) {
            JSValue a[1]; a[0] = JS_DupValue(ctx, argv[0]);
            JS_FreeValue(ctx, nb_el_removeAttribute(ctx, this_val, 1, a));
            JS_FreeValue(ctx, a[0]);
        }
    }
    int now_present = should_set ? 1 : 0;
    if (has_force && force_present && present) now_present = 1;
    JS_FreeCString(ctx, nm);
    return JS_NewBool(ctx, now_present);
}
