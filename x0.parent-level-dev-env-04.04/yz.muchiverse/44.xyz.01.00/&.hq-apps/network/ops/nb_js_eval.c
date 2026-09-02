#define _POSIX_C_SOURCE 200809L
/* nb_js_eval.c — one-job JavaScript op for network-browser-hq.
 * Reads one .js file, runs it in Duktape, writes a pipe-table of effects.
 * usage: nb_js_eval.+x <script.js> <out.txt> [href] [initial_title]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../js/duktape.h"

#define LINE_CAP 2048
#define TITLE_CAP 512

static FILE *g_out;
static char g_title[TITLE_CAP];
static int g_title_set;
static char g_href[4096];

static void pipe_one(const char *key, const char *val) {
    char buf[LINE_CAP];
    size_t o = 0;
    if (!g_out || !key) return;
    for (size_t i = 0; val && val[i] && o + 1 < sizeof(buf); i++) {
        unsigned char c = (unsigned char)val[i];
        if (c == '\r') continue;
        if (c == '\n' || c == '|') buf[o++] = ' ';
        else buf[o++] = (char)c;
    }
    buf[o] = 0;
    fprintf(g_out, "%s|%s\n", key, buf);
}

static duk_ret_t native_log(duk_context *ctx) {
    duk_idx_t n = duk_get_top(ctx);
    char line[LINE_CAP];
    size_t o = 0;
    line[0] = 0;
    for (duk_idx_t i = 0; i < n; i++) {
        const char *s = duk_safe_to_string(ctx, i);
        if (i > 0 && o + 1 < sizeof(line)) line[o++] = ' ';
        if (!s) continue;
        size_t sl = strlen(s);
        if (o + sl >= sizeof(line)) sl = sizeof(line) - 1 - o;
        memcpy(line + o, s, sl);
        o += sl;
        line[o] = 0;
    }
    pipe_one("LOG", line);
    return 0;
}

static duk_ret_t native_write(duk_context *ctx) {
    duk_idx_t n = duk_get_top(ctx);
    for (duk_idx_t i = 0; i < n; i++)
        pipe_one("TEXT", duk_safe_to_string(ctx, i));
    return 0;
}

static duk_ret_t native_get_title(duk_context *ctx) {
    duk_push_string(ctx, g_title);
    return 1;
}

static duk_ret_t native_set_title(duk_context *ctx) {
    const char *s = duk_safe_to_string(ctx, 0);
    snprintf(g_title, sizeof(g_title), "%s", s ? s : "");
    g_title_set = 1;
    return 0;
}

static duk_ret_t native_get_href(duk_context *ctx) {
    duk_push_string(ctx, g_href);
    return 1;
}

static duk_ret_t native_null(duk_context *ctx) {
    (void)ctx;
    duk_push_null(ctx);
    return 1;
}

static duk_ret_t native_undefined(duk_context *ctx) {
    (void)ctx;
    duk_push_undefined(ctx);
    return 1;
}

static duk_ret_t native_noop(duk_context *ctx) {
    (void)ctx;
    return 0;
}

static void fatal_handler(void *udata, const char *msg) {
    (void)udata;
    if (g_out) pipe_one("ERROR", msg ? msg : "fatal");
    if (g_out) fprintf(g_out, "OK|0\n");
    if (g_out) fclose(g_out);
    abort();
}

static int read_file(const char *path, char **out, size_t *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long n = ftell(f);
    if (n < 0 || n > 512 * 1024) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    *out = buf;
    *out_n = got;
    return 1;
}

static void install_host(duk_context *ctx) {
    duk_push_global_object(ctx);
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "print");
    duk_push_object(ctx);
    duk_push_c_function(ctx, native_log, DUK_VARARGS);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "log");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "info");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "warn");
    duk_put_prop_string(ctx, -2, "error");
    duk_put_prop_string(ctx, -2, "console");
    duk_push_object(ctx);
    duk_push_string(ctx, "title");
    duk_push_c_function(ctx, native_get_title, 0);
    duk_push_c_function(ctx, native_set_title, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);
    duk_push_c_function(ctx, native_write, DUK_VARARGS);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "write");
    duk_put_prop_string(ctx, -2, "writeln");
    duk_push_c_function(ctx, native_null, 1);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "getElementById");
    duk_put_prop_string(ctx, -2, "querySelector");
    duk_put_prop_string(ctx, -2, "document");
    duk_push_object(ctx);
    duk_push_string(ctx, "href");
    duk_push_c_function(ctx, native_get_href, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_ENUMERABLE);
    duk_put_prop_string(ctx, -2, "location");
    duk_push_object(ctx);
    duk_push_c_function(ctx, native_undefined, 1);
    duk_put_prop_string(ctx, -2, "getItem");
    duk_push_c_function(ctx, native_noop, DUK_VARARGS);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "setItem");
    duk_put_prop_string(ctx, -2, "removeItem");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "sessionStorage");
    duk_put_prop_string(ctx, -2, "localStorage");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -2, "window");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -2, "self");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -2, "globalThis");
    duk_pop(ctx);
}

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
