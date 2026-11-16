/* nb_worker_ctx_str.c - canvas context string property helper (mechanical extract, single-TU include) */
static void nb_ctx_str(JSContext *ctx, JSValueConst o, const char *k, char *out, size_t n, const char *def) {
    JSValue v = JS_GetPropertyStr(ctx, o, k);
    const char *s = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    snprintf(out, n, "%s", s ? s : def);
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
}
