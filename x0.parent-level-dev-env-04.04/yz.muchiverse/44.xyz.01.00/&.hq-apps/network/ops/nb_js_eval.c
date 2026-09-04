#define _POSIX_C_SOURCE 200809L
/* nb_js_eval.c — one-job JavaScript op for network-browser-hq.
 * Reads one .js file, runs it in Duktape, writes a pipe-table of effects.
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

    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if (!ctx) {
        pipe_one("ERROR", "duk_create_heap failed");
        fprintf(g_out, "OK|0\n");
        fclose(g_out);
        free(src);
        return 1;
    }
    install_host(ctx);

    /* rung 6 prelude: URL + URLSearchParams polyfill. If it fails the
     * page script still runs (URL just stays undefined). */
    if (duk_peval_string(ctx, g_js_prelude) != 0) {
        /* polyfill hygiene: swallow its own error, page continues */
        duk_pop(ctx);
    }
    duk_pop(ctx);

    duk_push_lstring(ctx, src, src_n);
    free(src);
    if (duk_peval(ctx) != 0) {
        pipe_one("ERROR", duk_safe_to_string(ctx, -1));
        fprintf(g_out, "OK|0\n");
        duk_destroy_heap(ctx);
        fclose(g_out);
        return 1;
    }
    duk_pop(ctx);
    if (g_title_set) pipe_one("TITLE", g_title);
    fprintf(g_out, "OK|1\n");
    duk_destroy_heap(ctx);
    fclose(g_out);
    return 0;
}
