#define _POSIX_C_SOURCE 200809L
/* nb_js_eval.c — one-job JavaScript op for network-browser-hq.
 * Reads one .js file, runs it in Duktape, writes a pipe-table of effects.
 * The manager is the only writer of page state; this process only writes
 * its own out file, then exits.
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

/* Push each component of an href as a plain string property on the object
 * at the top of the stack. Rung 1: parsing only, no navigation. */
static void install_location_parts(duk_context *ctx, const char *href) {
    char protocol[32] = "";
    char host[1024]   = "";   /* hostname[:port] */
    char hostname[1024] = "";
    char port[16]     = "";
    char pathname[2048] = "";
    char search[2048] = "";
    char hash[2048]   = "";
    char origin[1100] = "null";

    const char *p = href ? href : "";
    const char *sep = strstr(p, "://");
    const char *authority_start = p;
    if (sep) {
        size_t plen = (size_t)(sep - p);
        if (plen < sizeof(protocol) - 1) {
            memcpy(protocol, p, plen);
            protocol[plen] = 0;
            strcat(protocol, ":");
        }
        authority_start = sep + 3;
    }

    /* authority runs until the first '/', '?' or '#' */
    const char *a = authority_start;
    const char *ae = a;
    while (*ae && *ae != '/' && *ae != '?' && *ae != '#') ae++;
    {
        size_t alen = (size_t)(ae - a);
        char authority[1024] = "";
        if (alen < sizeof(authority) - 1) { memcpy(authority, a, alen); authority[alen] = 0; }
        /* drop userinfo */
        char *at = strrchr(authority, '@');
        const char *hp = at ? at + 1 : authority;
        snprintf(host, sizeof(host), "%s", hp);
        /* split host:port (last ':' that is not inside [] IPv6 — keep it simple) */
        char *colon = strrchr(host, ':');
        char *rb = strrchr(host, ']');
        if (colon && (!rb || colon > rb)) {
            snprintf(port, sizeof(port), "%s", colon + 1);
            size_t hn = (size_t)(colon - host);
            if (hn < sizeof(hostname)) { memcpy(hostname, host, hn); hostname[hn] = 0; }
        } else {
            snprintf(hostname, sizeof(hostname), "%s", host);
        }
    }

    /* pathname / search / hash from ae onward */
    const char *rest = ae;
    const char *q = strchr(rest, '?');
    const char *h = strchr(rest, '#');
    const char *path_end = rest + strlen(rest);
    if (q) path_end = q;
    if (h && h < path_end) path_end = h;
    {
        size_t pl = (size_t)(path_end - rest);
        if (pl && pl < sizeof(pathname)) { memcpy(pathname, rest, pl); pathname[pl] = 0; }
        else if (!pl) snprintf(pathname, sizeof(pathname), "/");
    }
    if (q) {
        const char *se = h && h > q ? h : q + strlen(q);
        size_t sl = (size_t)(se - q);
        if (sl < sizeof(search)) { memcpy(search, q, sl); search[sl] = 0; }
    }
    if (h) snprintf(hash, sizeof(hash), "%s", h);

    if (protocol[0] && host[0])
        snprintf(origin, sizeof(origin), "%s//%s", protocol, host);

    duk_push_string(ctx, protocol); duk_put_prop_string(ctx, -2, "protocol");
    duk_push_string(ctx, host);     duk_put_prop_string(ctx, -2, "host");
    duk_push_string(ctx, hostname); duk_put_prop_string(ctx, -2, "hostname");
    duk_push_string(ctx, port);     duk_put_prop_string(ctx, -2, "port");
    duk_push_string(ctx, pathname); duk_put_prop_string(ctx, -2, "pathname");
    duk_push_string(ctx, search);   duk_put_prop_string(ctx, -2, "search");
    duk_push_string(ctx, hash);     duk_put_prop_string(ctx, -2, "hash");
    duk_push_string(ctx, origin);   duk_put_prop_string(ctx, -2, "origin");
}

static void install_host(duk_context *ctx) {
    duk_push_global_object(ctx);
    duk_idx_t g = duk_get_top(ctx) - 1;   /* absolute index of the real JS global */

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
    install_location_parts(ctx, g_href);
    /* rung 6 will make these navigate; for now they must not throw */
    duk_push_c_function(ctx, native_noop, DUK_VARARGS);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "assign");
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "replace");
    duk_put_prop_string(ctx, -2, "reload");
    duk_put_prop_string(ctx, -2, "location");

    /* navigator — plain data props only, no functions (rung 1) */
    duk_push_object(ctx);
    duk_push_string(ctx, "Mozilla/5.0 (X11; Linux x86_64) nb_js_eval");
    duk_put_prop_string(ctx, -2, "userAgent");
    duk_push_string(ctx, "en");
    duk_put_prop_string(ctx, -2, "language");
    duk_push_array(ctx);
    duk_push_string(ctx, "en");
    duk_put_prop_index(ctx, -2, 0);
    duk_put_prop_string(ctx, -2, "languages");
    duk_push_string(ctx, "Linux x86_64");
    duk_put_prop_string(ctx, -2, "platform");
    duk_push_boolean(ctx, 1);
    duk_put_prop_string(ctx, -2, "onLine");
    duk_push_boolean(ctx, 0);
    duk_put_prop_string(ctx, -2, "cookieEnabled");
    duk_push_null(ctx);
    duk_put_prop_string(ctx, -2, "doNotTrack");
    duk_put_prop_string(ctx, -2, "navigator");

    /* screen */
    duk_push_object(ctx);
    duk_push_int(ctx, 1920); duk_put_prop_string(ctx, -2, "width");
    duk_push_int(ctx, 1080); duk_put_prop_string(ctx, -2, "height");
    duk_push_int(ctx, 1920); duk_put_prop_string(ctx, -2, "availWidth");
    duk_push_int(ctx, 1080); duk_put_prop_string(ctx, -2, "availHeight");
    duk_push_int(ctx, 24);   duk_put_prop_string(ctx, -2, "colorDepth");
    duk_push_int(ctx, 24);   duk_put_prop_string(ctx, -2, "pixelDepth");
    duk_put_prop_string(ctx, -2, "screen");

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

    /* window / self / globalThis ARE the real Duktape global object. */
    duk_dup(ctx, g);
    duk_put_prop_string(ctx, g, "window");
    duk_dup(ctx, g);
    duk_put_prop_string(ctx, g, "self");
    duk_dup(ctx, g);
    duk_put_prop_string(ctx, g, "globalThis");

    /* cheap always-safe window scalars */
    duk_push_string(ctx, "");
    duk_put_prop_string(ctx, g, "name");
    duk_push_boolean(ctx, 0);
    duk_put_prop_string(ctx, g, "closed");
    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, g, "length");

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
