#define _POSIX_C_SOURCE 200809L
/* nb_js_eval.c — one-job JavaScript op for network-browser-hq.
 * Reads one .js file, runs it in QuickJS, writes a pipe-table of effects.
 * The manager is the only writer of page state; this process only writes
 * its own out file, then exits.
 *
 * Kept as the headless-test + rollback path (NB-JS worker plan §2C); the
 * resident worker is ops/nb_js_worker.c. The rung-1/6 host (install_host,
 * the URL/history/timers prelude, native accessors) now lives in the
 * shared nb_host.h included by both.
 *
 * usage: nb_js_eval.+x <script.js> <out.txt> [href] [initial_title]
 *
 * out.txt rows:
 *   LOG|<console line>
 *   TEXT|<document.write payload, one line>
 *   TITLE|<document.title if set>
 *   OK|1
 *   ERROR|<message>     (still writes OK|0)
 */
#include "nb_host.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <script.js> <out.txt> [href] [initial_title]\n", argv[0]);
        return 1;
    }
    g_title[0] = 0;
    g_href[0] = 0;
    if (argc >= 4) snprintf(g_href, sizeof(g_href), "%s", argv[3]);
    if (argc >= 5) snprintf(g_title, sizeof(g_title), "%s", argv[4]);

    g_out = fopen(argv[2], "w");
    if (!g_out) {
        fprintf(stderr, "nb_js_eval: cannot write %s\n", argv[2]);
        return 1;
    }

    char *src = NULL;
    size_t src_n = 0;
    if (!read_file(argv[1], &src, &src_n)) {
        pipe_one("ERROR", "cannot read script (missing, empty, or over 512KiB)");
        fprintf(g_out, "OK|0\n");
        fclose(g_out);
        return 1;
    }
    if (src_n == 0) {
        fprintf(g_out, "OK|1\n");
        fclose(g_out);
        free(src);
        return 0;
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        pipe_one("ERROR", "JS_NewRuntime failed");
        fprintf(g_out, "OK|0\n");
        fclose(g_out);
        free(src);
        return 1;
    }
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        pipe_one("ERROR", "JS_NewContext failed");
        fprintf(g_out, "OK|0\n");
        fclose(g_out);
        free(src);
        return 1;
    }
    install_host(ctx);

    /* rung 6 prelude: URL + URLSearchParams polyfill. If it fails the
     * page script still runs (URL just stays undefined). */
    {
        JSValue r = JS_Eval(ctx, g_js_prelude, strlen(g_js_prelude),
                            "<prelude>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(r)) {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
    }

    JSValue s = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    free(src);
    if (JS_IsException(s)) {
        char tmp[512];
        JSValue e = JS_GetException(ctx);
        const char *m = JS_ToCString(ctx, e);
        if (m) { snprintf(tmp, sizeof(tmp), "%s", m); JS_FreeCString(ctx, m); }
        else snprintf(tmp, sizeof(tmp), "script error");
        JS_FreeValue(ctx, e);
        JS_FreeValue(ctx, s);
        pipe_one("ERROR", tmp);
        fprintf(g_out, "OK|0\n");
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        fclose(g_out);
        return 1;
    }
    JS_FreeValue(ctx, s);
    if (g_title_set) pipe_one("TITLE", g_title);
    fprintf(g_out, "OK|1\n");
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    fclose(g_out);
    return 0;
}