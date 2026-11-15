/* nb_worker_insert_adjacent_parse.c - insertAdjacent position parse/error helpers (mechanical extract, single-TU include) */
enum { NB_IA_BEFORE_BEGIN = 0, NB_IA_AFTER_BEGIN = 1, NB_IA_BEFORE_END = 2, NB_IA_AFTER_END = 3 };
static int nb_ia_parse_pos(JSContext *ctx, JSValueConst v) {
    const char *s = JS_ToCString(ctx, v);   /* stringify like the DOM does */
    if (!s) return -2;                      /* exception already pending */
    int r = -1;
    if (!strcasecmp(s, "beforebegin")) r = NB_IA_BEFORE_BEGIN;
    else if (!strcasecmp(s, "afterbegin")) r = NB_IA_AFTER_BEGIN;
    else if (!strcasecmp(s, "beforeend")) r = NB_IA_BEFORE_END;
    else if (!strcasecmp(s, "afterend")) r = NB_IA_AFTER_END;
    JS_FreeCString(ctx, s);
    return r;
}
static JSValue nb_ia_syntax_error(JSContext *ctx) {
    JSValue e = JS_NewError(ctx);
    if (!JS_IsException(e)) {
        JS_SetPropertyStr(ctx, e, "name", JS_NewString(ctx, "SyntaxError"));
        JS_SetPropertyStr(ctx, e, "message",
            JS_NewString(ctx, "SyntaxError: the position is not beforebegin, afterbegin, beforeend, or afterend"));
    }
    return JS_Throw(ctx, e);
}
