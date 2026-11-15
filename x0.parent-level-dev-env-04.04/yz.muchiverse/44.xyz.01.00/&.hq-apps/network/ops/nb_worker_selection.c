/* nb_worker_selection.c - Selection implementation (mechanical extract, single-TU include) */
static JSValue nb_sel_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_win_getSelection(ctx, this_val, argc, argv);
}
static JSValue nb_sel_add_range(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0])) {
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "TypeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "TypeError: addRange requires a range"));
        return JS_Throw(ctx, er);
    }
    JSValue a = nb_sel_ranges(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    JS_SetPropertyUint32(ctx, a, n, JS_DupValue(ctx, argv[0]));
    JS_FreeValue(ctx, a);
    return JS_UNDEFINED;
}
static JSValue nb_sel_remove_range(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_UNDEFINED;
    JSValue a = nb_sel_ranges(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    JSValue kept = JS_NewArray(ctx);
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        JSValue e = JS_GetPropertyUint32(ctx, a, i);
        if (!JS_StrictEq(ctx, e, argv[0])) JS_SetPropertyUint32(ctx, kept, k++, e);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, a);
    JS_SetPropertyStr(ctx, this_val, "__nb_sel_ranges", kept);
    return JS_UNDEFINED;
}
static JSValue nb_sel_remove_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JS_SetPropertyStr(ctx, this_val, "__nb_sel_ranges", JS_NewArray(ctx));
    return JS_UNDEFINED;
}
static JSValue nb_sel_collapse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return nb_sel_remove_all(ctx, this_val, argc, argv);
}
static JSValue nb_sel_get_range(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t i = argc > 0 ? JS_ToInt64(ctx, (int64_t[]){0}, argv[0]) : -1;
    (void)i;
    JSValue a = nb_sel_ranges(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    if (argc > 0 && JS_IsNumber(argv[0])) {
        int64_t idx = 0; JS_ToInt64(ctx, &idx, argv[0]);
        if (idx < 0 || (uint64_t)idx >= n) {
            JS_FreeValue(ctx, a);
            JSValue er = JS_NewError(ctx);
            JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "IndexSizeError"));
            JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "IndexSizeError: range index out of bounds"));
            return JS_Throw(ctx, er);
        }
        JSValue out = JS_GetPropertyUint32(ctx, a, (uint32_t)idx);
        JS_FreeValue(ctx, a);
        return out;
    }
    JS_FreeValue(ctx, a);
    JSValue er = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "TypeError"));
    JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "TypeError: getRangeAt requires an index"));
    return JS_Throw(ctx, er);
}
static JSValue nb_sel_to_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue a = nb_sel_ranges(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    SB b = {0, 0, 0};
    for (uint32_t i = 0; i < n; i++) {
        JSValue r = JS_GetPropertyUint32(ctx, a, i);
        JSValue t = JS_GetPropertyStr(ctx, r, "__nb_r_text");
        if (JS_IsString(t)) {
            char *s = JS_ToCString(ctx, t);
            if (s) sb_put(&b, s);
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, t); JS_FreeValue(ctx, r);
    }
    JS_FreeValue(ctx, a);
    JSValue out = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return out;
}
static JSValue nb_sel_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)argc; (void)argv;
    JSValue a = nb_sel_ranges(ctx, this_val);
    JSValue lenv = JS_GetPropertyStr(ctx, a, "length");
    uint32_t n = 0; JS_ToUint32(ctx, &n, lenv); JS_FreeValue(ctx, lenv);
    if (magic == 4) { JS_FreeValue(ctx, a); return JS_NewUint32(ctx, n); }
    if (n == 0) { JS_FreeValue(ctx, a); return magic == 5 ? JS_NewString(ctx, "") : JS_NULL; }
    JSValue r = JS_GetPropertyUint32(ctx, a, 0);
    JS_FreeValue(ctx, a);
    if (magic == 2) { JSValue v = JS_GetPropertyStr(ctx, r, "__nb_r_sc"); JS_FreeValue(ctx, r); return v; }
    if (magic == 3) { JSValue v = JS_GetPropertyStr(ctx, r, "__nb_r_so"); JS_FreeValue(ctx, r); return v; }
    if (magic == 6) { JSValue v = JS_GetPropertyStr(ctx, r, "__nb_r_ec"); JS_FreeValue(ctx, r); return v; }
    if (magic == 7) {
        JSValue sc = JS_GetPropertyStr(ctx, r, "__nb_r_sc");
        JSValue ec = JS_GetPropertyStr(ctx, r, "__nb_r_ec");
        JSValue so = JS_GetPropertyStr(ctx, r, "__nb_r_so");
        JSValue eo = JS_GetPropertyStr(ctx, r, "__nb_r_eo");
        int same = JS_StrictEq(ctx, sc, ec);
        int32_t x = 0, y = 0; JS_ToInt32(ctx, &x, so); JS_ToInt32(ctx, &y, eo);
        same = same && (x == y);
        JS_FreeValue(ctx, sc); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, so); JS_FreeValue(ctx, eo); JS_FreeValue(ctx, r);
        return JS_NewBool(ctx, same);
    }
    JSValue v = nb_sel_to_string(ctx, this_val, 0, NULL);
    JS_FreeValue(ctx, r);
    return v;
}
