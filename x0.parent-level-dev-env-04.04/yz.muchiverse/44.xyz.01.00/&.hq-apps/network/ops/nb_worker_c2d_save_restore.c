/* nb_worker_c2d_save_restore.c - canvas 2D save/restore implementation (mechanical extract, single-TU include) */
static char nb_c2d_save_fill[8][128];
static JSValue nb_c2d_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv || cv->save_depth >= 8) return JS_UNDEFINED;
    memcpy(cv->save_stack[cv->save_depth], cv->m, sizeof(cv->m));
    cv->save_stack[cv->save_depth][6] = nb_ctx_num(ctx, this_val, "globalAlpha", 1);
    JSValue v = JS_GetPropertyStr(ctx, this_val, "fillStyle");
    const char *s = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    snprintf(nb_c2d_save_fill[cv->save_depth], sizeof(nb_c2d_save_fill[0]), "%s", s ? s : "");
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
    cv->save_depth++;
    return JS_UNDEFINED;
}
static JSValue nb_c2d_restore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbCanvas *cv = nb_canvas_get(ctx, this_val);
    if (!cv || cv->save_depth <= 0) return JS_UNDEFINED;
    cv->save_depth--;
    memcpy(cv->m, cv->save_stack[cv->save_depth], sizeof(cv->m));
    JS_SetPropertyStr(ctx, this_val, "globalAlpha",
                      JS_NewFloat64(ctx, cv->save_stack[cv->save_depth][6]));
    JS_SetPropertyStr(ctx, this_val, "fillStyle",
                      JS_NewString(ctx, nb_c2d_save_fill[cv->save_depth]));
    return JS_UNDEFINED;
}
