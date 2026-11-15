/* nb_worker_range_clone_detach.c - Range clone/detach/toString (mechanical extract, single-TU include) */
static JSValue nb_range_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue cls = JS_GetPropertyStr(ctx, g, "Range");
    JSValue proto = JS_GetPropertyStr(ctx, cls, "prototype");
    JSValue r = JS_IsObject(proto) ? JS_NewObjectProto(ctx, proto) : JS_NewObject(ctx);
    JS_FreeValue(ctx, proto); JS_FreeValue(ctx, cls); JS_FreeValue(ctx, g);
    JSValue sc = JS_GetPropertyStr(ctx, this_val, "__nb_r_sc");
    JSValue so = JS_GetPropertyStr(ctx, this_val, "__nb_r_so");
    JSValue ec = JS_GetPropertyStr(ctx, this_val, "__nb_r_ec");
    JSValue eo = JS_GetPropertyStr(ctx, this_val, "__nb_r_eo");
    JS_SetPropertyStr(ctx, r, "__nb_r_sc", JS_DupValue(ctx, sc));
    JS_SetPropertyStr(ctx, r, "__nb_r_so", JS_DupValue(ctx, so));
    JS_SetPropertyStr(ctx, r, "__nb_r_ec", JS_DupValue(ctx, ec));
    JS_SetPropertyStr(ctx, r, "__nb_r_eo", JS_DupValue(ctx, eo));
    nb_range_fill(ctx, r);
    JS_FreeValue(ctx, sc); JS_FreeValue(ctx, so); JS_FreeValue(ctx, ec); JS_FreeValue(ctx, eo);
    return r;
}
static JSValue nb_range_detach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue doc = get_global_attr(ctx, "document");
    JS_SetPropertyStr(ctx, this_val, "__nb_r_sc", JS_DupValue(ctx, doc));
    JS_SetPropertyStr(ctx, this_val, "__nb_r_so", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, this_val, "__nb_r_ec", JS_DupValue(ctx, doc));
    JS_SetPropertyStr(ctx, this_val, "__nb_r_eo", JS_NewInt32(ctx, 0));
    JS_FreeValue(ctx, doc);
    nb_range_fill(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_range_to_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    return nb_range_get(ctx, this_val, 0, NULL, 6);
}
