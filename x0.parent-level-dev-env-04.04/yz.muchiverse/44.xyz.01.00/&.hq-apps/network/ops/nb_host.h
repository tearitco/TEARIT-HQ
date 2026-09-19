/* nb_host.h — shared NB-JS QuickJS host, used by BOTH the one-shot
 * eval (nb_js_eval.c, kept as headless-test + rollback) and the resident
 * worker (nb_js_worker.c). NB-JS worker plan §2A: "install_host (reuse
 * the rung-1/6 host from nb_js_eval.c, shared via a small nb_host.c or
 * #include)".
 *
 * Everything here is `static` so each including translation unit gets its
 * own private copy — no ABI/link coupling, no shared-mutable state. A .c
 * that includes this just calls install_host(ctx) then eval's the page.
 *
 * The page script sees one consistent rung-1/6 host: a real QuickJS
 * global object exposed as window/self/globalThis, plus document /
 * location / navigator / screen / localStorage / console, and a rung-6
 * pure-JS prelude wiring URL, URLSearchParams, history, matchMedia,
 * getComputedStyle, MutationObserver, atob/btoa, timer stubs and
 * document.cookie.
 */
#ifndef NB_HOST_H
#define NB_HOST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../js/quickjs.h"

#ifndef NB_HOST_DEFINE_GLOBALS
#define NB_HOST_DEFINE_GLOBALS() \
    static FILE *g_out; \
    static char g_title[512]; \
    static int  g_title_set; \
    static char g_href[4096]; \
    static int  g_cli_log;      /* CLI mode: bare console lines, no prefix */\
    static char g_nav_kind[16]; /* rung-6 slice 2: pending NAV request the  */\
    static char g_nav_url[4096];/*  page asked for; emitted as a NAV frame  */\
    static int  g_nav_count;    /*  (BACK/FORWARD step count) by the worker  */
#endif

#define LINE_CAP 2048
#define TITLE_CAP 512

NB_HOST_DEFINE_GLOBALS()

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

static JSValue native_log(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    char line[LINE_CAP];
    size_t o = 0;
    line[0] = 0;
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (!s) {
            JSValue ex = JS_GetException(ctx);
            JS_FreeValue(ctx, ex);
            if (o + 9 < sizeof(line)) { memcpy(line + o, "undefined", 9); o += 9; line[o] = 0; }
            continue;
        }
        if (i > 0 && o + 1 < sizeof(line)) line[o++] = ' ';
        size_t sl = strlen(s);
        if (o + sl >= sizeof(line)) sl = sizeof(line) - 1 - o;
        memcpy(line + o, s, sl);
        o += sl;
        line[o] = 0;
        JS_FreeCString(ctx, s);
    }
    if (g_cli_log) fprintf(g_out, "%s\n", line);
    else pipe_one("LOG", line);
    return JS_UNDEFINED;
}

static JSValue native_write(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (!s) {
            JSValue ex = JS_GetException(ctx);
            JS_FreeValue(ctx, ex);
            continue;
        }
        pipe_one("TEXT", s);
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue native_get_title(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewString(ctx, g_title);
}

static JSValue native_set_title(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
        if (!s) { JSValue ex = JS_GetException(ctx); JS_FreeValue(ctx, ex); }
    }
    snprintf(g_title, sizeof(g_title), "%s", s ? s : "");
    if (s) JS_FreeCString(ctx, s);
    g_title_set = 1;
    return JS_UNDEFINED;
}

static JSValue native_get_href(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewString(ctx, g_href);
}

static JSValue native_null(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_NULL;
}

static JSValue native_undefined(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue native_noop(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

/* ---- rung-6 slice 2: real navigation request plumbing. The page's
 * location.* / history.* calls funnel into a pending NAV request recorded
 * in g_nav_kind/g_nav_url/g_nav_count. The worker daemon (its own private
 * g_nav_emit, see nb_js_worker.c) emits it as a NAV frame before STATUS;
 * the manager then runs it through its own fetch/back/forward machinery.
 * In the eval/CLI paths nothing is ever emitted - the request is inert,
 * exactly the old 'must not throw' noop behaviour. */
static void nav_resolve(const char *rel, char *out, size_t outsz) {
    if (!rel || !rel[0] || strstr(rel, "://")) {
        snprintf(out, outsz, "%s", rel ? rel : "");
        return;
    }
    const char *base = g_href[0] ? g_href : "about:blank";
    if (rel[0] == '/' && rel[1] == '/') {          /* //host/x protocol-relative */
        const char *se = strstr(base, "://");
        if (se) {
            size_t pl = (size_t)(se - base + 3);
            snprintf(out, outsz, "%.*s%s", (int)pl, base, rel + 2);
        } else snprintf(out, outsz, "%s", rel);
        return;
    }
    if (strncmp(base, "about:", 6) == 0) { snprintf(out, outsz, "%s", rel); return; }
    const char *sep = strstr(base, "://");
    if (!sep) { snprintf(out, outsz, "%s", rel); return; }
    const char *host_start = sep + 3;
    const char *path_start = strchr(host_start, '/');
    if (rel[0] == '/') {                           /* root-relative /x */
        size_t host_len = path_start ? (size_t)(path_start - base) : strlen(base);
        snprintf(out, outsz, "%.*s%s", (int)host_len, base, rel);
        return;
    }
    if (rel[0] == '?' || rel[0] == '#') {          /* same path, new query/hash */
        const char *stop = strchr(base, rel[0]);
        size_t upto = stop ? (size_t)(stop - base) : strlen(base);
        snprintf(out, outsz, "%.*s%s", (int)upto, base, rel);
        return;
    }
    if (path_start) {                              /* directory-relative */
        const char *last_slash = strrchr(path_start, '/');
        size_t dir_len = last_slash ? (size_t)(last_slash - base + 1)
                                    : (size_t)(path_start - base + 1);
        snprintf(out, outsz, "%.*s%s", (int)dir_len, base, rel);
    } else {
        size_t bl = strlen(base), rl = strlen(rel);
        if (bl + 1 >= outsz) bl = outsz - 2;
        if (outsz - bl - 1 < rl) rl = outsz - bl - 1;
        snprintf(out, outsz, "%.*s/%.*s", (int)bl, base, (int)rl, rel);
    }
}

static void nav_request(const char *kind, const char *param, int resolve) {
    snprintf(g_nav_kind, sizeof(g_nav_kind), "%s", kind);
    if (resolve) nav_resolve(param, g_nav_url, sizeof(g_nav_url));
    else         snprintf(g_nav_url, sizeof(g_nav_url), "%s", param ? param : "");
    g_nav_count = 1;
}

static JSValue nb_nav_go(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    (void)this_val;
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
        if (!s) { JSValue ex = JS_GetException(ctx); JS_FreeValue(ctx, ex); }
    }
    nav_request("GO", s, 1);
    if (s) JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
static JSValue nb_nav_replace(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
        if (!s) { JSValue ex = JS_GetException(ctx); JS_FreeValue(ctx, ex); }
    }
    nav_request("REPLACE", s, 1);
    if (s) JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}
static JSValue nb_nav_reload(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    nav_request("RELOAD", NULL, 0);
    return JS_UNDEFINED;
}
static JSValue nb_nav_back(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    nav_request("BACK", NULL, 0);
    return JS_UNDEFINED;
}
static JSValue nb_nav_forward(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    nav_request("FORWARD", NULL, 0);
    return JS_UNDEFINED;
}
static JSValue nb_nav_go_n(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val;
    double n = 0;
    if (argc > 0 && JS_IsNumber(argv[0])) {
        double d;
        if (JS_ToFloat64(ctx, &d, argv[0]) == 0) n = d;
    }
    int ni = (n < 0) ? (int)(-n) : (int)n;
    if (ni > 8) ni = 8;
    if (n < 0)      { nav_request("BACK", NULL, 0);    g_nav_count = ni; }
    else if (n > 0) { nav_request("FORWARD", NULL, 0); g_nav_count = ni; }
    else            { nav_request("RELOAD", NULL, 0); }
    return JS_UNDEFINED;
}
static JSValue nb_nav_addr(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    (void)this_val;
    /* pushState/replaceState: address bar update without a fetch. */
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
        if (!s) { JSValue ex = JS_GetException(ctx); JS_FreeValue(ctx, ex); }
    }
    nav_request("ADDR", s, 1);
    if (s) JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}

/* rung 7 slice 1: default getComputedStyle — a minimal object usable by
 * the one-shot eval/standalone hosts that have no DOM/style engine. The
 * resident worker overwrites __nb_ges with its rich CSS resolver after
 * install_host. */
static JSValue nb_ges(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "display", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "visibility", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "opacity", JS_NewString(ctx, "1"));
    JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, obj, "getPropertyValue",
                      JS_NewCFunction(ctx, native_null, "getPropertyValue", 1));
    return obj;
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

/* Page-loader-only read for real bundles (row-31): youtube's kevlar_base.js
 * is a single 10.8MB IIFE that can only be evaled whole, far past the 512KB
 * guard above. Unlike read_file, this has no small cap — it realloc-grows in
 * chunks up to a hard READ_FILE_BIG_MAX ceiling so the worker can't be asked
 * to materialize an unbounded file. fs-lite and CJS keep the 512KB read_file
 * (their users could legitimately request huge files and OOM the heap). The
 * eval watchdog (EVAL_BUDGET_SEC) remains the real runaway guard. */
#define READ_FILE_BIG_MAX (64 * 1024 * 1024)
static int read_file_big(const char *path, char **out, size_t *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t cap = 1024 * 1024, n = 0;
    char *buf = malloc(cap);
    if (!buf) { fclose(f); return 0; }
    for (;;) {
        size_t room = cap - n - 1;
        if (room == 0) {
            if (cap >= READ_FILE_BIG_MAX) { free(buf); fclose(f); return 0; }
            cap *= 2;
            if (cap > READ_FILE_BIG_MAX) cap = READ_FILE_BIG_MAX;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); fclose(f); return 0; }
            buf = nb;
            continue;
        }
        size_t got = fread(buf + n, 1, room, f);
        n += got;
        if (got < room) {
            if (feof(f)) break;
            if (ferror(f)) { free(buf); fclose(f); return 0; }
        }
    }
    fclose(f);
    buf[n] = 0;
    *out = buf;
    *out_n = n;
    return 1;
}

/* Push each component of an href as a plain string property on the given
 * object. Rung 1: parsing only, no navigation. */
static void install_location_parts(JSContext *ctx, JSValue obj, const char *href) {
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

    JS_SetPropertyStr(ctx, obj, "protocol", JS_NewString(ctx, protocol));
    JS_SetPropertyStr(ctx, obj, "host",     JS_NewString(ctx, host));
    JS_SetPropertyStr(ctx, obj, "hostname", JS_NewString(ctx, hostname));
    JS_SetPropertyStr(ctx, obj, "port",     JS_NewString(ctx, port));
    JS_SetPropertyStr(ctx, obj, "pathname", JS_NewString(ctx, pathname));
    JS_SetPropertyStr(ctx, obj, "search",   JS_NewString(ctx, search));
    JS_SetPropertyStr(ctx, obj, "hash",     JS_NewString(ctx, hash));
    JS_SetPropertyStr(ctx, obj, "origin",   JS_NewString(ctx, origin));
}

/* Rung 4 + rung 6 prelude: URL/URLSearchParams/history/matchMedia/
 * getComputedStyle/MutationObserver/atob/btoa/timers/document.cookie, plus
 * fetch()/XMLHttpRequest over the worker's nbFetchSync native. The rung-4
 * Promise polyfill was DELETED in the QuickJS graft — the engine ships a
 * native Promise + queueMicrotask (microtasks drained via
 * JS_ExecutePendingJob). Evaluated once just before the page script. See
 * nb_js_eval.c's comments for provenance. */
static const char g_js_prelude[] =
"/* NB-JS host prelude: URL + URLSearchParams polyfill (ES5.1) */\n"
"(function(){\n"
"var EMPTY_URL='about:blank';\n"
"function enc(s){ return encodeURIComponent(String(s)); }\n"
"function dec(s){ try{ return decodeURIComponent(String(s).replace(/\\+/g,' ')); }catch(e){ return String(s); } }\n"
"function splitParts(urlString, base){\n"
"  var s=String(urlString||'');\n"
"  if(s.indexOf('://')<0 && s.charAt(0)!=='/' && s.indexOf('?')!==0 && s.charAt(0)!=='#' && s!==''){\n"
"    if(base && base!==EMPTY_URL){ var b=splitParts(base,null); var pos=b.pathname.lastIndexOf('/'); s=b.protocol+'//'+b.host+(pos>=0?b.pathname.slice(0,pos+1):'/')+s; }\n"
"    else { s=EMPTY_URL; }\n"
"  }\n"
"if(s===EMPTY_URL) return {protocol:'',authority:'',host:'',hostname:'',port:'',pathname:'',search:'',hash:'',origin:'null'};\n"
"  var proto='', rest=s, hash='', search='', path='', host='', hostname='', port='';\n"
"  var m=s.match(/^([a-zA-Z][a-zA-Z0-9+.-]*):\\/\\//);\n"
"  if(m){ proto=m[1].toLowerCase()+':'; rest=s.slice(m[0].length); }\n"
"  var hio=rest.indexOf('#'); if(hio>=0){ hash=rest.slice(hio); rest=rest.slice(0,hio); }\n"
"  var qio=rest.indexOf('?'); if(qio>=0){ search=rest.slice(qio); rest=rest.slice(0,qio); }\n"
"  var aio=rest.indexOf('/'); var raw=aio<0?rest:rest.slice(0,aio);\n"
"  var uio=raw.indexOf('@'); var auth=uio>=0?raw.slice(uio+1):raw;\n"
"  var colon=auth.lastIndexOf(':'); if(colon>0 && auth.charAt(0)!=='['){ hostname=auth.slice(0,colon); port=auth.slice(colon+1); }\n"
"  else hostname=auth;\n"
"  host=hostname+(port?(':'+port):'');\n"
"  path=(aio<0)?'/'+rest:rest.slice(aio); if(path===''||path.charAt(0)!=='/') path='/'+path;\n"
"  var origin=(proto&&auth)?(proto+'//'+auth):'null';\n"
"  return {protocol:proto,authority:raw,host:host,hostname:hostname,port:port,pathname:path,search:search,hash:hash,origin:origin};\n"
"}\n"
"function URLSearchParams(init){\n"
"  var self=this; self._p=[];\n"
"  if(init===undefined||init===null) return;\n"
"  if(typeof init==='object' && init._p){ self._p=init._p.slice(); return; }\n"
"  if(typeof init==='string'){\n"
"    var q=init.replace(/^\\?/,'');\n"
"    var parts=q?q.split('&'):[];\n"
"    for(var i=0;i<parts.length;i++){ var kv=parts[i]; var eq=kv.indexOf('='); var k=(eq<0)?kv:kv.slice(0,eq); var v=(eq<0)?'':kv.slice(eq+1); if(!k && !v) continue; self._p.push([dec(k),dec(v)]); }\n"
"    return;\n"
"  }\n"
"  if(typeof init==='object' && init.forEach){ init.forEach(function(k,v){ self._p.push([String(k),String(v)]); }); return; }\n"
"  throw new Error('URLSearchParams: bad init');\n"
"}\n"
"URLSearchParams.prototype.get=function(k){ for(var i=0;i<this._p.length;i++) if(this._p[i][0]===String(k)) return this._p[i][1]; return null; };\n"
"URLSearchParams.prototype.getAll=function(k){ var r=[]; for(var i=0;i<this._p.length;i++) if(this._p[i][0]===String(k)) r.push(this._p[i][1]); return r; };\n"
"URLSearchParams.prototype.has=function(k){ return this.get(k)!==null; };\n"
"URLSearchParams.prototype.append=function(k,v){ this._p.push([String(k),String(v)]); };\n"
"URLSearchParams.prototype.set=function(k,v){ this.delete(k); this.append(k,v); };\n"
"URLSearchParams.prototype.delete=function(k){ var s=String(k), out=[]; for(var i=0;i<this._p.length;i++) if(this._p[i][0]!==s) out.push(this._p[i]); this._p=out; };\n"
"URLSearchParams.prototype.toString=function(){ var out=[]; for(var i=0;i<this._p.length;i++){ if(!this._p[i][0] && !this._p[i][1]) continue; out.push(enc(this._p[i][0])+'='+enc(this._p[i][1])); } return out.join('&'); };\n"
"URLSearchParams.prototype.forEach=function(fn){ for(var i=0;i<this._p.length;i++) fn.call(this, this._p[i][1], this._p[i][0], i); };\n"
"URLSearchParams.prototype.keys=function(){ return this._p.map(function(x){ return x[0]; }); };\n"
"URLSearchParams.prototype.values=function(){ return this._p.map(function(x){ return x[1]; }); };\n"
"URLSearchParams.prototype.entries=function(){ var a=[]; for(var i=0;i<this._p.length;i++) a.push([this._p[i][0],this._p[i][1]]); return a; };\n"
"function URL(urlString, base){\n"
"  var self=this; self._u=splitParts(urlString, base);\n"
"  self._params=new URLSearchParams(self._u.search.replace(/^\\?/,''));\n"
"}\n"
"URL.prototype.toString=function(){ var u=this._u; return (u.protocol?u.protocol:'')+(u.host?('//'+u.host):'')+u.pathname+(u.search?u.search:'')+(u.hash?u.hash:''); };\n"
"URL.prototype.valueOf=function(){ return this.toString(); };\n"
"URL.prototype.toJSON=function(){ return this.toString(); };\n"
"var RW=function(k){ return { get:function(){ return this._u[k]; } }; };\n"
"var URLWORD=function(k){ return { get:function(){ return this._u[k]; }, set:function(v){ this._u[k]=String(v); } }; };\n"
"function setHref(v){ this._u=splitParts(String(v)); this._params=new URLSearchParams(this._u.search.replace(/^\\?/,'')); }\n"
"Object.defineProperties(URL.prototype, {\n"
"  href: { get:function(){ return this.toString(); }, set:setHref },\n"
"  protocol: RW('protocol'), origin: RW('origin'), host: RW('host'), hostname: URLWORD('hostname'), port: URLWORD('port'), pathname: URLWORD('pathname'),\n"
"  search: { get:function(){ return this._u.search; }, set:function(v){ v=String(v||''); this._u.search=(v&&v.charAt(0)!=='?')?('?'+v):v; this._params=new URLSearchParams(this._u.search.replace(/^\\?/,'')); } },\n"
"  hash: URLWORD('hash'),\n"
"  searchParams: { get:function(){ return this._params; } },\n"
"  username: { get:function(){ var a=this._u.authority||''; var at=a.indexOf('@'); var pre=(at>0)?a.slice(0,at):''; var c=pre.indexOf(':'); return c>=0?pre.slice(0,c):pre; } },\n"
"  password: { get:function(){ var a=this._u.authority||''; var at=a.indexOf('@'); if(at<=0) return ''; var p=a.slice(0,at); var c=p.indexOf(':'); return c>=0?p.slice(c+1):''; } }\n"
"});\n"
"Object.defineProperty(window,'URL',{ value:URL, configurable:true, writable:true });\n"
"Object.defineProperty(window,'URLSearchParams',{ value:URLSearchParams, configurable:true, writable:true });\n"
"\n"
"/* ---- history (rung 6 slice 2): real navigation via worker natives. ---- */\n"
"/* ---- pushState/replaceState keep a per-heap bookkeeping stack (state, ---- */\n"
"/* ---- length) and ALSO notify the manager to update the address bar sum ---- */\n"
"/* ---- (ADDR, no fetch). back/forward/go route to the worker; the manager ---- */\n"
"/* ---- walks its file stacks / re-fetches on its own loop. In eval/CLI ---- */\n"
"/* ---- (no daemon) the natives are inert no-ops. ---- */\n"
"(function(){\n"
"  var _stack=[{state:null,title:'',url:''}], _idx=0;\n"
"  var h={scrollRestoration:'auto',\n"
"    back:function(){ if(typeof __nb_nav_back==='function') __nb_nav_back(); },\n"
"    forward:function(){ if(typeof __nb_nav_forward==='function') __nb_nav_forward(); },\n"
"    go:function(n){ if(typeof __nb_nav_go==='function') __nb_nav_go(n); },\n"
"    pushState:function(st,t,u){ _stack[_stack.length]={state:st||null,title:String(t||''),url:String(u||'')}; _idx=_stack.length-1; if(typeof __nb_nav_addr==='function') __nb_nav_addr(String(u||'')); },\n"
"    replaceState:function(st,t,u){ _stack[_idx]={state:st||null,title:String(t||''),url:String(u||'')}; if(typeof __nb_nav_addr==='function') __nb_nav_addr(String(u||'')); }\n"
"  };\n"
"  Object.defineProperty(h,'length',{get:function(){return _stack.length;}});\n"
"  Object.defineProperty(h,'state',{get:function(){return _stack[_idx].state;}});\n"
"  Object.defineProperty(window,'history',{value:h,configurable:true,writable:true});\n"
"})();\n"
"\n"
"/* ---- matchMedia / getComputedStyle / MutationObserver stubs ---- */\n"
"Object.defineProperty(window,'matchMedia',{value:function(q){return{matches:false,media:String(q||''),addListener:function(){},removeListener:function(){},addEventListener:function(){},removeEventListener:function(){},dispatchEvent:function(){return false;}};},configurable:true,writable:true});\n"
"Object.defineProperty(window,'getComputedStyle',{value:function(el){var s=__nb_ges(el);return s;},configurable:true,writable:true});\n"
"function MutationObserver(cb){this._cb=cb;}\n"
"MutationObserver.prototype.observe=function(){};\n"
"MutationObserver.prototype.disconnect=function(){};\n"
"MutationObserver.prototype.takeRecords=function(){return[];};\n"
"Object.defineProperty(window,'MutationObserver',{value:MutationObserver,configurable:true,writable:true});\n"
"\n"
"/* ---- atob / btoa (Base64, ES5.1 safe) ---- */\n"
"(function(){\n"
"  var C='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';\n"
"  function btoa(s){ s=String(s); var o='',i;\n"
"    for(i=0;i<s.length;i+=3){\n"
"      var a=s.charCodeAt(i),b=(i+1<s.length)?s.charCodeAt(i+1):0,c=(i+2<s.length)?s.charCodeAt(i+2):0;\n"
"      o+=C.charAt((a>>2)&63); o+=C.charAt(((a&3)<<4)|((b>>4)&15));\n"
"      o+=(i+1<s.length)?C.charAt(((b&15)<<2)|((c>>6)&3)): '=';\n"
"      o+=(i+2<s.length)?C.charAt(c&63):'=';\n"
"    } return o; }\n"
"  var D={}; for(var i=0;i<C.length;i++) D[C.charAt(i)]=i;\n"
"  function atob(s){ s=String(s).replace(/[^A-Za-z0-9+/=]/g,''); var o='',i;\n"
"    for(i=0;i<s.length;i+=4){\n"
"      var a=(D[s.charAt(i)]||0)<<2|(D[s.charAt(i+1)]||0)>>4;\n"
"      var b=((D[s.charAt(i+1)]||0)&15)<<4|(D[s.charAt(i+2)]||0)>>2;\n"
"      var c=((D[s.charAt(i+2)]||0)&3)<<6|(D[s.charAt(i+3)]||0);\n"
"      o+=String.fromCharCode(a); if(s.charAt(i+2)!=='=') o+=String.fromCharCode(b);\n"
"      if(s.charAt(i+3)!=='=') o+=String.fromCharCode(c);\n"
"    } return o; }\n"
"  Object.defineProperty(window,'btoa',{value:btoa,configurable:true,writable:true});\n"
"  Object.defineProperty(window,'atob',{value:atob,configurable:true,writable:true});\n"
"})();\n"
"\n"
"/* ---- setTimeout / setInterval / clearTimeout / clearInterval stubs ---- */\n"
"/* The one-shot nb_js_eval process exits right after eval; there is no\n"
" * event loop. Scripts that call setTimeout(fn,0) to defer work (common\n"
" * in analytics/consent) must not throw. The callback is never invoked\n"
" * here — rung 3 will wire these to the real event loop. */\n"
"var _timer_id=0;\n"
"Object.defineProperty(window,'setTimeout',{value:function(fn,ms){var id=++_timer_id; return id;},configurable:true,writable:true});\n"
"Object.defineProperty(window,'setInterval',{value:function(fn,ms){var id=++_timer_id; return id;},configurable:true,writable:true});\n"
"Object.defineProperty(window,'clearTimeout',{value:function(){},configurable:true,writable:true});\n"
"Object.defineProperty(window,'clearInterval',{value:function(){},configurable:true,writable:true});\n"
"\n"
"/* ---- document.cookie ---- */\n"
"/* Safe fallback: reads return '', writes are accepted and dropped. The\n"
" * resident worker (nb_js_worker.c) replaces this configurable property\n"
" * with real C natives backed by a file jar ($NB_COOKIES_FILE, default\n"
" * ~/.config/nbjs/nb_cookies.txt) — rung 6. Anything else that runs the\n"
" * prelude without install_dom() keeps this no-op stub. */\n"
"try{ Object.defineProperty(document,'cookie',{ get:function(){return '';}, set:function(v){}, configurable:true }); }catch(e){}\n"
"})();\n"
"\n"
"/* ---- rung 4: fetch() + XMLHttpRequest over nbFetchSync ---- */\n"
"(function(){\n"
"  function resolveUrl(base,url){\n"
"    url=String(url||''); base=String(base||'');\n"
"    if(/^[a-zA-Z][a-zA-Z0-9+.-]*:/i.test(url)) return url;\n"
"    if(base.slice(0,5)==='file:'){\n"
"      if(url.charAt(0)==='/') return 'file://'+url;\n"
"      var fi=base.lastIndexOf('/');\n"
"      return fi>=0 ? base.slice(0,fi+1)+url : ('file:///'+url);\n"
"    }\n"
"    if(url.charAt(0)==='/' && url.charAt(1)==='/'){\n"
"      var m=base.match(/^([a-zA-Z][a-zA-Z0-9+.-]*):/);\n"
"      return (m?m[1]:'http')+url;\n"
"    }\n"
"    if(url.charAt(0)==='/' || url.charAt(0)==='?' || url.charAt(0)==='#'){\n"
"      var sm=base.match(/^([a-zA-Z][a-zA-Z0-9+.-]*:\\/\\/[^\\/]*)/);\n"
"      if(sm) return sm[1]+url;\n"
"    }\n"
"    try{ var u=new URL(url, base); var s=u.toString(); if(s) return s; }catch(e){}\n"
"    var li=base.lastIndexOf('/');\n"
"    return li>=0 ? base.slice(0,li+1)+url : url;\n"
"  }\n"
"  function reqHeaders(headers){\n"
"    var h='';\n"
"    if(headers===undefined||headers===null) return h;\n"
"    if(typeof headers==='string') return headers;\n"
"    if(typeof headers.forEach==='function'){ headers.forEach(function(v,k){ if(k!=null) h+=String(k)+': '+String(v)+'\\n'; }); return h; }\n"
"    for(var k in headers){ if(Object.prototype.hasOwnProperty.call(headers,k)) h+=String(k)+': '+String(headers[k])+'\\n'; }\n"
"    return h;\n"
"  }\n"
"  function fetch(input, init){\n"
"    input=String(input); init=init||{};\n"
"    var url=resolveUrl((location&&location.href)?location.href:'', input);\n"
"    var method=String(init.method||'GET');\n"
"    var body=(init.body===undefined||init.body===null)?'':String(init.body);\n"
"    return new Promise(function(resolve,reject){\n"
"      if(typeof nbFetchSync!=='function'){ reject(new Error('fetch unavailable in this host')); return; }\n"
"      var r;\n"
"      try{ r=nbFetchSync(method, url, reqHeaders(init.headers), body); }\n"
"      catch(e){ reject(e); return; }\n"
"      if(!r.ok){ reject(new Error('fetch failed status='+r.status+(r.error?(' '+r.error):''))); return; }\n"
"      var resp={\n"
"        ok:(r.status>=200&&r.status<300), status:r.status, statusText:'', url:url,\n"
"        text:function(){ return Promise.resolve(String(r.body||'')); },\n"
"        json:function(){ return new Promise(function(res2,rej2){ try{ res2(JSON.parse(String(r.body||''))); }catch(e){ rej2(e); } }); },\n"
"        blob:function(){ return Promise.resolve({}); }, arrayBuffer:function(){ return Promise.resolve({}); },\n"
"        clone:function(){ return this; }\n"
"      };\n"
"      resp.headers={ get:function(){ return null; }, has:function(){ return false; }, forEach:function(){},\n"
"        entries:function(){ return []; }, keys:function(){ return []; }, values:function(){ return []; } };\n"
"      resolve(resp);\n"
"    });\n"
"  }\n"
"  function XMLHttpRequest(){\n"
"    this.readyState=0; this._method='GET'; this._url=''; this._headers=''; this._body='';\n"
"    this.status=0; this.statusText=''; this.responseText=''; this.responseURL='';\n"
"    this.onload=null; this.onreadystatechange=null; this.onerror=null; this.onloadend=null; this._evs={};\n"
"  }\n"
"  XMLHttpRequest.prototype.addEventListener=function(t,f){ (this._evs[t]=this._evs[t]||[]).push(f); };\n"
"  XMLHttpRequest.prototype.removeEventListener=function(t,f){ var a=this._evs[t]; if(!a) return; var i=a.indexOf(f); if(i>=0) a.splice(i,1); };\n"
"  XMLHttpRequest.prototype.open=function(m,u,async){ this._method=String(m||'GET').toUpperCase(); this._url=String(u||''); this._async=(async!==false); };\n"
"  XMLHttpRequest.prototype.setRequestHeader=function(n,v){ this._headers+=String(n).trim()+': '+String(v)+'\\n'; };\n"
"  XMLHttpRequest.prototype.getResponseHeader=function(){ return null; };\n"
"  XMLHttpRequest.prototype.getAllResponseHeaders=function(){ return ''; };\n"
"  XMLHttpRequest.prototype.abort=function(){ this.readyState=0; };\n"
"  XMLHttpRequest.prototype.send=function(body){\n"
"    var self=this;\n"
"    var fire=function(t){ var a=self._evs[t]; if(!a) return;\n"
"      for(var i=0;i<a.length;i++){ (function(f){ try{ f.call(self,{type:t}); }catch(e){} })(a[i]); } };\n"
"    this._body=(body===undefined||body===null)?'':String(body);\n"
"    var s4=function(ok){ self.status=ok.status; self.responseText=String(ok.body||''); self.responseURL=self._url;\n"
"      self.readyState=4; if(self.onreadystatechange) try{ self.onreadystatechange({type:'readystatechange'}); }catch(e){}\n"
"      fire('readystatechange'); if(self.onload) try{ self.onload({type:'load'}); }catch(e){}\n"
"      fire('load'); if(self.onloadend) try{ self.onloadend({type:'loadend'}); }catch(e){} fire('loadend'); };\n"
"    if(typeof nbFetchSync!=='function'){ s4({status:0,body:''}); return; }\n"
"    this.readyState=1; if(this.onreadystatechange) try{ this.onreadystatechange({type:'readystatechange'}); }catch(e){} fire('readystatechange');\n"
"    try{ s4(nbFetchSync(this._method, resolveUrl((location&&location.href)?location.href:'', this._url), this._headers, this._body)); }\n"
"    catch(e){ s4({status:0,body:''}); }\n"
"  };\n"
"  Object.defineProperty(XMLHttpRequest.prototype,'response',{ get:function(){ return this.responseText; }, configurable:true });\n"
"  Object.defineProperty(XMLHttpRequest.prototype,'responseType',{ get:function(){ return ''; }, set:function(){}, configurable:true });\n"
"  Object.defineProperty(XMLHttpRequest.prototype,'withCredentials',{ get:function(){ return false; }, set:function(){}, configurable:true });\n"
"  Object.defineProperty(XMLHttpRequest.prototype,'timeout',{ get:function(){ return 0; }, set:function(){}, configurable:true });\n"
"  try{\n"
"    Object.defineProperty(window,'fetch',{ value:fetch, configurable:true, writable:true });\n"
"    Object.defineProperty(window,'XMLHttpRequest',{ value:XMLHttpRequest, configurable:true, writable:true });\n"
"    Object.defineProperty(window,'Headers',{ value:function(){ var h={};\n"
"      this.append=function(k,v){ h[String(k)]=String(v); };\n"
"      this.get=function(k){ return h[String(k)]===undefined?null:h[String(k)]; };\n"
"      this.has=function(k){ return h[String(k)]!==undefined; };\n"
"      this.forEach=function(f){ for(var k in h) if(Object.prototype.hasOwnProperty.call(h,k)) f(h[k],k); };\n"
"    this.keys=function(){ var a=[]; for(var k in h) a.push(k); return a; }; }, configurable:true, writable:true });\n"
"  }catch(e){}\n"
"})();\n"
"/* ---- rung 3: Event constructor (ES5.1; dispatchEvent/on-* live in the worker) ---- */\n"
"(function(){\n"
"  function Event(type, opts){\n"
"    opts=opts||{};\n"
"    this.type=String(type||''); this.bubbles=!!opts.bubbles; this.cancelable=!!opts.cancelable;\n"
"    this.defaultPrevented=false; this.propagationStopped=false;\n"
"    this.target=null; this.currentTarget=null;\n"
"  }\n"
"  Event.prototype.preventDefault=function(){ if(this.cancelable) this.defaultPrevented=true; };\n"
"  Event.prototype.stopPropagation=function(){ this.propagationStopped=true; };\n"
"  Event.prototype.stopImmediatePropagation=function(){ this.propagationStopped=true; this.immediatePropagationStopped=true; };\n"
"  Object.defineProperty(window,'Event',{ value:Event, configurable:true, writable:true });\n"
"})();\n"
"\n"
"/* ---- MessageChannel (row-31): youtube's kevlar schedules work via\n"
"/* ---- new MessageChannel().port2; needs async cross-wired ports. ---- */\n"
"(function(){\n"
"  if (typeof window.MessageChannel !== 'undefined') return;\n"
"  function Port(){ this.onmessage=null; this._ls=[]; this._peer=null; this._closed=false; }\n"
"  Port.prototype.addEventListener=function(t,f){ if(t==='message' && typeof f==='function') this._ls.push(f); };\n"
"  Port.prototype.removeEventListener=function(t,f){ if(t!=='message') return; var i=this._ls.indexOf(f); if(i>=0) this._ls.splice(i,1); };\n"
"  Port.prototype.start=function(){};\n"
"  Port.prototype.close=function(){ this._closed=true; this._ls=[]; this.onmessage=null; };\n"
"  Port.prototype.postMessage=function(m){ var self=this; setTimeout(function(){ var p=self._peer; if(!p||p._closed) return; var ev={data:m,type:'message',target:p,currentTarget:p}; try{ if(typeof p.onmessage==='function') p.onmessage(ev); }catch(e){} for(var i=0;i<p._ls.length;i++){ try{ p._ls[i].call(p,ev); }catch(e){} } },0); };\n"
"  function MessageChannel(){ var a=new Port(), b=new Port(); a._peer=b; b._peer=a; this.port1=a; this.port2=b; }\n"
"  Object.defineProperty(window,'MessageChannel',{ value:MessageChannel, configurable:true, writable:true });\n"
"})();\n"
"\n"
"/* ---- DocumentFragment for <template>.content (row-31): kevlar builds\n"
"/* ---- real template content fragments (shadycss). Minimal node bag. ---- */\n"
"(function(){\n"
"  if (window.__nb_docfrag) return;\n"
"  window.__nb_docfrag=function(owner){\n"
"    var f={ nodeType:11, nodeName:'#document-fragment', ownerDocument:owner||window.document,\n"
"      childNodes:[], children:[], firstChild:null, lastChild:null, parentNode:null,\n"
"      appendChild:function(c){ if(c){ this.childNodes.push(c); this.children.push(c); this.lastChild=c; this.firstChild=this.childNodes[0]; } return c; },\n"
"      insertBefore:function(c,ref){ if(!ref) return this.appendChild(c); var i=this.childNodes.indexOf(ref); if(i<0) return this.appendChild(c); this.childNodes.splice(i,0,c); this.children.splice(i,0,c); this.firstChild=this.childNodes[0]; return c; },\n"
"      removeChild:function(c){ var i=this.childNodes.indexOf(c); if(i>=0){ this.childNodes.splice(i,1); this.children.splice(i,1); } this.firstChild=this.childNodes[0]||null; this.lastChild=this.childNodes[this.childNodes.length-1]||null; return c; },\n"
"      replaceChild:function(nc,oc){ var i=this.childNodes.indexOf(oc); if(i>=0){ this.childNodes[i]=nc; this.children[i]=nc; } return oc; },\n"
"      hasChildNodes:function(){ return this.childNodes.length>0; },\n"
"      cloneNode:function(){ return window.__nb_docfrag(this.ownerDocument); },\n"
"      querySelector:function(){ return null; },\n"
"      querySelectorAll:function(){ return []; },\n"
"      addEventListener:function(){}, removeEventListener:function(){}, dispatchEvent:function(){ return true; }\n"
"    };\n"
"    return f;\n"
"  };\n"
"})();\n";

static void install_host(JSContext *ctx) {
    JSValue g = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, g, "print",
                      JS_NewCFunction(ctx, native_log, "print", 0));

    /* console */
    JSValue cons = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cons, "log",
                      JS_NewCFunction(ctx, native_log, "log", 0));
    JS_SetPropertyStr(ctx, cons, "info",
                      JS_NewCFunction(ctx, native_log, "info", 0));
    JS_SetPropertyStr(ctx, cons, "warn",
                      JS_NewCFunction(ctx, native_log, "warn", 0));
    JS_SetPropertyStr(ctx, cons, "error",
                      JS_NewCFunction(ctx, native_log, "error", 0));
    JS_SetPropertyStr(ctx, g, "console", cons);

    /* document */
    JSValue doc = JS_NewObject(ctx);
    JSAtom a_title = JS_NewAtom(ctx, "title");
    JS_DefinePropertyGetSet(ctx, doc, a_title,
                            JS_NewCFunction(ctx, native_get_title, "get title", 0),
                            JS_NewCFunction(ctx, native_set_title, "set title", 1),
                            JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                            JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_title);
    JS_SetPropertyStr(ctx, doc, "write",
                      JS_NewCFunction(ctx, native_write, "write", 0));
    JS_SetPropertyStr(ctx, doc, "writeln",
                      JS_NewCFunction(ctx, native_write, "writeln", 0));
    JS_SetPropertyStr(ctx, doc, "getElementById",
                      JS_NewCFunction(ctx, native_null, "getElementById", 1));
    JS_SetPropertyStr(ctx, doc, "querySelector",
                      JS_NewCFunction(ctx, native_null, "querySelector", 1));
    JS_SetPropertyStr(ctx, g, "document", doc);

    /* location */
    JSValue loc = JS_NewObject(ctx);
    JSAtom a_href = JS_NewAtom(ctx, "href");
    JS_DefinePropertyGetSet(ctx, loc, a_href,
                            JS_NewCFunction(ctx, native_get_href, "get href", 0),
                            JS_NewCFunction(ctx, nb_nav_go, "set href", 1),
                            JS_PROP_HAS_GET | JS_PROP_HAS_SET |
                            JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_href);
    install_location_parts(ctx, loc, g_href);
    /* rung 6 slice 2: real navigation. assign/href= push a history entry
     * (manager do_fetch record_history=1); replace swaps the current page
     * without a new history entry; reload re-fetches the same URL. */
    JS_SetPropertyStr(ctx, loc, "assign",
                      JS_NewCFunction(ctx, nb_nav_go, "assign", 1));
    JS_SetPropertyStr(ctx, loc, "replace",
                      JS_NewCFunction(ctx, nb_nav_replace, "replace", 1));
    JS_SetPropertyStr(ctx, loc, "reload",
                      JS_NewCFunction(ctx, nb_nav_reload, "reload", 0));
    JS_SetPropertyStr(ctx, g, "location", loc);

    /* navigator — plain data props only, no functions (rung 1) */
    JSValue nav = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, nav, "userAgent",
                      JS_NewString(ctx, "Mozilla/5.0 (X11; Linux x86_64) nb_js_eval"));
    JS_SetPropertyStr(ctx, nav, "language", JS_NewString(ctx, "en"));
    JSValue langs = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, langs, 0, JS_NewString(ctx, "en"));
    JS_SetPropertyStr(ctx, nav, "languages", langs);
    JS_SetPropertyStr(ctx, nav, "platform", JS_NewString(ctx, "Linux x86_64"));
    JS_SetPropertyStr(ctx, nav, "onLine", JS_NewBool(ctx, 1));
    JS_SetPropertyStr(ctx, nav, "cookieEnabled", JS_NewBool(ctx, 0));
    JS_SetPropertyStr(ctx, nav, "doNotTrack", JS_NULL);
    JS_SetPropertyStr(ctx, g, "navigator", nav);

    /* screen */
    JSValue scr = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, scr, "width",       JS_NewInt32(ctx, 1920));
    JS_SetPropertyStr(ctx, scr, "height",      JS_NewInt32(ctx, 1080));
    JS_SetPropertyStr(ctx, scr, "availWidth",  JS_NewInt32(ctx, 1920));
    JS_SetPropertyStr(ctx, scr, "availHeight", JS_NewInt32(ctx, 1080));
    JS_SetPropertyStr(ctx, scr, "colorDepth",  JS_NewInt32(ctx, 24));
    JS_SetPropertyStr(ctx, scr, "pixelDepth",  JS_NewInt32(ctx, 24));
    JS_SetPropertyStr(ctx, g, "screen", scr);

    /* localStorage / sessionStorage share one inert storage object */
    JSValue stor = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, stor, "getItem",
                      JS_NewCFunction(ctx, native_undefined, "getItem", 1));
    JS_SetPropertyStr(ctx, stor, "setItem",
                      JS_NewCFunction(ctx, native_noop, "setItem", 0));
    JS_SetPropertyStr(ctx, stor, "removeItem",
                      JS_NewCFunction(ctx, native_noop, "removeItem", 0));
    JS_SetPropertyStr(ctx, g, "sessionStorage", JS_DupValue(ctx, stor));
    JS_SetPropertyStr(ctx, g, "localStorage", stor);

    /* window / self / globalThis ARE the real QuickJS global object. */
    JS_SetPropertyStr(ctx, g, "window", JS_DupValue(ctx, g));
    JS_SetPropertyStr(ctx, g, "self", JS_DupValue(ctx, g));
    JS_SetPropertyStr(ctx, g, "globalThis", JS_DupValue(ctx, g));

    /* rung-6 slice 2: history prelude hooks. The prelude's history object
     * calls these to hand back/forward/go/ADDR to the worker (which turns
     * them into a NAV frame when a manager is listening). */
    JS_SetPropertyStr(ctx, g, "__nb_nav_back",
                      JS_NewCFunction(ctx, nb_nav_back, "__nb_nav_back", 0));
    JS_SetPropertyStr(ctx, g, "__nb_nav_forward",
                      JS_NewCFunction(ctx, nb_nav_forward, "__nb_nav_forward", 0));
    JS_SetPropertyStr(ctx, g, "__nb_nav_go",
                      JS_NewCFunction(ctx, nb_nav_go_n, "__nb_nav_go", 1));
    JS_SetPropertyStr(ctx, g, "__nb_nav_addr",
                      JS_NewCFunction(ctx, nb_nav_addr, "__nb_nav_addr", 1));

    /* rung 7 slice 1: window.getComputedStyle -> native bridge. The
     * minimal object here is the eval/standalone default; the resident
     * worker re-registers __nb_ges with its rich CSS-backed resolver
     * after install_host (the prelude resolves __nb_ges at call time). */
    JS_SetPropertyStr(ctx, g, "__nb_ges",
                      JS_NewCFunction(ctx, nb_ges, "__nb_ges", 1));

    /* cheap always-safe window scalars */
    JS_SetPropertyStr(ctx, g, "name", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, g, "closed", JS_NewBool(ctx, 0));
    JS_SetPropertyStr(ctx, g, "length", JS_NewInt32(ctx, 0));

    JS_FreeValue(ctx, g);
}

#endif /* NB_HOST_H */
