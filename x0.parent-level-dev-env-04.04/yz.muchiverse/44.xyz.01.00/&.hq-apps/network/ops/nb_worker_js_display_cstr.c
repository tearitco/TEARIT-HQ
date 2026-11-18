/* nb_worker_js_display_cstr.c - JS value display string helper (mechanical extract, single-TU include) */
/* Format a value for console display (duk_safe_to_string semantics); clears
 * any exception a throwing toString() may have left behind. */
static const char *js_display_cstr(JSContext *ctx, JSValueConst v, char *buf, size_t bl) {
    if (JS_IsUndefined(v)) { snprintf(buf, bl, "undefined"); return buf; }
    char *s = JS_ToCString(ctx, v);
    if (!s) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
        snprintf(buf, bl, "undefined");
        return buf;
    }
    snprintf(buf, bl, "%s", s);
    JS_FreeCString(ctx, s);
    if (JS_HasException(ctx)) {
        JSValue e = JS_GetException(ctx);
        JS_FreeValue(ctx, e);
    }
    return buf;
}
