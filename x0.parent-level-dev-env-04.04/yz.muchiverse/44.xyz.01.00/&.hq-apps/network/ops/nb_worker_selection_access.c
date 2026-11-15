/* nb_worker_selection_access.c - selection range storage and getSelection (mechanical extract, single-TU include) */
static JSValue nb_sel_ranges(JSContext *ctx, JSValueConst sel) {
    JSValue a = JS_GetPropertyStr(ctx, sel, "__nb_sel_ranges");
    if (!JS_IsArray(ctx, a)) {
        JS_FreeValue(ctx, a);
        a = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, sel, "__nb_sel_ranges", JS_DupValue(ctx, a));
    }
    return a;
}
static JSValue nb_win_getSelection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue sel = JS_GetPropertyStr(ctx, g, "__nb_selection");
    if (!JS_IsObject(sel)) {
        JS_FreeValue(ctx, sel);
        JSValue cls = JS_GetPropertyStr(ctx, g, "Selection");
        JSValue p = JS_GetPropertyStr(ctx, cls, "prototype");
        sel = JS_NewObjectProto(ctx, p);
        JS_FreeValue(ctx, p);
        JS_FreeValue(ctx, cls);
        JS_SetPropertyStr(ctx, sel, "__nb_sel_ranges", JS_NewArray(ctx));
        JS_SetPropertyStr(ctx, g, "__nb_selection", JS_DupValue(ctx, sel));
    }
    JS_FreeValue(ctx, g);
    return sel;
}
