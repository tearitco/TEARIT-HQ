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

/* Rung 6: inject a pure-JS URL + URLSearchParams polyfill (ES5.1-safe).
 * Roadmap NB-JS-ENGINE-ROADMAP.md §6. Real navigation (history / fetch)
 * is a later rung; these only PARSE + expose, so an analytics/consent
 * /router script that does "new URL(location.href)" / ".searchParams"
 * stops throwing. Evaluated once in main() just before the page script. */
static const char g_js_prelude[] =
"/* nb_js_eval prelude: URL + URLSearchParams polyfill (ES5.1) */\n"
"(function(){\n"
"var EMPTY_URL='about:blank';\n"
"function enc(s){ return encodeURIComponent(String(s)); }\n"
"function dec(s){ try{ return decodeURIComponent(String(s).replace(/\\+/g,' ')); }catch(e){ return String(s); } }\n"
"function splitParts(urlString, base){\n"
"  var s=String(urlString||'');\n"
"  if(s.indexOf('://')<0 && s.charAt(0)!=='/' && s.indexOf('?')!==0 && s.charAt(0)!=='#' && s!==''){\n"
"    if(base && base!==EMPTY_URL){ var b=splitParts(base,null); var pos=b.pathname.lastIndexOf('/'); s=b.protocol+'//'+b.host+(b.port?':'+b.port:'')+(pos>=0?b.pathname.slice(0,pos+1):'/')+s; }\n"
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
"/* ---- history (rung 6 stubs — no real navigation until rung 6 fully lands) ---- */\n"
"(function(){\n"
"  var _stack=[{state:null,title:'',url:''}], _idx=0;\n"
"  var h={scrollRestoration:'auto', back:function(){if(_idx>0)_idx--;}, forward:function(){if(_idx<_stack.length-1)_idx++;},\n"
"    go:function(n){var t=_idx+n;if(t>=0&&t<_stack.length)_idx=t;}};\n"
"  Object.defineProperty(h,'length',{get:function(){return _stack.length;}});\n"
"  Object.defineProperty(h,'state',{get:function(){return _stack[_idx].state;}});\n"
"  Object.defineProperty(h,'href',{get:function(){return _stack[_idx].url;}});\n"
"  h.pushState=function(st,t,u){_stack.push({state:st||null,title:String(t||''),url:String(u||'')});_idx=_stack.length-1;};\n"
"  h.replaceState=function(st,t,u){_stack[_idx]={state:st||null,title:String(t||''),url:String(u||'')};};\n"
"  Object.defineProperty(window,'history',{value:h,configurable:true,writable:true});\n"
"})();\n"
"\n"
"/* ---- matchMedia / getComputedStyle / MutationObserver stubs ---- */\n"
"Object.defineProperty(window,'matchMedia',{value:function(q){return{matches:false,media:String(q||''),addListener:function(){},removeListener:function(){},addEventListener:function(){},removeEventListener:function(){},dispatchEvent:function(){return false;}};},configurable:true,writable:true});\n"
"Object.defineProperty(window,'getComputedStyle',{value:function(){return{getPropertyValue:function(){return'';}};},configurable:true,writable:true});\n"
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
"/* Empty in-memory jar. Reads return '' so cookie-consent / analytics\n"
" * reads don't throw; writes are accepted but not persisted. A real\n"
" * #.desktop/nb_cookies.txt jar is a C native function -> rung 6. */\n"
"try{ Object.defineProperty(document,'cookie',{ get:function(){return '';}, set:function(v){}, configurable:true }); }catch(e){}\n"
"})();\n";

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
