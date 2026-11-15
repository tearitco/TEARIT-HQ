/* nb_worker_range_core.c - Range core set/ctor/createRange (mechanical extract, single-TU include) */
static JSValue nb_range_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_start) {
    (void)argc;
    if (!JS_IsObject(argv[0])) {
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "TypeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "TypeError: setStart/setEnd requires a node"));
        return JS_Throw(ctx, er);
    }
    int32_t off = 0;
    JS_ToInt32(ctx, &off, argv[1]);
    long max = 0;
    if (!nb_validate_endpoint(ctx, argv[0], &max)) {
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "TypeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "TypeError: setStart/setEnd requires a node"));
        return JS_Throw(ctx, er);
    }
    if (off < 0 || off > max) {
        JSValue er = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, er, "name", JS_NewString(ctx, "IndexSizeError"));
        JS_SetPropertyStr(ctx, er, "message", JS_NewString(ctx, "IndexSizeError: offset out of range"));
        return JS_Throw(ctx, er);
    }
    if (is_start) {
        JS_SetPropertyStr(ctx, this_val, "__nb_r_sc", JS_DupValue(ctx, argv[0]));
        JS_SetPropertyStr(ctx, this_val, "__nb_r_so", JS_NewInt32(ctx, off));
    } else {
        JS_SetPropertyStr(ctx, this_val, "__nb_r_ec", JS_DupValue(ctx, argv[0]));
        JS_SetPropertyStr(ctx, this_val, "__nb_r_eo", JS_NewInt32(ctx, off));
    }
    nb_range_fill(ctx, this_val);
    return JS_UNDEFINED;
}
static JSValue nb_range_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue obj = class_noop(ctx, this_val, 0, argv);
    if (!JS_IsObject(obj)) return obj;
    JSValue doc = get_global_attr(ctx, "document");
    JS_SetPropertyStr(ctx, obj, "__nb_r_sc", doc);
    JS_SetPropertyStr(ctx, obj, "__nb_r_so", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, obj, "__nb_r_ec", JS_DupValue(ctx, doc));
    JS_SetPropertyStr(ctx, obj, "__nb_r_eo", JS_NewInt32(ctx, 0));
    JS_FreeValue(ctx, doc);
    nb_range_fill(ctx, obj);
    return obj;
}
static JSValue nb_dom_createRange(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue cls = JS_GetPropertyStr(ctx, g, "Range");
    JSValue proto = JS_GetPropertyStr(ctx, cls, "prototype");
    JSValue r = JS_IsObject(proto) ? JS_NewObjectProto(ctx, proto) : JS_NewObject(ctx);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, cls);
    JSValue doc = (JS_IsObject(this_val) && get_node(ctx, this_val) &&
                    get_node(ctx, this_val)->tag && !strcmp(get_node(ctx, this_val)->tag, "#document"))
                    ? JS_DupValue(ctx, this_val)
                    : JS_GetPropertyStr(ctx, g, "document");
    JS_FreeValue(ctx, g);
    JS_SetPropertyStr(ctx, r, "__nb_r_sc", JS_DupValue(ctx, doc));
    JS_SetPropertyStr(ctx, r, "__nb_r_so", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, r, "__nb_r_ec", JS_DupValue(ctx, doc));
    JS_SetPropertyStr(ctx, r, "__nb_r_eo", JS_NewInt32(ctx, 0));
    JS_FreeValue(ctx, doc);
    nb_range_fill(ctx, r);
    return r;
}
