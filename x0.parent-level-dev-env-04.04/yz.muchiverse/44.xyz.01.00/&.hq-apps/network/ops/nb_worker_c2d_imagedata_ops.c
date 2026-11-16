/* nb_worker_c2d_imagedata_ops.c - canvas 2D image data operations (mechanical extract, single-TU include) */
static JSValue nb_c2d_getImageData_real(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int sx = js_arg_index(ctx, argv, argc, 0);
    int sy = js_arg_index(ctx, argv, argc, 1);
    int w = js_arg_index(ctx, argv, argc, 2);
    int h = js_arg_index(ctx, argv, argc, 3);
    if (sx < 0) sx = 0; if (sy < 0) sy = 0;
    if (w < 0) w = 0; if (h < 0) h = 0;
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    int cw = cv ? cv->w : 0, ch = cv ? cv->h : 0;
    if (w == 0 || h == 0) {
        JSValue ex = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, ex, "name", JS_NewString(ctx, "IndexSizeError"));
        JS_Throw(ctx, ex);
        return JS_EXCEPTION;
    }
    JSValue img = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, img, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, img, "height", JS_NewInt32(ctx, h));
    JSValue len = JS_NewInt32(ctx, 4 * w * h);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, g, "Uint8ClampedArray");
    JS_FreeValue(ctx, g);
    JSValue data = (JS_IsObject(ctor) && !JS_IsNull(ctor))
                       ? JS_CallConstructor(ctx, ctor, 1, &len)
                       : JS_NewArray(ctx);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(data)) { JS_FreeValue(ctx, JS_GetException(ctx)); data = JS_NewArray(ctx); }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned char px[4] = {0, 0, 0, 0};
            if (cv && sx + x < cw && sy + y < ch)
                memcpy(px, cv->px + ((size_t)(sy + y) * cw + sx + x) * 4, 4);
            for (int k = 0; k < 4; k++) {
                JSValue ev = JS_NewInt32(ctx, px[k]);
                JS_SetPropertyUint32(ctx, data, (uint32_t)(4 * (y * w + x) + k), ev);
            }
        }
    JS_SetPropertyStr(ctx, img, "data", data);
    JS_FreeValue(ctx, len);
    return img;
}
static JSValue nb_c2d_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int w = js_arg_index(ctx, argv, argc, 0);
    int h = js_arg_index(ctx, argv, argc, 1);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    JSValue img = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, img, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, img, "height", JS_NewInt32(ctx, h));
    JSValue len = JS_NewInt32(ctx, 4 * w * h);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_GetPropertyStr(ctx, g, "Uint8ClampedArray");
    JS_FreeValue(ctx, g);
    JSValue data = (JS_IsObject(ctor) && !JS_IsNull(ctor))
                       ? JS_CallConstructor(ctx, ctor, 1, &len)
                       : JS_NewArray(ctx);
    JS_FreeValue(ctx, ctor);
    if (JS_IsException(data)) { JS_FreeValue(ctx, JS_GetException(ctx)); data = JS_NewArray(ctx); }
    JS_SetPropertyStr(ctx, img, "data", data);
    JS_FreeValue(ctx, len);
    return img;
}
