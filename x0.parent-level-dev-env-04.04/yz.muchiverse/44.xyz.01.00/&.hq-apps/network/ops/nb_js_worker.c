#define _POSIX_C_SOURCE 200809L
/* nb_js_worker.c — resident JavaScript worker for network-browser-hq.
 * Owned by network_browser_manager (its direct child, spawned lazily on
 * first <script> presence), talking line-RPC over a socketpair dup2'd to
 * stdin/stdout. NB-JS worker plan §1/§2/§4.
 *
 * This is the step-2 SKELETON: it creates one QuickJS runtime/context, reuses the
 * shared rung-1/6 host (nb_host.h), reads LOAD-delivered files, runs the
 * page's JS, and reports STATUS ok|err. It writes nothing to the page yet
 * (the DOM-tree accessors + RENDER merge land in steps 3-4). It stays
 * resident across LOADs and never bare-spins (blocks on stdin read).
 *
 * RPC framing (plan §4): length-prefixed lines.
 *   send:  "%.6d\n" + payload + "\n"
 *   recv:  read a %06d length line, then that many payload bytes.
 *
 * manager -> worker: LOAD\n<path page.js>\n<path fetch.dom>\n<href>\n<title>
 * worker  -> manager: RENDER\n<len>\n<page.state rows>   (post-JS DOM, step 4)
 * worker  -> manager: STATUS ok|err:<message>
 * manager -> worker: QUIT            (shut down cleanly)
 */
#include "nb_host.h"
#include <execinfo.h>   /* NB_CRASH_BT: fatal-signal backtrace into the worker log */
#include <time.h>
#include <sys/random.h>  /* getrandom(2) for nb_rand_bytes (crypto entropy) */
#include <errno.h>       /* EINTR in nb_rand_bytes' getrandom loop */
#include "../nb_css.h"
#include "../nb_dom.h"

/* R4 minimal shims */
static unsigned char _nbbd(char c){ if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26; if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; if(c=='=')return 64; return 255; }
/* OS entropy for the crypto surfaces (2026-11-02).
 *
 * WHY: getRandomValues used rand()&0xFF and NOTHING calls srand(), so glibc
 * seeded from 1 and every worker process produced the SAME bytes. Measured on
 * three separate processes: all returned 67c6697351ff4aec, and the same
 * randomUUID. That is a real correctness bug in a CSPRNG-shaped API, and a
 * loud bot signal - a browser whose "random" values repeat every session.
 *
 * Read the kernel CSPRNG via getrandom(2). rand() survives only as the
 * fallback for the case where the kernel call fails: degraded, but explicitly
 * so, rather than silently deterministic. (getrandom rather than
 * open("/dev/urandom") because this translation unit already has a `read`
 * symbol that collides with <unistd.h>'s.) */
static void nb_rand_bytes(unsigned char *out, size_t n){
    size_t got = 0;
    while (got < n) {
        ssize_t r = getrandom(out + got, n - got, 0);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; break; }
        got += (size_t)r;
    }
    if (got == n) return;
    for (size_t i = 0; i < n; i++) out[i] = (unsigned char)(rand() & 0xFF);
}
/* crypto.randomUUID (2026-11-02): a v4 UUID from the same OS entropy.
 * getRandomValues already draws from. Lives here, not in the prelude, because
 * install_events_timers builds `crypto` AFTER the prelude is evaluated - a
 * prelude guard on getRandomValues never fires. Format per spec: version
 * nibble 4 in byte 6, variant bits 10 in byte 8. */
static JSValue _nb_uuid(JSContext *ctx, JSValueConst this_val,int argc,JSValueConst *argv){ (void)this_val; (void)argc; (void)argv; unsigned char b[16]; nb_rand_bytes(b,16); b[6]=(unsigned char)((b[6]&0x0F)|0x40); b[8]=(unsigned char)((b[8]&0x3F)|0x80); char s[40]; snprintf(s,sizeof(s),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]); return JS_NewString(ctx,s); }
static JSValue _nb_atob(JSContext *ctx, JSValueConst this_val,int argc,JSValueConst *argv){ (void)this_val; if(argc<1)return JS_ThrowTypeError(ctx,"Failed to execute 'atob'"); const char *str=JS_ToCString(ctx,argv[0]); if(!str)return JS_EXCEPTION; size_t n=strlen(str); unsigned char *out=malloc((n/4+2)*3); if(!out){JS_FreeCString(ctx,str);return JS_EXCEPTION;} size_t o=0,i=0,ti=0; unsigned char t[4]; while(i<n){ char c=str[i++]; if(c==' '||c=='\n'||c=='\r'||c=='\t'||c=='\f'||c=='\v')continue; if(c=='=')break; unsigned char v=_nbbd(c); if(v==255)continue; t[ti++]=v; if(ti==4){ out[o++]=(t[0]<<2)|(t[1]>>4); out[o++]=((t[1]&0xF)<<4)|(t[2]>>2); out[o++]=((t[2]&0x3)<<6)|t[3]; ti=0; }} /* Flush the final partial group. Without this a padded tail was silently DROPPED: 'yhbPEA==' carries three bytes plus one, and only the three were emitted, so every padded base64 string came back short by its last byte. Verified 2026-11-02: Cloudflare's orchestrate decoder read its integer strings one character short and BigInt() then failed. */ if(ti==2){ out[o++]=(t[0]<<2)|(t[1]>>4); } else if(ti==3){ out[o++]=(t[0]<<2)|(t[1]>>4); out[o++]=((t[1]&0xF)<<4)|(t[2]>>2); } JS_FreeCString(ctx,str); /* atob yields a BINARY string: one char per byte, code 0..255. JS_NewStringLen decodes its buffer as UTF-8, so passing the raw bytes turned every byte >= 0x80 into U+FFFD (0xCA -> U+FFFD) and left every charCodeAt() consumer reading garbage. Verified 2026-11-02 against Cloudflare's orchestrate decoder, whose base64+XOR integer strings arrived as "\uff0ax\uff0a0\uff0a3..." and made BigInt() throw "invalid bigint literal". Re-encode each byte as a 2-byte UTF-8 sequence (0xC0|b>>6, 0x80|b&0x3F) when b >= 0x80 so the decode round-trips to exactly b. */ { unsigned char *u8=malloc(o*2+1); if(!u8){ free(out); return JS_ThrowOutOfMemory(ctx); } size_t u=0; for(size_t k=0;k<o;k++){ unsigned char b=out[k]; if(b<0x80){ u8[u++]=b; } else { u8[u++]=(unsigned char)(0xC0|(b>>6)); u8[u++]=(unsigned char)(0x80|(b&0x3F)); } } JSValue r=JS_NewStringLen(ctx,(const char*)u8,u); free(u8); free(out); return r; } }
/* btoa: base64 of a BINARY string.
 *
 * KNOWN LIMITATION (verified 2026-11-02, NOT fixed - it is not the blocker):
 * this converts its input with JS_ToCString, i.e. as UTF-8, so a code unit
 * >= 0x80 is encoded as the bytes of its UTF-8 form instead of its own low
 * byte. btoa(String.fromCharCode(0xCA)) returns 'w4o=' (the encoding of
 * C3 8A) where a browser returns 'yg=='. ASCII input is unaffected, which is
 * why the ASCII-only rung6 test never caught it. Fixing it needs the string's
 * CODE UNITS rather than its UTF-8 bytes: a separate change. atob() below is
 * the same family of bug and IS fixed (see tests/nb_atob_test.sh). */
static JSValue _nb_btoa(JSContext *ctx, JSValueConst this_val,int argc,JSValueConst *argv){ (void)this_val; if(argc<1)return JS_ThrowTypeError(ctx,"Failed to execute 'btoa'"); const char *str=JS_ToCString(ctx,argv[0]); if(!str)return JS_EXCEPTION; const char tbl[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; size_t len=strlen(str); size_t olen=((len+2)/3)*4; char *out=malloc(olen+1); if(!out){JS_FreeCString(ctx,str);return JS_EXCEPTION;} size_t i=0,j=0; const unsigned char *p=(const unsigned char*)str; while(i+2<len){ out[j++]=tbl[(p[i]>>2)&0x3F]; out[j++]=tbl[((p[i]&0x3)<<4)|((p[i+1]>>4)&0xF)]; out[j++]=tbl[((p[i+1]&0xF)<<2)|((p[i+2]>>6)&0x3)]; out[j++]=tbl[p[i+2]&0x3F]; i+=3; } if(i+1==len){ out[j++]=tbl[(p[i]>>2)&0x3F]; out[j++]=tbl[((p[i]&0x3)<<4)]; out[j++]='='; out[j++]='='; }else if(i<len){ out[j++]=tbl[(p[i]>>2)&0x3F]; out[j++]=tbl[((p[i]&0x3)<<4)|((p[i+1]>>4)&0xF)]; out[j++]=tbl[((p[i+1]&0xF)<<2)]; out[j++]='='; } out[j]=0; JS_FreeCString(ctx,str); JSValue r=JS_NewStringLen(ctx,out,j); free(out); return r; }
static JSValue _nb_perfnow(JSContext *ctx,JSValueConst this_val,int argc,JSValueConst *argv){ (void)this_val;(void)argc;(void)argv; struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return JS_NewFloat64(ctx, ts.tv_sec*1000.0 + ts.tv_nsec/1000000.0); }
static JSValue _nb_grv(JSContext *ctx,JSValueConst this_val,int argc,JSValueConst *argv){ (void)this_val; if(argc<1)return JS_ThrowTypeError(ctx,"Failed to execute 'getRandomValues'"); JSValue arr=argv[0]; JSValue lval = JS_GetPropertyStr(ctx, arr, "length"); int64_t lenv = 0; JS_ToInt64(ctx, &lenv, lval); JS_FreeValue(ctx, lval); uint32_t len = (uint32_t)lenv; if(len==0||len>65536)return JS_ThrowTypeError(ctx,"Invalid typed array length"); { unsigned char *buf=malloc(len?len:1); if(!buf)return JS_ThrowOutOfMemory(ctx); nb_rand_bytes(buf,len); for(uint32_t i=0;i<len;i++){ JS_SetPropertyUint32(ctx,arr,i,JS_NewInt32(ctx,buf[i])); } free(buf); } return JS_DupValue(ctx,arr); }

#define STB_IMAGE_IMPLEMENTATION
#include "../js/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

extern char **environ;

#define MAX_MSG (1024 * 1024)

/* ---- wire framing ---- */
static char g_rbuf[MAX_MSG];      /* command payload buffer */
static size_t g_rlen = 0;

static char g_page_js[4096];
static char g_fetch_dom[4096];
static char g_style_css[4096];   /* rung 7: LOAD-delivered stylesheet file */
static NbCss *g_css = NULL;      /* parsed rule cache, per LOAD */
static char g_href[4096];
static char g_title[512];

static int g_cli = 0;            /* argv mode: plain text out, no RPC framing */
static int g_cli_status_ok = 0;  /* CLI exit-status latch set by send_status */
static int g_nav_emit = 0;       /* rung-6 slice 2: daemon only (!g_cli, set */
static int g_module_page_loaded = 0;  /* set when a module page is loaded; worker exits after */
                                 /* per run_page) - NAV frames go to manager */

/* devtools console EVAL: the last LOAD's QuickJS context + DOM tree stay live
 * (g_live_ctx / g_dom_root) between LOAD and EVAL, so an eval:<js> snippet
 * runs against the real page context (document, window, globals, timer
 * bindings) and can mutate the DOM. Tear down only at the next LOAD or on
 * QUIT/error - see run_page() and live_teardown(). */
static JSContext *g_live_ctx = NULL;
static JSRuntime *g_live_rt  = NULL;

/* Worker wire frame sender extracted (Phase 1, single-TU include). */
#include "nb_worker_send_payload.c"

/* Worker diagnostic ERROR frame responder extracted (Phase 1, single-TU include). */
#include "nb_worker_send_worker_error.c"

/* Worker STATUS responder extracted (Phase 1, single-TU include). */
#include "nb_worker_send_status.c"

/* Read one length-prefixed payload from stdin into g_rbuf.
 * Returns 1 on success, 0 on EOF/shutdown. */
/* Monotonic deadline helper extracted (Phase 1, single-TU include). */
#include "nb_worker_ms_until.c"

/* Bounded frame read. budget_ms < 0 keeps the historical blocking
 * behaviour; >= 0 gives up once the budget is spent and returns 0.
 *
 * Why: recv_frame() used a bare blocking read(), so a driver that never
 * answered parked the worker in recv() FOREVER. The worker is a long-lived
 * resident process (see the "keep the resident browser worker alive through
 * long page loads" change), so one unanswered FETCH wedged it permanently
 * and SILENTLY - `make check` printed nothing and had to be killed. Callers
 * already treat 0 as "no frame" and fall back, so a bounded wait degrades
 * into the direct-curl path instead of hanging the house.
 */
/* Why recv_frame_budget() gave up. Needed because "the caller timed out"
 * and "the peer went away" were BOTH a bare 0, and the tick loop must
 * do opposite things with them: keep ticking versus shut down. */
#define NB_RECV_SHUTDOWN 0
#define NB_RECV_TIMEOUT  1

/* Frame-reader pushback byte helper extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_getc.c"

/* Bounded frame reader extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_frame_ex.c"

/* Frame reader convenience wrappers extracted (Phase 1, single-TU include). */
#include "nb_worker_recv_frame_wrappers.c"

/* LOAD frame field splitter extracted (Phase 1, single-TU include). */
#include "nb_worker_split_lines.c"

/* ==================== rung 2 DOM (worker side) ====================
 * The manager serializes the DOM to fetch.dom (nb_dom.h/.c); the worker
 * rebuilds the NbNode tree here and exposes it to JS via native QuickJS
 * accessors (plan §7 step 3, roadmap §2 minimum API). The JS side only
 * holds opaque pointer handles; the tree is C-side. No shared memory. */


/* Viewport width the page's box tree is computed against (G2.2). Defined
 * next to NB_CONTENT_PX further down, forward-declared here because
 * window.innerWidth is exported from the globals block. */
static double g_viewport_px;
static NbNode *g_active_node = NULL;  /* element.focus()/blur() tracker */
static int g_search_wired = 0;  /* document-level Enter search proxy wired per page */
static NbNode *g_orphans = NULL;   /* detached createElement() nodes still to free */
#define NODEKEY "_nbnode"

/* Node handle index helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_index.c"

/* Forward decls for the customElements / ShadowRoot / template-content
 * section (2026-09-20). Defined AFTER js_error_to_cstr so lifecycle hooks
 * reuse the g_trace_cb dump channel. */
static JSValue ce_define(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_when_def(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue ce_upgrade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_attachShadow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static NbNode *nb_template_content_frag(JSContext *ctx, JSValue el, NbNode *n);
static JSValue make_fragment_wrapper(JSContext *ctx, NbNode *frag, const char *nodeName);

/* Small string builder helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_sb.c"

/* Basic node tree helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_tree_helpers.c"

/* ---- text / html serialization ---- */
/* Node textContent helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_text_content.c"
/* HTML fragment serialization (innerHTML / outerHTML / XMLSerializer).
 * The old emitter wrote text verbatim, which is wrong for the three things
 * a page actually does with innerHTML:
 *   - ESCAPE text: '&' -> &amp;, '<' -> &lt;, '>' -> &gt;, U+00A0 -> &nbsp;.
 *     Verbatim text re-parses as markup or as an entity, so a round-trip
 *     (`el.innerHTML = el.innerHTML`) silently changed the tree.
 *   - Raw-text elements (script/style/xmp/iframe/noembed/noframes/
 *     plaintext/noscript) keep their data literally: the parser never
 *     treated it as markup, so escaping it would corrupt JS/CSS.
 *   - VOID elements have no end tag. Emitting `</br>` makes a round-trip
 *     add a second break, because the HTML parser reads `</br>` as `<br>`.
 * XML mode escapes the same three bytes but not U+00A0 (XML has no &nbsp;
 * entity) and keeps closing tags for void elements (XML is not HTML).
 * Attribute blobs are still emitted as stored: parser/load blobs are
 * already source markup, and setAttribute values are not re-escaped yet. */
/* HTML serializer tag/escape helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ser_escape_helpers.c"
/* Attribute escaping helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_attr_escaped.c"
/* Canonical attribute serialization helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_attrs_canonical.c"
/* Safe comment serialization helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sb_put_comment_data_safe.c"
/* Outer HTML serialization helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_outer_html.c"

/* ---- selector engine (subset: tag, #id, .class, tag.class, descendants) ---- */
/* Basic selector compound matching helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selector_compound.c"
/* Selector chain and query helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selector_query.c"
/* Basic DOM find helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_find_helpers.c"

static const char *img_get_src(const NbNode *n);
static void img_set_src(struct NbNode *n, const char *s);
static int layout_hidden_anc(struct NbNode *n);
static void layout_xy(struct NbNode *n, double *out_x, double *out_y);
static void img_set_decoded(NbNode *n, int w, int h, unsigned char *data);
static int img_get_decoded(const NbNode *n, int *w, int *h);
static unsigned char *b64_decode(const char *in, size_t *out_len);

/* Forward declarations for functions used in script src setter */
static void cookie_header_for_url(const char *url, char *out, size_t olen);
static void cfg_line(FILE *f, const char *key, const char *val);
static void resolve_doc_url(const char *rel, char *out, size_t olen);
static int peval_budget(JSContext *ctx, const char *src, size_t src_n, char *errbuf, size_t errbuf_sz);
static void nb_mutation_notify_childlist(JSContext *ctx, NbNode *target, NbNode *added, NbNode *removed, NbNode *prev, NbNode *next);
static JSValue nb_dom_createDocumentFragment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void nb_mutation_notify_attribute(JSContext *ctx, NbNode *target, const char *name, const char *old_value);
static void nb_mutation_notify_characterData(JSContext *ctx, NbNode *target, const char *old_value);
static void nb_obs_recheck(JSContext *ctx);
static NbNode *get_this(JSContext *ctx, JSValueConst this_val);
static char *attrs_set(const NbNode *n, const char *name, const char *val);

/* Image src storage helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_img_src.c"

/* Script src storage and execution helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_helpers.c"

/* Script src getter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_get.c"

/* Script src setter binding extracted (Phase 1, single-TU include). */
#include "nb_worker_script_src_set.c"
/* Decoded image cache helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_img_decoded.c"

/* ---- step 4: RENDER — serialize the (post-JS) DOM into page.state.txt
 * rows exactly as the manager's projector consumes them (TITLE/TEXT/LINK/IMG).
 * Only the worker's own tree is authoritative here, so JS mutations
 * (document.title, el.textContent, innerHTML=, appendChild) become visible. */
#define RWS_TEXT 88
#define RENDER_MAX 60000        /* keep the RENDER frame inside every buffer */
static void rw_row(SB *b, const char *key, const char *val) {
    if (b->len >= RENDER_MAX) return;
    size_t o = 0;
    char buf[4096];
    for (size_t i = 0; val && val[i] && o + 1 < sizeof(buf); i++) {
        unsigned char c = (unsigned char)val[i];
        if (c == '\r') continue;
        if (c == '\n' || c == '|') buf[o++] = ' ';
        else buf[o++] = (char)c;
    }
    buf[o] = 0;
    if (o) { sb_put(b, key); sb_put(b, "|"); sb_put(b, buf); sb_put(b, "\n"); }
}
static void rw_sel(SB *b, const NbNode *n);

/* Long text is split into several TEXT rows; each one gets its own SEL row so
 * every rendered row is independently clickable back to its element. */
static void rw_wrap(SB *b, char *s, const NbNode *n) {
    while (s && *s) {
        size_t L = strlen(s);
        if (L <= RWS_TEXT) { rw_sel(b, n); rw_row(b, "TEXT", s); break; }
        size_t cut = RWS_TEXT;
        while (cut > RWS_TEXT / 2 && s[cut] && s[cut] != ' ') cut--;
        if (s[cut] == ' ') { char save = s[cut]; s[cut] = 0; rw_sel(b, n); rw_row(b, "TEXT", s); s[cut] = save; s += cut + 1; }
        else { char save = s[RWS_TEXT]; s[RWS_TEXT] = 0; rw_sel(b, n); rw_row(b, "TEXT", s); s[RWS_TEXT] = save; s += RWS_TEXT; }
    }
}
static char *attrs_set(const NbNode *n, const char *name, const char *val);

/* Every row the walk emits is preceded by a SEL row naming a selector that
 * document.querySelector() can resolve back to the same element, so the
 * manager can turn a khtpm click on a rendered row into a real DOM EVENT.
 * Elements that already carry an id use it; the rest get a stable synthetic
 * id written into the raw attribute blob (nb_attr_get reads that blob, not
 * n->id, so setting n->id alone would not be findable). */
static int g_auto_id_seq;
static void rw_sel(SB *b, const NbNode *n) {
    if (!n) return;
    const char *id = nb_attr_get(n, "id");
    char auto_id[48];
    if (!id || !id[0]) {
        snprintf(auto_id, sizeof(auto_id), "nb-auto-%d", ++g_auto_id_seq);
        NbNode *w = (NbNode *)n;
        char *na = attrs_set(w, "id", auto_id);
        free(w->attrs);
        w->attrs = na;
        if (w->id) { free(w->id); }
        w->id = strdup(auto_id);
        id = auto_id;
    }
    char sel[96];
    snprintf(sel, sizeof(sel), "#%s", id);
    rw_row(b, "SEL", sel);
}

static void dom_walk_render(const NbNode *n, int *titled, SB *b, int no_text) {
    if (!n) return;
    const char *tg = n->tag;
    if (tg && !strcmp(tg, "#comment")) return;
    /* Raw-text/RCDATA bodies are data, not visible prose: script/style/
     * noscript/xmp/iframe/noembed/noframes/plaintext text now survives parsing
     * (JSON data islands, style rules, embedded markup strings) and must never
     * become a TEXT row. A <title> is handled below. */
    if (tg && (!strcasecmp(tg, "script") || !strcasecmp(tg, "style") || !strcasecmp(tg, "noscript")
            || !strcasecmp(tg, "xmp") || !strcasecmp(tg, "iframe") || !strcasecmp(tg, "noembed")
            || !strcasecmp(tg, "noframes") || !strcasecmp(tg, "plaintext")))
        return;
    /* REAL, NEW 2026-10-07 (live scroll-through: Wikipedia's own footer
     * farm rendered as article content) - prune chrome SUBTREES here,
     * structurally: nav/footer/aside landmarks + ARIA sectioning roles.
     * Same predicate family as the static extractor's tag_is_chrome()
     * (worker side of the lane split) - never text matching, so no site
     * can ever be hardcoded. Whole subtree unwalked. */
    if (tg) {
        if (!strcasecmp(tg, "nav") || !strcasecmp(tg, "footer") || !strcasecmp(tg, "aside"))
            return;
        const char *role = nb_attr_get(n, "role");
        if (role && (!strcasecmp(role, "navigation") || !strcasecmp(role, "banner") ||
                     !strcasecmp(role, "complementary") || !strcasecmp(role, "search") ||
                     !strcasecmp(role, "contentinfo")))
            return;
    }
    int caption_used = 0;   /* element's own text already surfaced as TITLE/LINK/IMG */
    if (tg && !strcasecmp(tg, "title") && !*titled) {
        SB t = {0, 0, 0};
        node_text_content(n, &t);
        rw_sel(b, n);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
        *titled = 1;
        caption_used = 1;
    } else if (tg && !strcasecmp(tg, "a")) {
        const char *href = nb_attr_get(n, "href");
        if (href && *href) {
            SB t = {0, 0, 0};
            node_text_content(n, &t);
            char linkbuf[2048];
            snprintf(linkbuf, sizeof(linkbuf), "%s|%s", href, t.s && t.s[0] ? t.s : href);
            rw_sel(b, n);
            rw_row(b, "LINK", linkbuf);
            free(t.s);
            caption_used = 1;
        }
    } else if (tg && !strcasecmp(tg, "img")) {
        char srcbuf[1024] = "", altbuf[1024] = "";
        snprintf(srcbuf, sizeof(srcbuf), "%s", img_get_src(n));
        snprintf(altbuf, sizeof(altbuf), "%s", nb_attr_get(n, "alt"));
        if (srcbuf[0]) {
            int dw = 0, dh = 0;
            if (!img_get_decoded(n, &dw, &dh) && srcbuf[0]) {
                if (strncmp(srcbuf, "data:image/", 11) == 0) {
                    const char *comma = strchr(srcbuf, ',');
                    if (comma && strstr(srcbuf, ";base64,")) {
                        size_t png_len = 0;
                        unsigned char *png_data = b64_decode(comma+1, &png_len);
                        if (png_data && png_len) {
                            int w = 0, h = 0, comp = 0;
                            unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &w, &h, &comp, 4);
                            if (rgba) { img_set_decoded((NbNode *)n, w, h, rgba); dw = w; dh = h; }
                            free(png_data);
                        }
                    }
                }
            }
            char imgbuf[2048];
            if (img_get_decoded(n, &dw, &dh) && dw > 0 && dh > 0) {
                // Write decoded RGBA to temp file for renderer (Step 3 wire)
                char imgpath[256];
                snprintf(imgpath, sizeof(imgpath), "/tmp/nb_img_%p.png", (void*)n);
                // Find decoded data
                unsigned char *rgba = NULL;
                for (int i = 0; i < g_img_decoded_count; i++) if (g_img_decoded[i].n == n) { rgba = g_img_decoded[i].data; break; }
                if (rgba) stbi_write_png(imgpath, dw, dh, 4, rgba, dw * 4);
                snprintf(imgbuf, sizeof(imgbuf), "%s|%d|%d|%s|%s", srcbuf, dw, dh, imgpath, altbuf);
            } else {
                snprintf(imgbuf, sizeof(imgbuf), "%s|%s", srcbuf, altbuf);
            }
            rw_sel(b, n);
            rw_row(b, "IMG", imgbuf);
            caption_used = 1;
        }
    }
    /* the fetch.dom wire form carries an element's direct text inline on
     * the element node, so surface n->text itself (skip captions above and
     * whitespace-only runs, matching the static extractor) */
    if (!no_text && !caption_used && n->text && n->text[0] && strspn(n->text, " \t\r\n") < strlen(n->text)) {
        size_t L = strlen(n->text);
        char *copy = malloc(L + 1);
        /* #text nodes have no selector of their own; a click on their row
         * belongs to the element that owns them, as events would bubble. */
        const NbNode *seln = (n->tag && n->tag[0]) ? n : (n->parent ? n->parent : n);
        if (copy) { memcpy(copy, n->text, L + 1); rw_wrap(b, copy, seln); free(copy); }
    }
    /* A caption row (TITLE/LINK/IMG) already carries this element's whole
     * textContent, so a #text child must not be emitted a second time as a
     * TEXT row. Only TEXT is silenced; nested elements still render. */
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        dom_walk_render(c, titled, b, no_text || caption_used);
}
/* REAL, NEW 2026-10-07 (maximal-scope directive: canvas fast) - software
 * rasterizer backing every canvas 2D context. Before this, every draw op
 * was a noop and pages rendered blank canvases; now fill/clear/blit ops
 * write real pixels readable back via getImageData. Deliberately partial:
 * full affine image/rect sampling, source-over alpha, save/restore;
 * text drawing stays noop, gradients/patterns are flat colors, shadow/
 * filter/composite ops ignored. Display plumbing (surface publish) is
 * the next slice - this one proves pixel truth through getImageData. */
#define NB_CANVAS_MAX 64
#define NB_CANVAS_PX_MAX (2048 * 2048)
typedef struct {
    int used, w, h;
    unsigned char *px;      /* RGBA, w*h*4, straight alpha */
    double m[6];            /* current transform a,b,c,d,e,f */
    double alpha;           /* snapshot cache; live prop re-read per draw */
    int dirty;              /* set by any mutating op, cleared on publish */
    int save_depth;
    double save_stack[8][7]; /* m[6] + alpha */
    char fillstyle[128];
} NbCanvas;
static NbCanvas g_canvases[NB_CANVAS_MAX];

static void nb_canvas_reset(void) {
    for (int i = 0; i < NB_CANVAS_MAX; i++) {
        free(g_canvases[i].px);
        g_canvases[i].px = NULL;
        g_canvases[i].used = 0;
    }
}

static void dom_render_rows(SB *b) {
    /* TITLE first, like the static extractor: from the <title> element if the
     * parser kept one, else document.title / the LOAD title (g_title). */
    NbNode *te = g_dom_root ? find_tag_first(g_dom_root, "title") : NULL;
    if (te) {
        SB t = {0, 0, 0};
        node_text_content(te, &t);
        rw_row(b, "TITLE", t.s ? t.s : "");
        free(t.s);
    } else if (g_title[0]) {
        rw_row(b, "TITLE", g_title);
    }
    int titled = 1;                 /* <title> already handled above */
    dom_walk_render(g_dom_root, &titled, b, 0);
    /* Canvas display publish (2026-10-07): dirty backing stores become
     * PNGs + 5-field IMG rows; the manager's own MEDIA->sprite pipeline
     * (IMG without nb_sprites_ → sprite) tiles them with zero new
     * machinery. Clears dirty so quiet canvases don't re-emit. */
    for (int i = 0; i < NB_CANVAS_MAX; i++) {
        NbCanvas *cv = &g_canvases[i];
        if (!cv->used || !cv->dirty || !cv->px) continue;
        char path[128];
        snprintf(path, sizeof(path), "/tmp/nb_canvas_%d.png", i);
        if (!stbi_write_png(path, cv->w, cv->h, 4, cv->px, cv->w * 4)) continue;
        char row[512];
        snprintf(row, sizeof(row), "file://%s|%d|%d|%s|canvas", path, cv->w, cv->h, path);
        /* NOTE: rw_row() maps '|' to space (wire escaping for TEXT/LINK
         * rows), which would collapse this 5-field row - emit raw. */
        if (b->len < RENDER_MAX) {
            sb_put(b, "IMG|");
            sb_put(b, row);
            sb_put(b, "\n");
        }
        cv->dirty = 0;
    }
}

/* ---- JS <-> C node binding ---- */
static NbNode *get_node(JSContext *ctx, JSValueConst v) {
    if (!JS_IsObject(v)) return NULL;
    JSValue k = JS_GetPropertyStr(ctx, v, NODEKEY);
    int i = -1;
    if (JS_IsNumber(k)) {
        int32_t iv;
        if (JS_ToInt32(ctx, &iv, k) == 0) i = (int)iv;
    }
    JS_FreeValue(ctx, k);
    if (i < 0 || i >= g_nodecount) return NULL;
    return g_nodeindex[i];
}
/* Node from the `this` binding of an element native (QuickJS passes the
 * this-binding in the native's JSValueConst this_val argument). */
static NbNode *get_this(JSContext *ctx, JSValueConst this_val) {
    return get_node(ctx, this_val);
}
/* String arg i as own C string (only for string-typed args — the call sites
 * previously used duk_get_string + a "" fallback), or NULL when absent. */
static const char *js_arg_str(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc || !JS_IsString(argv[i])) return NULL;
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned;
}
/* String arg i coercing any value via toString (duk_safe_to_string), "" when
 * absent. */
static const char *js_arg_str_any(JSContext *ctx, JSValueConst *argv, int argc, int i, char **owned) {
    *owned = NULL;
    if (i >= argc) return "";
    *owned = JS_ToCString(ctx, argv[i]);
    return *owned ? *owned : "";
}
/* Numeric arg i (numbers only), or -1 when absent/non-number. */
static int js_arg_index(JSContext *ctx, JSValueConst *argv, int argc, int i) {
    if (i >= argc || !JS_IsNumber(argv[i])) return -1;
    int32_t iv;
    if (JS_ToInt32(ctx, &iv, argv[i]) != 0) return -1;
    return (int)iv;
}
/* el.on<name> accessor names (also parsed from property arg0 of the natives) */
static const char *const ONPROPS[] = {
    "click", "dblclick", "change", "input", "submit", "keydown", "keyup",
    "keypress", "mouseover", "mouseout", "mouseenter", "mouseleave",
    "mousedown", "mouseup", "mousemove", "focus", "blur", "load", "error",
    "resize", "scroll", "contextmenu", NULL
};

/* ---- rung 7 slice 1: CSS/layout-intent ---------------------------------
 * The page's stylesheet text (inline <style> + linked CSS, manager-side)
 * reaches the worker via the LOAD style file; g_css holds the parsed rule
 * cache for this page. We model exactly: display: none, visibility:
 * hidden/collapse, opacity, and px width/height — enough for
 * getComputedStyle, display:none-driven hiding, and the offset/rect
 * family to behave on real widgets. There is no real layout engine: x/y
 * are always 0, sizes come from the CSS declaration or 0. */

#define STYK "_nbsty"   /* per-wrapper style-snapshot cache key */

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static int css_hidden(const NbNode *n);

static int css_hidden(const NbNode *n) {
    /* nb_css_resolve tolerates a NULL rule cache (a page with no stylesheet),
     * and inline display:none still has to count there: without this, the
     * probe's no-CSS fixtures reported a hidden box at its content size. */
    return nb_css_hidden(g_css, n);
}
static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* Return (as JSValue) an object with the modeled computed style of `n` plus a
 * getPropertyValue(). Missing display => "" (visible), opacity => "1". */
static JSValue push_computed_style(JSContext *ctx, NbNode *n) {
    JSValue o = JS_NewObject(ctx);
    NbCssStyle st;
    memset(&st, 0, sizeof(st));
    if (n) nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    JS_SetPropertyStr(ctx, o, "display", JS_NewString(ctx, st.display[0] ? st.display : ""));
    JS_SetPropertyStr(ctx, o, "visibility", JS_NewString(ctx, st.visibility));
    JS_SetPropertyStr(ctx, o, "opacity", JS_NewString(ctx, st.opacity[0] ? st.opacity : "1"));
    JS_SetPropertyStr(ctx, o, "width", JS_NewFloat64(ctx, st.width));
    JS_SetPropertyStr(ctx, o, "height", JS_NewFloat64(ctx, st.height));
    JS_SetPropertyStr(ctx, o, "font-family", JS_NewString(ctx, st.font_family[0] ? st.font_family : ""));
    /* font-size is read as a CSS string (kevlar: fontSize.replace('px','')) */
    JS_SetPropertyStr(ctx, o, "fontSize", JS_NewString(ctx, "16px"));
    JS_SetPropertyStr(ctx, o, "getPropertyValue",
                      JS_NewCFunction(ctx, nb_css_getprop, "getPropertyValue", 1));
    return o;
}

static JSValue nb_css_getprop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!name) return JS_NewString(ctx, "");
    JSValue v = JS_GetPropertyStr(ctx, this_val, name);
    JS_FreeCString(ctx, name);
    return JS_ToString(ctx, v);
}

/* window.getComputedStyle(el) — registered globally as `__nb_ges` by the
 * resident worker (overriding install_host's minimal default); the host
 * prelude's getComputedStyle delegates here. */
static JSValue nb_ges_rich(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_node(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    return push_computed_style(ctx, n);
}

/* el.style — identity-cached snapshot of the inline style attribute.
 * Reads mirror the declared inline props; JS writes persist on the
 * snapshot (browser-like authoring) but — no layout engine — do not feed
 * the metrics below. Non-enumerable cache prop keeps it per-wrapper. */
static JSValue nb_el_style_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue cached = JS_GetPropertyStr(ctx, this_val, STYK);
    if (JS_IsObject(cached)) return cached;         /* identity-cached snapshot */
    JS_FreeValue(ctx, cached);
    JSValue o = JS_NewObject(ctx);                  /* style snapshot */
    NbNode *n = get_this(ctx, this_val);
    if (n) {
        const char *attr = nb_attr_get(n, "style");
        if (attr && *attr) {
            const char *p = attr;
            while (*p) {
                while (*p && (*p == ';' || *p == ' ' || *p == '\t' || *p == '\n')) p++;
                if (!*p) break;
                const char *cs = p;
                while (*p && *p != ';') p++;
                const char *colon = memchr(cs, ':', (size_t)(p - cs));
                if (colon && p > cs) {
                    size_t kl = (size_t)(colon - cs);
                    while (kl && (cs[kl - 1] == ' ' || cs[kl - 1] == '\t')) kl--;
                    if (kl > 0 && kl < 48) {
                        char kbuf[48];
                        for (size_t i = 0; i < kl; i++)
                            kbuf[i] = (char)tolower((unsigned char)cs[i]);
                        kbuf[kl] = 0;
                        const char *vs = colon + 1;
                        size_t vn = (size_t)(p - vs);
                        while (vn && (vs[vn - 1] == ' ' || vs[vn - 1] == '\t' ||
                                      vs[vn - 1] == '\n')) vn--;
                        char vbuf[96];
                        size_t vv = vn < sizeof(vbuf) - 1 ? vn : sizeof(vbuf) - 1;
                        memcpy(vbuf, vs, vv);
                        vbuf[vv] = 0;
                        JS_SetPropertyStr(ctx, o, kbuf, JS_NewString(ctx, vbuf));
                    }
                }
            }
        }
    }
    JS_SetPropertyStr(ctx, this_val, STYK, JS_DupValue(ctx, o));
    return o;
}

/* offsetWidth/offsetHeight/clientWidth/clientHeight — magic 0/1 for the
 * offset pair, 2/3 for the client pair (client dims special-case the root
 * scrolling element, whose client box is the viewport, not a CSS width).
 * Hidden elements measure 0 (browser display:none semantics); visible
 * elements report the modeled CSS px size, else 0 (no layout engine). */
static double nb_client_dim(NbNode *n, int which);
static JSValue nb_el_scroll_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_scroll_dim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_offdim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    NbNode *n = get_this(ctx, this_val);
    if (magic >= 2) return JS_NewFloat64(ctx, nb_client_dim(n, magic & 1));
    int which = magic;
    double v = 0;
    if (n && !css_hidden(n)) {
        NbCssStyle st;
        nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
        v = which ? st.height : st.width;
    }
    return JS_NewFloat64(ctx, v);
}

static JSValue nb_el_offset_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || css_hidden(n)) return JS_NULL;
    NbNode *p = n->parent;
    while (p && !p->tag) p = p->parent;   /* climb past #document */
    if (!p) return JS_NULL;
    return push_node(ctx, p);
}

/* Phase 3 slice 2: simple block layout for getBoundingClientRect.
 * x is 0 for now (no inline flow), y is stacked: parent y + sum of
 * previous siblings' CSS heights. Hidden ancestors (display:none) give 0,0,0,0. */
static int layout_hidden_anc(NbNode *n) {
    for (NbNode *a = n; a; a = a->parent) if (css_hidden(a)) return 1;
    return 0;
}
/* Text metrics for unstyled content. The khtpm renderer draws 12px text on a
 * 14px baseline, so a line box is 14px tall and an average glyph advances
 * ~7px; content wraps at the renderer's 640px column. */
#define NB_FONT_PX     12
#define NB_LINE_PX     14
/* ROW height, which is NOT the line height (2026-10-14, G2.2).
 *
 * Measured off the renderer's own element dump on a 6-paragraph fixture:
 * every content row is painted at a CONSTANT height of 30, and y steps
 * 180, 210, 240, 270, 300, 330 - exactly +30 each. The worker's own
 * NB_LINE_PX (14, "12px text on a 14px baseline") is a LINE box: text
 * leading with no row padding around it. Using 14 for stack height made
 * the worker's y-step disagree with the renderer's by more than 2x, which
 * is the whole of G2.2's scale disagreement - two constants that meant
 * different words, not a disagreement about layout.
 *
 * So block stacking uses NB_ROW_PX. NB_LINE_PX stays where it belongs:
 * inside the wrap calculation, as the per-line text box. */
#define NB_ROW_PX     30
#define NB_CHAR_PX     7
#define NB_CONTENT_PX  640

/* VIEWPORT width, runtime (2026-10-14, G2.2).
 *
 * The worker's box tree assumed a 640px content column. Measured against
 * the renderer's own element dump the pane is 389px, so every width the
 * worker reports is wrong by ~250px - and no amount of box-model work
 * fixes it: the worker simply does not know how wide the pane it lands in
 * will be. This makes that width a VALUE rather than a compile-time
 * constant, so it can be supplied per page and seen by the page itself.
 *
 * Defaults to NB_CONTENT_PX, so behaviour is unchanged until something
 * supplies a real number - which is the point: the guess is now visible
 * and replaceable instead of baked in. */
static double g_viewport_px = NB_CONTENT_PX;

/* The scrollbar gutter (G2.2, 2026-10-15).
 *
 * The renderer paints a VISIBLE scrollbar for any scrolllist that OVERFLOWS
 * its viewport - track + thumb + arrow navs - and reserves GENERIC_SCROLLBAR_W
 * (8) of the column for it (khtpm_core_render.c:5383). Measured live on the
 * G2.2 fixture: the renderer publishes content_w=389 and the painted content
 * rows are 381. So without this the box tree reports every scrolling row 8px
 * wider than the renderer actually draws it, which is precisely the kind of
 * confident wrong number G2 exists to remove.
 *
 * The deduction is CONDITIONAL: a page that fits has no scrollbar and keeps
 * the full width. That means the decision needs the viewport HEIGHT, which
 * LOAD did not carry - it sent width only (field 6). Field 7 now carries
 * content_h. An absent or unusable height leaves the gutter OFF, because
 * guessing "it probably scrolls" would be the same class of lie in the
 * opposite direction.
 *
 * There is a genuine cycle here: wrapping (nb_content_h) depends on the
 * column width, and the column width depends on whether the wrapped height
 * overflows. It is broken by deciding ONCE per page - measure the document
 * height at the FULL width, cache the verdict - with g_measuring making that
 * inner measurement ignore the gutter. Deciding per call would make the
 * answer depend on which element happened to be measured first. */
#define NB_SCROLLBAR_W 8
static double g_viewport_h = 0.0;
static int    g_gutter_state = -1;   /* -1 undecided this page, 0 none, 1 present */
static int    g_measuring = 0;
/* Page-driven scroll offset (2026-11-09). The renderer owns the native pane
 * scroll and exposes no channel for it, so the worker tracks the offset the
 * PAGE drives: window.scrollTo/scrollBy, element.scrollIntoView. Rects are
 * viewport-relative (document y minus this offset), and changing the offset
 * re-evaluates IntersectionObserver targets. A page script cannot scroll
 * the native pane; its ISV and IO math see a consistent within-worker world. */
static double g_scroll_x = 0.0;
static double g_scroll_y = 0.0;

static double nb_content_h(NbNode *n);
/* 6.2/6.3 (2026-10-19): both are called from functions defined ABOVE their
 * own definitions (nb_box_w -> nb_edge_pad_h, nb_content_h -> nb_outer_h),
 * so forward-declare rather than reorder the file. */
static double nb_outer_h(NbNode *n);
static double nb_edge_pad_h(NbNode *n);

/* Total height of the document `from` belongs to. Walks to the topmost
 * ancestor, then sums its children - the same shape nb_content_h uses, so the
 * measurement is the one the box tree would report anyway. */
static double nb_doc_root_h(NbNode *from) {
    if (!from) return 0.0;
    NbNode *top = from;
    while (top->parent) top = top->parent;
    double sum = 0.0;
    for (NbNode *c = top->first_child; c; c = c->next_sibling) sum += nb_content_h(c);
    return sum;
}

/* The usable content column: the pane width, minus the scrollbar when the
 * document actually overflows it. */
static double g_col_px(NbNode *from) {
    if (g_measuring) return g_viewport_px;
    if (g_gutter_state < 0) {
        if (g_viewport_h <= 0.0 || !from) {
            g_gutter_state = 0;              /* no measurement -> no guess */
        } else {
            g_measuring = 1;
            double doc_h = nb_doc_root_h(from);
            g_measuring = 0;
            g_gutter_state = (doc_h > g_viewport_h) ? 1 : 0;
        }
    }
    return g_gutter_state ? (g_viewport_px - NB_SCROLLBAR_W) : g_viewport_px;
}

/* The chrome the renderer puts ABOVE the first content row inside the
 * scrolled column. Both terms are measured off the renderer, not guessed:
 *
 *   - GENERIC_SBAR_ARROW_H (14, khtpm_core_render.c:5428). layout_scroll_region
 *     sets `content_y = y + (have_arrows ? GENERIC_SBAR_ARROW_H : 0)`, so a
 *     column that scrolls starts its first row 14px down, under the scrollbar's
 *     up-arrow. Only when it scrolls - have_arrows requires max_scroll > 0.
 *   - NB_ROW_PX (30) for the page-title row. The manager projects <head><title>
 *     into a `page-title` row as the FIRST child of the content scrolllist
 *     (network_browser_manager.c:5570), so it occupies a real row before any
 *     <p>. A DOM-only model cannot see it: <title> lives in <head> and takes no
 *     space in a browser's body, which is exactly why this offset was 44px
 *     (14 + 30) and looked like a mystery.
 *
 * This is the worker mirroring the house renderer on purpose. It is NOT
 * claiming <title> takes body space in some other browser - it is saying the
 * box tree must describe the pixels this renderer actually draws, which is
 * what G2.2 measures. */
#define NB_SBAR_ARROW_H 14
static char g_page_title[256];

static double g_content_inset_top(void) {
    double inset = g_gutter_state ? (double)NB_SBAR_ARROW_H : 0.0;
    if (g_page_title[0]) inset += (double)NB_ROW_PX;
    return inset;
}

/* WIDTH of an element's border box - the half of the box tree that was
 * missing (G2.1, slice 1, 2026-10-14).
 *
 * Before this, getBoundingClientRect() reported `w = st.width`, the
 * DECLARED CSS width and nothing else, so every unstyled element on every
 * page reported 0. Measured on the G2-a fixture: box_d1=w0/h+, box_p1=w0
 * /h+, box_s1=w0/h+, box_p2=w0/h+ - heights were already right (nb_content_h
 * below), only widths were absent. A browser that reports width 0 for
 * ordinary markup is not reporting geometry, it is reporting an absence.
 *
 * Two rules, which is what block layout actually does:
 *
 *   - A BLOCK box fills its containing block. Declared width wins; else
 *     the parent's own box; the document root is the content column.
 *   - An INLINE box shrinks to fit its text - it does NOT stretch to the
 *     column, and reporting 640px for a three-word <span> would be its own
 *     kind of lie.
 *
 * Deliberately NOT here yet: padding/border/margin in the width sum, and
 * inline-vs-block detection by tag beyond the common set. Those are
 * slice-2 work and pretending otherwise is how a box tree starts lying.
 */
static int layout_is_block_tag(const char *t) {
    if (!t) return 1;
    static const char *inline_tags[] = {
        "a", "b", "em", "i", "strong", "span", "code", "small", "u",
        "label", "sub", "sup", "abbr", "cite", "q", "kbd", "samp", NULL
    };
    for (int i = 0; inline_tags[i]; i++)
        if (!strcasecmp(t, inline_tags[i])) return 0;
    return 1;
}

static double nb_box_w(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    if (st.has_width && st.width > 0) return st.width;

    /* 6.3 (2026-10-19) - a FLEX ROW's child takes a SHARE of the row, not
     * the whole content column. Without this every child of the test's
     * `#fx` would report w=389 (the full column), so three "columns" would
     * total 1167px inside a 389px span and clause 4 of nb_flex_test.sh
     * would fail - the widths, not the positions, are what make it a row.
     *
     * Equal share when no child declares flex-grow; otherwise weighted by
     * flex-grow, which is what the renderer's css_layout_pass() already
     * does (grow_total/fixed_total). The test's three children carry no
     * CSS beyond display:flex, so they take the equal-share path.
     *
     * Only block-level children are sized here; an inline child keeps its
     * shrink-to-fit behaviour below (a <span> in a flex row still hugs its
     * text), matching how the renderer treats inline content. */
    if (n->parent && n->parent->tag) {
        NbCssStyle pst;
        nb_css_resolve(g_css, n->parent, nb_attr_get(n->parent, "style"), &pst);
        if (pst.has_display_flex && pst.display_flex == 2) {
            /* simple grid: distribute across grid_cols */
            int cols = pst.has_grid_template_columns && pst.grid_cols > 0 ? pst.grid_cols : 1;
            if (cols < 1) cols = 1;
            double row_w = nb_box_w(n->parent);
            if (row_w > 0) {
                /* count visible children to place */
                int idx = 0;
                for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
                    if (layout_hidden_anc(s)) continue;
                    idx++;
                }
                int col = idx % cols;
                /* compute column width */
                double col_w = row_w / (double)cols;
                if (pst.has_grid_template_columns) {
                    /* rough equal split */
                    col_w = row_w / (double)cols;
                }
                return col_w;
            }
        }
        if (pst.has_display_flex && pst.display_flex == 1 &&
            !(pst.has_flex_direction && !pst.flex_row) &&  /* default is ROW */
            layout_is_block_tag(n->tag)) {
            double row_w = nb_box_w(n->parent);
            if (row_w > 0) {
                int nvis = 0;
                double grow_total = 0.0, grow_me = 0.0;
                for (NbNode *s = n->parent->first_child; s; s = s->next_sibling) {
                    if (layout_hidden_anc(s)) continue;
                    nvis++;
                    NbCssStyle sst;
                    nb_css_resolve(g_css, s, nb_attr_get(s, "style"), &sst);
                    if (sst.has_flex_grow && sst.flex_grow > 0) grow_total += sst.flex_grow;
                    if (s == n && sst.has_flex_grow && sst.flex_grow > 0) grow_me = sst.flex_grow;
                }
                if (nvis < 1) nvis = 1;
                if (grow_total > 0.0 && grow_me > 0.0)
                    return row_w * (grow_me / grow_total);
                if (grow_total <= 0.0)
                    return row_w / (double)nvis;   /* equal share */
                /* mixed: a grow child is handled above; a fixed child with
                 * no grow falls through to shrink-to-text/column below. */
            }
        }
    }

    if (!layout_is_block_tag(n->tag)) {
        /* shrink to text, clamped to the column so a long span still has
         * SOME width rather than none */
        double text_w = (n->text && n->text[0])
                        ? (double)strlen(n->text) * (double)NB_CHAR_PX : 0.0;
        double avail = nb_box_w(n->parent);
        if (avail <= 0) avail = g_col_px(n);
        return text_w < avail ? text_w : avail;
    }
    if (n->parent) {
        double avail = nb_box_w(n->parent);
        if (avail > 0) return avail;
    }
    return g_col_px(n);            /* the document's usable content column */
}

/* Height of an element's content box. A declared CSS height wins (that is what
 * the offset/client metrics report). Otherwise a block sums its children and a
 * text node gets one line per full column of characters, so unstyled paragraphs
 * stack the way they do in a browser instead of all reporting 0. */
static double nb_content_h(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    if (st.has_height && st.height > 0) return st.height;
    /* 6.3 (2026-10-19) - a flex ROW's height is the MAX of its children's,
     * not their sum. Summing is correct for a stacked column (that is what
     * this did), but a row's children all occupy the SAME vertical band, so
     * summing would make the container three times too tall and push
     * everything after it down. This mirrors the renderer's flex path,
     * where the cross-axis size is the largest child, not the total. */
    int flex_row = st.has_display_flex && st.display_flex &&
                   !(st.has_flex_direction && !st.flex_row);  /* default is ROW */
    if (flex_row) {
        double mx = 0;
        for (NbNode *c = n->first_child; c; c = c->next_sibling) {
            double ch = nb_outer_h(c);
            if (ch > mx) mx = ch;
        }
        return mx;
    }
    double sum = 0;
    for (NbNode *c = n->first_child; c; c = c->next_sibling) sum += nb_content_h(c);
    if (sum > 0) return sum;
    if (!n->text || !n->text[0]) return 0;
    int len = (int)strlen(n->text);
    /* wrap width follows the USABLE column (pane minus the scrollbar gutter
     * when it scrolls), or the text wraps at a column the pane does not have
     * and every line count downstream is wrong. */
    double col = g_col_px(n);
    if (col <= 0) col = (double)NB_CONTENT_PX;
    int per_line = (int)(col / NB_CHAR_PX);
    if (per_line < 1) per_line = 1;
    int lines = (len + per_line - 1) / per_line;
    if (lines < 1) lines = 1;
    /* Each wrapped line projects as its OWN row, so height is rows x row
     * height - not lines x line height. */
    return (double)lines * NB_ROW_PX;
}

/* Margin + padding of one edge, resolved from the element's own style. */
static double nb_edge_pad(NbNode *n, int top) {
    if (!n) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    double p = top ? (st.has_pad_top ? st.pad_top : 0)
                   : (st.has_pad_bottom ? st.pad_bottom : 0);
    double m = top ? (st.has_mar_top ? st.mar_top : 0)
                   : (st.has_mar_bottom ? st.mar_bottom : 0);
    return p + m;
}

/* Horizontal counterpart of nb_edge_pad: left+right padding/margin, the
 * amount that displaces the NEXT sibling along a flex row's main axis.
 * 6.2/6.3 (2026-10-19). */
static double nb_edge_pad_h(NbNode *n) {
    if (!n) return 0;
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    double p = (st.has_pad_top ? st.pad_top : 0); /* only a top value is modelled */
    double m = (st.has_mar_top ? st.mar_top : 0);
    return p + m;
}

/* Outer height of a node: its content plus its own vertical margin, which
 * is what actually displaces the NEXT sibling. Without this a paragraph
 * with margin-bottom 20px would be followed by the next one immediately,
 * which is the "stacked but cramped" look you get when margins are
 * dropped on the floor. */
static double nb_outer_h(NbNode *n) {
    if (!n || layout_hidden_anc(n)) return 0;
    return nb_content_h(n) + nb_edge_pad(n, 0) + nb_edge_pad(n, 1);
}

static void layout_xy(NbNode *n, double *out_x, double *out_y) {
    if (!n || layout_hidden_anc(n)) { *out_x = 0; *out_y = 0; return; }
    NbCssStyle st;
    nb_css_resolve(g_css, n, nb_attr_get(n, "style"), &st);
    /* position:absolute or position:fixed handling - positioned elements
     * are taken out of normal flow and positioned via top/left. */
    if (st.has_position && (st.position_absolute || st.position_fixed)) {
        double x = st.has_left ? st.left : 0;
        double y = st.has_top ? st.top : 0;
        if (st.position_fixed) {
            /* fixed: relative to viewport (window), not parent. Viewport
             * origin is (0,0) at window top-left. g_viewport_px is width. */
            *out_x = x;
            *out_y = y;
            return;
        }
        /* absolute: relative to nearest positioned ancestor (one with
         * has_position). relative/absolute/fixed all create positioning
         * contexts. If none, use viewport. */
        NbNode *anc = n->parent;
        while (anc && anc->tag) {
            NbCssStyle ast;
            nb_css_resolve(g_css, anc, nb_attr_get(anc, "style"), &ast);
            if (ast.has_position) {
                layout_xy(anc, out_x, out_y);
                *out_x += x;
                *out_y += y;
                return;
            }
            anc = anc->parent;
        }
        /* no positioned ancestor: fall back to viewport origin */
        *out_x = x;
        *out_y = y;
        return;
    }

    /* normal flow layout - original code continues below */
    double x = 0, y = 0;
    if (n->parent && n->parent->tag) {
        double px, py;
        layout_xy(n->parent, &px, &py);
        y += py;
        x += px;   /* 6.2: the parent ORIGIN, both axes */
        /* Inside the parent's content box: its padding-top, then this
         * node's own margin-top. G2.1's "strictly increasing y per depth"
         * depends on exactly this - without it a first child sits at the
         * same y as its parent and no amount of walking produces depth. */
        y += nb_edge_pad(n->parent, 1);
        y += nb_edge_pad(n, 1);
    }
    /* 6.2/6.3 (2026-10-19) - THE HORIZONTAL AXIS. Before this, the walk
     * below accumulated heights unconditionally and finished `*out_x = 0`,
     * so a flex row's children all reported x=0 and only the renderer (once
     * it consumed display:flex) would have disagreed. That is the G2.2
     * re-open nb_flex_test.sh warns about.
     *
     * When the parent is a flex ROW, the main axis is horizontal: siblings
     * accumulate along x and every child shares the parent's content-box
     * top (NOT a stacked y). When it is not, behaviour is exactly as before
     * - heights accumulate, x stays at the parent origin - so no existing
     * page moves by a pixel. `flex-direction` defaults to ROW in CSS, so a
     * bare `display:flex` IS a row; only an explicit
     * `flex-direction:column` stacks. (Getting this backwards is how the
     * first cut kept the test red - see the note on parent_is_row below.)
     *
     * The child's own horizontal share is nb_box_w()'s job (6.3), not this
     * function's - this walk only places origins. */
    NbCssStyle pst;
    int parent_is_row = 0;
    if (n->parent && n->parent->tag) {
        nb_css_resolve(g_css, n->parent, nb_attr_get(n->parent, "style"), &pst);
        if (pst.has_display_flex && pst.display_flex == 2) {
            /* grid layout */
            int cols = pst.has_grid_template_columns && pst.grid_cols > 0 ? pst.grid_cols : 1;
            if (cols < 1) cols = 1;
            int idx = 0;
            for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
                if (layout_hidden_anc(s)) continue;
                idx++;
            }
            int col = idx % cols;
            double row_w = nb_box_w(n->parent);
            if (row_w <= 0) row_w = g_col_px(n->parent);
            double gap = 0;
            double col_w = row_w / (double)cols;
            if (cols > 1) col_w = (row_w - gap * (cols - 1)) / (double)cols;
            if (col_w < 0) col_w = row_w / (double)cols;
            x += col * (col_w + gap);
            int row_idx = idx / cols;
            y += row_idx * 30;
            *out_x = x;
            *out_y = y;
            return;
        }
        /* flex-direction DEFAULTS TO ROW, per CSS. Only an explicit
         * `flex-direction:column` turns the main axis vertical. */
        int dir_col = pst.has_flex_direction && !pst.flex_row;
        parent_is_row = pst.has_display_flex && pst.display_flex == 1 && !dir_col;
    }
    if (parent_is_row) {
        for (NbNode *s = n->parent->first_child; s && s != n; s = s->next_sibling) {
            if (layout_hidden_anc(s)) continue;
            x += nb_box_w(s) + nb_edge_pad_h(s);
        }
        *out_x = x;
        *out_y = y;
        return;
    }
    for (NbNode *s = n->parent ? n->parent->first_child : NULL; s && s != n; s = s->next_sibling) {
        if (layout_hidden_anc(s)) continue;
        y += nb_outer_h(s);
    }
    *out_x = x; *out_y = y;
}

static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* DOMRect.toJSON helper extracted (Phase 1, single-TU include). */
#include "nb_worker_rect_tojson.c"

/* ---- IntersectionObserver (2026-11-08, extended 2026-11-09) ----
 * Real initial entries from the worker's box geometry, delivered as a
 * queued microtask and batched per task. DOM-driven geometry changes
 * (childList / attribute edits, textContent / innerHTML) re-evaluate
 * observed targets and deliver change entries. It intentionally does not
 * claim scroll delivery: the worker knows the LOAD viewport, not a live
 * pane-scroll channel. Thresholds are not yet modelled; a change entry
 * fires when isIntersecting or the ratio changes. */
#define NB_INT_MAX_OBS 32
/* IntersectionObserver helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_intersection_observer.c"
/* ResizeObserver helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_resize_observer.c"
/* ---- matchMedia (2026-11-09) ----
 * The prelude used to define matchMedia as a stub whose .matches was the
 * constant false. Every media query therefore took its "no" branch: a
 * 784px window still claimed to be narrower than nothing, and the mobile
 * detection in real bundles (window.matchMedia("(pointer:coarse)").matches)
 * silently answered false. This is a real evaluator over the viewport the
 * manager supplies on LOAD (fields 6 and 7) - the same g_viewport_px /
 * g_viewport_h that window.innerWidth / innerHeight report.
 *
 * Scope, stated honestly: .matches is recomputed on every read, so polling
 * code is correct. Change events are NOT delivered - the worker has no live
 * pane-resize channel (LOAD carries one viewport and the worker lives in it
 * until the next navigation), so a listener waiting for a change waits
 * forever. The listener surface itself is real (add/remove/dispatch), which
 * is what feature detection and framework wrappers touch.
 *
 * Unknown media features answer false: this worker refuses to claim a
 * capability the renderer does not have. (hover: hover) and (pointer: fine)
 * are true because this IS a mouse-driven window; (prefers-color-scheme:
 * light) is true because there is no OS preference channel to consult and
 * light is the platform default we can defend, not a guess about dark.
 */
/* matchMedia helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_match_media.c"
/* getBoundingClientRect helper extracted (Phase 1, single-TU include). */
#include "nb_worker_gbcr.c"

static JSValue push_node(JSContext *ctx, NbNode *n);
static JSValue nb_img_src_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_src_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_img_naturalWidth_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_naturalHeight_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_img_complete_get(JSContext *ctx, JSValueConst this_val);
static JSValue nb_image_ctor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_fetch_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static int dispatch_event(JSContext *ctx, int kind, NbNode *node, JSValue ev, int bubbles);
static JSValue peval_budget_value(JSContext *ctx, const char *src, size_t src_n);
static const char *img_get_src(const NbNode *n);
static void img_set_src(NbNode *n, const char *s);
static int layout_hidden_anc(NbNode *n);
static void layout_xy(NbNode *n, double *out_x, double *out_y);
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* canvas 2D natives defined with the other DOM natives (getBoundingClientRect
 * already ships on every wrapper via the existing definition above) */
static JSValue nb_el_getContext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_canvas_toDataURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

static JSValue get_global_attr(JSContext *ctx, const char *name);

/* NodeList/HTMLCollection helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_collections.c"

static JSValue nb_doc_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_doc_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_doc_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_dom_createEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* ---- document natives ---- */
static JSValue nb_dom_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *id = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!id || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = find_by_id(c, id);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static void collect_tag_into(JSContext *ctx, NbNode *n, const char *tag, JSValue arr, int *i) {
    if (!n) return;
    if (!tag || !*tag || !strcmp(tag, "*") || (n->tag && !strcasecmp(n->tag, tag))) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        collect_tag_into(ctx, (NbNode *)c, tag, arr, i);
}
static JSValue nb_dom_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *tag = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            collect_tag_into(ctx, (NbNode *)c, tag, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "HTMLCollection");
    return arr;
}
static void qsa_into(JSContext *ctx, NbNode *n, const char *sel, JSValue arr, int *i) {
    if (!n) return;
    if (match_any_selector(n, sel)) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)(*i)++, push_node(ctx, n));
    }
    for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        qsa_into(ctx, (NbNode *)c, sel, arr, i);
}
static JSValue nb_dom_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!sel || !g_dom_root) { JS_FreeCString(ctx, owned); return JS_NULL; }
    for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
        NbNode *r = query_first((NbNode *)c, sel);
        if (r) { JS_FreeCString(ctx, owned); return push_node(ctx, r); }
    }
    JS_FreeCString(ctx, owned);
    return JS_NULL;
}
static JSValue nb_dom_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *sel = (argc > 0 && JS_IsString(argv[0])) ? (owned = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue arr = JS_NewArray(ctx);
    if (g_dom_root && sel) {
        int i = 0;
        for (const NbNode *c = g_dom_root->first_child; c; c = c->next_sibling)
            qsa_into(ctx, (NbNode *)c, sel, arr, &i);
        JS_SetPropertyStr(ctx, arr, "length", JS_NewInt32(ctx, i));
    }
    JS_FreeCString(ctx, owned);
    nb_list_proto(ctx, arr, "NodeList");
    return arr;
}

/* TreeWalker/NodeFilter helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_treewalker.c"
/* document/DOM factory helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_document.c"
/* ---- element natives (this = element object) ---- */
static int attrs_has(const NbNode *n, const char *name);
static JSValue nb_el_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_normalize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_remove_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_before_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_after_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_replaceWith_js(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static const NbNode *nb_top_root(const NbNode *n);
static JSValue nb_global_document(JSContext *ctx);
/* getAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_get_attribute.c"
/* element.attributes getter helper extracted (Phase 1, single-TU include). */
#include "nb_worker_attributes.c"
/* rung 8: hasAttribute — real bundles gate on documentElement.hasAttribute */
static int attrs_has(const NbNode *n, const char *name);
/* hasAttribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_has_attribute.c"
/* hasAttributes/getAttributeNames helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_attr_names.c"
static JSValue nb_el_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_removeAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_el_toggleAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, nm); return JS_FALSE; }
    int present = attrs_has(n, name);
    int force_present = 0;
    int has_force = (argc > 1);
    if (has_force) force_present = JS_ToBool(ctx, argv[1]);
    int should_set = has_force ? force_present : !present;
    if (should_set) {
        if (!present) {
            JSValue a[2]; a[0] = JS_DupValue(ctx, argv[0]); a[1] = JS_NewString(ctx, "");
            JS_FreeValue(ctx, nb_el_setAttribute(ctx, this_val, 2, a));
            JS_FreeValue(ctx, a[0]); JS_FreeValue(ctx, a[1]);
        }
    } else {
        if (present) {
            JSValue a[1]; a[0] = JS_DupValue(ctx, argv[0]);
            JS_FreeValue(ctx, nb_el_removeAttribute(ctx, this_val, 1, a));
            JS_FreeValue(ctx, a[0]);
        }
    }
    int now_present = should_set ? 1 : 0;
    if (has_force && force_present && present) now_present = 1;
    JS_FreeCString(ctx, nm);
    return JS_NewBool(ctx, now_present);
}
static char *attrs_set(const NbNode *n, const char *name, const char *val) {
    SB b = {0, 0, 0};
    const char *p = n->attrs ? n->attrs : "";
    size_t nl = strlen(name);
    int found = 0, first = 1;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>') p++;
        size_t kl = (size_t)(p - ks);
        char kbuf[64]; size_t kc = kl < 63 ? kl : 63; memcpy(kbuf, ks, kc); kbuf[kc] = 0;
        int is_target = kl == nl && !strncasecmp(ks, name, nl);
        char vtmp[1200]; int hasv = 0;
        const char *savep = p;
        if (*p == '=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p == '"' || *p == '\'') { qc = *p; p++; }
            const char *vs = p;
            while (*p && !(qc ? (*p == qc) : (isspace((unsigned char)*p) || *p == '>'))) p++;
            size_t vl = (size_t)(p - vs);
            if (qc && *p) p++;
            size_t vc = vl < 1199 ? vl : 1199; memcpy(vtmp, vs, vc); vtmp[vc] = 0;
            hasv = 1;
        }
        if (is_target) {
            found = 1;
            if (!first) sb_put(&b, " ");
            sb_put(&b, name); sb_put(&b, "=\"");
            sb_put_attr_escaped(&b, val ? val : "");
            sb_put(&b, "\"");
        } else {
            const char *after = hasv ? p : savep;
            size_t ll = (size_t)(after - ks);
            if (!first) sb_put(&b, " ");
            char tmp[8196]; size_t lc = ll < 8191 ? ll : 8191;
            memcpy(tmp, ks, lc); tmp[lc] = 0;
            sb_put(&b, tmp);
        }
        while (*p && !isspace((unsigned char)*p)) p++;
        first = 0;
    }
    if (!found) {
        if (!first) sb_put(&b, " ");
        sb_put(&b, name); sb_put(&b, "=\"");
        sb_put_attr_escaped(&b, val ? val : "");
        sb_put(&b, "\"");
    }
    return b.s ? b.s : strdup("");
}
static JSValue nb_el_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    char *nm = NULL, *vl = NULL;
    const char *name = (argc > 0 && JS_IsString(argv[0])) ? (nm = JS_ToCString(ctx, argv[0])) : NULL;
    const char *val = (argc > 1 && JS_IsString(argv[1])) ? (vl = JS_ToCString(ctx, argv[1])) : NULL;
    if (!n || !name) { JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl); return JS_UNDEFINED; }
    if (!val) val = "";
    const char *old0 = nb_attr_get(n, name);
    char *old_value = old0 ? strdup(old0) : NULL;
    if (!strcasecmp(name, "id")) { free(n->id); n->id = strdup(val); }
    else if (!strcasecmp(name, "class")) { free(n->cls); n->cls = strdup(val); }
    char *na = attrs_set(n, name, val);
    free(n->attrs);
    n->attrs = na;
    nb_mutation_notify_attribute(ctx, n, name, old_value);
    free(old_value);
    JS_FreeCString(ctx, nm); JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
/* rung-2 remainder: element.removeAttribute(name) — rebuild the raw attrs
 * blob without the named attribute (attrs_set's loop, skipping the match). */
/* attrs_has/attrs_del/removeAttribute helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_attr_remove.c"
/* id/className property helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_id_class.c"
static JSValue nb_el_textContent_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    /* Node.textContent: for a Document this is null, not the concatenated
     * text of the tree. DocumentFragment (nodeType 11) still concatenates.
     * Comment nodes return their data. */
    if (n && n->tag && !strcmp(n->tag, "#document")) return JS_NULL;
    if (n && n->tag && !strcmp(n->tag, "#comment"))
        return JS_NewString(ctx, n->text ? n->text : "");
    SB b = {0, 0, 0};
    node_text_content(n, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_textContent_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    /* Node.textContent: Document and DocumentType are a no-op; a Text node
     * (tag is NULL here) replaces its data; everything else replaces all
     * children with one real Text node, and an empty string adds none.
     * Only null is special-cased to "": undefined converts like any other
     * DOMString ("undefined"), as in a browser. */
    if (n->tag && !strcmp(n->tag, "#document")) return JS_UNDEFINED;
    if (!n->tag || !n->tag[0] || (n->tag && !strcmp(n->tag, "#comment"))) {   /* character-data node replaces its data */
        char *vl = NULL;
        const char *v = "";
        if (argc > 0 && !JS_IsNull(argv[0])) {
            v = (vl = JS_ToCString(ctx, argv[0]));
            if (!v) return JS_EXCEPTION;
        }
        free(n->text);
        n->text = strdup(v);
        JS_FreeCString(ctx, vl);
        nb_obs_recheck(ctx);
        return JS_UNDEFINED;
    }
    char *vl = NULL;
    const char *v = "";
    if (argc > 0 && !JS_IsNull(argv[0])) {
        v = (vl = JS_ToCString(ctx, argv[0]));
        if (!v) return JS_EXCEPTION;
    }
    clear_children(n);
    free(n->text);
    n->text = NULL;
    NbNode *tn = NULL;
    if (v && v[0]) {
        tn = calloc(1, sizeof(*tn));
        if (tn) {
            tn->text = strdup(v);
            local_append(n, tn);
        }
    }
    JS_FreeCString(ctx, vl);
    /* The added Text node is real, so a childList record reporting it is
     * true. Removed children are not reported: one record carries a single
     * removedNode and a consumer could retain it after its C node is gone.
     * Geometry may change (text length -> height) either way. */
    if (tn) nb_mutation_notify_childlist(ctx, n, tn, NULL, NULL, NULL);
    else nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
/* CharacterData (Text/Comment) data+nodeValue (2026-11-09). Text nodes are
 * the only mutation source for characterData; setting data replaces the
 * text, notifies characterData observers with the old value, and
 * re-evaluates geometry. nodeValue mirrors data on text nodes and is null
 * elsewhere, as in a browser. */
static JSValue nb_text_data_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    return JS_NewString(ctx, (n && n->text) ? n->text : "");
}
static JSValue nb_text_data_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    char *vl = NULL;
    const char *v = "";
    if (argc > 0 && JS_IsString(argv[0])) v = (vl = JS_ToCString(ctx, argv[0]));
    else if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) v = (vl = JS_ToCString(ctx, argv[0]));
    char *old = strdup(n->text ? n->text : "");
    free(n->text);
    n->text = strdup(v ? v : "");
    JS_FreeCString(ctx, vl);
    nb_mutation_notify_characterData(ctx, n, old);
    free(old);
    nb_obs_recheck(ctx);
    return JS_UNDEFINED;
}
/* CharacterData/Text helpers extracted for modularity (Phase 1 pilot, single-TU include). */
#include "nb_worker_text.c"
/* nodeValue helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_value.c"
/* ---- dataset (2026-11-09) ----
 * dataset is a DOMStringMap view over an element's data-* attributes.
 * Handlers are C traps on a Proxy so reads stay live and writes land in
 * the attribute store (prop `userName` <-> attribute `data-user-name`). */
static JSValue push_node(JSContext *ctx, NbNode *n);
/* push_node_or_null helper extracted (Phase 1, single-TU include). */
#include "nb_worker_push_node_or_null.c"
/* dataset node lookup helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_node.c"
/* dataset key-to-attribute helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_attr.c"
/* dataset get trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_get.c"
/* dataset set trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_set.c"
/* dataset delete trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_del.c"
/* dataset ownKeys trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_own_keys.c"
/* dataset getOwnPropertyDescriptor trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_desc.c"
/* dataset has trap helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_has.c"
/* dataset Proxy factory helper extracted (Phase 1, single-TU include). */
#include "nb_worker_dataset_make.c"
static JSValue nb_el_innerHTML_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    SB b = {0, 0, 0};
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling) node_outer_html(c, &b);
    JSValue r = JS_NewString(ctx, b.s ? b.s : "");
    free(b.s);
    return r;
}
static JSValue nb_el_innerHTML_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (n->tag && !strcmp(n->tag, "template")) {   /* <template>.innerHTML lands in content */
        NbNode *frag = nb_template_content_frag(ctx, this_val, n);
        if (frag) n = frag;
    }
    char *vl = NULL;
    const char *v = (argc > 0 && JS_IsString(argv[0])) ? (vl = JS_ToCString(ctx, argv[0])) : "";
    NbNode *frag = nb_parse_html_nodes(v, strlen(v));
    NbNode *child = frag ? frag->first_child : NULL;
    clear_children(n);
    /* innerHTML replaces ALL content. A LOAD-tree element keeps its direct
     * text in n->text (the folded model), so clearing children alone left the
     * old page text behind and the assignment appeared to append. */
    free(n->text); n->text = NULL;
    if (child) {
        frag->first_child = frag->last_child = NULL;
        NbNode *cur = child;
        while (cur) {
            NbNode *nx = cur->next_sibling;
            cur->next_sibling = NULL;
            cur->parent = NULL;       /* let local_append set it */
            local_append(n, cur);
            cur = nx;
        }
    }
    nb_node_free(frag);
    JS_FreeCString(ctx, vl);
    return JS_UNDEFINED;
}
/* ---- insertAdjacentHTML / Text / Element (2026-11-09) ----
 * Real DOM insertion at the four spec positions, built on the same parser
 * and tree helpers that innerHTML/appendChild already use. Positions are
 * ASCII case-insensitive; anything else is a SyntaxError. beforebegin and
 * afterend on a node with no parent are a no-op, exactly as in a browser
 * (#document is the top; there is no place beside it).
 *
 * ChildList MutationObserver records are emitted for each inserted node.
 * A parsed fragment contributes one record per node (the C record model
 * carries one addedNode); a browser would batch them into one record. Both
 * describe the same real insertions, which is what an observer reacts to.
 *
 * insertAdjacentElement moves an existing node and returns it, or null when
 * the insertion is a no-op. Script-execution on insertion is not claimed
 * here: appendChild's dynamic <script> path is separate and this slice does
 * not duplicate it. */
/* insertAdjacent position parse/error helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_parse.c"
/* insertAdjacent placement/attach helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_place.c"
/* insertAdjacent HTML insertion helper extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_html_helper.c"
/* Element.insertAdjacentHTML extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_html.c"
/* Element.insertAdjacentText extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_text.c"
/* Element.insertAdjacentElement extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_adjacent_element.c"
/* children/childNodes getters extracted (Phase 1, single-TU include). */
#include "nb_worker_children_nodes.c"
/* parentNode/firstChild/nextSibling getters extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_basic.c"
/* lastChild/previousSibling/parentElement and helper extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_struct.c"
/* element-only relation getters/count extracted (Phase 1, single-TU include). */
#include "nb_worker_node_relations_element.c"
/* DocumentFragment insertion/validation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_fragment_utils.c"
static JSValue nb_el_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *ch = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (n && n->tag && !strcmp(n->tag, "template")) {   /* appends land in content */
        NbNode *frag = nb_template_content_frag(ctx, this_val, n);
        if (frag) n = frag;
    }
    if (!n || !ch || ch == n) return JS_NULL;
    if (nb_is_doc_fragment_node(ch)) {
        if (nb_node_is_ancestor(ch, n))
            return nb_throw_hierarchy(ctx, "HierarchyRequestError: the fragment contains this node");
        orphan_remove(ch);
        nb_insert_fragment_children(ctx, n, ch, NULL);
        orphan_add(ch);
        return JS_DupValue(ctx, argv[0]);   /* spec returns the fragment */
    }
    node_detach(ch);
    orphan_remove(ch);
    local_append(n, ch);
    nb_mutation_notify_childlist(ctx, n, ch, NULL, NULL, NULL);
    
    /* If a script element with src is appended to head/body, fetch and execute it */
    if (ch && ch->tag && !strcmp(ch->tag, "script")) {
        fprintf(stderr, "DEBUG: script element appended, tag: %s\n", ch->tag);
        const char *src = nb_attr_get(ch, "src");
        fprintf(stderr, "DEBUG: script src: %s\n", src ? src : "(null)");
        if (src && src[0]) {
            /* Resolve URL against current page URL */
            char resolved_url[2048];
            resolve_doc_url(src, resolved_url, sizeof(resolved_url));
            
            /* Fetch the script content */
            char *rb = NULL; size_t rn = 0; int status = 0; char errbuf[256] = "";
            
            if (strncmp(resolved_url, "file:", 5) == 0) {
                const char *p = resolved_url + 5;
                while (*p == '/') p++;
                if (strncmp(p, "localhost", 9) == 0 && p[9] == '/') p += 10;
                char abspath[2048];
                snprintf(abspath, sizeof(abspath), "/%s", p);
                if (read_file(abspath, &rb, &rn)) status = 200;
                else snprintf(errbuf, sizeof(errbuf), "cannot read %s", abspath);
            } else if (strncmp(resolved_url, "http:", 5) == 0 || strncmp(resolved_url, "https:", 6) == 0) {
                /* Direct curl fallback */
                char cfgpath[1024] = "";
                char t1[] = "/tmp/nbfetch.XXXXXX";
                int fd1 = mkstemp(t1);
                if (fd1 < 0) {
                    snprintf(errbuf, sizeof(errbuf), "mkstemp failed");
                } else {
                    close(fd1);
                    snprintf(cfgpath, sizeof(cfgpath), "%s", t1);
                    FILE *cf = fopen(cfgpath, "w");
                    if (!cf) {
                        snprintf(errbuf, sizeof(errbuf), "cannot write curl config");
                    } else {
                        cfg_line(cf, "url", resolved_url);
                        cfg_line(cf, "user-agent", "Mozilla/5.0 (NNEST network-browser-hq nb-js-worker rung4)");
                        cfg_line(cf, "max-time", "8");
                        fputs("silent\nlocation\nfail\n", cf);
                        {
                            char ckhdr[4096];
                            cookie_header_for_url(resolved_url, ckhdr, sizeof(ckhdr));
                            if (ckhdr[0]) cfg_line(cf, "header", ckhdr);
                        }
                        char hdrpath[256] = "";
                        {
                            char t3[] = "/tmp/nbfetchhdr.XXXXXX";
                            int fd3 = mkstemp(t3);
                            if (fd3 >= 0) { close(fd3); snprintf(hdrpath, sizeof(hdrpath), "%s", t3); cfg_line(cf, "dump-header", hdrpath); }
                        }
                        cfg_line(cf, "output", "/tmp/nbfetchbody");
                        fclose(cf);
                        
                        char cmd[2048];
                        snprintf(cmd, sizeof(cmd), "curl -K '%s' 2>/dev/null", cfgpath);
                        int rc = system(cmd);
                        unlink(cfgpath);
                        
                        if (rc == 0) {
                            FILE *bf = fopen("/tmp/nbfetchbody", "rb");
                            if (bf) {
                                fseek(bf, 0, SEEK_END);
                                long fsize = ftell(bf);
                                fseek(bf, 0, SEEK_SET);
                                if (fsize > 0 && fsize < 4 * 1024 * 1024) {
                                    rb = malloc((size_t)fsize + 1);
                                    if (rb) {
                                        rn = fread(rb, 1, (size_t)fsize, bf);
                                        rb[rn] = 0;
                                        status = 200;
                                    }
                                }
                                fclose(bf);
                            }
                            unlink("/tmp/nbfetchbody");
                            if (hdrpath[0]) unlink(hdrpath);
                        }
                    }
                }
            }
            
            if (status == 200 && rb && rn > 0) {
                char errbuf2[512];
                if (peval_budget(ctx, rb, rn, errbuf2, sizeof(errbuf2)) != 0) {
                    fprintf(stderr, "WERR| dynamic script appendChild %s: %s\n", resolved_url, errbuf2[0] ? errbuf2 : "eval error");
                }
                free(rb);
            } else {
                if (rb) free(rb);
                if (errbuf[0]) fprintf(stderr, "WERR| script load appendChild %s: %s\n", resolved_url, errbuf);
            }
        }
    }
    
    return push_node(ctx, ch);
}
/* Node.removeChild extracted (Phase 1, single-TU include). */
#include "nb_worker_remove_child.c"
/* Node.insertBefore extracted (Phase 1, single-TU include). */
#include "nb_worker_insert_before.c"
/* Node.replaceChild extracted (Phase 1, single-TU include). */
#include "nb_worker_replace_child.c"
/* form-field tag helper extracted (Phase 1, single-TU include). */
#include "nb_worker_is_form_field.c"
/* select/option selection primitives extracted (Phase 1, single-TU include). */
#include "nb_worker_select_primitives.c"
/* option selected state helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_option_selected.c"
/* option value/index helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_option_value_index.c"
/* select selection helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_select_selection.c"
/* select options/length/index accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_select_options.c"
/* element value get/set and fire_change decl extracted (Phase 1, single-TU include). */
#include "nb_worker_value_get_set.c"
/* select.selectedIndex and option.selected accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_select_index_selected.c"
/* classList native methods extracted (Phase 1, single-TU include). */
#include "nb_worker_classlist.c"

#define IDMAPNAME "__nb_idmap"    /* global object: node index -> JS wrapper (identity) */
/* DOM class prototypes (2026-09-18): real bundles branch on
 * `instanceof Element` and touch `Element.prototype` at load time. The
 * per-context global object `__nb_protos` holds the constructor-chain
 * prototypes for element/text/document wrappers built by install_dom_classes. */
#define PROTONAME "__nb_protos"
#define PROTO_EL 0   /* element wrappers ride HTMLElement.prototype */
#define PROTO_TEXT 1 /* text nodes ride Text.prototype */
#define PROTO_DOC 2  /* the document object rides Document.prototype */

/* upgrade a freshly built wrapper if a custom-element class is registered
 * for its tag (customElements upgrade-on-creation). Defined in the CE
 * section below (line ~3070). */
static void ce_upgrade_node_if_registered(JSContext *ctx, JSValue el, NbNode *n);

/* Build a JS element object wrapping a C NbNode. */
static JSValue push_node(JSContext *ctx, NbNode *n) {
    int nidx = node_index(n);
    /* wrapper identity: one JS object per C node, held in the global
     * __nb_idmap object (the context is fresh per page). */
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue map = JS_GetPropertyStr(ctx, g, IDMAPNAME);
    if (!JS_IsObject(map)) {
        JS_FreeValue(ctx, map);
        map = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, g, IDMAPNAME, JS_DupValue(ctx, map));
    }
    JS_FreeValue(ctx, g);
    JSValue ex = JS_GetPropertyUint32(ctx, map, (uint32_t)nidx);
    if (JS_IsObject(ex)) {           /* existing wrapper */
        JS_FreeValue(ctx, map);
        return ex;
    }
    JS_FreeValue(ctx, ex);
    if (nb_is_doc_fragment_node(n)) {
        JS_FreeValue(ctx, map);
        return make_fragment_wrapper(ctx, n, "#document-fragment");
    }
    int is_comment = (n->tag && !strcmp(n->tag, "#comment"));
    JSValue el = JS_NewObject(ctx);
    /* DOM class prototype (install_dom_classes) so `el instanceof Element`
     * and Element.prototype.* resolve like a browser; falls back to a plain
     * object when the holder is absent (node mode / pre-install). */
    {
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue holder = JS_GetPropertyStr(ctx, g2, PROTONAME);
        JS_FreeValue(ctx, g2);
        if (JS_IsObject(holder)) {
            int which = (n->tag && n->tag[0] && !is_comment) ? PROTO_EL : PROTO_TEXT;
            JSValue p = JS_GetPropertyUint32(ctx, holder, (uint32_t)which);
            if (JS_IsObject(p)) JS_SetPrototype(ctx, el, p);
            JS_FreeValue(ctx, p);
        }
        JS_FreeValue(ctx, holder);
    }
    JS_SetPropertyStr(ctx, el, NODEKEY, JS_NewInt32(ctx, nidx));
    {
        const char *label = (n->tag && n->tag[0]) ? n->tag : "#text";
        /* nodeType (2026-09-20): kevlar's materializer gate is
         * BD(O)=_.qh(O)&&"nodeType" in O — elements without a nodeType
         * property fall through every Qkq branch and the render dies with
         * Error("Oc"). Browsers expose it on the prototype; our wrappers
         * are plain objects so an own data property is the equivalent.
         * #document reports 9, like a browser. */
        int nt = (n->tag && !strcmp(n->tag, "#document")) ? 9
               : ((n->tag && !strcmp(n->tag, "#document-fragment")) ? 11
               : (is_comment ? 8
               : ((n->tag && n->tag[0]) ? 1 : 3)));
        JS_SetPropertyStr(ctx, el, "nodeType", JS_NewInt32(ctx, nt));
        JS_SetPropertyStr(ctx, el, "nodeName", JS_NewString(ctx, label));
        if (n->tag && n->tag[0] && !is_comment)
            JS_SetPropertyStr(ctx, el, "tagName", JS_NewString(ctx, label));
    }

    JS_SetPropertyStr(ctx, el, "getAttribute", JS_NewCFunction(ctx, nb_el_getAttribute, "getAttribute", 1));
    JS_SetPropertyStr(ctx, el, "setAttribute", JS_NewCFunction(ctx, nb_el_setAttribute, "setAttribute", 2));
    JS_SetPropertyStr(ctx, el, "removeAttribute", JS_NewCFunction(ctx, nb_el_removeAttribute, "removeAttribute", 1));
    JS_SetPropertyStr(ctx, el, "hasAttribute", JS_NewCFunction(ctx, nb_el_hasAttribute, "hasAttribute", 1));
    JS_SetPropertyStr(ctx, el, "hasAttributes", JS_NewCFunction(ctx, nb_el_hasAttributes, "hasAttributes", 0));
    JS_SetPropertyStr(ctx, el, "getAttributeNames", JS_NewCFunction(ctx, nb_el_getAttributeNames, "getAttributeNames", 0));
    JS_SetPropertyStr(ctx, el, "toggleAttribute", JS_NewCFunction(ctx, nb_el_toggleAttribute, "toggleAttribute", 2));
    JS_SetPropertyStr(ctx, el, "appendChild", JS_NewCFunction(ctx, nb_el_appendChild, "appendChild", 1));
    JS_SetPropertyStr(ctx, el, "removeChild", JS_NewCFunction(ctx, nb_el_removeChild, "removeChild", 1));
    JS_SetPropertyStr(ctx, el, "insertBefore", JS_NewCFunction(ctx, nb_el_insertBefore, "insertBefore", 2));
    JS_SetPropertyStr(ctx, el, "replaceChild", JS_NewCFunction(ctx, nb_el_replaceChild, "replaceChild", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentHTML", JS_NewCFunction(ctx, nb_el_insertAdjacentHTML, "insertAdjacentHTML", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentText", JS_NewCFunction(ctx, nb_el_insertAdjacentText, "insertAdjacentText", 2));
    JS_SetPropertyStr(ctx, el, "insertAdjacentElement", JS_NewCFunction(ctx, nb_el_insertAdjacentElement, "insertAdjacentElement", 2));
    JS_SetPropertyStr(ctx, el, "addEventListener", JS_NewCFunction(ctx, nb_el_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, el, "removeEventListener", JS_NewCFunction(ctx, nb_el_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, el, "dispatchEvent", JS_NewCFunction(ctx, nb_el_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, el, "contains", JS_NewCFunction(ctx, nb_el_contains, "contains", 1));
    JS_SetPropertyStr(ctx, el, "click", JS_NewCFunction(ctx, nb_el_click, "click", 0));
    JS_SetPropertyStr(ctx, el, "focus", JS_NewCFunction(ctx, nb_el_focus, "focus", 0));
    JS_SetPropertyStr(ctx, el, "blur", JS_NewCFunction(ctx, nb_el_blur, "blur", 0));
    /* CharacterData (Text/Comment) ChildNode methods. Element wrappers
     * inherit these from Element.prototype; text/comment wrappers need
     * them here because push_node does not install Element.prototype. */
    if (!n->tag || !n->tag[0] || is_comment) {
        JS_SetPropertyStr(ctx, el, "remove",      JS_NewCFunction(ctx, nb_el_remove_js, "remove", 0));
        JS_SetPropertyStr(ctx, el, "before",      JS_NewCFunction(ctx, nb_el_before_js, "before", 0));
        JS_SetPropertyStr(ctx, el, "after",       JS_NewCFunction(ctx, nb_el_after_js, "after", 0));
        JS_SetPropertyStr(ctx, el, "replaceWith", JS_NewCFunction(ctx, nb_el_replaceWith_js, "replaceWith", 0));
    }
    /* resource/URL attributes real bundles read directly off the element.
     * Browsers expose src/href as STRINGS even when the attribute is absent
     * (an inline <script>.src is ""), and youtube's global error reporter
     * does `script.src.indexOf("/debug-")` across getElementsByTagName
     * ("script") — undefined there throws. closure's module loader also does
     * `O.src ? O.src : O.getAttribute("href")` on <script id="base-js">. */
    if (n->tag) {
        if (!strcmp(n->tag, "img")) {
            JSAtom nm = JS_NewAtom(ctx, "src");
            JS_DefinePropertyGetSet(ctx, el, nm,
                JS_NewCFunction(ctx, nb_img_src_get, "get src", 0),
                JS_NewCFunction(ctx, nb_img_src_set, "set src", 1),
                JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "naturalWidth");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_naturalWidth_get, "get naturalWidth", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "naturalHeight");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_naturalHeight_get, "get naturalHeight", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
            nm = JS_NewAtom(ctx, "complete");
            JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_img_complete_get, "get complete", 0), JS_UNDEFINED,
                JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, nm);
        } else {
            if (!strcmp(n->tag, "script")) {
                /* script.src setter: when set, fetch and execute the script (Cloudflare challenge) */
                JSAtom nm = JS_NewAtom(ctx, "src");
                JS_DefinePropertyGetSet(ctx, el, nm,
                    JS_NewCFunction(ctx, nb_script_src_get, "get src", 0),
                    JS_NewCFunction(ctx, nb_script_src_set, "set src", 1),
                    JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
                JS_FreeAtom(ctx, nm);
            } else {
                static const char *src_tags[] = { "iframe","input",
                    "source","track","embed","video","audio","frame", NULL };
                static const char *href_tags[] = { "a","link","area","base", NULL };
                for (int i = 0; src_tags[i]; i++)
                    if (!strcmp(n->tag, src_tags[i])) {
                        JS_SetPropertyStr(ctx, el, "src", JS_NewString(ctx, nb_attr_get(n, "src")));
                        break;
                    }
                for (int i = 0; href_tags[i]; i++)
                    if (!strcmp(n->tag, href_tags[i])) {
                        JS_SetPropertyStr(ctx, el, "href", JS_NewString(ctx, nb_attr_get(n, "href")));
                        break;
                    }
            }
        }
    }
    /* canvas 2D (2026-09-18): real bundles probe <canvas> via
     * createElementNS('...','canvas') and immediately call getContext('2d')
     * through a fillStyle/color parse. Minimal context: geometry/measure
     * stubs that return well-formed objects so parser/feature paths don't
     * throw; rasterization is out of scope (see NB-JS-ENGINE-ROADMAP). */
    if (n->tag && strcmp(n->tag, "canvas") == 0) {
        JS_SetPropertyStr(ctx, el, "getContext", JS_NewCFunction(ctx, nb_el_getContext, "getContext", 1));
        JS_SetPropertyStr(ctx, el, "toDataURL", JS_NewCFunction(ctx, nb_canvas_toDataURL, "toDataURL", 0));
    }
    if (n->tag && strcmp(n->tag, "template") == 0) {
        /* <template>.content = C-backed #document-fragment (2026-09-20).
         * kevlar does template.content.insertBefore(content.cloneNode(true),
         * firstChild) while wiring its css style tags; the old prelude JS
         * docfrag couldn't produce/accept C nodes and the insert threw
         * HierarchyRequestError at bundle line 16063. */
        (void)nb_template_content_frag(ctx, el, n);
    }

    /* el.on<type> = cb accessors; native magic carries the ONPROPS index
     * (QuickJS accessors carry no property name — the magic is the index). */
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom nm = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_el_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* read-only accessor properties: children, childNodes, parentNode, firstChild, nextSibling */
    {
        JSAtom nm = JS_NewAtom(ctx, "children");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_children, "children", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childNodes");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_childNodes, "childNodes", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "attributes");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_attributes, "attributes", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentNode");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_parentNode, "parentNode", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "firstChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_firstChild, "firstChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_nextSibling, "nextSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "lastChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_lastChild, "lastChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_previousSibling, "previousSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "parentElement");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_parentElement, "parentElement", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "firstElementChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_firstElementChild, "firstElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "lastElementChild");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_lastElementChild, "lastElementChild", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "nextElementSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_nextElementSibling, "nextElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "previousElementSibling");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_previousElementSibling, "previousElementSibling", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "childElementCount");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_childElementCount, "childElementCount", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* accessors: id, className, textContent, innerHTML */
    {
        JSAtom nm = JS_NewAtom(ctx, "id");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_id_get, "get id", 0), JS_NewCFunction(ctx, nb_el_id_set, "set id", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "className");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_className_get, "get className", 0), JS_NewCFunction(ctx, nb_el_className_set, "set className", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_textContent_get, "get textContent", 0), JS_NewCFunction(ctx, nb_el_textContent_set, "set textContent", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunction(ctx, nb_el_innerHTML_get, "get innerHTML", 0), JS_NewCFunction(ctx, nb_el_innerHTML_set, "set innerHTML", 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung-2 remainder + rung 7: el.style — identity-cached snapshot of
     * the inline style attribute (reads mirror inline decls; JS writes
     * persist on the snapshot; not fed back into layout metrics). */
    {
        JSAtom nm = JS_NewAtom(ctx, "style");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_style_get, "get style", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }

    /* rung 7: layout-intent metrics (display:none-aware, CSS px from the
     * cascade, else 0 — see NB-JS-ENGINE-ROADMAP rung 7). */
    {
        JSAtom nm = JS_NewAtom(ctx, "offsetWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get offsetHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientWidth", 0, JS_CFUNC_generic_magic, 2), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "clientHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_offdim, "get clientHeight", 0, JS_CFUNC_generic_magic, 3), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "offsetParent");
        JS_DefinePropertyGetSet(ctx, el, nm, JS_NewCFunction(ctx, nb_el_offset_parent, "get offsetParent", 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollTop");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollTop", 0, JS_CFUNC_generic_magic, 1),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollTop", 1, JS_CFUNC_generic_magic, 1),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollLeft");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_get, "get scrollLeft", 0, JS_CFUNC_generic_magic, 0),
            JS_NewCFunctionMagic(ctx, nb_el_scroll_set, "set scrollLeft", 1, JS_CFUNC_generic_magic, 0),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollHeight");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollHeight", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollWidth");
        JS_DefinePropertyGetSet(ctx, el, nm,
            JS_NewCFunctionMagic(ctx, nb_el_scroll_dim, "get scrollWidth", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, el, "getBoundingClientRect",
                      JS_NewCFunction(ctx, nb_el_getBoundingClientRect, "getBoundingClientRect", 0));

/* rung-2 remainder: el.value get/set for form fields. */
      if (is_form_field(n->tag)) {
          JSAtom nm = JS_NewAtom(ctx, "value");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_el_value_get, "get value", 0), JS_NewCFunction(ctx, nb_el_value_set, "set value", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      }
      /* select/option state (2026-10-05): selectedIndex on the dropdown,
       * selected on the option. Both are registered per-node by tag so a
       * page's `typeof sel.selectedIndex` feature-detects honestly. */
      if (node_is_tag(n, "select")) {
          JSAtom nm = JS_NewAtom(ctx, "selectedIndex");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_index_get, "get selectedIndex", 0),
              JS_NewCFunction(ctx, nb_sel_index_set, "set selectedIndex", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          /* options/index are VALUE properties, not methods: registering
           * them with JS_SetPropertyStr made sel.options a function, and
           * sel.options.length then answered the function's arity (0). */
          nm = JS_NewAtom(ctx, "options");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_options, "get options", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          nm = JS_NewAtom(ctx, "length");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_sel_length, "get length", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      } else if (node_is_tag(n, "option")) {
          JSAtom nm = JS_NewAtom(ctx, "selected");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_opt_selected_get, "get selected", 0),
              JS_NewCFunction(ctx, nb_opt_selected_set, "set selected", 1),
              JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
          nm = JS_NewAtom(ctx, "index");
          JS_DefinePropertyGetSet(ctx, el, nm,
              JS_NewCFunction(ctx, nb_opt_index, "get index", 0), JS_UNDEFINED,
              JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
          JS_FreeAtom(ctx, nm);
      }

    /* classList */
    {
        JSValue cl = JS_NewObject(ctx);                /* classList */
        JS_SetPropertyStr(ctx, cl, NODEKEY, JS_NewInt32(ctx, nidx));
        JS_SetPropertyStr(ctx, cl, "add", JS_NewCFunction(ctx, nb_cl_add, "add", 1));
        JS_SetPropertyStr(ctx, cl, "remove", JS_NewCFunction(ctx, nb_cl_remove, "remove", 1));
        JS_SetPropertyStr(ctx, cl, "toggle", JS_NewCFunction(ctx, nb_cl_toggle, "toggle", 1));
        JS_SetPropertyStr(ctx, cl, "contains", JS_NewCFunction(ctx, nb_cl_contains, "contains", 1));
        JS_SetPropertyStr(ctx, cl, "item", JS_NewCFunction(ctx, nb_cl_item, "item", 1));
        {
            JSValue cls = get_global_attr(ctx, "DOMTokenList");
            JSValue proto = JS_GetPropertyStr(ctx, cls, "prototype");
            if (JS_IsObject(proto)) JS_SetPrototype(ctx, cl, proto);
            JS_FreeValue(ctx, proto);
            JS_FreeValue(ctx, cls);
        }
        JS_SetPropertyStr(ctx, el, "classList", cl);
    }
    /* dataset (DOMStringMap over data-* attributes) */
    JS_SetPropertyStr(ctx, el, "dataset", nb_ds_make(ctx, n, nidx));

    /* cache wrapper in the identity map so later push_node calls return the
     * same object (map originally FIXED a self-storing bug: the old code
     * stored the wrapper into itself -> identity broke). */
    JS_SetPropertyUint32(ctx, map, (uint32_t)nidx, JS_DupValue(ctx, el));
    JS_FreeValue(ctx, map);
    /* customElements upgrade-on-creation (2026-09-20): elements built at
     * runtime (createElement/appendChild/innerHTML after classes define) get
     * the registered class + lifecycle like a browser; hyphenated tags only. */
    if (n->tag && strchr(n->tag, '-'))
        ce_upgrade_node_if_registered(ctx, el, n);
    return el;
}

/* ============================= rung 6: file-backed document.cookie jar =====
 * document.cookie getter/setter as C natives (the prelude in nb_host.h leaves
 * a configurable stub; install_dom redefines it with these). The jar lives on
 * disk at $NB_COOKIES_FILE (fallback $HOME/.config/nbjs/nb_cookies.txt), so
 * cookies survive across LOADs — each LOAD runs in a fresh engine instance, so
 * the file is the only persistence. Jar line format (TAB-separated legend):
 *   host<TAB>path<TAB>name<TAB>value<TAB>expires_epoch<TAB>secure
 * host "*" = set from a URI with no host. expires 0 = session cookie.
 * RFC 6265 subset: name=value + Domain/Path/Expires/Max-Age/Secure.
 * Reads tolerate damage: junk lines are skipped, not fatal. */
#define COOKIE_MAX_ENT 512

typedef struct {
    char host[128];
    char path[256];
    char name[128];
    char value[1024];
    time_t expires;
    int secure;
} CookieEnt;

static char g_cookie_path[PATH_MAX];
static int  g_cookie_path_set = 0;

static void mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void cookie_jar_init(void) {
    g_cookie_path_set = 1;
    const char *env = getenv("NB_COOKIES_FILE");
    if (env && env[0]) { snprintf(g_cookie_path, sizeof(g_cookie_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_cookie_path, sizeof(g_cookie_path), "%s/.config/nbjs/nb_cookies.txt", home);
    else
        g_cookie_path[0] = 0;   /* no writable location: reads '', writes no-op */
}

/* Split the current g_href into host + request path (bare host/port dropped).
 * Port numbers are skipped (cookie scoping ignores ports, RFC 6265 §1). */
static int href_parts(char *hostb, size_t hl, char *pathb, size_t pl) {
    const char *p = g_href;
    const char *a = strstr(p, "://");
    const char *s = a ? a + 3 : p;
    const char *q = s;
    while (*q) {
        if (*q == '/' || *q == '?' || *q == '#') break;
        q++;
    }
    size_t hn = (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    /* RFC 6265 §1: cookie scope is host-only — cut a ":port" suffix if the
     * colon slice between scheme and path is entirely digits. */
    for (char *c = hostb; *c; c++) if (*c == ':') { *c = 0; break; }
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *ph = q;
    while (*ph && *ph != '?' && *ph != '#') ph++;
    size_t pn = (size_t)(ph - q);
    if (pn >= pl) pn = pl - 1;
    memcpy(pathb, q, pn); pathb[pn] = 0;
    if (!pathb[0]) snprintf(pathb, pl, "/");
    return 1;
}

/* RFC 6265 §5.1.4 default-path for a Set-Cookie with no explicit Path. */
static void default_cookie_path(const char *rp, char *out, size_t olen) {
    if (!rp || rp[0] != '/') { snprintf(out, olen, "/"); return; }
    const char *r = strrchr(rp, '/');
    if (!r || r == rp) { snprintf(out, olen, "/"); return; }
    size_t n = (size_t)(r - rp);
    if (n >= olen) n = olen - 1;
    memcpy(out, rp, n); out[n] = 0;
    if (!out[0]) snprintf(out, olen, "/");
}

/* RFC 6265 §5.1.4 path-match: `cp` is the cookie path, `rp` the request path. */
static int cookie_path_match(const char *cp, const char *rp) {
    if (!cp || !rp) return 0;
    if (!strcmp(cp, rp)) return 1;
    size_t n = strlen(cp);
    if (n == 0) return 1;
    if (strncmp(rp, cp, n) != 0) return 0;
    if (cp[n - 1] == '/') return 1;
    return rp[n] == '/';
}

static char *trim_c(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = 0;
    return s;
}

static void sanitize_cookie_value(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 1 < olen; c++) {
        if (*c < 0x20 || *c == 0x7f) continue;   /* drop CR/LF/controls (line-injection) */
        out[o++] = (char)*c;
    }
    out[o] = 0;
}

/* days-from-civil -> UNIX epoch (no TZ dependence; glibc timegm macro-gated). */
static time_t epoch_from_ymd(int y, int m, int d, int hh, int mi, int ss) {
    if (m < 3) { m += 12; y--; }
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (unsigned)(m > 2 ? m - 3 : m + 9) + 2) / 5 + (unsigned)d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    long days = (long)(era * 146097) + (long)doe - 719468L;
    return (time_t)days * 86400L + hh * 3600L + mi * 60L + ss;
}

static const char *const COOKIE_MONTHS[12] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* IMF-fixdate ("Sun, 06 Nov 1994 08:49:37 GMT"); (time_t)-1 = unparseable.
 * Note a real "01 Jan 1970" parses to epoch 0 (a valid instant, NOT the
 * "no expiry" sentinel — that distinction is handled in the setter). */
static time_t cookie_datetime(const char *s) {
    if (!s || !*s) return (time_t)-1;
    int d = 0, y = 0, hh = 0, mi = 0, ss = 0, mon = -1;
    char monname[8] = {0};
    if (sscanf(s, "%*[^,], %d %7s %d %d:%d:%d",
               &d, monname, &y, &hh, &mi, &ss) == 6) {
        for (int i = 0; i < 12 && mon < 0; i++)
            if (!strncasecmp(COOKIE_MONTHS[i], monname, 3)) mon = i;
        if (mon >= 0 && y >= 1970 && y <= 9999)
            return epoch_from_ymd(y, mon + 1, d, hh, mi, ss);
    }
    return (time_t)-1;
}

static int cookie_parse_line(char *line, CookieEnt *e) {
    memset(e, 0, sizeof(*e));
    char *f[6];
    int nf = 0;
    char *q = line;
    while (nf < 6 && *q) {
        f[nf] = q;
        char *t = strchr(q, '\t');
        if (t) { *t = 0; q = t + 1; }
        else { q += strlen(q); }
        nf++;
    }
    if (nf < 5) return 0;
    snprintf(e->host, sizeof(e->host), "%s", f[0]);
    snprintf(e->path, sizeof(e->path), "%s", f[1]);
    snprintf(e->name, sizeof(e->name), "%s", f[2]);
    snprintf(e->value, sizeof(e->value), "%s", f[3]);
    e->expires = (time_t)atol(f[4]);
    if (nf >= 6) e->secure = atoi(f[5]) ? 1 : 0;
    return 1;
}

static int cookie_load_file(CookieEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(g_cookie_path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) { *nl = 0; }
        if (cookie_parse_line(p, &ents[n])) n++;
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}

static void cookie_save_file(const CookieEnt *ents, int n) {
    if (!g_cookie_path[0]) return;
    char dirbuf[PATH_MAX];
    snprintf(dirbuf, sizeof(dirbuf), "%s", g_cookie_path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) { *slash = 0; if (slash != dirbuf) mkdir_p(dirbuf); }
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        sb_put(&b, ents[i].host); sb_put(&b, "\t");
        sb_put(&b, ents[i].path); sb_put(&b, "\t");
        sb_put(&b, ents[i].name); sb_put(&b, "\t");
        sb_put(&b, ents[i].value);
        char tail[64];
        snprintf(tail, sizeof(tail), "\t%ld\t%d\n",
                 (long)ents[i].expires, ents[i].secure ? 1 : 0);
        sb_put(&b, tail);
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_cookie_path);
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (b.s && b.len) fwrite(b.s, 1, b.len, f);
        fclose(f);
        rename(tmp, g_cookie_path);
    }
    free(b.s);
}

/* ======================== rung 6 seam: unified cookie store =============
 * ONE authoritative jar shared by document.cookie AND the network layer
 * (nb_fetch_sync).  Chromium-parity: the old two-store split (NB_COOKIES_FILE
 * for document.cookie, NB_CURL_COOKIES_FILE for curl -b/-c) is eliminated;
 * all Set-Cookie ingress and Cookie egress goes through our jar.  The manager
 * may still set NB_CURL_COOKIES_FILE for its own curls; the worker ignores it
 * from here forward.
 *
 * cookie_header_for_url()  — scope-match + emit Cookie: for outgoing request
 * cookie_set_from_wire()   — parse response Set-Cookie into the jar
 */

/* Extract host and path from an arbitrary URL (scheme://host[:port]/path).
 * Port is stripped for cookie scoping (RFC 6265 section 1). */
static int url_host_path(const char *url, char *hostb, size_t hl,
                         char *pathb, size_t pl) {
    hostb[0] = 0; pathb[0] = 0;
    const char *a = strstr(url, "://");
    if (!a) return 0;
    const char *s = a + 3;
    const char *q = s;
    while (*q && *q != '/' && *q != '?') q++;
    const char *col = NULL;
    for (const char *p = s; p < q; p++) if (*p == ':') { col = p; break; }
    size_t hn = col ? (size_t)(col - s) : (size_t)(q - s);
    if (hn >= hl) hn = hl - 1;
    memcpy(hostb, s, hn); hostb[hn] = 0;
    for (char *c = hostb; *c; c++) *c = (char)tolower((unsigned char)*c);
    const char *pp = (*q == '/') ? q : "/";
    snprintf(pathb, pl, "%s", pp);
    return 1;
}

/* Scope-match cookies from the jar for an outgoing request URL.
 * Writes "Cookie: n1=v1; n2=v2" into out.  Respects Domain/Path/Secure. */
static void cookie_header_for_url(const char *url, char *out, size_t olen) {
    out[0] = 0;
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return;
    char host[128], rp[512];
    if (!url_host_path(url, host, sizeof(host), rp, sizeof(rp))) return;
    int is_https = (strncmp(url, "https://", 8) == 0);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;
        if (ents[i].secure && !is_https) continue;
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;
        if (!cookie_path_match(ents[i].path, rp)) continue;
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    if (b.len && b.len + 10 < olen) {
        snprintf(out, olen, "Cookie: %.*s", (int)b.len, b.s);
    }
    free(b.s);
}

/* Parse a raw Set-Cookie header value and upsert into the jar.
 * request_url provides the default Domain/Path when the header omits them. */
static void cookie_set_from_wire(const char *header_val, const char *request_url) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0] || !header_val || !*header_val) return;

    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", header_val);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) return;
    while (*tok == ' ') tok++;
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) return;
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) return;

    while ((tok = strtok(NULL, ";")) != NULL) {
        while (*tok == ' ') tok++;
        if (!strncasecmp(tok, "domain=", 7))
            snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "path=", 5))
            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "max-age=", 8)) maxage = atol(tok + 8);
        else if (!strncasecmp(tok, "expires=", 8))
            snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure")) secure = 1;
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            snprintf(scope_host, sizeof(scope_host), "%s", dh);
    }
    if (!scope_path[0]) {
        char dh[128], dp[512];
        if (url_host_path(request_url, dh, sizeof(dh), dp, sizeof(dp)))
            default_cookie_path(dp, scope_path, sizeof(scope_path));
        else
            snprintf(scope_path, sizeof(scope_path), "/");
    }

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;
        else if (exp <= nowt) delete = 1;
    }

    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcasecmp(ents[i].host, scope_host) != 0) continue;
        if (ents[i].path[0] && strcmp(ents[i].path, scope_path) != 0) continue;
        if (strcmp(ents[i].name, name) != 0) continue;
        found = i;
        break;
    }
    if (delete) {
        if (found >= 0) {
            for (int i = found; i + 1 < n; i++) ents[i] = ents[i + 1];
            n--;
        }
    } else {
        if (found >= 0) {
            snprintf(ents[found].value, sizeof(ents[found].value), "%s", value);
            ents[found].expires = exp;
            ents[found].secure = secure;
        } else if (n < COOKIE_MAX_ENT) {
            CookieEnt *e = &ents[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->host, sizeof(e->host), "%s", scope_host);
            snprintf(e->path, sizeof(e->path), "%s", scope_path);
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->value, sizeof(e->value), "%s", value);
            e->expires = exp;
            e->secure = secure;
        }
    }
    cookie_save_file(ents, n);
}

static JSValue nb_dom_cookie_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (!g_cookie_path_set) cookie_jar_init();
    if (!g_cookie_path[0]) return JS_NewString(ctx, "");
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) return JS_NewString(ctx, "");
    for (char *c = host; *c; c++) *c = (char)tolower((unsigned char)*c);
    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);
    time_t now = time(NULL);
    SB b = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        if (ents[i].expires && ents[i].expires <= now) continue;   /* expired */
        if (strcmp(ents[i].host, "*") != 0 &&
            strcasecmp(ents[i].host, host) != 0) continue;          /* other host */
        if (!cookie_path_match(ents[i].path, rp)) continue;         /* other path */
        if (ents[i].name[0] == 0) continue;
        if (b.len) sb_put(&b, "; ");
        sb_put(&b, ents[i].name);
        sb_put(&b, "=");
        sb_put(&b, ents[i].value);
    }
    JSValue r = JS_NewStringLen(ctx, b.s ? b.s : "", b.len);
    free(b.s);
    return r;
}

static JSValue nb_dom_cookie_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *spec_owned = NULL;
    const char *spec = argc > 0 ? (spec_owned = JS_ToCString(ctx, argv[0])) : NULL;
    if (!spec) return JS_UNDEFINED;   /* JS_ToCString already threw on coercion failure */
    if (!g_cookie_path_set) cookie_jar_init();
    if (!spec || !*spec || !g_cookie_path[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    char host[128], rp[512];
    if (!href_parts(host, sizeof(host), rp, sizeof(rp))) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", spec);
    char name[128] = "", value[1024] = "";
    char scope_host[128] = "", scope_path[256] = "";
    char expire_s[256] = "";
    long maxage = -1;
    int secure = 0;

    char *tok = strtok(buf, ";");
    if (!tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    tok = trim_c(tok);
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }
    *eq = 0;
    snprintf(name, sizeof(name), "%s", tok);
    sanitize_cookie_value(eq + 1, value, sizeof(value));
    if (!name[0]) { JS_FreeCString(ctx, spec_owned); return JS_UNDEFINED; }

    while ((tok = strtok(NULL, ";")) != NULL) {
        tok = trim_c(tok);
        if (!strncasecmp(tok, "path=", 5))            snprintf(scope_path, sizeof(scope_path), "%s", tok + 5);
        else if (!strncasecmp(tok, "domain=", 7))     snprintf(scope_host, sizeof(scope_host), "%s", tok + 7);
        else if (!strncasecmp(tok, "max-age=", 8))    { maxage = atol(tok + 8); }
        else if (!strncasecmp(tok, "expires=", 8))    snprintf(expire_s, sizeof(expire_s), "%s", tok + 8);
        else if (!strcasecmp(tok, "secure"))          secure = 1;
        /* HttpOnly / SameSite / unknown attrs are accepted and ignored. */
    }

    if (scope_host[0]) {
        char *sh = scope_host;
        while (*sh == '.') sh++;              /* strip leading dots */
        snprintf(scope_host, sizeof(scope_host), "%s", sh);
    }
    if (!scope_host[0]) snprintf(scope_host, sizeof(scope_host), "%s", host);
    if (!scope_path[0]) default_cookie_path(rp, scope_path, sizeof(scope_path));

    time_t exp = 0;
    int delete = 0;
    time_t nowt = time(NULL);
    if (maxage >= 0) {
        if (maxage == 0) delete = 1;               /* max-age=0 -> remove */
        else exp = nowt + maxage;
    } else if (expire_s[0]) {
        exp = cookie_datetime(trim_c(expire_s));
        if (exp == (time_t)-1) exp = 0;            /* unparseable -> session cookie */
        else if (exp <= nowt) delete = 1;          /* expired date -> remove */
    }

    CookieEnt ents[COOKIE_MAX_ENT];
    int n = cookie_load_file(ents, COOKIE_MAX_ENT);

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcasecmp(ents[i].host, scope_host) != 0) continue;
        if (ents[i].path[0] && strcmp(ents[i].path, scope_path) != 0) continue;
        if (strcmp(ents[i].name, name) != 0) continue;
        found = i;
        break;
    }
    if (delete) {
        if (found >= 0) {
            for (int i = found; i + 1 < n; i++) ents[i] = ents[i + 1];
            n--;
        }
    } else {
        if (found >= 0) {
            snprintf(ents[found].host, sizeof(ents[found].host), "%s", scope_host);
            snprintf(ents[found].path, sizeof(ents[found].path), "%s", scope_path);
            snprintf(ents[found].value, sizeof(ents[found].value), "%s", value);
            ents[found].expires = exp;
            ents[found].secure = secure;
        } else if (n < COOKIE_MAX_ENT) {
            CookieEnt *e = &ents[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->host, sizeof(e->host), "%s", scope_host);
            snprintf(e->path, sizeof(e->path), "%s", scope_path);
            snprintf(e->name, sizeof(e->name), "%s", name);
            snprintf(e->value, sizeof(e->value), "%s", value);
            e->expires = exp;
            e->secure = secure;
        }
    }
    cookie_save_file(ents, n);
    JS_FreeCString(ctx, spec_owned);
    return JS_UNDEFINED;
}

/* ===================== rung 6: localStorage (disk jar) + sessionStorage (per-LOAD) =====
 * install_host leaves getItem/setItem/removeItem as no-op stubs; install_dom replaces
 * BOTH globals with real C-backed objects here. localStorage persists across LOADs via
 * a jar on disk at $NB_LOCALSTORAGE_FILE (fallback ~/.config/nbjs/nb_localstorage.txt);
 * sessionStorage lives in process memory and is cleared at the top of every run_page,
 * so each LOAD gets a fresh session (a fresh runtime/context could not carry JS state
 * anyway). Jar line format: <pct-encoded key>\t<pct-encoded value>\n — keys/values are
 * percent-encoded (RFC 3986 unreserved pass through, everything else %XX) so tabs,
 * newlines and control chars are safe inside a line. Reads tolerate damage. */
#define ST_MAX_ENT 256

typedef struct { char key[256]; char value[4096]; } StEnt;

static char g_ls_path[PATH_MAX];
static int  g_ls_path_set = 0;
static StEnt g_ls[ST_MAX_ENT];      /* reused load/save buffer (single-threaded) */
static StEnt g_ss[ST_MAX_ENT];      /* in-memory session map, cleared per LOAD */
static int  g_ss_count = 0;

static void ls_jar_init(void) {
    g_ls_path_set = 1;
    const char *env = getenv("NB_LOCALSTORAGE_FILE");
    if (env && env[0]) { snprintf(g_ls_path, sizeof(g_ls_path), "%s", env); return; }
    const char *home = getenv("HOME");
    if (home && home[0])
        snprintf(g_ls_path, sizeof(g_ls_path), "%s/.config/nbjs/nb_localstorage.txt", home);
    else
        g_ls_path[0] = 0;   /* no writable location: reads null, writes no-op */
}

static int pct_is_safe(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
static void pct_encode(const char *in, char *out, size_t olen) {
    size_t o = 0;
    for (const unsigned char *c = (const unsigned char *)in; *c && o + 3 < olen; c++) {
        if (pct_is_safe(*c)) out[o++] = (char)*c;
        else { unsigned char uc = *c;
            static const char H[] = "0123456789ABCDEF";
            out[o++] = '%'; out[o++] = H[uc >> 4]; out[o++] = H[uc & 15];
        }
    }
    out[o] = 0;
}
static void pct_decode(char *s) {
    char *w = s;
    for (const char *r = s; *r;) {
        if (r[0] == '%' && r[1] && r[2]) {
            int hi, lo;
            char h1 = r[1], h2 = r[2];
            hi = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            lo = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) { *w++ = (char)((hi << 4) | lo); r += 3; continue; }
        }
        *w++ = *r++;
    }
    *w = 0;
}

static int st_load_file(const char *path, StEnt *ents, int maxn) {
    char *buf = NULL;
    size_t bl = 0;
    if (!read_file(path, &buf, &bl)) return 0;
    int n = 0;
    char *p = buf;
    while (p && *p && n < maxn) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *tab = strchr(p, '\t');
        if (tab) {
            *tab = 0;
            snprintf(ents[n].key, sizeof(ents[n].key), "%s", p);
            pct_decode(ents[n].key);
            snprintf(ents[n].value, sizeof(ents[n].value), "%s", tab + 1);
            pct_decode(ents[n].value);
            n++;
        }
        p = nl ? nl + 1 : NULL;
    }
    free(buf);
    return n;
}

/* localStorage implementation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_localstorage.c"

/* sessionStorage implementation helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_sessionstorage.c"

/* ---- DOM class hierarchy (2026-09-18) ----
 * Real bundles feature-detect via `instanceof Element/Node` and read
 * `Element.prototype` at load. We expose browser constructor globals
 * and chain their prototypes in the standard order
 * (EventTarget <- Node <- Element <- HTMLElement / SVGElement),
 * plus Text, Document and Animation. Wrappers produced by push_node()
 * ride those prototypes (keyed in the __nb_protos holder), and the
 * global `document` object rides Document.prototype with a minimal
 * timeline for web-animations feature code. Constructors are inert
 * here — illegal-constructor throws are out of scope for this step. */
static JSValue class_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    /* quickjs.c: C constructors receive new_target as this_val, so build the
     * instance off its .prototype (the fresh object already carries f.prototype)
     * — `new Element()` then yields an Element instance and instanceof works.
     * Generic method calls get a non-function receiver and return undefined. */
    if (JS_IsFunction(ctx, this_val)) {
        JSValue proto = JS_GetPropertyStr(ctx, this_val, "prototype");
        JSValue obj = JS_IsObject(proto) ? JS_NewObjectProto(ctx, proto) : JS_NewObject(ctx);
        JS_FreeValue(ctx, proto);
        return obj;
    }
    return JS_UNDEFINED;
}
/* promise-resolved and empty-array helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_promise_empty.c"
/* XMLSerializer/outerHTML get serialization helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_serialize_get.c"
/* outerHTML setter extracted (Phase 1, single-TU include). */
#include "nb_worker_outerhtml_set.c"
/* parsed-document accessors extracted (Phase 1, single-TU include). */
#include "nb_worker_parsed_doc_accessors.c"
static JSValue nb_dom_createRange(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue nb_win_getSelection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
/* ---- DOMParser: HTML/XML snippet parsing (2026-11-09) ----
 * The previous parseFromString was the inert nb_empty_object: it returned a
 * plain {} with no document surface, so every library parsing a snippet
 * crashed on the first query. This parses with the house HTML parser
 * (nb_parse_html, a #document tree) and hands back a document wrapper.
 * Scoped own properties are installed on the parsed wrapper — getElementById
 * plus documentElement/body/head getters — because push_node gives it the
 * Element prototype (element-scoped queries already work) but no Document
 * members. Honest limits: one lenient HTML parser serves HTML, XHTML, XML
 * and SVG mime types (no strict XML error detection); an unknown mime
 * throws TypeError instead of pretending to parse it. Parsed trees live in
 * g_orphans until page teardown. */
/* ---- parsed-document surface (2026-11-11) ----
 * parseFromString returns a push_node() wrapper: it carries the Element
 * prototype (element-scoped queries work) but had no Document members. The
 * host installs document.title on the live global only, so a parsed
 * document's title read undefined and createElement/createTextNode/
 * createDocumentFragment were missing entirely.
 *
 * title reads the first <title> in the tree; on this parse path RCDATA is a
 * real #text child, so node_text_content sees it. The setter replaces that
 * element's content and, when the document has no title, creates one under
 * <head> (synthesizing <head> as the root element's first child when it is
 * absent, the way the HTML parser would). Parsed trees live in g_orphans;
 * mutation records are emitted the same way the textContent setter emits
 * them (the added node is reported, removed children are not). */
/* document.title getter helper extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_title_get.c"
/* document.title setter extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_title_set.c"
/* document surface/install helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_doc_surface.c"
/* DOMParser.parseFromString extracted (Phase 1, single-TU include). */
#include "nb_worker_domparser_parse.c"
/* PerformanceObserver mark delivery extracted (Phase 1, single-TU include). */
#include "nb_worker_performance_observer.c"
/* Range/Selection text and endpoint helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_helpers.c"
/* Range core set/ctor/createRange extracted (Phase 1, single-TU include). */
#include "nb_worker_range_core.c"
/* selection access helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_selection_access.c"
/* Range property/get and boundary helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_props_boundary.c"
/* Range select/collapse helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_select_collapse.c"
/* Range clone/detach/toString extracted (Phase 1, single-TU include). */
#include "nb_worker_range_clone_detach.c"
/* Range text trim/delete/prune helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_prune.c"
/* Range delete/extract core wrappers extracted (Phase 1, single-TU include). */
#include "nb_worker_range_delete.c"
/* Range clone_contents implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_range_clone_contents.c"
/* Selection implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_selection.c"
/* MutationObserver core and notify helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_mutation_core.c"
/* MutationObserver public API methods extracted (Phase 1, single-TU include). */
#include "nb_worker_mutation_api.c"
/* FormData helpers and methods extracted (Phase 1, single-TU include). */
#include "nb_worker_formdata.c"
/* Blob implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_blob.c"
/* FileReader read and FormData/Blob/FileReader install extracted (Phase 1, single-TU include). */
#include "nb_worker_filereader_install.c"
/* DOM class constructor helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_class_helpers.c"
/* element insertion arg helper and mode macros extracted (Phase 1, single-TU include). */
#include "nb_worker_node_insert_arg.c"
/* element insert/remove/replace operations extracted (Phase 1, single-TU include). */
#include "nb_worker_node_insert_ops.c"

/* element localName/matches/closest extracted (Phase 1, single-TU include). */
#include "nb_worker_el_match_closest.c"
/* element query/collection methods extracted (Phase 1, single-TU include). */
#include "nb_worker_el_query.c"

/* cloneNode/nbi_clone helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_clone.c"
/* Node.isEqualNode/isSameNode helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_equality.c"

/* getClientRects helper extracted (Phase 1, single-TU include). */
#include "nb_worker_rects.c"
/* window scroll helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_scroll.c"
/* element/window scroll metrics helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_scroll_metrics.c"

/* Node root/ownership helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_root.c"

static void install_dom_classes(JSContext *ctx) {
    /* DOM class hierarchy base setup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_base_body.c"
    /* Element prototype methods install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_methods.c"
    /* Element structural navigation getters install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_nav.c"
    /* Element scroll properties install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_scroll.c"
    /* Element insertAdjacent/outerHTML installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_el_adjacent.c"
    /* Node prototype core methods/getters install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_core.c"
    /* Node equality/root/position installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_compare.c"
    /* Node isConnected/ownerDocument/nodeValue installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_state.c"
    /* Element.remove and HTMLElement/SVGElement/Text prototype bases extracted (Phase 1, single-TU include). */
#include "nb_worker_install_proto_bases.c"
    /* Text/CharacterData prototype installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_text_cdata.c"
    /* Document/Animation prototypes and global DOM exports extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_exports.c"

    /* Node static type constants extracted (Phase 1, single-TU include). */
#include "nb_worker_install_node_constants.c"

    /* document object and timeline setup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_document_object.c"

    /* customElements and CSSStyleSheet globals extracted (Phase 1, single-TU include). */
#include "nb_worker_install_ce_cssom.c"

    /* standard element/event constructor global stubs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_global_stubs.c"
        /* FormData/Blob/FileReader and MutationObserver installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_formdata_mutation.c"
        /* Intersection/Resize/Performance observer installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_observers.c"
        /* Range global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_range.c"
        /* Selection global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_selection.c"
        /* DOMParser/XMLSerializer global installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_parser_serializer.c"
        /* NodeList/HTMLCollection global installs extracted (Phase 1, single-TU include). */
#include "nb_worker_install_list_classes.c"
        /* DOMTokenList global install extracted (Phase 1, single-TU include). */
#include "nb_worker_install_domtokenlist.c"
        /* install_dom_classes tail and cleanup extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom_tail.c"

/* minimal TTF font state and table lookup extracted (Phase 1, single-TU include). */
#include "nb_worker_font_state.c"
/* minimal TTF font load and table parsing extracted (Phase 1, single-TU include). */
#include "nb_worker_font_load.c"
/* minimal TTF glyph index and advance helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_font_glyph.c"
/* minimal TTF glyph outline decoding extracted (Phase 1, single-TU include). */
#include "nb_worker_font_points.c"
/* minimal TTF glyph raster/blit extracted (Phase 1, single-TU include). */
#include "nb_worker_font_blit.c"
/* canvas font string parsing helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_font_parse.c"
/* canvas context string property helper extracted (Phase 1, single-TU include). */
#include "nb_worker_ctx_str.c"
/* ---- canvas 2D ---- */
static JSValue nb_c2d_noop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { (void)ctx; (void)this_val; (void)argc; (void)argv; return JS_UNDEFINED; }
static void nb_grad_sample(JSContext *ctx, JSValue grad, double t, unsigned char out[4]);

static double nb_ctx_num(JSContext *ctx, JSValueConst o, const char *k, double def);
/* canvas context lookup/resize sync helper extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_get.c"
/* canvas/CSS color parsing helper extracted (Phase 1, single-TU include). */
#include "nb_worker_color_parse.c"
/* canvas pixel blend/fill helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_px.c"
/* canvas context utility helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_ctx_utils.c"
/* canvas/image source pixel resolver extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_source.c"
/* canvas 2D fillRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_fillrect.c"
/* canvas 2D strokeRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_strokerect.c"
/* canvas 2D clearRect implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_clearrect.c"
/* canvas 2D save/restore implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_save_restore.c"
/* canvas 2D transform operations extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_transform_ops.c"
/* canvas 2D putImageData implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_putimagedata.c"
/* canvas 2D image data operations extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_imagedata_ops.c"
/* canvas 2D measureText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_measuretext.c"
/* canvas 2D fillText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_filltext.c"
/* canvas 2D strokeText implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_stroketext.c"
/* canvas 2D gradient helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_gradient.c"
/* canvas 2D pattern/line-dash helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_c2d_pattern_linedash.c"
/* canvas WebGL (GLES2/EGL) implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_webgl.c"
/* canvas element getContext implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_el_getcontext.c"
/* canvas export helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_canvas_export.c"
/* ---- DOM class one JS object per C node (see PROTONAME) ---- */

/* Attach the DOM natives to the global `document` object. */
/* DOM installation helper extracted (Phase 1, single-TU include). */
#include "nb_worker_install_dom.c"
/* eval budget/watchdog and error helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_eval_watchdog.c"
/* ---- customElements registry + ShadowRoot / template-content fragments
 * (2026-09-20) ----
 * kevlar DEFERS element registration: the ytd-masthead class (with its
 * searchbox <template> string) is wired by
 * `_.f9({disableElementRegistration:!0,...})(ZW); _.OV(ZW,"ytd-masthead",...)`
 * only when `window.customElements` is present, and the masthead + search UI
 * then get created at upgrade time (createElement("template") + innerHTML +
 * attachShadow + stamp). A REAL define/get/whenDefined/upgrade + C-backed
 * ShadowRoot is the slice that starts that path; the registry is a plain
 * object customElements.__registry (plus an ordered ""__names"" array) so
 * C holds NO JSValues across calls (no GC rooting hazards). Upgraded
 * wrappers carry __nb_upgraded; the lifecycle runs best-effort (prototype
 * swap + _initializeProperties/ready/connectedCallback) with exceptions
 * dumped to the TRACE_CB channel so the bundle keeps bootstrapping. */
/* custom elements core helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_core.c"
/* custom elements upgrade helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_upgrade.c"
/* custom elements registry API extracted (Phase 1, single-TU include). */
#include "nb_worker_ce_registry_api.c"
/* fragment query helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_query.c"
/* fragment clone helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_clone.c"
/* fragment wrapper construction extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_wrapper.c"
/* document.createDocumentFragment() / new DocumentFragment(): a detached
 * #document-fragment kept on the orphan list, wrapped with the element
 * insert/query natives. Appending it MOVES its children into the target and
 * leaves it empty (see nb_insert_fragment_children). */

static JSValue nb_dom_createComment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *owned = NULL;
    const char *v = "";
    if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]))
        v = (owned = JS_ToCString(ctx, argv[0]));
    if (argc > 0 && !owned && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]))
        return JS_EXCEPTION;
    NbNode *n = calloc(1, sizeof(*n));
    if (!n) { JS_FreeCString(ctx, owned); return JS_ThrowOutOfMemory(ctx); }
    n->tag = strdup("#comment");
    n->text = strdup(v ? v : "");
    orphan_add(n);
    JS_FreeCString(ctx, owned);
    JSValue w = push_node(ctx, n);
    nb_set_owner_if_doc(ctx, w, this_val);
    return w;
}

static void nb_set_owner_prop(JSContext *ctx, JSValue wrapper, JSValueConst owner_doc) {
    if (JS_IsObject(wrapper) && JS_IsObject(owner_doc))
        JS_SetPropertyStr(ctx, wrapper, "__nb_owner", JS_DupValue(ctx, owner_doc));
}
static JSValue nb_doc_throw_dom(JSContext *ctx, const char *name, const char *msg) {
    JSValue e = JS_NewError(ctx);
    if (!JS_IsException(e)) {
        JS_SetPropertyStr(ctx, e, "name", JS_NewString(ctx, name));
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, msg));
    }
    return JS_Throw(ctx, e);
}
static JSValue nb_doc_importNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return nb_doc_throw_dom(ctx, "TypeError", "importNode requires a node");
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) return nb_doc_throw_dom(ctx, "NotFoundError", "importNode: unknown node");
    if (n->tag && !strcmp(n->tag, "#document"))
        return nb_doc_throw_dom(ctx, "NotSupportedError", "importNode: cannot import a document");
    int deep = 0;
    if (argc > 1) deep = JS_ToBool(ctx, argv[1]);
    NbNode *c = nbi_clone(n, deep);
    if (!c) return JS_ThrowOutOfMemory(ctx);
    JSValue w;
    if (nb_is_doc_fragment_node(c))
        w = make_fragment_wrapper(ctx, c, "#document-fragment");
    else
        w = push_node(ctx, c);
    nb_set_owner_prop(ctx, w, this_val);
    return w;
}
static JSValue nb_doc_adoptNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return nb_doc_throw_dom(ctx, "TypeError", "adoptNode requires a node");
    NbNode *n = get_node(ctx, argv[0]);
    if (!n) return nb_doc_throw_dom(ctx, "NotFoundError", "adoptNode: unknown node");
    if (n->tag && !strcmp(n->tag, "#document"))
        return nb_doc_throw_dom(ctx, "NotSupportedError", "adoptNode: cannot adopt a document");
    if (n->parent) node_detach(n);
    nb_set_owner_prop(ctx, argv[0], this_val);
    return JS_DupValue(ctx, argv[0]);
}


/* DocumentFragment factory extracted (Phase 1, single-TU include). */
#include "nb_worker_dom_createfragment.c"

/* template content fragment helper extracted (Phase 1, single-TU include). */
#include "nb_worker_frag_template.c"

/* Element.prototype.attachShadow implementation extracted (Phase 1, single-TU include). */
#include "nb_worker_el_attachshadow.c"

/* Release every JS callback this file currently holds (timers/events/on-props).
 * Must run BEFORE the owning context is freed; idempotent (slots are set to
 * JS_UNDEFINED after releasing, so a second call / a later per-LOAD reset
 * cannot double-free). */
static void free_held_callbacks(JSContext *ctx) {
    for (int i = 0; i < g_timer_count; i++)
        if (!JS_IsUndefined(g_timers[i].cb) && JS_IsFunction(ctx, g_timers[i].cb)) {
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_evl_count; i++)
        if (!JS_IsUndefined(g_evl[i].cb) && JS_IsFunction(ctx, g_evl[i].cb)) {
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_onprop_count; i++)
        if (!JS_IsUndefined(g_onprop[i].cb) && JS_IsFunction(ctx, g_onprop[i].cb)) {
            JS_FreeValue(ctx, g_onprop[i].cb);
            g_onprop[i].cb = JS_UNDEFINED;
        }
    for (int i = 0; i < g_mut_obs_count; i++) {
        if (!JS_IsUndefined(g_mut_obs[i].self)) JS_FreeValue(ctx, g_mut_obs[i].self);
        if (!JS_IsUndefined(g_mut_obs[i].callback)) JS_FreeValue(ctx, g_mut_obs[i].callback);
        g_mut_obs[i].self = g_mut_obs[i].callback = JS_UNDEFINED;
        g_mut_obs[i].target_count = 0;
    }
    g_mut_obs_count = 0;
    for (int i = 0; i < g_int_obs_count; i++) {
        if (!JS_IsUndefined(g_int_obs[i].self)) JS_FreeValue(ctx, g_int_obs[i].self);
        if (!JS_IsUndefined(g_int_obs[i].callback)) JS_FreeValue(ctx, g_int_obs[i].callback);
        if (JS_IsObject(g_int_obs[i].root_js)) JS_FreeValue(ctx, g_int_obs[i].root_js);
        for (int ti = 0; ti < g_int_obs[i].target_count; ti++)
            if (JS_IsObject(g_int_obs[i].targets[ti].target_js)) JS_FreeValue(ctx, g_int_obs[i].targets[ti].target_js);
        g_int_obs[i].self = g_int_obs[i].callback = JS_UNDEFINED;
        g_int_obs[i].root_js = JS_UNDEFINED;
        g_int_obs[i].target_count = 0;
    }
    g_int_obs_count = 0;
    for (int i = 0; i < g_ro_obs_count; i++) {
        if (!JS_IsUndefined(g_ro_obs[i].self)) JS_FreeValue(ctx, g_ro_obs[i].self);
        if (!JS_IsUndefined(g_ro_obs[i].callback)) JS_FreeValue(ctx, g_ro_obs[i].callback);
        for (int ti = 0; ti < g_ro_obs[i].target_count; ti++)
            if (JS_IsObject(g_ro_obs[i].targets[ti].target_js)) JS_FreeValue(ctx, g_ro_obs[i].targets[ti].target_js);
        g_ro_obs[i].self = g_ro_obs[i].callback = JS_UNDEFINED;
        g_ro_obs[i].target_count = 0;
    }
    g_ro_obs_count = 0;
    for (int i = 0; i < g_po_obs_count; i++) {
        if (!JS_IsUndefined(g_po_obs[i].self)) JS_FreeValue(ctx, g_po_obs[i].self);
        if (!JS_IsUndefined(g_po_obs[i].callback)) JS_FreeValue(ctx, g_po_obs[i].callback);
        g_po_obs[i].self = g_po_obs[i].callback = JS_UNDEFINED;
    }
    g_po_obs_count = 0;
}

/* Run the callback held in `cb` with this=globalThis, 0 args.
 * Returns 0 on success, 1 on thrown error (message captured, exception freed). */
static int invoke_cb0(JSContext *ctx, JSValue cb) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, cb, g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        /* diagnosis aid: NB_TRACE_CB=1 prints the FULL callback exception
         * (message + stack when NB_STACK=1) to stderr, even when the page's
         * own error handler swallows it into a truncated console line. */
        if (g_trace_cb) {
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            fprintf(stderr, "TRACE_CB| %s\n", m);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}

/* ---- timers ---- */
static int timer_schedule(JSContext *ctx, JSValueConst cb, long interval, int repeat) {
    if (!JS_IsFunction(ctx, cb) || g_timer_count >= MAX_TIMERS) return 0;
    int slot = g_timer_count++;
    g_timers[slot].id = g_next_id++;
    g_timers[slot].active = 1;
    g_timers[slot].interval = repeat ? (interval > 0 ? interval : 1) : 0;
    g_timers[slot].due = now_ms() + (uint64_t)(interval > 0 ? interval : 1);
    g_timers[slot].slot = slot;
    g_timers[slot].cb = JS_DupValue(ctx, cb);
    return g_timers[slot].id;
}
static JSValue nb_timer_setTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 0) ms = 0;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 0));
}
static JSValue nb_timer_setInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double ms = 0;
    if (argc > 1 && JS_IsNumber(argv[1])) (void)JS_ToFloat64(ctx, &ms, argv[1]);
    if (ms < 1) ms = 1;
    return JS_NewInt32(ctx, timer_schedule(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, (long)ms, 1));
}
static void timer_clear(JSContext *ctx, int want_oneshot, JSValueConst idv) {
    if (!JS_IsNumber(idv)) return;
    double d;
    if (JS_ToFloat64(ctx, &d, idv) != 0) return;
    int id = (int)d;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active && g_timers[i].id == id &&
            (want_oneshot ? g_timers[i].interval == 0 : g_timers[i].interval > 0)) {
            g_timers[i].active = 0;
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
            break;
        }
}
static JSValue nb_timer_clearTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 1, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_timer_clearInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    timer_clear(ctx, 0, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}

/* ---- microtasks + rAF ---- */
/* A queueMicrotask() callback, executed by the QuickJS native job queue
 * (JS_EnqueueJob) inside run_event_loop's drain; `this` = globalThis. */
static JSValue microtask_job(JSContext *ctx, int argc, JSValueConst *argv) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue r = JS_Call(ctx, argv[0], g, 0, NULL);
    JS_FreeValue(ctx, g);
    if (JS_IsException(r)) {
        if (!g_pending_err) {
            g_pending_err = 1;
            char buf[512];
            const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
            snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
        }
        JS_FreeValue(ctx, r);
    } else {
        JS_FreeValue(ctx, r);
    }
    return JS_UNDEFINED;
}
static JSValue nb_queueMicrotask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        JS_ThrowTypeError(ctx, "queueMicrotask requires a function argument");
        return JS_EXCEPTION;
    }
    if (JS_EnqueueJob(ctx, microtask_job, 1, argv) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}
static JSValue nb_raf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc > 0 && JS_IsFunction(ctx, argv[0]) && g_raf_fires < MAX_RAF_FRAMES) {
        g_raf_fires++;
        timer_schedule(ctx, argv[0], RAF_MS, 0);
    }
    return JS_UNDEFINED;
}
/* ---- rung 4: fetch / XHR transport — blocking curl child, Promise-shaped ---- */
/* The JS prelude (nb_host.h) wraps this in a Promise polyfill + fetch() +
 * XMLHttpRequest. Blocking is fine: the drain loop afterwards exhausts the
 * microtask queue, so .then() chains render before RENDER. The manager stays
 * parse-time network owner; the worker is JS-time network owner. */
static void cfg_put(FILE *f, const char *val) {
    for (const char *p = val; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
}
static void cfg_line(FILE *f, const char *key, const char *val) {
    fputs(key, f); fputs(" = \"", f); cfg_put(f, val); fputs("\"\n", f);
}
static void cfg_data(FILE *f, const char *val) {
    fputs("data = \"", f);
    for (const char *p = val; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 && c != '\t') { fputc(' ', f); continue; }  /* curl config is single-line */
        if (c == '"') fputs("\\\"", f);
        else if (c == '\\') fputs("\\\\", f);
        else fputc(c, f);
    }
    fputs("\"\n", f);
}

/* Resolve a fetch/XHR URL against the current document URL (g_href), the
 * same RFC 3986 §5-style merge the manager does for script srcs:
 *   already-schemed   -> as-is (file:, http:, https:, data:, ...)
 *   //host/[...]      -> scheme of g_href + the rest
 *   /abs/path         -> scheme://host of g_href + the rest
 *   rel/path          -> scheme://host + dirname(g_href's path) + the rest
 * g_href may itself be file:///... (file-based suites keep working). */
static void resolve_doc_url(const char *rel, char *out, size_t olen) {
    out[0] = 0;
    if (!rel || !rel[0]) return;
    const char *sc = strstr(g_href, "://");
    const char *rp = rel;
    while (*rp && *rp != ':' && *rp != '/' && *rp != '?' && *rp != '#') rp++;
    if (*rp == ':') { snprintf(out, olen, "%s", rel); return; }      /* has scheme */
    if (strncmp(rel, "//", 2) == 0 && sc) {
        snprintf(out, olen, "%.*s%s", (int)(sc - g_href + 3), g_href, rel);
        return;
    }
    const char *hp = sc ? sc + 3 : NULL;
    const char *hostslash = hp ? strchr(hp, '/') : NULL;
    if (rel[0] == '/') {
        if (sc && hostslash) snprintf(out, olen, "%.*s%s",
                                      (int)(hostslash - g_href), g_href, rel);
        else if (sc) snprintf(out, olen, "%s%s", g_href, rel);
        else snprintf(out, olen, "%s", rel);
        return;
    }
    if (sc && hp) {
        const char *dlast = strrchr(hp, '/');
        if (dlast && dlast > hp)
            snprintf(out, olen, "%.*s%s", (int)(dlast - g_href + 1), g_href, rel);
        else if (hostslash)
            snprintf(out, olen, "%.*s%s", (int)(hostslash - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s/%s", g_href, rel);
    } else {
        const char *lst = strrchr(g_href, '/');
        if (lst) snprintf(out, olen, "%.*s%s", (int)(lst - g_href + 1), g_href, rel);
        else snprintf(out, olen, "%s", rel);
    }
}

/* Try manager RPC for fetch (async per spec §8.2): worker -> manager FETCH, manager -> worker FETCHED.
 * Returns 1 if manager handled it (out_body/status set), 0 to fallback to direct curl. */
/* How long to wait for the manager's FETCHED before giving up and letting
 * the caller fall back to a direct curl. The direct path's own max-time is
 * 8s, so this is a little more than double: generous enough that a slow
 * manager is never mistaken for a dead one, bounded enough that an
 * unanswered FETCH cannot wedge a resident worker. */
#define FETCH_RPC_WAIT_MS 15000

/* Worker asks the manager to perform a fetch (async, spec 8.2).
 *
 * Frame out:  FETCH\n<id>\n<method>\n<url>\n<nreq>\n<h1>..\n<hN>\n\n<body>
 * Frame in :  FETCHED\n<id>\n<status>\n<nresp>\n<r1>..\n<rM>\n\n<body>
 *
 * The nreq/nresp tail is what the protocol was missing. It used to be
 * FETCH\n<id>\n<method>\n<url> -> FETCHED\n<id>\n<status>\n<body>, which
 * dropped every page-controlled request header and the whole request body
 * and could not return Set-Cookie. worker_sapisid_test could not deliver
 * its Authorization: SAPISIDHASH header (fixture said sign-fail) and
 * worker_fetch_post_test hung with the POST body stranded here.
 *
 * Cookie handling deliberately mirrors the direct-curl path rather than
 * being left to the manager: attach jar cookies outbound, ingest Set-Cookie
 * from the returned headers inbound. Both sides share one NB_COOKIES_FILE,
 * and one implementation means the two paths cannot drift.
 *
 * Returns 0 for "manager could not do it" in every failure mode - no
 * manager, timeout, short/mismatched reply, unparsable URL - and the
 * caller then uses the direct path. A manager that never replies therefore
 * costs one bounded wait, not a hang.
 */
static int try_fetch_via_manager(const char *method, const char *url,
                                  const char *headers, const char *body,
                                  char **out_body, size_t *out_len,
                                  int *out_status, char *errbuf, size_t errcap) {
    if (g_cli) return 0;
    if (isatty(STDIN_FILENO)) return 0;
    static int next_id = 1;
    int id = next_id++;

    /* split the page's headers into individual lines */
    char reqh[16][512];
    int nreq = 0;
    if (headers && *headers) {
        const char *hp = headers;
        while (*hp && nreq < 16) {
            const char *nl = strchr(hp, '\n');
            size_t n = nl ? (size_t)(nl - hp) : strlen(hp);
            while (n && (hp[n-1] == '\r' || hp[n-1] == ' ')) n--;
            if (n && memchr(hp, ':', n) && n < sizeof reqh[0]) {
                memcpy(reqh[nreq], hp, n); reqh[nreq][n] = '\0';
                nreq++;
            }
            if (!nl) break;
            hp = nl + 1;
        }
    }
    /* unified jar, same as the direct path: cookies go out with the request */
    char ckhdr[4096];
    ckhdr[0] = '\0';
    cookie_header_for_url(url, ckhdr, sizeof(ckhdr));
    if (ckhdr[0] && nreq < 16) {
        snprintf(reqh[nreq], sizeof reqh[0], "%s", ckhdr);   /* already "Cookie: ..." */
        nreq++;
    }

    static char payload[65536];
    int n = snprintf(payload, sizeof(payload), "FETCH\n%d\n%s\n%s\n%d",
                     id, method, url, nreq);
    for (int i = 0; i < nreq && n > 0 && (size_t)n < sizeof(payload); i++)
        n += snprintf(payload + n, sizeof(payload) - n, "\n%s", reqh[i]);
    if (n > 0 && (size_t)n < sizeof(payload))
        n += snprintf(payload + n, sizeof(payload) - n, "\n\n%s", (body && *body) ? body : "");
    if (n < 0 || (size_t)n >= sizeof(payload)) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "fetch request too large");
        return 0;
    }
    send_payload(payload, (size_t)n);

    if (!recv_frame_budget(FETCH_RPC_WAIT_MS)) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "manager fetch: no reply");
        return 0;
    }
    if (strncmp(g_rbuf, "FETCHED\n", 8) != 0) {
        if (errbuf && errcap) snprintf(errbuf, errcap, "manager fetch: unexpected reply");
        return 0;
    }
    char *p = g_rbuf + 8;
    char *n1 = strchr(p, '\n');
    if (!n1) return 0;
    *n1 = '\0';
    int r_id = atoi(p);
    if (r_id != id) { *n1 = '\n'; return 0; }
    char *q2 = n1 + 1;
    char *n2 = strchr(q2, '\n');
    if (!n2) { *n1 = '\n'; return 0; }
    *n2 = '\0';
    int status = atoi(q2);

    char *rest = n2 + 1;
    /* <nresp> - if this field is not a plain count, the manager is old and
     * the remainder of the frame is the body (back-compat). */
    char *e3 = strchr(rest, '\n');
    int nresp = 0;
    int old_format = 1;
    if (e3) {
        char cnt[16];
        size_t cl = (size_t)(e3 - rest);
        if (cl < sizeof(cnt)) {
            memcpy(cnt, rest, cl); cnt[cl] = '\0';
            char *endp = NULL;
            long v = strtol(cnt, &endp, 10);
            if (endp && *endp == '\0' && v >= 0 && v <= 32) { nresp = (int)v; old_format = 0; }
        }
    }
    if (!old_format) {
        char *cur = e3 + 1;
        for (int i = 0; i < nresp; i++) {
            char *e = strchr(cur, '\n');
            size_t hl = e ? (size_t)(e - cur) : strlen(cur);
            if (hl && strncasecmp(cur, "Set-Cookie:", 11) == 0) {
                char val[512];
                if (hl - 11 < sizeof(val)) {
                    memcpy(val, cur + 11, hl - 11); val[hl - 11] = '\0';
                    char *v = val;
                    while (*v == ' ') v++;
                    cookie_set_from_wire(v, url);
                }
            }
            if (!e) { cur += hl; break; }
            cur = e + 1;
        }
        /* blank separator line, then the body */
        if (*cur == '\n') cur++;
        rest = cur;
    }

    size_t body_len = g_rlen - (size_t)(rest - g_rbuf);
    char *rb = (char *)malloc(body_len + 1);
    if (!rb) { *n1 = '\n'; *n2 = '\n'; return 0; }
    memcpy(rb, rest, body_len);
    rb[body_len] = '\0';
    *out_body = rb;
    *out_len = body_len;
    *out_status = status;
    *n1 = '\n'; *n2 = '\n';
    if (errbuf && errcap) errbuf[0] = '\0';
    return 1;
}

static JSValue nb_fetch_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *m_own = NULL, *u_own = NULL, *h_own = NULL, *b_own = NULL;
    const char *method = NULL, *url = NULL, *headers = NULL, *body = NULL;
    if (argc > 0) method = (m_own = JS_ToCString(ctx, argv[0]));
    if (argc > 1) url = (u_own = JS_ToCString(ctx, argv[1]));
    if (argc > 2) headers = (h_own = JS_ToCString(ctx, argv[2]));
    if (argc > 3) body = (b_own = JS_ToCString(ctx, argv[3]));
    if (!method || !url) {
        JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
        JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
        JS_ThrowTypeError(ctx, "nbFetchSync needs method and url strings");
        return JS_EXCEPTION;
    }
    if (!headers) headers = "";
    if (!body) body = "";

    char urlb[2300];
    resolve_doc_url(url, urlb, sizeof(urlb));
    url = urlb;

    char *rb = NULL; size_t rn = 0; int status = 0; char errbuf[256] = "";

    nb_watchdog_pause();   /* a blocking curl must never trip the EVAL_BUDGET_SEC watchdog */

    if (strncmp(url, "file:", 5) == 0) {
        /* normalize file://host/path, file:///path, file:/path -> /path */
        const char *p = url + 5;
        while (*p == '/') p++;
        if (strncmp(p, "localhost", 9) == 0 && p[9] == '/') p += 10;
        char abspath[2048];
        snprintf(abspath, sizeof(abspath), "/%s", p);
        if (read_file(abspath, &rb, &rn)) status = 200;
        else snprintf(errbuf, sizeof(errbuf), "cannot read %s", abspath);
    } else if (strncmp(url, "http:", 5) == 0 || strncmp(url, "https:", 6) == 0) {
        char *mgr_body = NULL; size_t mgr_len = 0; int mgr_status = 0; char mgr_err[256] = "";
        /* Manager RPC caps bodies at 60KB and NUL-truncates binary in the
         * text frame: a 200 with an empty body means "too big for RPC",
         * not success - fall through to direct curl below (2026-10-07:
         * every remote image died exactly this way). */
        if (try_fetch_via_manager(method, url, headers, body, &mgr_body, &mgr_len, &mgr_status, mgr_err, sizeof(mgr_err)) &&
            !(mgr_status == 200 && mgr_len == 0)) {
            rb = mgr_body; rn = mgr_len; status = mgr_status;
            if (mgr_err[0]) snprintf(errbuf, sizeof(errbuf), "%s", mgr_err);
        } else {
            free(mgr_body);
        char cfgpath[1024] = "", bodypath[1024] = "";
        char t1[] = "/tmp/nbfetch.XXXXXX", t2[] = "/tmp/nbfetchbody.XXXXXX";
        int fd1 = mkstemp(t1), fd2 = mkstemp(t2);
        if (fd1 < 0 || fd2 < 0) { snprintf(errbuf, sizeof(errbuf), "mkstemp failed"); }
        else {
            close(fd1); close(fd2);
            snprintf(cfgpath, sizeof(cfgpath), "%s", t1);
            snprintf(bodypath, sizeof(bodypath), "%s", t2);
            FILE *cf = fopen(cfgpath, "w");
            if (!cf) snprintf(errbuf, sizeof(errbuf), "cannot write curl config");
            else {
                cfg_line(cf, "url", url);
                cfg_line(cf, "user-agent", "Mozilla/5.0 (NNEST network-browser-hq nb-js-worker rung4)");
                cfg_line(cf, "max-time", "8");
                fputs("silent\nlocation\nfail\n", cf);
                /* unified cookie jar: attach matching cookies from our jar
                 * (Chromium-parity — one store, both directions). */
                {
                    char ckhdr[4096];
                    cookie_header_for_url(url, ckhdr, sizeof(ckhdr));
                    if (ckhdr[0]) cfg_line(cf, "header", ckhdr);
                }
                /* dump response headers so we can parse Set-Cookie into jar */
                char hdrpath[256] = "";
                {
                    char t3[] = "/tmp/nbfetchhdr.XXXXXX";
                    int fd3 = mkstemp(t3);
                    if (fd3 >= 0) { close(fd3); snprintf(hdrpath, sizeof(hdrpath), "%s", t3); }
                }
                if (hdrpath[0]) cfg_line(cf, "dump-header", hdrpath);
                /* one header = line per raw "Name: value" line (no strtok_r) */
                for (const char *p = headers; *p; ) {
                    const char *nl = strchr(p, '\n');
                    size_t n = nl ? (size_t)(nl - p) : strlen(p);
                    while (n && (p[n-1] == '\r' || p[n-1] == ' ')) n--;
                    if (n && memchr(p, ':', n)) {
                        char hb[512];
                        if (n >= sizeof(hb)) n = sizeof(hb) - 1;
                        memcpy(hb, p, n); hb[n] = 0;
                        cfg_line(cf, "header", hb);
                    }
                    p += n + (nl ? 1 : 0);
                }
                if (body[0]) cfg_data(cf, body);
                cfg_line(cf, "request", method);
                fputs("output = \"", cf); cfg_put(cf, bodypath); fputs("\"\n", cf);
                fputs("write-out = \"%{http_code}\"\n", cf);
                fclose(cf);
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "curl --compressed -sS -K '%s' 2>/dev/null", cfgpath);
                FILE *po = popen(cmd, "r");
                if (po) {
                    char code[16] = ""; size_t got = 0;
                    while (got + 1 < sizeof(code)) {
                        int c = fgetc(po);
                        if (c == EOF) break;
                        code[got++] = (char)c;
                    }
                    code[got] = 0;
                    int rc = pclose(po);
                    status = (int)strtol(code, NULL, 10);
                    if (status <= 0) status = 0;
                    /* Page/asset bodies are legitimately larger than the
                     * 512KB fs-lite guard: read_file() returned 0 for a
                     * 940KB homepage, so nbFetchSync reported status 200 with
                     * an empty body (measured 2026-10-26 on
                     * www.coinmarketcap.com). read_file_big is the existing
                     * chunked reader with a 64MB ceiling. */
                    if (status > 0 && read_file_big(bodypath, &rb, &rn)) {
                        /* treats zero-byte bodies as a successful empty read */
                    }
                    if (!rb) snprintf(errbuf, sizeof(errbuf),
                                      "curl rc=%d status=%d url=%s",
                                      rc, status, url);
                } else snprintf(errbuf, sizeof(errbuf), "popen curl failed");
                /* ingest Set-Cookie response headers into the unified jar */
                if (hdrpath[0] && status >= 200 && status < 400) {
                    char *hbuf = NULL; size_t hbn = 0;
                    if (read_file(hdrpath, &hbuf, &hbn)) {
                        char *hp = hbuf;
                        while (hp && *hp) {
                            char *nl = strchr(hp, '\n');
                            if (nl) *nl = 0;
                            char *cr = hp + strlen(hp);
                            if (cr > hp && cr[-1] == '\r') cr[-1] = 0;
                            if (strncasecmp(hp, "Set-Cookie:", 11) == 0) {
                                const char *val = hp + 11;
                                while (*val == ' ') val++;
                                cookie_set_from_wire(val, url);
                            }
                            hp = nl ? nl + 1 : NULL;
                        }
                        free(hbuf);
                    }
                }
                unlink(cfgpath);
                unlink(bodypath);
                if (hdrpath[0]) unlink(hdrpath);
            }
        }
        }
    } else {
        /* data: URLs can carry small inline blobs; everything else is refused */
        if (strncmp(url, "data:", 5) == 0) {
            const char *c = strchr(url, ',');
            rb = strdup(c ? c + 1 : "");
            rn = rb ? strlen(rb) : 0;
            status = 200;
        } else snprintf(errbuf, sizeof(errbuf), "unsupported scheme in %s", url);
    }

    alarm(nb_budget());   /* re-arm the budget for the rest of the drain */

    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "ok", JS_NewBool(ctx, status >= 200 && status < 300 && rb != NULL));
    JS_SetPropertyStr(ctx, o, "status", JS_NewInt32(ctx, status));
    JS_SetPropertyStr(ctx, o, "body", JS_NewString(ctx, rb ? rb : ""));
    /* Binary-safe twin: JS strings NUL-truncate (PNG dies). bytes carries
     * the exact rn bytes; img decode prefers it (2026-10-07: remote Image
     * loads decoded nothing because of the truncation). */
    {
        JSValue len = JS_NewInt32(ctx, (int32_t)(rn > INT32_MAX ? INT32_MAX : rn));
        JSValue gobj = JS_GetGlobalObject(ctx);
        JSValue ctor = JS_GetPropertyStr(ctx, gobj, "Uint8Array");
        JS_FreeValue(ctx, gobj);
        JSValue data = (JS_IsObject(ctor) && !JS_IsNull(ctor))
                           ? JS_CallConstructor(ctx, ctor, 1, &len)
                           : JS_NewArray(ctx);
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, len);
        size_t fill = rn;
        for (size_t i = 0; i < fill; i++) {
            JSValue e = JS_NewInt32(ctx, rb ? (unsigned char)rb[i] : 0);
            JS_SetPropertyUint32(ctx, data, (uint32_t)i, e);
        }
        JS_SetPropertyStr(ctx, o, "bytes", data);
        JS_SetPropertyStr(ctx, o, "byteLength", JS_NewInt32(ctx, (int32_t)(rn > INT32_MAX ? INT32_MAX : rn)));
    }
    JS_SetPropertyStr(ctx, o, "error", JS_NewString(ctx, errbuf[0] ? errbuf : ""));
    free(rb);
    JS_FreeCString(ctx, m_own); JS_FreeCString(ctx, u_own);
    JS_FreeCString(ctx, h_own); JS_FreeCString(ctx, b_own);
    return o;
}

static uint64_t timer_min_due(void) {
    uint64_t m = 0; int have = 0;
    for (int i = 0; i < g_timer_count; i++)
        if (g_timers[i].active) { if (!have || g_timers[i].due < m) { m = g_timers[i].due; have = 1; } }
    return have ? m : 0;
}
/* Drain the native job queue (promise reactions + queueMicrotask callbacks). */
static void drain_jobs(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *jctx;
    while (JS_IsJobPending(rt)) {
        if (g_pending_err) return;
        if (JS_ExecutePendingJob(rt, &jctx) < 0) {
            if (jctx && !g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(jctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            if (g_trace_cb && jctx) {
                char buf[512];
                const char *m = js_error_to_cstr(jctx, buf, sizeof(buf));
                fprintf(stderr, "TRACE_CB| %s\n", m);
            }
        }
    }
}
static int run_due_timers(JSContext *ctx, uint64_t now) {
    int ran = 0;
    for (int i = 0; i < g_timer_count; i++) {
        if (!g_timers[i].active || g_timers[i].due > now) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        long iv = g_timers[i].interval;
        if (invoke_cb0(ctx, g_timers[i].cb)) { g_invocations++; break; }
        g_invocations++; ran = 1;
        if (iv > 0) g_timers[i].due = now + (uint64_t)iv;   /* repeating — re-arm */
        else {
            g_timers[i].active = 0;                          /* oneshot */
            JS_FreeValue(ctx, g_timers[i].cb);
            g_timers[i].cb = JS_UNDEFINED;
        }
    }
    return ran;
}

/* ---- events (EventTarget add/removeEventListener; dispatch is commit 8) ---- */
/* Fetch an attribute from the JS global object (returns owned value). */
static JSValue get_global_attr(JSContext *ctx, const char *name) {
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, g, name);
    JS_FreeValue(ctx, g);
    return v;
}
static void evl_add(JSContext *ctx, int kind, NbNode *n, JSValueConst typev, JSValueConst cb) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    if (!type || !type[0] || !JS_IsFunction(ctx, cb) || g_evl_count >= MAX_EVENTS) { JS_FreeCString(ctx, to); return; }
    int slot = g_evl_count++;
    g_evl[slot].kind = kind; g_evl[slot].node = n; g_evl[slot].slot = slot; g_evl[slot].active = 1;
    snprintf(g_evl[slot].type, sizeof(g_evl[slot].type), "%s", type);
    g_evl[slot].cb = JS_DupValue(ctx, cb);
    JS_FreeCString(ctx, to);
}
static void evl_del(JSContext *ctx, int kind, NbNode *n, JSValueConst typev) {
    char *to = NULL;
    const char *type = NULL;
    if (JS_IsString(typev)) type = (to = JS_ToCString(ctx, typev));
    for (int i = 0; i < g_evl_count; i++)
        if (g_evl[i].active && g_evl[i].kind == kind && g_evl[i].node == n &&
            (!type || !type[0] || !strcmp(g_evl[i].type, type))) {
            g_evl[i].active = 0;
            JS_FreeValue(ctx, g_evl[i].cb);
            g_evl[i].cb = JS_UNDEFINED;
            break;
        }
    JS_FreeCString(ctx, to);
}
static JSValue nb_el_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_NODE, get_this(ctx, this_val),
            argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_el_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_NODE, get_this(ctx, this_val), argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_DOC, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_add(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED, argc > 1 ? argv[1] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    evl_del(ctx, EVT_WIN, NULL, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_event_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue c;
    if (JS_IsObject(this_val) && !JS_IsException((c = JS_GetPropertyStr(ctx, this_val, "cancelable")))) {
        int can = JS_ToBool(ctx, c);
        if (can < 0) can = 0;
        JS_FreeValue(ctx, c);
        if (can) JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, 1));
    } else if (JS_IsObject(this_val)) JS_FreeValue(ctx, c);
    return JS_UNDEFINED;
}
static JSValue nb_event_stopPropagation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (JS_IsObject(this_val)) JS_SetPropertyStr(ctx, this_val, "propagationStopped", JS_NewBool(ctx, 1));
    return JS_UNDEFINED;
}
/* ---- on-* handlers: el.onclick = fn (C-side registry, per kind+node+type).
 * The wrapper objects carry a generic accessor per key; magic identifies the
 * ONPROPS key (native signature gains `int magic`). */
static int onprop_find(int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_onprop_count; i++) {
        if (!g_onprop[i].active) continue;
        if (g_onprop[i].kind != kind || g_onprop[i].node != n) continue;
        if (strcmp(g_onprop[i].type, type)) continue;
        return i;
    }
    return -1;
}
static const char *onprop_type_from_magic(int magic) {
    if (magic < 0 || !ONPROPS[magic]) return NULL;
    return ONPROPS[magic];
}
static int onprop_set_core(JSContext *ctx, int kind, NbNode *n, int magic, JSValueConst cb) {
    const char *type = onprop_type_from_magic(magic);
    if (!type || !JS_IsFunction(ctx, cb)) return 0;
    int i = onprop_find(kind, n, type);
    if (i < 0) {
        if (g_onprop_count >= MAX_ONPROPS) return 0;
        i = g_onprop_count++;
        g_onprop[i].kind = kind; g_onprop[i].node = n;
        snprintf(g_onprop[i].type, sizeof(g_onprop[i].type), "%s", type);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    } else {
        JS_FreeValue(ctx, g_onprop[i].cb);
        g_onprop[i].cb = JS_DupValue(ctx, cb);
    }
    g_onprop[i].active = 1;
    g_onprop[i].slot = i;
    return 0;
}
static JSValue nb_el_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_NODE, get_this(ctx, this_val), magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_doc_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_DOC, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_win_onprop_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    onprop_set_core(ctx, EVT_WIN, NULL, magic, argc > 0 ? argv[0] : JS_UNDEFINED);
    return JS_UNDEFINED;
}
static JSValue nb_onprop_get_core(JSContext *ctx, int kind, NbNode *n, int magic) {
    const char *type = onprop_type_from_magic(magic);
    if (!type) return JS_UNDEFINED;
    int i = onprop_find(kind, n, type);
    if (i < 0 || !g_onprop[i].active) return JS_UNDEFINED;
    return JS_DupValue(ctx, g_onprop[i].cb);
}
static JSValue nb_el_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_NODE, get_this(ctx, this_val), magic);
}
static JSValue nb_doc_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_DOC, NULL, magic);
}
static JSValue nb_win_onprop_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    return nb_onprop_get_core(ctx, EVT_WIN, NULL, magic);
}
/* one dispatch level for a single node/window/document: exact evl registrations,
 * then the on-* callback. Each target scans the registers from scratch (an
 * entry's kind+node binds it to exactly one target), so bubbling reaches the
 * document/window levels without a shared cursor skipping their listeners. */
static void dispatch_level(JSContext *ctx, int kind, NbNode *node, JSValue ev,
                           const char *type, int *stopped) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != node) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != node) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) return;
        JSValue thr = (kind == EVT_NODE && node) ? push_node(ctx, node)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");   /* this */
        JSValue argv[1]; argv[0] = ev;                     /* event argument */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r); g_invocations++; return;
        }
        JS_FreeValue(ctx, r); g_invocations++;
        JSValue sp = JS_GetPropertyStr(ctx, ev, "propagationStopped");
        *stopped = JS_ToBool(ctx, sp);
        if (*stopped < 0) *stopped = 0;
        JS_FreeValue(ctx, sp);
        if (*stopped) return;
    }
}
static int dispatch_event(JSContext *ctx, int kind, NbNode *node, JSValue ev, int bubbles) {
    JSValue tv = JS_GetPropertyStr(ctx, ev, "type");
    char *to = NULL;
    const char *type = JS_IsString(tv) ? (to = JS_ToCString(ctx, tv)) : NULL;
    JS_FreeValue(ctx, tv);
    if (!type) return 1;
    int stopped = 0;

    if (kind == EVT_NODE && node) {
        JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, node));
        JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, node));
        dispatch_level(ctx, EVT_NODE, node, ev, type, &stopped);
        if (!stopped && bubbles) {               /* bubble node->...->root->document->window */
            NbNode *a = node->parent;
            while (a) {
                if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
                JS_SetPropertyStr(ctx, ev, "currentTarget", push_node(ctx, a));
                dispatch_level(ctx, EVT_NODE, a, ev, type, &stopped);
                if (stopped) break;
                a = a->parent;
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
                dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
            }
            if (!stopped) {
                JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
                dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
            }
        }
    } else if (kind == EVT_DOC) {
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "document"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "document"));
        dispatch_level(ctx, EVT_DOC, NULL, ev, type, &stopped);
        if (!stopped) {
            JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
            dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
        }
    } else {                                       /* EVT_WIN */
        JS_SetPropertyStr(ctx, ev, "target", get_global_attr(ctx, "window"));
        JS_SetPropertyStr(ctx, ev, "currentTarget", get_global_attr(ctx, "window"));
        dispatch_level(ctx, EVT_WIN, NULL, ev, type, &stopped);
    }
    JS_FreeCString(ctx, to);
    JSValue dpv = JS_GetPropertyStr(ctx, ev, "defaultPrevented");
    int dp = JS_ToBool(ctx, dpv);
    if (dp < 0) dp = 0;
    JS_FreeValue(ctx, dpv);
    return !dp;
}

static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static unsigned char *b64_decode(const char *in, size_t *out_len) {
    size_t len = strlen(in);
    size_t out_cap = (len * 3) / 4 + 4;
    unsigned char *out = (unsigned char *)malloc(out_cap);
    if (!out) return NULL;
    size_t o = 0; int v = 0, bits = -8;
    for (size_t i = 0; i < len; i++) {
        char c = in[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int d = b64_val(c);
        if (d < 0) continue;
        v = (v << 6) + d;
        bits += 6;
        if (bits >= 0) { out[o++] = (unsigned char)((v >> bits) & 0xFF); bits -= 8; }
    }
    *out_len = o;
    return out;
}
static JSValue nb_img_naturalWidth_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewInt32(ctx, w);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); w = ww; h = hh; free(png_data); return JS_NewInt32(ctx, w); }
                free(png_data);
            }
        }
    }
    return JS_NewInt32(ctx, 0);
}
static JSValue nb_img_naturalHeight_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewInt32(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewInt32(ctx, h);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); w = ww; h = hh; free(png_data); return JS_NewInt32(ctx, h); }
                free(png_data);
            }
        }
    }
    return JS_NewInt32(ctx, 0);
}
static JSValue nb_img_complete_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_NewBool(ctx, 0);
    int w, h; if (img_get_decoded(n, &w, &h)) return JS_NewBool(ctx, 1);
    const char *src = img_get_src(n);
    if (src && strncmp(src, "data:image/", 11) == 0) {
        const char *comma = strchr(src, ',');
        if (comma && strstr(src, ";base64,")) {
            size_t png_len = 0; unsigned char *png_data = b64_decode(comma+1, &png_len);
            if (png_data && png_len) {
                int ww = 0, hh = 0, comp = 0;
                unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &ww, &hh, &comp, 4);
                if (rgba) { img_set_decoded(n, ww, hh, rgba); return JS_NewBool(ctx, 1); }
                free(png_data);
            }
        }
    }
    return JS_NewBool(ctx, 0);
}
/* HTMLImageElement src accessor — Step 1: fetch via nb_fetch_sync and fire load/error.
 * No decode yet; just verifies that img src triggers network and onload. */
static JSValue nb_img_src_get(JSContext *ctx, JSValueConst this_val) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    return JS_NewString(ctx, img_get_src(n));
}
static JSValue nb_img_src_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n) return JS_UNDEFINED;
    if (argc < 1) return JS_UNDEFINED;
    JSValueConst val2 = argv[0];
    JSValue tmp = JS_ToString(ctx, val2);
    char *s = JS_ToCString(ctx, tmp);
    JS_FreeValue(ctx, tmp);
    if (s) {
        img_set_src(n, s);
        if (s[0]) {
            JSValue args[2];
            args[0] = JS_NewString(ctx, "GET");
            args[1] = JS_NewString(ctx, s);
            JSValue res = nb_fetch_sync(ctx, JS_UNDEFINED, 2, args);
            JSValue okv = JS_GetPropertyStr(ctx, res, "ok");
            int ok = JS_ToBool(ctx, okv);
            JS_FreeValue(ctx, okv);
            if (ok) {
                /* Prefer the binary-safe bytes field; the body string
                 * NUL-truncates binary payloads (PNG dies at IHDR). */
                unsigned char *img_bin = NULL; size_t img_n = 0;
                JSValue bytesv = JS_GetPropertyStr(ctx, res, "bytes");
                JSValue blv = JS_IsObject(bytesv) ? JS_GetPropertyStr(ctx, bytesv, "length") : JS_UNDEFINED;
                int32_t blen = 0;
                if (JS_IsNumber(blv)) JS_ToInt32(ctx, &blen, blv);
                JS_FreeValue(ctx, blv);
                if (blen > 0 && blen <= 64 * 1024 * 1024) {
                    img_bin = malloc((size_t)blen);
                    if (img_bin) {
                        img_n = (size_t)blen;
                        for (int32_t i = 0; i < blen; i++) {
                            JSValue e = JS_GetPropertyUint32(ctx, bytesv, (uint32_t)i);
                            int32_t ev = 0;
                            if (JS_IsNumber(e)) JS_ToInt32(ctx, &ev, e);
                            JS_FreeValue(ctx, e);
                            img_bin[i] = (unsigned char)(ev & 0xFF);
                        }
                    } else img_n = 0;
                }
                JS_FreeValue(ctx, bytesv);
                if (!img_bin) {
                    JSValue bodyv = JS_GetPropertyStr(ctx, res, "body");
                    char *bstr = JS_ToCString(ctx, bodyv);
                    if (bstr) {
                    unsigned char *png_data = NULL; size_t png_len = 0;
                    if (strncmp(s, "data:image/", 11) == 0) {
                        const char *comma = strchr(s, ',');
                        if (comma && strstr(s, ";base64,")) png_data = b64_decode(comma+1, &png_len);
                        else if (comma) { png_data = (unsigned char *)strdup(comma+1); png_len = strlen(comma+1); }
                    } else {
                        png_data = (unsigned char *)strdup(bstr);
                        png_len = strlen(bstr);
                    }
                    if (png_data && png_len) {
                        int w = 0, h = 0, comp = 0;
                        unsigned char *rgba = stbi_load_from_memory(png_data, (int)png_len, &w, &h, &comp, 4);
                        if (rgba) img_set_decoded(n, w, h, rgba);
                        free(png_data);
                    }
                    JS_FreeCString(ctx, bstr);
                }
                JS_FreeValue(ctx, bodyv);
                }
                /* bytes path succeeded above: decode the exact bytes */
                if (img_bin && img_n) {
                    int w = 0, h = 0, comp = 0;
                    unsigned char *rgba = stbi_load_from_memory(img_bin, (int)(img_n > INT_MAX ? INT_MAX : img_n), &w, &h, &comp, 4);
                    if (rgba) img_set_decoded(n, w, h, rgba);
                    free(img_bin);
                }
            }
            const char *evtype = ok ? "load" : "error";
            char js[64];
            snprintf(js, sizeof(js), "new Event('%s',{bubbles:false})", evtype);
            JSValue ev = JS_Eval(ctx, js, strlen(js), "<img src>", JS_EVAL_TYPE_GLOBAL);
            if (!JS_IsException(ev)) {
                dispatch_event(ctx, EVT_NODE, n, ev, 0);
                JS_FreeValue(ctx, ev);
            } else JS_FreeValue(ctx, JS_GetException(ctx));
            JS_FreeValue(ctx, res);
            JS_FreeValue(ctx, args[0]); JS_FreeValue(ctx, args[1]);
        }
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}
/* Image constructor helper extracted (Phase 1, single-TU include). */
#include "nb_worker_image_ctor.c"
/* element/document dispatchEvent helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_dispatch.c"
/* Node.contains (row-31): webcomponents-lite.js does
 * `document.contains ? document.contains.bind(document)
 *  : document.documentElement.contains.bind(...)` and Polymer/Closure
 * ask it during upgrades. Ancestry walk over the C tree. */
/* Node/Document.contains helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_node_contains.c"
/* window.dispatchEvent helper extracted (Phase 1, single-TU include). */
#include "nb_worker_win_dispatch.c"
/* Element click/change helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_click.c"
/* element.focus()/blur() (2026-09-21): typed-input path needs the search
 * input focusable (kevlar reads document.activeElement and toggles the
 * yt-searchbox focused classes) and focus/blur listeners come and go. */
/* Element focus/blur helpers extracted (Phase 1, single-TU include). */
#include "nb_worker_focus.c"
/* ---- search-box Enter -> /results (2026-09-21): kevlar's sport masthead
 * is a string-template component, so its closure `on:keydown`/`on:input`
 * attach never runs against the parsed <input name=search_query> and the
 * live input could never submit. Engine-side proxy (document-level keydown):
 * Enter on any input[name=search_query] navigates /results?search_query=
 * (the same URL kevlar's own searchEndpoint builds ~<script>:268939).
 * Document-level (EVT_DOC, node NULL) keeps it free of node bookkeeping —
 * no dangling refs across navigations, and g_evl is cleared per page. */
static JSValue nb_search_submit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    static const char hex[] = "0123456789ABCDEF";
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_UNDEFINED;
    JSValue ev = argv[0];
    JSValue kv = JS_GetPropertyStr(ctx, ev, "key");
    int is_enter = 0;
    if (JS_IsString(kv)) {
        const char *k = JS_ToCString(ctx, kv);
        is_enter = k && !strcmp(k, "Enter");
        JS_FreeCString(ctx, k);
    }
    JS_FreeValue(ctx, kv);
    if (!is_enter) return JS_UNDEFINED;
    JSValue tv = JS_GetPropertyStr(ctx, ev, "target");
    NbNode *n = JS_IsObject(tv) ? get_node(ctx, tv) : NULL;
    if (!n || !n->tag || strcmp(n->tag, "input")) { JS_FreeValue(ctx, tv); return JS_UNDEFINED; }
    const char *attr = nb_attr_get(n, "name");
    if (!attr || strcmp(attr, "search_query")) { JS_FreeValue(ctx, tv); return JS_UNDEFINED; }
    JSValue v = JS_GetPropertyStr(ctx, tv, "value");
    const char *val = JS_IsString(v) ? JS_ToCString(ctx, v) : NULL;
    char q[2048]; size_t qi = 0;
    if (val) {
        for (const unsigned char *p = (const unsigned char *)val; *p && qi < sizeof(q) - 16; p++) {
            unsigned char c = *p;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                q[qi++] = (char)c;
            } else if (c == ' ') {
                q[qi++] = '+';
            } else {
                q[qi++] = '%'; q[qi++] = hex[c >> 4]; q[qi++] = hex[c & 15];
            }
        }
        JS_FreeCString(ctx, val);
    }
    q[qi] = 0;
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, tv);
    char url[4096];
    snprintf(url, sizeof(url), "/results?search_query=%s", q);
    JSValue loc = get_global_attr(ctx, "location");
    if (JS_IsObject(loc)) JS_SetPropertyStr(ctx, loc, "href", JS_NewString(ctx, url));
    JS_FreeValue(ctx, loc);
    return JS_UNDEFINED;
}

static void nb_wire_search_proxy(JSContext *ctx) {
    if (g_search_wired) return;
    JSValue type = JS_NewString(ctx, "keydown");
    JSValue cb = JS_NewCFunction(ctx, nb_search_submit, "searchEnter", 1);
    evl_add(ctx, EVT_DOC, NULL, type, cb);
    JS_FreeValue(ctx, type);
    JS_FreeValue(ctx, cb);
    g_search_wired = 1;
}

/* el.blur is wired to nb_el_blur (magic 1); keep the click/misc natives below. */
static void fire_event(JSContext *ctx, int kind, NbNode *n, const char *type) {
    for (int i = 0; i < g_evl_count; i++) {
        if (!g_evl[i].active || g_evl[i].kind != kind || g_evl[i].node != n) continue;
        if (strcmp(g_evl[i].type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        if (kind == EVT_NODE && n) JS_SetPropertyStr(ctx, ev, "target", push_node(ctx, n));
        else JS_SetPropertyStr(ctx, ev, "target",
             get_global_attr(ctx, kind == EVT_WIN ? "window" : "document"));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, g_evl[i].cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
    /* lifecycle on-* props: window.onload, document.onDOMContentLoaded (commit 8) */
    for (int oi = 0; oi < g_onprop_count; oi++) {
        OnProp *o = &g_onprop[oi];
        if (!o->active || o->kind != kind || o->node != n) continue;
        if (strcmp(o->type, type)) continue;
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        JSValue ev = JS_NewObject(ctx);            /* minimal Event (argument) */
        JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, type));
        JS_SetPropertyStr(ctx, ev, "defaultPrevented", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "cancelable", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, ev, "preventDefault",
             JS_NewCFunction(ctx, nb_event_preventDefault, "preventDefault", 0));
        JS_SetPropertyStr(ctx, ev, "stopPropagation",
             JS_NewCFunction(ctx, nb_event_stopPropagation, "stopPropagation", 0));
        JSValue thr = (kind == EVT_NODE && n) ? push_node(ctx, n)
                     : get_global_attr(ctx, kind == EVT_WIN ? "window" : "document");
        JSValue argv[1]; argv[0] = ev;             /* callback arg */
        JSValue r = JS_Call(ctx, o->cb, thr, 1, argv);
        JS_FreeValue(ctx, thr);
        JS_FreeValue(ctx, ev);
        if (JS_IsException(r)) {
            if (!g_pending_err) {
                g_pending_err = 1;
                char buf[512];
                const char *m = js_error_to_cstr(ctx, buf, sizeof(buf));
                snprintf(g_pending_errmsg, sizeof(g_pending_errmsg), "%s", m);
            }
            JS_FreeValue(ctx, r);
            g_invocations++;
            break;
        }
        JS_FreeValue(ctx, r);
        g_invocations++;
    }
}

/* ---- the loop: lifecycle -> job/timer drain until quiescent or budget ---- */
/* returns nonzero if an event-loop callback threw (caller -> STATUS err) */
/* ---- G1-b: the tick loop (2026-10-13) ----------------------------------
 *
 * A page's setInterval has to keep running AFTER LOAD returns. Until this,
 * it did not: run_page() drained the timer queue once (bounded by
 * MAX_DRAIN_MS = 800ms, which is why the G1 fixture came back saying
 * "tick 2" and then stopped dead), emitted one RENDER, and the worker went
 * back to blocking on stdin forever. Measured 2026-10-13: 0 frames in
 * 10.1s of doing nothing at all.
 *
 * Deliberately NOT reusing run_event_loop() here. That drains to
 * QUIESCENT and sleeps until the next timer, so calling it per tick would
 * hold stdin hostage for up to MAX_DRAIN_MS and the page could not be
 * clicked. The tick slice runs only what is DUE NOW and returns, which is
 * the same pair run_event_loop() calls inside its own loop body:
 * run_due_timers() + drain_jobs(). Same semantics, no waiting.
 *
 * Change detection is a string compare against the last pushed rows
 * rather than a dirty flag in the DOM: one memcmp is exact, cannot be
 * forgotten, and needs no new bookkeeping on every mutation site - which
 * is where a dirty flag would rot.
 *
 * OFF by default (g_tick_cap_ms == 0). The resident manager worker must
 * not start emitting spontaneous frames until the manager side of the
 * push protocol is ready to consume them; the G1 probe opts in with an
 * explicit TICK frame.
 */
static int g_tick_cap_ms;             /* 0 = off; else the max stdin wait */
static char *g_last_push;             /* rows of the last frame we pushed */

static void send_render_rows(const char *rows) {
    size_t rn = rows ? strlen(rows) : 0;
    if (!rn || rn >= RENDER_MAX) return;
    size_t pn = 7 + rn;                        /* "RENDER\n" + rows */
    char *pay = malloc(pn + 1);
    if (!pay) return;
    memcpy(pay, "RENDER\n", 7);
    memcpy(pay + 7, rows, rn);
    pay[pn] = 0;
    send_payload(pay, pn);
    free(pay);
}

/* How long the main loop may block on stdin before a tick is due. -1 when
 * ticking is off (the historical indefinite wait), else the gap to the
 * next scheduled timer, capped so a page cannot park the loop for long. */
static int tick_wait_ms(void) {
    if (g_tick_cap_ms <= 0) return -1;
    uint64_t due = timer_min_due();
    if (!due) return g_tick_cap_ms;            /* nothing scheduled */
    uint64_t now = now_ms();
    if (due <= now) return 0;                   /* already due: tick now */
    uint64_t gap = due - now;
    if (gap > (uint64_t)g_tick_cap_ms) gap = (uint64_t)g_tick_cap_ms;
    return (int)gap;
}

/* One tick: fire whatever is due, drain microtasks, push only if the rows
 * actually changed. Returns 1 if a frame went out. */
static int tick_slice(void) {
    if (!g_live_ctx) return 0;
    uint64_t now = now_ms();
    uint64_t due = timer_min_due();
    if (!due || due > now) return 0;            /* nothing due yet */
    run_due_timers(g_live_ctx, now);
    drain_jobs(g_live_ctx);
    if (g_pending_err) return 0;

    SB r = {0, 0, 0};
    dom_render_rows(&r);
    int pushed = 0;
    if (r.s && r.s[0]) {
        if (!g_last_push || strcmp(g_last_push, r.s) != 0) {
            send_render_rows(r.s);
            free(g_last_push);
            g_last_push = r.s;
            r.s = NULL;                        /* ownership moved */
            pushed = 1;
        }
    }
    free(r.s);
    return pushed;
}

static int run_event_loop(JSContext *ctx) {
    /* The page's timer queue can keep this loop busy for minutes under the
     * resident worker (no JIT), and the manager guards its LOAD read with a
     * 3s stall watchdog. Emit a LIVE| beat every ~1.8s so a slow-but-alive
     * drain is never mistaken for a dead worker; RENDER still follows once
     * the loop is actually quiescent. */
    static uint64_t keep_last = 0;
    #define NB_KEEPALIVE_LIVE() do { \
        if (!g_cli) { \
            uint64_t _k = now_ms(); \
            if (_k - keep_last >= 1800) { \
                keep_last = _k; \
                char _b[96]; \
                int _n = snprintf(_b, sizeof(_b), "LIVE|drain|%u", g_invocations); \
                send_payload(_b, (size_t)_n); \
            } \
        } \
    } while (0)
    nb_watchdog_begin();   /* phase-1 backstop also covers timer/job callbacks */
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_DOC, NULL, "DOMContentLoaded");
    drain_jobs(ctx);
    if (!g_pending_err) fire_event(ctx, EVT_WIN, NULL, "load");
    drain_jobs(ctx);
    uint64_t start = now_ms();
    for (int guard = 0; guard < 100000 && !g_pending_err; guard++) {
        if (g_invocations >= MAX_TIMER_INVOCATIONS) break;
        if (now_ms() - start > MAX_DRAIN_MS) break;   /* bound page_load wait */
        NB_KEEPALIVE_LIVE();
        uint64_t now = now_ms();
        int ran = run_due_timers(ctx, now);
        drain_jobs(ctx);
        if (!ran) {
            uint64_t m = timer_min_due();
            if (!m) break;                    /* nothing scheduled — quiescent */
            if (m <= now) continue;           /* due but callback skipped? re-drain */
            /* next timer in the future — sleep up to it (bounded by MAX_DRAIN_MS) */
            uint64_t d = m - now;
            if (d > 5) d = 5;
            struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
            nanosleep(&ts, NULL);
            continue;
        }
    }
    nb_watchdog_pause();
    nb_wire_search_proxy(ctx);
    return g_pending_err;
}

/* ==================== rung 6 seam: generic SHA-1 primitive =============
 * Row-34 login: Google's page JS signs SAPISIDHASH =
 *   <ts>_<base64(sha1(<ts> " " <SAPISID> " " <origin>))>
 * by itself (Chromium parity — browsers have no LOGIN op; the site's own
 * script does the crypto). But the page needs a SHA-1 primitive to do so.
 * This is the engine-generic one: `__nb_sha1(utf8)->base64(digest)`, exposed
 * to any page. Not youtube-specific; the page JS handles the SAPISIDHASH
 * composition purely in JS. Raw digest bytes can't round-trip through a JS
 * string, so the digest is pre-encoded ASCII base64 here — verified against openssl
 * in worker_sapisid_test).  Shared impl in nb_sha1.h so fixture servers
 * recompute the SAME digest when verifying a received signature. */
#include "nb_sha1.h"

/* Generic SHA-1 native helper extracted (Phase 1, single-TU include). */
#include "nb_worker_sha1_native.c"

static void install_events_timers(JSContext *ctx) {
    g_perf_mark_hook = nb_po_on_mark;
    JSValue doc = get_global_attr(ctx, "document");
    JS_SetPropertyStr(ctx, doc, "addEventListener",
        JS_NewCFunction(ctx, nb_doc_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "removeEventListener",
        JS_NewCFunction(ctx, nb_doc_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, doc, "dispatchEvent",
        JS_NewCFunction(ctx, nb_doc_dispatchEvent, "dispatchEvent", 1));
    JS_SetPropertyStr(ctx, doc, "contains",
        JS_NewCFunction(ctx, nb_doc_contains, "contains", 1));
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, doc, key,
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_doc_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, doc);
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "__nb_sha1",   JS_NewCFunction(ctx, nb_sha1_native, "__nb_sha1", 1));
    JS_SetPropertyStr(ctx, g, "setTimeout",  JS_NewCFunction(ctx, nb_timer_setTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, g, "setInterval", JS_NewCFunction(ctx, nb_timer_setInterval, "setInterval", 2));
    JS_SetPropertyStr(ctx, g, "clearTimeout",  JS_NewCFunction(ctx, nb_timer_clearTimeout, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, g, "clearInterval", JS_NewCFunction(ctx, nb_timer_clearInterval, "clearInterval", 1));
    JS_SetPropertyStr(ctx, g, "queueMicrotask", JS_NewCFunction(ctx, nb_queueMicrotask, "queueMicrotask", 1));
    JS_SetPropertyStr(ctx, g, "requestAnimationFrame", JS_NewCFunction(ctx, nb_raf, "requestAnimationFrame", 1));
    /* window.innerWidth is a VALUE in every browser, not a function. The
     * first version registered a native function here and
     * ""+window.innerWidth came back as "function innerWidth() { [native
     * code] }" - a stringified function where a number belongs, which
     * silently corrupts every page that does arithmetic on it. */
    JS_SetPropertyStr(ctx, g, "innerWidth", JS_NewFloat64(ctx, g_viewport_px));
    JS_SetPropertyStr(ctx, g, "innerHeight", JS_NewFloat64(ctx, g_viewport_h > 0.0 ? g_viewport_h : 600.0));
    /* matchMedia: real evaluator (see nb_mq_*). The prelude used to install
     * a constant-false stub; that line is gone so this one is what pages see. */
    JS_SetPropertyStr(ctx, g, "matchMedia", JS_NewCFunction(ctx, nb_match_media, "matchMedia", 1));
    {
        JSAtom nm = JS_NewAtom(ctx, "scrollX");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get scrollX", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "scrollY");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get scrollY", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        /* Legacy aliases: pageYOffset is what most lazy loaders still read. */
        nm = JS_NewAtom(ctx, "pageXOffset");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get pageXOffset", 0, JS_CFUNC_generic_magic, 0), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
        nm = JS_NewAtom(ctx, "pageYOffset");
        JS_DefinePropertyGetSet(ctx, g, nm, JS_NewCFunctionMagic(ctx, nb_win_scroll_get, "get pageYOffset", 0, JS_CFUNC_generic_magic, 1), JS_UNDEFINED,
            JS_PROP_HAS_GET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, nm);
    }
    JS_SetPropertyStr(ctx, g, "scrollTo", JS_NewCFunction(ctx, nb_win_scrollTo, "scrollTo", 2));
    JS_SetPropertyStr(ctx, g, "scrollBy", JS_NewCFunction(ctx, nb_win_scrollBy, "scrollBy", 2));
    JS_SetPropertyStr(ctx, g, "scroll", JS_NewCFunction(ctx, nb_win_scrollTo, "scroll", 2));
    JS_SetPropertyStr(ctx, g, "getSelection", JS_NewCFunction(ctx, nb_win_getSelection, "getSelection", 0));
    JS_SetPropertyStr(ctx, g, "nbFetchSync", JS_NewCFunction(ctx, nb_fetch_sync, "nbFetchSync", 4));
    JS_SetPropertyStr(ctx, g, "addEventListener",    JS_NewCFunction(ctx, nb_win_addEventListener, "addEventListener", 2));
    JS_SetPropertyStr(ctx, g, "removeEventListener", JS_NewCFunction(ctx, nb_win_removeEventListener, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, g, "dispatchEvent",       JS_NewCFunction(ctx, nb_win_dispatchEvent, "dispatchEvent", 1));
    /* ShadyDOM (webcomponents-lite.js) captures "native" methods by copying
     * property descriptors off prototypes (L(Window.prototype, ...)); our
     * natives are own props on window/elements, so the copy is empty and
     * window.__shady_native_addEventListener stays undefined → Ae() throws
     * "not a function" at init. Define the aliases on window directly. */
    JS_SetPropertyStr(ctx, g, "__shady_native_addEventListener",    JS_NewCFunction(ctx, nb_win_addEventListener, "__shady_native_addEventListener", 2));
    JS_SetPropertyStr(ctx, g, "__shady_native_removeEventListener", JS_NewCFunction(ctx, nb_win_removeEventListener, "__shady_native_removeEventListener", 2));
    JS_SetPropertyStr(ctx, g, "__shady_native_dispatchEvent",       JS_NewCFunction(ctx, nb_win_dispatchEvent, "__shady_native_dispatchEvent", 1));
    JS_SetPropertyStr(ctx,g,"atob",JS_NewCFunction(ctx,_nb_atob,"atob",1));
    JS_SetPropertyStr(ctx,g,"btoa",JS_NewCFunction(ctx,_nb_btoa,"btoa",1));
    { JSValue p=JS_GetPropertyStr(ctx,g,"performance"); if(!JS_IsObject(p)){ JS_FreeValue(ctx,p); p=JS_NewObject(ctx); JS_SetPropertyStr(ctx,g,"performance",JS_DupValue(ctx,p)); } JS_SetPropertyStr(ctx,p,"now",JS_NewCFunction(ctx,_nb_perfnow,"now",0)); JS_FreeValue(ctx,p); }
    { JSValue c=JS_NewObject(ctx); JS_SetPropertyStr(ctx,c,"getRandomValues",JS_NewCFunction(ctx,_nb_grv,"getRandomValues",1)); JS_SetPropertyStr(ctx,c,"randomUUID",JS_NewCFunction(ctx,_nb_uuid,"randomUUID",0)); JS_SetPropertyStr(ctx,g,"crypto",c); }
    for (int i = 0; ONPROPS[i]; i++) {
        char onname[64];
        snprintf(onname, sizeof(onname), "on%s", ONPROPS[i]);
        JSAtom key = JS_NewAtom(ctx, onname);
        JS_DefinePropertyGetSet(ctx, g, key,
            JS_NewCFunctionMagic(ctx, nb_win_onprop_get, onname, 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(ctx, nb_win_onprop_set, onname, 1, JS_CFUNC_generic_magic, i),
            JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_HAS_ENUMERABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, g);
}

/* boot hygiene (2026-09-09): install_dom / install_events_timers ran under a
 * protected call so a throw was reported to stderr as WERR| and the
 * page still loaded. QuickJS property definitions don't throw on non-writable
 * targets (JS_DefinePropertyGetSet just returns FALSE), so the direct calls
 * are enough; keep the name for call-site clarity. */
static void boot_install_safe(JSContext *ctx) {
    install_dom(ctx);
    install_events_timers(ctx);
}

/* plan step 5: CPU budget for script eval. If page.js burns through
 * EVAL_BUDGET_SEC of CPU (while(true) {} and friends) SIGALRM fires while
 * the engine is running; the deadly default _exit kills the worker mid-eval, the
 * manager sees the socket close and respawns on the next LOAD. */
/* Evaluate src with a SIGALRM CPU budget. Returns 0 on success; on error
 * returns 1 and, if errbuf is non-NULL, formats the exception message into
 * it (the exception is always consumed/cleared). The completion value is
 * discarded — use peval_budget_value() when the result is wanted. */
static int peval_budget(JSContext *ctx, const char *src, size_t src_n,
                        char *errbuf, size_t errlen) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    nb_watchdog_pause();
    if (JS_IsException(r)) {
        if (errbuf && errlen) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            snprintf(errbuf, errlen, "%s", m);
        } else {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
/* Like peval_budget but returns the completion JSValue (owned; freed by the
 * caller — also on JS_EXCEPTION). No exception is consumed here. */
static JSValue peval_budget_value(JSContext *ctx, const char *src, size_t src_n) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<script>", JS_EVAL_TYPE_GLOBAL);
    nb_watchdog_pause();
    return r;
}
/* Like peval_budget but uses JS_EVAL_TYPE_MODULE for ES module scripts.
 * Modules without imports work; imports need a module loader (not yet set). */
static int peval_budget_module(JSContext *ctx, const char *src, size_t src_n,
                               char *errbuf, size_t errlen) {
    nb_watchdog_begin();
    JSValue r = JS_Eval(ctx, src, src_n, "<module>", JS_EVAL_TYPE_MODULE);
    nb_watchdog_pause();
    if (JS_IsException(r)) {
        if (errbuf && errlen) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            snprintf(errbuf, errlen, "%s", m);
        } else {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        }
        JS_FreeValue(ctx, r);
        return 1;
    }
    JS_FreeValue(ctx, r);
    return 0;
}
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

/* Run the page script; sends STATUS ok|err across the wire. */
static void dom_teardown(void) {
    /* free detached createElement() nodes, then the main tree */
    while (g_orphans) {
        NbNode *o = g_orphans;
        g_orphans = o->next_sibling;
        o->next_sibling = NULL;
        nb_node_free(o);
    }
    nb_node_free(g_dom_root);
    g_dom_root = NULL;
    nb_css_free(g_css);
    g_css = NULL;
}

/* devtools console EVAL: tear down everything a previous LOAD left live
 * (resident QuickJS runtime + its DOM tree). run_page() keeps those alive on
 * the success path so an eval:<js> snippet can run against the page; this
 * helper is the disciplined cleanup for the next LOAD, errors and QUIT. */
static void live_teardown(void) {
    nb_canvas_reset();   /* page-local raster buffers die with the page */
#ifdef NB_HAVE_GLES
    nb_gl_end_all();     /* EGL surfaces cannot outlive the page */
#endif
    if (g_live_ctx) {
        free_held_callbacks(g_live_ctx);   /* release refs before the heap dies */
        JS_FreeContext(g_live_ctx);
        g_live_ctx = NULL;
    }
    if (g_live_rt) {
        JS_FreeRuntime(g_live_rt);
        g_live_rt = NULL;
    }
    dom_teardown();
}

/* phase-2 (2026-09-09): document-order script runs. The manager writes one
 * <script> (inline or src-fetched) per slice of page.js, separated by the
 * SCRIPT_BOUNDARY sentinel. Each slice is compiled and run as its own
 * program — browser classic-script parity: a syntax error in slice 2 does
 * not stop slices 1/3, top-level `var` still lands on the shared global,
 * and external <script src> executes at its DOM position. Per-slice
 * failures print to stderr (surfaced by the manager as WERR|/[worker]).
 * A legacy page.js without any sentinel is treated as one program. */
#define SCRIPT_BOUNDARY "/*nbjs-script-boundary*/"
static const char *find_script_boundary(const char *p, const char *end,
                                        const char **after) {
    const char *q = p;
    for (; q + sizeof(SCRIPT_BOUNDARY) - 1 <= end; q++) {
        if (q[0] == '/' && q[1] == '*' &&
            memcmp(q, SCRIPT_BOUNDARY, sizeof(SCRIPT_BOUNDARY) - 1) == 0) {
            *after = q + sizeof(SCRIPT_BOUNDARY) - 1;
            return q;
        }
    }
    return NULL;
}
/* Debug-only (NB_BUNDLE_TRACE=1): inject console probes into kevlar's sxs
 * customElement wrapper (D.prototype.createElement) so a real LOAD reveals
 * what `_.a(O,null)` returns, the live binding of `_.a`/`O`, and whether the
 * wrapper's try threw (the catch swallows the error and RETURNS UNDEFINED).
 * The page bundle is re-extracted every load, so injection happens here at
 * eval time against the materialized slice text. Returns a malloc'd patched
 * buffer via *out (caller frees), or 0 when the env is off / needles absent.
 * Needle counts must match exactly, else the patch is declined. */
static int nb_bundle_probe(const char *src, size_t src_n,
                           char **out, size_t *out_n) {
    static const char *env = NULL;
    if (!env) env = getenv("NB_BUNDLE_TRACE");
    if (!env || !env[0] || env[0] == '0') return 0;
    static const struct { const char *nd; const char *rp; int want; } pat[] = {
        { "if(!this.isInert)if(_.f(\"web_monomer_web_component_wrapper_handle_errors\")){WI=this;try{",
          "if(!this.isInert)if(_.f(\"web_monomer_web_component_wrapper_handle_errors\")){try{console.log(\"NBT|enter:\"+this.tagName+\" flag=1\");}catch(e0){}WI=this;try{", 1 },
        { "function(){return _.a(O,null)}",
          "function(){var r=_.a(O,null);try{console.log(\"NBT|euv:a=\"+(typeof _.a)+\"|\"+String(_.a).slice(0,40)+\"|O=\"+(typeof O)+\"|\"+String(O).slice(0,40)+\"|nS=\"+(_.nS===void 0?\"u\":_.nS?1:0)+\"|ret=\"+(typeof r)+\"|\"+String(r).slice(0,40));}catch(e1){}return r}", 2 },
        { "catch(L){h=function(B)",
          "catch(L){try{console.log(\"NBT|ERR:\"+String(L&&L.message||L));}catch(e2){}h=function(B)", 1 },
        { "if(O instanceof cSc)return L=O.render(),Qkq(L,S,D,g,h);",
          "try{console.log(\"NBT|qkq c=\"+(O&&O.constructor&&O.constructor.name)+\" instcSc=\"+(O instanceof cSc)+\" render=\"+(typeof (O&&O.render)));}catch(eQ){console.log(\"NBT|qkqT:\"+eQ)}if(O instanceof cSc)return L=O.render(),Qkq(L,S,D,g,h);", 1 },
        { "(Q[R]instanceof Function&&FyC(\"Function props must be configured",
          "((function(){try{console.log(\"SBP| R=\"+R+\" q=\"+(typeof Q)+\" qr=\"+(Q==null?\"na\":(typeof Q[R]))+\" L=\"+(typeof L));}catch(e9){console.log(\"SBP|x \"+e9)}})(),Q[R]instanceof Function&&FyC(\"Function props must be configured", 1 },
    };
    const int pat_n = (int)(sizeof(pat) / sizeof(pat[0]));
    int cnt[5] = { 0, 0, 0, 0, 0 };
    const char *q = src, *end = src + src_n;
    for (int i = 0; i < pat_n; i++) {
        size_t nl = strlen(pat[i].nd);
        for (const char *z = src; z + nl <= end; ) {
            const char *hit = memmem(z, (size_t)(end - z), pat[i].nd, nl);
            if (!hit) break;
            cnt[i]++;
            q = hit + 1;
            z = q;
        }
    }
    for (int i = 0; i < pat_n; i++)
        if (cnt[i] != pat[i].want) {
            fprintf(stderr, "NBT|declined: counts %d/%d/%d/%d/%d (want %d/%d/%d/%d/%d)\n",
                    cnt[0], cnt[1], cnt[2], cnt[3], cnt[4],
                    pat[0].want, pat[1].want, pat[2].want, pat[3].want, pat[4].want);
            return 0;
        }
    size_t out_cap = src_n + 4096, off = 0;
    char *b = malloc(out_cap);
    if (!b) return 0;
    const char *z = src;
    while (z < end) {
        int done = 0;
        for (int i = 0; i < pat_n && !done; i++) {
            size_t nl = strlen(pat[i].nd);
            if ((size_t)(end - z) >= nl && memcmp(z, pat[i].nd, nl) == 0) {
                size_t rl = strlen(pat[i].rp);
                if (off + rl >= out_cap) { free(b); return 0; }
                memcpy(b + off, pat[i].rp, rl); off += rl; z += nl; done = 1;
            }
        }
        if (!done) { b[off++] = *z++; }
    }
    b[off] = 0;
    fprintf(stderr, "NBT|patch applied (%zu bytes)\n", off);
    *out = b; *out_n = off;
    return 1;
}
static void run_scripts_slices(JSContext *ctx, char *src, size_t src_n) {
    const char *p = src, *end = src + src_n;
    const char *after = NULL;
    char errbuf[512];
    char *probe = NULL; size_t probe_n = 0;
    if (nb_bundle_probe(src, src_n, &probe, &probe_n) && probe) {
        src = probe; src_n = probe_n;
        p = probe; end = probe + probe_n;
    }
    if (!find_script_boundary(p, end, &after)) {
        /* legacy single-program page.js */
        if (peval_budget(ctx, src, src_n, errbuf, sizeof(errbuf)) != 0)
            fprintf(stderr, "WERR| script 0: %s\n", errbuf[0] ? errbuf : "eval error");
        free(probe);
        return;
    }
    p = after;
    int idx = 0;
    for (;;) {
        const char *next = NULL;
        size_t slice = (size_t)(end - p);
        const char *bn = find_script_boundary(p, end, &next);
        if (bn) slice = (size_t)(bn - p);
        if (slice > 0) {
            /* QuickJS's lexer peeks input[input_len] for EOI, so each slice
             * must be NUL-terminated at its eval length. Slices own one
             * contiguous buffer, so terminate in place; the boundary's first
             * byte is never read again (next = the position after it). */
            ((char *)p)[slice] = 0;
            /* Check for module marker at slice start (skip leading whitespace). */
            int is_module = 0;
            const char *p2 = p;
            size_t slice2 = slice;
            while (slice2 > 0 && (*p2 == '\n' || *p2 == '\r' || *p2 == ' ' || *p2 == '\t')) {
                p2++; slice2--;
            }
            if (slice2 >= 15 && memcmp(p2, "/*nbjs-module*/", 15) == 0) {
                is_module = 1;
                /* Advance p past the marker and optional whitespace. */
                const char *marker_end = p2 + 15;
                while (marker_end < p + slice && (*marker_end == '\n' || *marker_end == '\r' || *marker_end == ' ' || *marker_end == '\t'))
                    marker_end++;
                size_t skip = (size_t)(marker_end - p);
                p = marker_end;
                slice = (slice > skip) ? (slice - skip) : 0;
            }
            if (slice > 0) {
                fprintf(stderr, "DEBUG: executing script slice %d, size: %zu, first 100 chars: %.100s\n", idx, slice, p);
                if (is_module) {
                    if (peval_budget_module(ctx, p, slice, errbuf, sizeof(errbuf)) != 0)
                        fprintf(stderr, "WERR| module %d: %s\n", idx, errbuf[0] ? errbuf : "eval error");
                } else {
                    if (peval_budget(ctx, p, slice, errbuf, sizeof(errbuf)) != 0)
                        fprintf(stderr, "WERR| script %d: %s\n", idx, errbuf[0] ? errbuf : "eval error");
                }
            }
        }
        idx++;
        if (!bn) break;
        p = next;
    }
    free(probe);
}
static void run_page(void) {
    /* phase-2 (commit 7): per-page event/timer/microtask state. The previous
     * LOAD's held callbacks belong to the old heap — release them FIRST, then
     * reset the counters so live_teardown()'s free_held_callbacks is a no-op. */
    if (g_live_ctx) free_held_callbacks(g_live_ctx);
    g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
    g_next_id = 1; g_invocations = 0; g_raf_fires = 0;
    g_pending_err = 0; g_pending_errmsg[0] = 0;
    g_scroll_x = 0; g_scroll_y = 0;

    /* rung-6 slice 2: only the daemon (manager) can act on navigation. */
    g_nav_kind[0] = 0; g_nav_url[0] = 0; g_nav_count = 1;
    g_nav_emit = !g_cli;

    /* rung-6: sessionStorage is per-LOAD — fresh session for this page. */
    g_ss_count = 0;

    /* devtools console EVAL: a previous LOAD may still have its resident
     * heap + DOM alive; tear it down before building this page. */
    live_teardown();

    g_dom_root = NULL;
    g_orphans = NULL;
    g_css = NULL;
    node_index_reset();

    /* build the DOM tree from the manager's fetch.dom (if present) */
    fprintf(stderr, "DEBUG: g_fetch_dom = '%s'\n", g_fetch_dom);
    if (g_fetch_dom[0]) {
        FILE *df = fopen(g_fetch_dom, "rb");
        if (!df) {
            /* SILENT BEFORE (2026-10-13): a fetch.dom the worker cannot
             * open left g_dom_root NULL and the page rendered as its
             * title only, with STATUS ok. */
            char m[900];
            snprintf(m, sizeof(m),
                     "DOM: cannot open fetch.dom (%s) - the page will render as its title only",
                     g_fetch_dom);
            send_worker_error(m);
        } else {
            fprintf(stderr, "DEBUG: loading DOM from fetch.dom\n");
            g_dom_root = nb_dom_load(df);
            fclose(df);
            fprintf(stderr, "DEBUG: nb_dom_load returned %p\n", (void*)g_dom_root);
            if (!g_dom_root) {
                /* Same silence, same consequence. nb_dom_load() reads the
                 * nb_serialize() wire format, not HTML - anything else
                 * (an HTML file, a stray comment at the top) returns
                 * NULL, which until now looked exactly like a page with
                 * no content. */
                send_worker_error("DOM: fetch.dom did not deserialize "
                                  "(nb_dom_load returned NULL; the file must be "
                                  "nb_serialize output, not HTML) - the page will "
                                  "render as its title only");
            } else {
                fprintf(stderr, "DEBUG: DOM loaded, root tag: %s, children: %d\n", g_dom_root->tag ? g_dom_root->tag : "NULL", g_dom_root->first_child ? 1 : 0);
                if (g_dom_root->first_child) {
                    for (NbNode *c = g_dom_root->first_child; c; c = c->next_sibling) {
                        fprintf(stderr, "DEBUG: child tag: %s\n", c->tag ? c->tag : "NULL");
                    }
                }
            }
        }
    } else if (g_cli) {
        /* standalone nbjs: give the page an empty document/body */
        static const char empty_html[] = "<html><body></body></html>";
        g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    }

    /* rung 7: parse the LOAD-delivered stylesheet into a rule cache.
     * nb_css_parse accepts empty files (nr==0), so no special casing
     * needed when there are no inline <style> blocks. */
    if (g_style_css[0]) {
        char *sc = NULL; size_t sn = 0;
        if (read_file(g_style_css, &sc, &sn)) {
            /* G2.5 (2026-11-04): parse WITH the viewport the page was laid
             * out in, so an @media block whose query matches becomes real
             * rules. g_viewport_px is set by the LOAD handler (field 6, the
             * pane's content width) before run_page() is called, and it is
             * the same value window.innerWidth reports - one viewport, one
             * number, so a page's own JS and its CSS cannot disagree about
             * how wide it is. viewport 0 (unmeasured) keeps the old
             * skip-every-@media behaviour. */
            g_css = nb_css_parse_vp(sc, sn, (int)g_viewport_px);
            free(sc);
        }
    }

    /* Read page.js FIRST to detect modules BEFORE creating runtime. */
    char *src = NULL;
    size_t src_n = 0;
    if (!read_file_big(g_page_js, &src, &src_n)) {
        send_status("STATUS err:cannot read page.js");
        live_teardown();
        return;
    }
    if (src_n == 0) { free(src); send_status("STATUS ok"); return; }

    /* Detect if page has any module scripts (by checking for marker). */
    int has_module = 0;
    if (src_n >= 15 && memcmp(src, "/*nbjs-module*/", 15) == 0) has_module = 1;
    else {
        const char *p = src;
        for (; p + 15 < src + src_n; p++) {
            if (p[0] == '/' && p[1] == '*' && memcmp(p, "/*nbjs-module*/", 15) == 0) {
                has_module = 1; break;
            }
        }
    }

    if (has_module) {
        /* Module page: use a completely isolated, non-resident runtime.
         * Module evaluation corrupts the runtime (GC list not empty), so we
         * create a fresh runtime, run the page, and free it immediately.
         * No resident context = no EVAL support for module pages. */
        JSRuntime *rt = JS_NewRuntime();
        nb_watchdog_install(rt);
        if (!rt) { free(src); send_status("STATUS err:heap"); return; }
        JSContext *ctx = JS_NewContext(rt);
        if (!ctx) { JS_FreeRuntime(rt); free(src); send_status("STATUS err:heap"); return; }
        install_host(ctx);

        /* rung 7: the worker's CSS resolve wins over install_host's
         * minimal __nb_ges default (the prelude reads __nb_ges at call time). */
        JSValue ge = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, ge, "__nb_ges",
                          JS_NewCFunction(ctx, nb_ges_rich, "__nb_ges", 1));
        JS_FreeValue(ctx, ge);

        /* rung-6 prelude: swallow its own failure, page continues */
        (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);

        boot_install_safe(ctx);

        /* For module pages in isolated runtime: strip 'export ' and module markers,
         * evaluate as classic script to avoid JS_EVAL_TYPE_MODULE GC corruption. */
        {
            char *buf = malloc(src_n + 1);
            if (buf) {
                size_t j = 0;
                for (size_t i = 0; i < src_n; ) {
                    if (i + 7 <= src_n && memcmp(src + i, "export ", 7) == 0) {
                        i += 7;
                    } else if (i + 6 <= src_n && memcmp(src + i, "export", 6) == 0 &&
                               (i+6 == src_n || isspace((unsigned char)src[i+6]))) {
                        i += 6;
                    } else if (i + 15 <= src_n && memcmp(src + i, "/*nbjs-module*/", 15) == 0) {
                        i += 15;
                        /* skip optional whitespace/newline after marker */
                        while (i < src_n && (src[i] == '\n' || src[i] == '\r' || src[i] == ' ' || src[i] == '\t'))
                            i++;
                    } else {
                        buf[j++] = src[i++];
                    }
                }
                buf[j] = 0;
                run_scripts_slices(ctx, buf, j);
                free(buf);
            } else {
                run_scripts_slices(ctx, src, src_n);
            }
        }
        free(src);

        /* phase-2: lifecycle + event loop — timers/microtasks now fire */
        int ev_err = run_event_loop(ctx);
        if (ev_err) {
            char msg[1100];
            snprintf(msg, sizeof(msg), "STATUS err:%s",
                     g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
            send_status(msg);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            return;
        }

        /* Emit RENDER rows from the post-JS DOM, then STATUS. */
        SB r = {0, 0, 0};
        dom_render_rows(&r);
        if (r.s && r.s[0]) {
            size_t rn = strlen(r.s);
            char *pay = NULL;
            size_t pn = 0;
            if (rn < RENDER_MAX) {
                pn = 7 + rn;
                pay = malloc(pn + 1);
                if (pay) {
                    memcpy(pay, "RENDER\n", 7);
                    memcpy(pay + 7, r.s, rn);
                    pay[pn] = 0;
                    send_payload(pay, pn);
                }
            }
            free(pay);
        }
        free(r.s);

        /* Navigation handling */
        if (g_nav_emit && g_nav_kind[0]) {
            char pay[4600];
            int pn = 0;
            if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                              g_nav_count > 0 ? g_nav_count : 1);
            else
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
            if (pn > 0 && pn < (int)sizeof(pay)) {
                char *ppay = malloc((size_t)pn + 1);
                if (ppay) {
                    memcpy(ppay, pay, (size_t)pn); ppay[pn] = 0;
                    send_payload(ppay, (size_t)pn);
                    free(ppay);
                }
            }
        }

        /* Free the isolated runtime immediately - no resident context */
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        g_module_page_loaded = 1;
        send_status("STATUS ok (no EVAL for module pages)");
    } else {
        /* Classic page: use the standard resident runtime for EVAL support. */
        JSRuntime *rt = JS_NewRuntime();
        nb_watchdog_install(rt);
        if (!rt) { free(src); live_teardown(); send_status("STATUS err:heap"); return; }
        JSContext *ctx = JS_NewContext(rt);
        if (!ctx) { JS_FreeRuntime(rt); free(src); live_teardown(); send_status("STATUS err:heap"); return; }
        g_live_rt = rt;
        g_live_ctx = ctx;   /* resident from here on — kept alive for EVAL */
        install_host(ctx);

        /* rung 7: the resident worker's CSS resolve wins over install_host's
         * minimal __nb_ges default (the prelude reads __nb_ges at call time). */
        JSValue ge = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, ge, "__nb_ges",
                          JS_NewCFunction(ctx, nb_ges_rich, "__nb_ges", 1));
        JS_FreeValue(ctx, ge);

        /* rung-6 prelude: swallow its own failure, page continues */
        (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);

        boot_install_safe(ctx);

        run_scripts_slices(ctx, src, src_n);
        free(src);

        /* phase-2 (commit 7): lifecycle + event loop — timers/microtasks now fire */
        int ev_err = run_event_loop(ctx);
        if (ev_err) {
            char msg[1100];
            snprintf(msg, sizeof(msg), "STATUS err:%s",
                     g_pending_errmsg[0] ? g_pending_errmsg : "event loop error");
            send_status(msg);
            live_teardown();
            return;
        }

        /* step 4: emit RENDER rows from the post-JS DOM, then STATUS. The
         * manager merges these into page.state.txt so JS mutations show. */
        SB r = {0, 0, 0};
        dom_render_rows(&r);
        if (r.s && r.s[0]) {
            size_t rn = strlen(r.s);
            char *pay = NULL;
            size_t pn = 0;
            if (rn < RENDER_MAX) {
                pn = 7 + rn;                       /* "RENDER\n" + rows */
                pay = malloc(pn + 1);
                if (pay) {
                    memcpy(pay, "RENDER\n", 7);
                    memcpy(pay + 7, r.s, rn);
                    pay[pn] = 0;
                    send_payload(pay, pn);
                }
            }
            free(pay);
        }
        free(r.s);

        /* rung-6 slice 2: if the page asked to navigate, ship a NAV frame so the
         * manager follows it through its own fetch/stack machinery (manager
         * worker_load captures it, the main loop consumes it next tick). */
        if (g_nav_emit && g_nav_kind[0]) {
            char pay[4600];
            int pn = 0;
            if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')  /* BACK/FORWARD: step count */
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                              g_nav_count > 0 ? g_nav_count : 1);
            else
                pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
            if (pn > 0 && pn < (int)sizeof(pay)) {
                char *ppay = malloc((size_t)pn + 1);
                if (ppay) {
                    memcpy(ppay, pay, (size_t)pn); ppay[pn] = 0;
                    send_payload(ppay, (size_t)pn);
                    free(ppay);
                }
            }
        }

        /* devtools console EVAL: keep the heap + DOM tree resident so an
         * eval:<js> snippet can run against this page context (see cmd_eval);
         * live_teardown() reclaims it at the next LOAD / QUIT / error. */
        send_status("STATUS ok");
    }
}

/* devtools console EVAL: run a <js> snippet against the resident page heap
 * (g_live_ctx). Source + result (+ any console.* noise) echo to the console
 * capture (g_out / NBW_CONSOLE); post-eval DOM rows are re-emitted so JS
 * mutations show in the browser; an eval-triggered navigation ships a NAV
 * frame. Budget-guarded like page scripts (peval_budget / EVAL_BUDGET_SEC). */
static void cmd_eval(const char *js) {
    if (!g_live_ctx) { send_status("STATUS err:no page loaded"); return; }
    if (g_out) { fprintf(g_out, ">%s\n", js); fflush(g_out); }
    JSValue rv = peval_budget_value(g_live_ctx, js, strlen(js));
    if (JS_IsException(rv)) {
        char tmp[512];
        const char *m = js_error_to_cstr(g_live_ctx, tmp, sizeof(tmp));
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "eval error");
        if (g_out) { fprintf(g_out, "!>%s\n", m ? m : "eval error"); fflush(g_out); }
        JS_FreeValue(g_live_ctx, rv);
        send_status(msg);
        return;
    }
    char disp[512];
    const char *r = js_display_cstr(g_live_ctx, rv, disp, sizeof(disp));
    if (g_out) { fprintf(g_out, "=>%s\n", r ? r : ""); fflush(g_out); }
    JS_FreeValue(g_live_ctx, rv);
    /* Microtasks (incl. MutationObserver delivery) drain before render,
     * as in a browser task boundary. */
    drain_jobs(g_live_ctx);

    /* re-emit post-eval RENDER rows so mutated DOM is reflected */
    SB rr = {0, 0, 0};
    dom_render_rows(&rr);
    if (rr.s && rr.s[0]) {
        size_t rn = strlen(rr.s);
        if (rn < RENDER_MAX) {
            char *pay = malloc(7 + rn + 1);
            if (pay) {
                memcpy(pay, "RENDER\n", 7);
                memcpy(pay + 7, rr.s, rn);
                pay[7 + rn] = 0;
                send_payload(pay, 7 + rn);
                free(pay);
            }
        }
    }
    free(rr.s);

    /* an eval can navigate too (e.g. location.href = "...") */
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind,
                          g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay))
            send_payload(pay, (size_t)pn);
    }
    send_status("STATUS ok");
}

/* Commit 8 (Rung 6 slice 1): EVENT RPC — user click in window reaches scripted el.
 * Manager -> worker: EVENT\n<selector>\n<type>  (type defaults to click, bubbles+cancelable).
 * Dispatch is via JS (document.querySelector + new Event) so on-* and bubbling reuse the
 * existing dispatch_event path. After dispatch we drain microtasks/timers and re-emit
 * RENDER so mutations show, same as EVAL. */
static void cmd_event(const char *selector, const char *type) {
    if (!g_live_ctx) { send_status("STATUS err:no page loaded"); return; }
    if (!selector || !selector[0]) { send_status("STATUS err:empty selector"); return; }
    const char *evtype = (type && type[0]) ? type : "click";
    JSContext *ctx = g_live_ctx;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__nb_event_selector", JS_NewString(ctx, selector));
    JS_SetPropertyStr(ctx, global, "__nb_event_type", JS_NewString(ctx, evtype));
    JS_FreeValue(ctx, global);
    const char *js = "var __nb_el=document.querySelector(__nb_event_selector);"
                     "if(!__nb_el) throw new Error('not found:'+__nb_event_selector);"
                     "var __nb_ev=new Event(__nb_event_type,{bubbles:true,cancelable:true});"
                     "__nb_el.dispatchEvent(__nb_ev); __nb_ev.type;";
    JSValue rv = peval_budget_value(ctx, js, strlen(js));
    JSValue g2 = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g2, "__nb_event_selector", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, g2, "__nb_event_type", JS_UNDEFINED);
    JS_FreeValue(ctx, g2);
    if (JS_IsException(rv)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        char msg[1100];
        snprintf(msg, sizeof(msg), "STATUS err:%s", m ? m : "event error");
        JS_FreeValue(ctx, rv);
        send_status(msg);
        return;
    }
    JS_FreeValue(ctx, rv);
    drain_jobs(ctx);
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    run_due_timers(ctx, now);
    drain_jobs(ctx);
    SB rr = {0, 0, 0};
    dom_render_rows(&rr);
    if (rr.s && rr.s[0]) {
        size_t rn = strlen(rr.s);
        if (rn < RENDER_MAX) {
            char *pay = malloc(7 + rn + 1);
            if (pay) {
                memcpy(pay, "RENDER\n", 7);
                memcpy(pay + 7, rr.s, rn);
                pay[7 + rn] = 0;
                send_payload(pay, 7 + rn);
                free(pay);
            }
        }
    }
    free(rr.s);
    if (g_nav_emit && g_nav_kind[0]) {
        char pay[4600];
        int pn = 0;
        if (g_nav_kind[0] == 'B' || g_nav_kind[0] == 'F')
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%d\n", g_nav_kind, g_nav_count > 0 ? g_nav_count : 1);
        else
            pn = snprintf(pay, sizeof(pay), "NAV\n%s\n%s\n", g_nav_kind, g_nav_url);
        if (pn > 0 && pn < (int)sizeof(pay))
            send_payload(pay, (size_t)pn);
    }
    send_status("STATUS ok");
}

/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon. */
/* bare `duk` on a terminal: a tiny stateful REPL (no page/lifecycle events).
 * Exits on EOF or exit/quit/.exit. Non-tty stdin stays the framed daemon, but
 * `duk -i` forces the REPL even when stdin is piped. */
/* CLI-2/CLI-3 native hook used by both the REPL and node mode. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void      nb_install_fs(JSContext *ctx);
static const char g_cjs_prelude[];
static int repl_main(void) {
    JSRuntime *rt = JS_NewRuntime();
    nb_watchdog_install(rt);
    if (!rt) return 1;
    JSContext *ctx = rt ? JS_NewContext(rt) : NULL;
    if (!ctx) { if (rt) JS_FreeRuntime(rt); return 1; }
    g_cli = 1; g_cli_log = 1;
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);
    install_host(ctx);
    (void)peval_budget(ctx, g_js_prelude, strlen(g_js_prelude), NULL, 0);
    /* CLI-2/CLI-3 in the REPL too: require() + fs work line-by-line,
     * sharing the browser prelude above (window/document still present). */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
    nb_install_fs(ctx);
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "err:%s\n", errbuf[0] ? errbuf : "loader error");
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    char cwd_buf[PATH_MAX];
    if (!getcwd(cwd_buf, sizeof(cwd_buf))) snprintf(cwd_buf, sizeof(cwd_buf), ".");
    JSValue av[2];
    av[0] = JS_NewString(ctx, cwd_buf);
    av[1] = JS_NewString(ctx, "[repl]");
    JSValue g0 = JS_GetGlobalObject(ctx);
    nb_watchdog_begin();
    JSValue rr = JS_Call(ctx, install, g0, 2, av);
    JS_FreeValue(ctx, g0);
    JS_FreeValue(ctx, install);
    nb_watchdog_pause();
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "err:%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        free_held_callbacks(ctx);
        JS_FreeContext(ctx); JS_FreeRuntime(rt); dom_teardown(); return 1;
    }
    JS_FreeValue(ctx, rr);
    static const char empty_html[] = "<html><body></body></html>";
    g_dom_root = nb_parse_html(empty_html, sizeof(empty_html) - 1);
    boot_install_safe(ctx);

    int tty_out = isatty(STDOUT_FILENO);
    char line[8192];
    if (tty_out) { printf("nbjs (duk) — type JS; Ctrl-D or 'exit' to quit\n"); fflush(stdout); }
    for (;;) {
        if (tty_out) { fputs("> ", stdout); fflush(stdout); }
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t ln = strlen(line);
        while (ln && (line[ln - 1] == '\n' || line[ln - 1] == '\r')) line[--ln] = 0;
        if (!ln) continue;
        if (!strcmp(line, "exit") || !strcmp(line, "quit") || !strcmp(line, ".exit")) break;

        /* don't leak listeners/timers across lines */
        free_held_callbacks(ctx);
g_timer_count = 0; g_evl_count = 0; g_onprop_count = 0;
    g_search_wired = 0;
        g_invocations = 0; g_raf_fires = 0;
        g_pending_err = 0; g_pending_errmsg[0] = 0;

        /* CLI-4: single-line ESM (import/export) → CJS in the REPL too.
         * Multi-line import/export statements are out of scope here. */
        JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
        if (JS_IsFunction(ctx, prep)) {
            JSValue av2[1]; av2[0] = JS_NewString(ctx, line);
            JSValue g1 = JS_GetGlobalObject(ctx);
            JSValue rp = JS_Call(ctx, prep, g1, 1, av2);
            JS_FreeValue(ctx, g1);
            JS_FreeValue(ctx, av2[0]);
            if (!JS_IsException(rp)) {
                if (JS_IsString(rp)) {
                    size_t sl;
                    char *ps = JS_ToCStringLen(ctx, &sl, rp);
                    if (ps) {
                        if (sl < sizeof(line)) { memcpy(line, ps, sl); line[sl] = 0; }
                        JS_FreeCString(ctx, ps);
                    }
                }
            } else {
                JSValue e = JS_GetException(ctx);
                JS_FreeValue(ctx, e);
            }
            JS_FreeValue(ctx, rp);
        }
        JS_FreeValue(ctx, prep);

        JSValue rv = peval_budget_value(ctx, line, strlen(line));
        if (JS_IsException(rv)) {
            char tmp[512];
            const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
            printf("err:%s\n", m ? m : "script error");
        } else {
            if (!JS_IsUndefined(rv)) {
                char disp[512];
                const char *s = js_display_cstr(ctx, rv, disp, sizeof(disp));
                printf("%s\n", s ? s : "");
            }
        }
        JS_FreeValue(ctx, rv);
        fflush(stdout);

        /* drain jobs + timers (no lifecycle events), CPU-bounded */
        nb_watchdog_begin();
        uint64_t start = now_ms();
        for (int guard = 0; guard < 10000 && !g_pending_err; guard++) {
            if (now_ms() - start > 200) break;
            uint64_t now = now_ms();
            int ran = run_due_timers(ctx, now);
            drain_jobs(ctx);
            if (!ran) {
                uint64_t m = timer_min_due();
                if (!m) break;
                if (m <= now) continue;
                uint64_t d = m - now; if (d > 5) d = 5;
                struct timespec ts = { (time_t)(d / 1000), (long)((d % 1000) * 1000000L) };
                nanosleep(&ts, NULL);
            }
        }
        nb_watchdog_pause();
    }
    free_held_callbacks(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    dom_teardown();
    return 0;
}

/* ---- CLI-1: node-like runner (nbjs file.js [args...]) ---- */
static JSValue nb_cli_stdout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (g_out) { fputs(s ? s : "", g_out); fflush(g_out); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static JSValue nb_cli_stderr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *s = NULL;
    if (argc > 0) s = (own = JS_ToCString(ctx, argv[0]));
    if (s) { fputs(s, stderr); fflush(stderr); }
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
/* console.error -> stderr (node parity; log/info/warn stay on stdout) */
static JSValue nb_cli_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        char *own = NULL;
        const char *s = (own = JS_ToCString(ctx, argv[i]));
        if (s) fputs(s, stderr);
        JS_FreeCString(ctx, own);
        if (i + 1 < argc) fputs(" ", stderr);
    }
    fputs("\n", stderr);
    fflush(stderr);
    return JS_UNDEFINED;
}
/* CLI-2: read a module file for require(). Returns the source string, or
 * undefined if the path is missing/unreadable (loader turns that into a
 * "Cannot find module" error). Same 512 kB cap as the page loader. */
static JSValue nb_cjs_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    JSValue r;
    if (p && p[0] && read_file(p, &s, &n)) {
        r = JS_NewStringLen(ctx, s, n);
        free(s);
    } else {
        r = JS_UNDEFINED;
    }
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_cli_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int code = 0;
    if (argc > 0 && JS_IsNumber(argv[0])) {
        double d;
        if (JS_ToFloat64(ctx, &d, argv[0]) == 0) code = (int)d;
    }
    if (g_out) fflush(g_out);
    exit(code);
    return JS_UNDEFINED;
}
static JSValue nb_cli_cwd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) return JS_NewString(ctx, buf);
    return JS_NewString(ctx, "/");
}
/* ---- CLI-3: fs-lite natives (sync-only, over the same read_file cap).
 * String-only payloads (no Buffer type); an optional encoding arg is
 * accepted so node-style call sites keep working. */
static JSValue nb_fs_readfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = NULL;
    if (argc > 0) p = (own = JS_ToCString(ctx, argv[0]));
    char *s = NULL; size_t n = 0;
    if (!p || !p[0] || !read_file(p, &s, &n)) {
        char eb[512];
        snprintf(eb, sizeof(eb), "ENOENT: cannot read '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    JSValue r = JS_NewStringLen(ctx, s, n);
    free(s);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_writefile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "wb");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot write '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_appendfile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *po = NULL, *do_ = NULL;
    const char *p = argc > 0 ? (po = JS_ToCString(ctx, argv[0])) : NULL;
    const char *d = argc > 1 ? (do_ = JS_ToCString(ctx, argv[1])) : NULL;
    FILE *f = fopen(p, "ab");
    if (!f) {
        char eb[512];
        snprintf(eb, sizeof(eb), "EIO: cannot append '%s'", p ? p : "(empty)");
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, eb));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
        return JS_EXCEPTION;
    }
    if (d) fwrite(d, 1, strlen(d), f);
    fclose(f);
    JS_FreeCString(ctx, po); JS_FreeCString(ctx, do_);
    return JS_UNDEFINED;
}
static JSValue nb_fs_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    JSValue r = JS_NewBool(ctx, p && p[0] && access(p, F_OK) == 0);
    JS_FreeCString(ctx, own);
    return r;
}
static JSValue nb_fs_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char *own = NULL;
    const char *p = argc > 0 ? (own = JS_ToCString(ctx, argv[0])) : NULL;
    if (!p || !p[0]) {
        JSValue e = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, e, "message", JS_NewString(ctx, "EINVAL: empty mkdir path"));
        JS_Throw(ctx, e);
        JS_FreeCString(ctx, own);
        return JS_EXCEPTION;
    }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", p);
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
    JS_FreeCString(ctx, own);
    return JS_UNDEFINED;
}
static void nb_install_fs(JSContext *ctx) {
    JSValue fs = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fs, "readFileSync",   JS_NewCFunction(ctx, nb_fs_readfile, "readFileSync", 1));
    JS_SetPropertyStr(ctx, fs, "writeFileSync",  JS_NewCFunction(ctx, nb_fs_writefile, "writeFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "appendFileSync", JS_NewCFunction(ctx, nb_fs_appendfile, "appendFileSync", 2));
    JS_SetPropertyStr(ctx, fs, "existsSync",     JS_NewCFunction(ctx, nb_fs_exists, "existsSync", 1));
    JS_SetPropertyStr(ctx, fs, "mkdirSync",      JS_NewCFunction(ctx, nb_fs_mkdir, "mkdirSync", 1));
    JSValue fsg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, fsg, "__nb_fs", fs);
    JS_FreeValue(ctx, fsg);
}

/* build a JS object from the process environment (not the full sys env —
 * see getenv below). Env exposure is opt-in via require('os')-free helper;
 * CLI-1 keeps it simple: expose a snapshot under process.env. */
static void nb_cli_install(JSContext *ctx, int argc, char **argv) {
    JSValue process = JS_NewObject(ctx);
    /* argv — node convention: [interpreter, script, args...] */
    JSValue argv_arr = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++) {
        JS_SetPropertyUint32(ctx, argv_arr, (uint32_t)i, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, process, "argv", argv_arr);

    /* env (snapshot of environ, ENAME="value" pairs) */
    JSValue env_obj = JS_NewObject(ctx);
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq) continue;
        char *k = strndup(*e, (size_t)(eq - *e));
        if (k) { JS_SetPropertyStr(ctx, env_obj, k, JS_NewString(ctx, eq + 1)); free(k); }
    }
    JS_SetPropertyStr(ctx, process, "env", env_obj);
    JS_SetPropertyStr(ctx, process, "cwd", JS_NewCFunction(ctx, nb_cli_cwd, "cwd", 0));
    /* stdout / stderr — each a small object with write() */
    JSValue so = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, so, "write", JS_NewCFunction(ctx, nb_cli_stdout, "write", 1));
    JS_SetPropertyStr(ctx, process, "stdout", so);
    JSValue se = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, se, "write", JS_NewCFunction(ctx, nb_cli_stderr, "write", 1));
    JS_SetPropertyStr(ctx, process, "stderr", se);
    JS_SetPropertyStr(ctx, process, "exit", JS_NewCFunction(ctx, nb_cli_exit, "exit", 1));

    /* expose as global `process` */
    JSValue pg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, pg, "process", process);
    JS_FreeValue(ctx, pg);
}

/* The released browser page runner: duk --browser page.js [fetch.dom]
 * runs the full DOM engine (tree, events+timer loop, fetch/XHR+Promise,
 * render-back) with rendered rows -> stdout. exit 0 ok / 1 js err / 2 usage.
 * Same code path the old `duk page.js` default took before --node/--browser. */
static int browser_cli_main(int argc, char **argv) {
    g_cli = 1;
    g_cli_log = 1;   /* bare console lines, no LOG| prefix */
    if (!g_out) { g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0); }
    const char *pg = argv[1];
    if (argc < 2 || strcmp(pg, "-h") == 0 || strcmp(pg, "--help") == 0) {
        fprintf(stderr, "usage: duk --browser <page.js> [fetch.dom]\n");
        return 2;
    }
    const char *base = strrchr(pg, '/');
    snprintf(g_title, sizeof(g_title), "%s", base ? base + 1 : pg);
    snprintf(g_href, sizeof(g_href), "file://%s", pg);
    snprintf(g_page_js, sizeof(g_page_js), "%s", pg);
    if (argc > 2) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", argv[2]);
    if (access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }
    run_page();
    return g_cli_status_ok ? 0 : 1;
}

/* CLI-2 CommonJS loader + CLI-4 source-level ESM transpiler. Written in
 * conservative ES5.1 (no arrows/let): relative/absolute
 * resolution, module/exports wrapper, JSON require, cycles, and a line-
 * based import/export → CJS rewrite. Exposes `require`/`__dirname`/`__filename`
 * plus `__nb_esm_prepare(src)` for the C entry hook and REPL. */
static const char g_cjs_prelude[] =
"/* NB-JS loader (CLI-2 CJS + CLI-4 ESM transpile). */\n"
"(function(){\n"
"var cache = {};\n"
"function dirname(p){\n"
"  var i = p.lastIndexOf('/');\n"
"  if (i <= 0) return '/';\n"
"  return p.slice(0, i);\n"
"}\n"
"function resolve(req, dir){\n"
"  if (req.charAt(0) === '/') return req;\n"
"  var two = req.slice(0, 2);\n"
"  if (two !== './' && two !== '..' && req.slice(0, 3) !== '../') return null;\n"
"  var base = (dir === '/' ? '' : dir).replace(/\\/+$/, '');\n"
"  var parts = (base + '/' + req).split('/');\n"
"  var out = [];\n"
"  for (var i = 0; i < parts.length; i++){\n"
"    var p = parts[i];\n"
"    if (p === '' || p === '.') continue;\n"
"    if (p === '..'){ if (out.length) out.pop(); else return null; continue; }\n"
"    out.push(p);\n"
"  }\n"
"  return '/' + out.join('/');\n"
"}\n"
"\n"
"/* --- CLI-4: ESM source-level transpile ---\n"
" * Heuristic: a line starting (after ws) with `import` or `export` triggers\n"
" * transpile. Handles: default/named/namespace/side-effect imports,\n"
" * function/variable/const/default/named/re-export/export-star-from.\n"
" * Output uses `var` (conservative). Does NOT support multi-line imports,\n"
" * decorators, type annotations, or dynamic `import()`. */\n"
"function esmLooks(s){\n"
"  return /(^|\\n)\\s*(import|export)\\b/.test(s);\n"
"}\n"
"\n"
"function esmTranspile(src){\n"
"  var lines = src.split('\\n');\n"
"  var out = [];\n"
"  var eq = [];     /* deferred: exports.X = expr; at end */\n"
"  var nsc = [];    /* export * from copy-loops */\n"
"  out.push(\"Object.defineProperty(exports,'__esModule',{value:true});\");\n"
"  function tr(s){ return s.replace(/^\\s+|\\s+$/g, ''); }\n"
"  for (var i = 0; i < lines.length; i++){\n"
"    var line = lines[i];\n"
"    var t = tr(line);\n"
"    var lead = line.slice(0, line.length - line.replace(/^\\s+/, '').length);\n"
"    var m;\n"
"    /* import * as ns from '...'; */\n"
"    m = t.match(/^import\\s*\\*\\s*as\\s+([A-Za-z_$][\\w$]*)\\s+from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){ out.push(lead+'var '+m[1]+' = require('+m[2]+m[3]+m[2]+');'); continue; }\n"
"    /* import { a, b as c } from '...'; */\n"
"    m = t.match(/^import\\s*\\{([^}]+)\\}\\s*from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad import spec at line '+(i+1));\n"
"        var loc = nm[1], al = am ? am[2] : nm[1];\n"
"        out.push(lead+'var '+al+' = require('+m[2]+m[3]+m[2]+').'+loc+';');\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* import X from '...'; — node interop: unwrap .default if __esModule */\n"
"    m = t.match(/^import\\s+([A-Za-z_$][\\w$]*)\\s+from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      out.push(lead+'var '+m[1]+' = (function(__nb_m){ return (__nb_m && __nb_m.__esModule) ? __nb_m.default : __nb_m; })(require('+m[2]+m[3]+m[2]+'));');\n"
"      continue;\n"
"    }\n"
"    /* import '...'; */\n"
"    m = t.match(/^import\\s+(['\"])([^'\"]+)\\1;?$/);\n"
"    if (m){ out.push(lead+'require('+m[1]+m[2]+m[1]+');'); continue; }\n"
"    /* export * from '...'; */\n"
"    m = t.match(/^export\\s*\\*\\s*from\\s+(['\"])([^'\"]+)\\1;?$/);\n"
"    if (m){\n"
"      var v = '__nb_ns_'+i;\n"
"      nsc.push({lead:lead,pkg:m[2],v:v});\n"
"      out.push(lead+'var '+v+' = require('+m[1]+m[2]+m[1]+');');\n"
"      continue;\n"
"    }\n"
"    /* export { x, y as z } from '...'; */\n"
"    m = t.match(/^export\\s*\\{([^}]+)\\}\\s*from\\s+(['\"])([^'\"]+)\\2;?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad re-export spec at line '+(i+1));\n"
"        var loc = nm[1], al = am ? am[2] : nm[1];\n"
"        eq.push({l:al, r:'require('+m[2]+m[3]+m[2]+').'+loc});\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* export { a, b as c }; */\n"
"    m = t.match(/^export\\s*\\{([^}]+)\\};?$/);\n"
"    if (m){\n"
"      var specs = m[1].split(',');\n"
"      for (var si = 0; si < specs.length; si++){\n"
"        var s = tr(specs[si]); if (!s) continue;\n"
"        var am = s.match(/^([A-Za-z_$][\\w$]*)\\s+as\\s+([A-Za-z_$][\\w$]*)$/);\n"
"        var nm = am ? am : s.match(/^([A-Za-z_$][\\w$]*)$/);\n"
"        if (!nm) throw new Error('nbjs ESM: bad export spec at line '+(i+1));\n"
"        var al = am ? am[2] : nm[1], loc = nm[1];\n"
"        eq.push({l:al, r:loc});\n"
"      }\n"
"      continue;\n"
"    }\n"
"    /* export default function name(...) { ... } — keep the body flowing */\n"
"    m = t.match(/^export\\s+default\\s+function\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+default\\s+/, ''));\n"
"      eq.push({l:'default', r:m[1]});\n"
"      continue;\n"
"    }\n"
"    /* export default function (...) { ... } — anonymous, name it */\n"
"    m = t.match(/^export\\s+default\\s+function\\b/);\n"
"    if (m){\n"
"      var dname = '__nb_default_'+i;\n"
"      out.push(lead+line.replace(/^export\\s+default\\s+function/, 'function '+dname));\n"
"      eq.push({l:'default', r:dname});\n"
"      continue;\n"
"    }\n"
"    /* export default expr */\n"
"    m = t.match(/^export\\s+default\\s+(.+)$/);\n"
"    if (m){ eq.push({l:'default', r:'('+m[1]+')'}); continue; }\n"
"    /* export function f(...)... */\n"
"    m = t.match(/^export\\s+function\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+function/, 'function'));\n"
"      eq.push({l:m[1], r:m[1]});\n"
"      continue;\n"
"    }\n"
"    /* export var/const f = ... */\n"
"    m = t.match(/^export\\s+(var|const)\\s+([A-Za-z_$][\\w$]*)/);\n"
"    if (m){\n"
"      out.push(lead+line.replace(/^export\\s+(var|const)/, '$1'));\n"
"      eq.push({l:m[2], r:m[2]});\n"
"      continue;\n"
"    }\n"
"    out.push(line);\n"
"  }\n"
"  for (var ei = 0; ei < eq.length; ei++)\n"
"    out.push('exports.'+eq[ei].l+' = '+eq[ei].r+';');\n"
"  for (var ci = 0; ci < nsc.length; ci++){\n"
"    var c = nsc[ci];\n"
"    out.push(c.lead+'var __nb_keys_'+ci+' = Object.keys('+c.v+');');\n"
"    out.push(c.lead+'for(var __nb_j_'+ci+'=0; __nb_j_'+ci+'<__nb_keys_'+ci+'.length; __nb_j_'+ci+'++){');\n"
"    out.push(c.lead+'  var __nb_k = __nb_keys_'+ci+'[__nb_j_'+ci+'];');\n"
"    out.push(c.lead+'  if(!(__nb_k in exports) && __nb_k !== \\'default\\' && __nb_k !== \\'__esModule\\') exports[__nb_k] = '+c.v+'[__nb_k];');\n"
"    out.push(c.lead+'}');\n"
"  }\n"
"  return out.join('\\n');\n"
"}\n"
"\n"
"function esmPrepare(src){\n"
"  if (esmLooks(src)) return esmTranspile(src);\n"
"  return src;\n"
"}\n"
"\n"
"function makeRequire(dir){\n"
"  return function(request){\n"
"    if (request === 'fs'){ if (typeof __nb_fs !== 'undefined') return __nb_fs; }\n"
"    var abs = resolve(request, dir);\n"
"    if (abs === null)\n"
"      throw new Error(\"Cannot find module '\" + request + \"' (nbjs has no packages/builtins)\");\n"
"    if (abs in cache) return cache[abs].exports;\n"
"    var src = __nb_read_file(abs);\n"
"    if (src === undefined || src === null)\n"
"      throw new Error(\"Cannot find module '\" + request + \"' (resolved to \" + abs + \")\");\n"
"    var mod = { id: abs, filename: abs, exports: {}, loaded: false };\n"
"    cache[abs] = mod;\n"
"    if (abs.slice(-5) === '.json'){\n"
"      mod.exports = JSON.parse(src);\n"
"      mod.loaded = true;\n"
"      return mod.exports;\n"
"    }\n"
"    if (src.charAt(0) === '#'){\n"
"      var nl = src.indexOf('\\n');\n"
"      if (nl >= 0) src = src.slice(nl + 1); else src = '';\n"
"    }\n"
"    src = esmPrepare(src);\n"
"    var rd = dirname(abs);\n"
"    var fn = new Function('exports', 'require', 'module', '__filename', '__dirname', src);\n"
"    fn.call(mod.exports, mod.exports, makeRequire(rd), mod, abs, rd);\n"
"    mod.loaded = true;\n"
"    return mod.exports;\n"
"  };\n"
"}\n"
"this.__nb_install_cjs = function(entryDir, entryFile){\n"
"  this.require = makeRequire(entryDir);\n"
"  this.__dirname = entryDir;\n"
"  this.__filename = entryFile;\n"
"  this.module = { id: entryFile, filename: entryFile, exports: {} };\n"
"  this.exports = this.module.exports;\n"
"};\n"
"this.__nb_esm_prepare = esmPrepare;\n"
"})();\n";

static int cli_main(int argc, char **argv) {
    g_cli = 1; g_cli_log = 1;         /* bare console lines -> stdout */
    g_out = stdout; setvbuf(g_out, NULL, _IONBF, 0);

    /* parse: duk [--node] file.js|-- [args...] */
    int script_i = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
            fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
            fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
            return 2;
        }
        if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) continue;
        script_i = i; break;
    }
    if (script_i < 0) {
        fprintf(stderr, "usage: duk <file.js|-> [args...]   (node mode; require()+fs, ESM import/export)\n");
        fprintf(stderr, "       duk --browser <page.js> [fetch.dom]   (DOM page mode)\n");
        fprintf(stderr, "       duk -i                        (interactive REPL, even piped)\n");
        return 2;
    }
    const char *pg = argv[script_i];
    if (strcmp(pg, "-") != 0 && access(pg, R_OK) != 0) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        return 2;
    }

    JSRuntime *rt = JS_NewRuntime();
    nb_watchdog_install(rt);
    if (!rt) return 1;
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) { JS_FreeRuntime(rt); return 1; }

    /* node-like host: console/print only, plus process. No DOM, no events,
     * no timers, no browser prelude — window/document/location are absent. */
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue console = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, console, "log",   JS_NewCFunction(ctx, native_log, "log", 1));
        JS_SetPropertyStr(ctx, console, "info",  JS_NewCFunction(ctx, native_log, "info", 1));
        JS_SetPropertyStr(ctx, console, "warn",  JS_NewCFunction(ctx, native_log, "warn", 1));
        JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, nb_cli_error, "error", 1));
        JS_SetPropertyStr(ctx, g, "print",  JS_NewCFunction(ctx, native_log, "print", 1));
        JS_SetPropertyStr(ctx, g, "console", console);
        JS_FreeValue(ctx, g);
    }

    /* process.argv = [interp, script, args...] (node convention) */
    {
        int nargv = 1 + (argc - script_i);
        char **a = calloc((size_t)nargv, sizeof(char *));
        if (!a) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
        a[0] = argv[0];
        for (int i = 0; i + script_i < argc; i++) a[i + 1] = argv[script_i + i];
        nb_cli_install(ctx, nargv, a);
        free(a);
    }

    /* CLI-2 CommonJS + CLI-3 fs: the only host hook the JS loader needs is
     * a module file reader; it defines `require`/`__dirname`/`__filename`
     * itself. `require('fs')` resolves to the fs-lite natives below. */
    JSValue rg = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, rg, "__nb_read_file",
                      JS_NewCFunction(ctx, nb_cjs_read_file, "__nb_read_file", 1));
    JS_FreeValue(ctx, rg);
    nb_install_fs(ctx);

    /* Entry directory/file, absolute, for the entry script's require base. */
    static char entry_dir[PATH_MAX], entry_file[PATH_MAX];
    {
        if (strcmp(pg, "-") == 0) {
            if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
            snprintf(entry_file, sizeof(entry_file), "[stdin]");
        } else {
            static char rp[PATH_MAX];
            if (!realpath(pg, rp)) snprintf(rp, sizeof(rp), "%s", pg);
            const char *abs = (rp[0] == '/' ? rp : pg);
            snprintf(entry_file, sizeof(entry_file), "%s", abs);
            const char *sl = strrchr(abs, '/');
            if (sl == abs) snprintf(entry_dir, sizeof(entry_dir), "/");
            else if (sl) { size_t d = (size_t)(sl - abs); memcpy(entry_dir, abs, d); entry_dir[d] = 0; }
            else if (!getcwd(entry_dir, sizeof(entry_dir))) snprintf(entry_dir, sizeof(entry_dir), ".");
        }
    }
    char errbuf[512];
    if (peval_budget(ctx, g_cjs_prelude, strlen(g_cjs_prelude), errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "loader error");
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue install = get_global_attr(ctx, "__nb_install_cjs");
    if (!JS_IsFunction(ctx, install)) {
        JS_FreeValue(ctx, install);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JSValue av[2];
    av[0] = JS_NewString(ctx, entry_dir);
    av[1] = JS_NewString(ctx, entry_file);
    JSValue g1 = JS_GetGlobalObject(ctx);
    nb_watchdog_begin();
    JSValue rr = JS_Call(ctx, install, g1, 2, av);
    JS_FreeValue(ctx, g1);
    JS_FreeValue(ctx, install);
    nb_watchdog_pause();
    JS_FreeValue(ctx, av[0]); JS_FreeValue(ctx, av[1]);
    if (JS_IsException(rr)) {
        char tmp[512];
        const char *m = js_error_to_cstr(ctx, tmp, sizeof(tmp));
        fprintf(stderr, "%s\n", m ? m : "loader error");
        JS_FreeValue(ctx, rr);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    JS_FreeValue(ctx, rr);

    char *src = NULL; size_t n = 0;
    if (strcmp(pg, "-") == 0) {
        /* read the whole script from stdin */
        {
            size_t cap = 1 << 16, len = 0;
            char *b = malloc(cap);
            if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
            for (;;) {
                size_t got = fread(b + len, 1, cap - len, stdin);
                len += got;
                if (len == cap) { cap *= 2; b = realloc(b, cap); if (!b) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; } }
                if (feof(stdin) || got == 0) break;
            }
            b[len] = 0; src = b; n = len;
        }
    } else if (!read_file(pg, &src, &n)) {
        fprintf(stderr, "nbjs: cannot read %s\n", pg);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 2;
    }

    /* CLI-4: source-level ESM — ask the loader's __nb_esm_prepare for
     * transpiled source (or the original unchanged). The loader prelude
     * must have run already (__nb_install_cjs installs it). */
    JSValue prep = get_global_attr(ctx, "__nb_esm_prepare");
    if (JS_IsFunction(ctx, prep)) {
        JSValue av2[1]; av2[0] = JS_NewStringLen(ctx, src, n);
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue rp = JS_Call(ctx, prep, g2, 1, av2);
        JS_FreeValue(ctx, g2);
        if (JS_IsException(rp)) {
            JSValue e = JS_GetException(ctx);
            JS_FreeValue(ctx, e);
        } else if (JS_IsString(rp)) {
            size_t sl;
            char *ps = JS_ToCStringLen(ctx, &sl, rp);
            if (ps) {
                char *ns = malloc(sl + 1);
                if (ns) { memcpy(ns, ps, sl + 1); free(src); src = ns; n = sl; }
                JS_FreeCString(ctx, ps);
            }
        }
        JS_FreeValue(ctx, rp);
        JS_FreeValue(ctx, av2[0]);
    }
    JS_FreeValue(ctx, prep);

    /* Strip a leading shebang line (the page runner has the same rule), then
     * run the module body with a CPU budget. */
    if (n > 2 && src[0] == '#' && src[1] == '!') {
        char *nl = strchr(src, '\n');
        if (nl) { size_t off = (size_t)(nl + 1 - src); n -= off; memmove(src, nl + 1, n); src[n] = 0; }
    }
    if (peval_budget(ctx, src, n, errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr, "%s\n", errbuf[0] ? errbuf : "script error");
        free(src);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        return 1;
    }
    free(src);
    if (g_out) fflush(g_out);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

/* ---- fatal-signal backtrace (2026-10-26) ----
 * A hostile or oversized page script can take the worker down in ways that
 * leave NOTHING in the log: no WERR, no STATUS, just a zombie the manager
 * never hears from again. Measured on Cloudflare's orchestrate/chl_page
 * script: the last line in the worker log is "resolved script URL", and the
 * process is then gone. Without gdb on the box, the only way to tell a
 * SIGSEGV from an abort() is to have the worker report it itself. Gated by
 * NB_CRASH_BT so normal runs stay quiet. backtrace_symbols_fd() is the
 * async-signal-safe half of execinfo, and stderr is already the per-house
 * worker log, so the trace lands next to the page errors. */
/* Worker fatal-signal handler extracted (Phase 1, single-TU include). */
#include "nb_worker_crash_handler.c"
/* Worker crash-signal installer extracted (Phase 1, single-TU include). */
#include "nb_worker_crash_install.c"

int main(int argc, char **argv) {
    nb_crash_install();
    g_out = NULL;   /* step 2: no effects file yet; console goes nowhere */
    if (getenv("NB_TRACE_CB")) g_trace_cb = 1;   /* full callback exceptions to stderr */
    {
        const char *nbw_out = getenv("NBW_CONSOLE");
        if (nbw_out && nbw_out[0]) {
            /* append ("ab") — the manager keeps a live console panel across
             * worker respawns and trims the file itself (trim_tail_file) */
            g_out = fopen(nbw_out, "ab");
            if (g_out) setvbuf(g_out, NULL, _IOLBF, 0);   /* console capture (debug/tests) */
        }
    }

    /* duk -i / --interactive: force the REPL even when stdin is piped
     * (bare `duk` also reaches it on a tty via the isatty check below). */
    if (argc > 1 && (strcmp(argv[1], "-i") == 0 || strcmp(argv[1], "--interactive") == 0))
        return repl_main();

    if (argc > 1) {
        /* duk --browser page.js [fetch.dom] — the released DOM page runner
         * (full DOM engine + render-back). duk file.js — node mode, no
         * browser globals, process/console + require()/fs host. Either way:
         * exit 0 clean / 1 thrown error / 2 usage. No RPC framing, no khtpm
         * dependency. */
        if (strcmp(argv[1], "--browser") == 0 || strcmp(argv[1], "-b") == 0)
            return browser_cli_main(argc - 1, argv + 1);
        return cli_main(argc, argv);
    }

    /* bare `duk` with a terminal as stdin: interactive REPL instead of the
     * framed daemon (the manager spawns over a socketpair -> not a tty). */
    if (isatty(STDIN_FILENO)) return repl_main();

    for (;;) {
        /* A bounded wait is what lets a page keep running between inputs.
         * NB_RECV_SHUTDOWN still ends the loop exactly as before; a budget
         * timeout is not an exit, it is a tick that is due. */
        int why = NB_RECV_SHUTDOWN;
        if (!recv_frame_ex(tick_wait_ms(), &why)) {
            if (why == NB_RECV_SHUTDOWN) break;
            tick_slice();
            continue;
        }

        char *f[8];           split_lines(f);
        const char *cmd = f[0] ? f[0] : "";
        if (strcmp(cmd, "QUIT") == 0) {
            live_teardown();
            break;
        } else if (strcmp(cmd, "LOAD") == 0) {
            g_title[0] = 0; g_href[0] = 0;
            g_style_css[0] = 0;
            if (f[1]) snprintf(g_page_js, sizeof(g_page_js), "%s", f[1]);
            if (f[2]) snprintf(g_fetch_dom, sizeof(g_fetch_dom), "%s", f[2]);
            if (f[3]) snprintf(g_href, sizeof(g_href), "%s", f[3]);
            fprintf(stderr, "DEBUG: LOAD received, g_href = '%s'\n", g_href);
            if (f[4]) snprintf(g_title, sizeof(g_title), "%s", f[4]);
            if (f[5]) snprintf(g_style_css, sizeof(g_style_css), "%s", f[5]);
            /* f[6] = the viewport width this page will be laid out in.
             *
             * Until now the box tree assumed a fixed 640px column while
             * the renderer actually paints the content pane at 389px, so
             * every width the worker reported was wrong by ~250px (G2.2).
             * The manager does not know the pane width yet - it is the
             * RENDERER that lays the pane out - so this field exists and
             * is honoured, and nothing sends it yet. When it is sent, the
             * guess becomes a fact; until then behaviour is unchanged,
             * which is deliberate: a wrong-but-supplied number would be
             * worse than an obviously-default one.
             *
             * Absent or unusable (<= 0) leaves the current value alone
             * rather than resetting to the default mid-session. */
            if (f[6] && atof(f[6]) > 0) g_viewport_px = atof(f[6]);
            /* Field 7: the pane's content height. Width alone cannot say
             * whether the column scrolls, and a scrolling column is 8px
             * narrower once the scrollbar is drawn - see g_col_px(). */
            g_viewport_h = (f[7] && atof(f[7]) > 0) ? atof(f[7]) : 0.0;
            /* The page title, which the manager projects into the content
             * scrolllist as a real row. Without it the worker's origin sits
             * 30px above where the renderer paints the first paragraph. */
            g_page_title[0] = '\0';
            if (f[4]) {
                size_t tl = strlen(f[4]);
                if (tl >= sizeof(g_page_title)) tl = sizeof(g_page_title) - 1;
                memcpy(g_page_title, f[4], tl);
                g_page_title[tl] = '\0';
            }
            /* New page, so the gutter is an open question again. Leaving the
             * previous page's verdict in place would carry a scrollbar
             * decision across a navigation onto a page that may not scroll. */
            g_gutter_state = -1;
            free(g_last_push); g_last_push = NULL;   /* rows are per-page */
            run_page();
            if (g_module_page_loaded) {
                g_module_page_loaded = 0;
                break;  /* exit worker so manager respawns fresh worker */
            }
        } else if (strcmp(cmd, "TICK") == 0) {
            /* TICK\n<ms>\n - enable periodic ticking with stdin waits capped
             * at <ms>. TICK\n0 disables it, restoring the historical
             * indefinite wait. Opt-in: until the manager side of the push
             * protocol is ready, the resident worker must not emit frames
             * nobody asked for. A new page starts ticking only if it was
             * already on - this flag is the driver's, not the page's. */
            int want = f[1] ? atoi(f[1]) : 0;
            if (want < 0) want = 0;
            g_tick_cap_ms = want;
            free(g_last_push); g_last_push = NULL;
            send_status(want ? "STATUS ok" : "STATUS err:TICK wants ms >= 1");
        } else if (strcmp(cmd, "EVAL") == 0) {
            /* devtools console: eval:<js> — js is field 1 (address-bar single
             * line; embedded '\n' is split out by split_lines, fine for REPL) */
            cmd_eval(f[1] ? f[1] : "");
        } else if (strcmp(cmd, "EVENT") == 0) {
            cmd_event(f[1] ? f[1] : "", f[2] ? f[2] : "click");
        } else {
            send_status("STATUS err:unknown command");
        }
    }
    return 0;
}

