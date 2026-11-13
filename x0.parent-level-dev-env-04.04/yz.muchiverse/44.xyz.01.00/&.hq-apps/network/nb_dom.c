#include "nb_dom.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

#define DOM_MAX_NODES 50000   /* roadmap §2 node cap */

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) abort();
    return p;
}
static char *str_dup_n(const char *s, size_t n) {
    char *d = xmalloc(n + 1);
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}
static char *str_dup(const char *s) {
    return str_dup_n(s, strlen(s));
}

/* ---- html entity decode: returns decoded length, writes to out (>= n+1) ---- */
/* Emit one code point as UTF-8. Invalid code points (NUL, surrogates,
 * out-of-range) become U+FFFD, matching the HTML input stream rules. An
 * entity is always at least three source bytes, so the output is never
 * longer than the input; callers' buffers stay safe. */
static size_t utf8_put(long cp, char *out) {
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}
static size_t decode_entities(const char *p, size_t n, char *out) {
    size_t o = 0, i = 0;
    while (i < n) {
        if (p[i] == '&' && i + 1 < n) {
            const char *semi = memchr(p + i + 1, ';', n - (i + 1));
            if (semi) {
                size_t body = (size_t)(semi - (p + i) - 1);
                size_t off = (size_t)((semi - p) + 1);
                long cp = -1;
                if (body >= 2 && p[i + 1] == '#') {
                    const char *dig = p + i + 2;
                    if (body >= 2 && (dig[0] == 'x' || dig[0] == 'X')) {
                        const char *d = dig + 1; long v = 0;
                        while (d < semi && isxdigit((unsigned char)*d)) {
                            v = v * 16 + (isdigit((unsigned char)*d) ? *d-'0' : (tolower((unsigned char)*d)-'a'+10));
                            if (v > 0x10FFFF) v = 0x110000;   /* keep the clamp */
                            d++;
                        }
                        if (d == semi) cp = v;
                    } else {
                        const char *d = dig; long v = 0;
                        while (d < semi && isdigit((unsigned char)*d)) {
                            v = v*10 + (*d-'0');
                            if (v > 0x10FFFF) v = 0x110000;
                            d++;
                        }
                        if (d == semi) cp = v;
                    }
                } else if (body >= 2) {
                    char name[16];
                    size_t bl = body < sizeof(name)-1 ? body : sizeof(name)-1;
                    memcpy(name, p + i + 1, bl); name[bl] = 0;
                    /* Named references decode to code points, not to bytes:
                     * &nbsp; is U+00A0 (2 UTF-8 bytes), not a plain space,
                     * and &mdash; is U+2014 (3 bytes), not 0x97. */
                    if      (!strcasecmp(name,"amp"))   cp = 0x26;
                    else if (!strcasecmp(name,"lt"))    cp = 0x3C;
                    else if (!strcasecmp(name,"gt"))    cp = 0x3E;
                    else if (!strcasecmp(name,"quot"))  cp = 0x22;
                    else if (!strcasecmp(name,"apos"))  cp = 0x27;
                    else if (!strcasecmp(name,"nbsp"))  cp = 0xA0;
                    else if (!strcasecmp(name,"copy"))  cp = 0xA9;
                    else if (!strcasecmp(name,"reg"))   cp = 0xAE;
                    else if (!strcasecmp(name,"times")) cp = 0xD7;
                    else if (!strcasecmp(name,"divide"))cp = 0xF7;
                    else if (!strcasecmp(name,"mdash")) cp = 0x2014;
                    else if (!strcasecmp(name,"ndash")) cp = 0x2013;
                    else if (!strcasecmp(name,"hellip"))cp = 0x2026;
                }
                if (cp >= 0) { o += utf8_put(cp, out + o); i = off; continue; }
            }
        }
        out[o++] = p[i];
        i++;
    }
    out[o] = '\0';
    return o;
}

static void my_tolower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static const char *VOID_TAGS[] = {
    "area","base","br","col","embed","hr","img","input","link",
    "meta","param","source","track","wbr", NULL
};
static int is_void(const char *tag) {
    for (int i = 0; VOID_TAGS[i]; i++)
        if (!strcmp(tag, VOID_TAGS[i])) return 1;
    return 0;
}
/* Raw-text/RCDATA elements: keep the ELEMENT (its id/src/href attributes are
 * part of the DOM and real scripts query them) AND its source text, but never
 * parse that text as markup. textarea/title are RCDATA (entities decode);
 * script/style/noscript/xmp/iframe/noembed/noframes/plaintext are raw text.
 * `<head>` is deliberately NOT skipped: its children
 * (<script id="base-js" src=…>, <link href=…>) must be reachable through
 * getElementById — the closure module loader resolves the bundle URL from
 * exactly that script element (window._F_jsUrl is unset in our fixtures). */
static int is_rawtext(const char *tag) {
    return !strcmp(tag,"script") || !strcmp(tag,"style")
        || !strcmp(tag,"title") || !strcmp(tag,"noscript")
        || !strcmp(tag,"textarea") || !strcmp(tag,"xmp")
        || !strcmp(tag,"iframe") || !strcmp(tag,"noembed")
        || !strcmp(tag,"noframes") || !strcmp(tag,"plaintext");
}

typedef struct {
    const char *p, *end;
    NbNode *root;
    NbNode *stack[256];
    int depth;
    long count;
    int err;
    int text_nodes;   /* 1: character data becomes child text nodes */
} Parser;

static void parser_add_raw_text(Parser *ps, NbNode *n, const char *tag,
                                const char *s, size_t len);

static NbNode *node_new(Parser *ps, const char *tag) {
    if (ps->count >= DOM_MAX_NODES) { ps->err = 1; return NULL; }
    NbNode *n = xmalloc(sizeof(*n));
    memset(n, 0, sizeof(*n));
    n->tag = str_dup(tag);
    ps->count++;
    return n;
}

static void append_child(NbNode *parent, NbNode *child) {
    child->parent = parent;
    if (parent->last_child) parent->last_child->next_sibling = child;
    else parent->first_child = child;
    parent->last_child = child;
}

static int stack_pop_to(Parser *ps, const char *tag) {
    for (int i = ps->depth - 1; i >= 0; i--)
        if (ps->stack[i] && !strcmp(ps->stack[i]->tag, tag)) { ps->depth = i; return 1; }
    return 0;
}

/* skip to "</tag>", return pointer just past it */
static const char *skip_to_close(Parser *ps, const char *tag, const char **content_end) {
    /* Raw-text/RCDATA end tags allow HTML whitespace before '>' (e.g.
     * </script >, </style\t>), case-insensitively. Anything else inside the
     * body is data, so we only stop on a matching close. */
    size_t tl = strlen(tag);
    const char *scan = ps->p;
    while (scan < ps->end) {
        const char *lx = scan;
        while (lx < ps->end && *lx != '<') lx++;
        if (lx >= ps->end) break;
        /* match "</" then tag then optional whitespace then ">" */
        if ((size_t)(ps->end - lx) >= tl + 3 && lx[1] == '/'
            && strncasecmp(lx + 2, tag, tl) == 0) {
            const char *q = lx + tl + 2;
            while (q < ps->end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == '\f'))
                q++;
            if (q < ps->end && *q == '>') {
                if (content_end) *content_end = lx;
                return q + 1;
            }
        }
        scan = lx + 1;
    }
    if (content_end) *content_end = ps->end;
    return ps->end;
}

static void parse_open(Parser *ps) {
    const char *p = ps->p + 1;
    if (p < ps->end && p[0]=='!' && p+2 < ps->end && p[1]=='-' && p[2]=='-') {
        const char *close = strstr(p, "-->");
        ps->p = close ? close + 3 : ps->end;
        return;
    }
    if (p < ps->end && (*p=='!' || *p=='?')) { const char *gt=strchr(p,'>'); ps->p = gt?gt+1:ps->end; return; }
    int closing = 0;
    if (p < ps->end && *p=='/') { closing=1; p++; }
    while (p<ps->end && isspace((unsigned char)*p)) p++;
    const char *tname = p;
    while (p<ps->end && (isalnum((unsigned char)*p)||*p=='-'||*p==':')) p++;
    size_t tlen = (size_t)(p-tname);
    if (tlen==0) { ps->p = p+1<ps->end?p+1:ps->end; return; }
    const char *gt = p;
    {
        char qc=0; int inq=0;
        while (gt<ps->end) {
            char c=*gt;
            if (inq) { if (c==qc) { inq=0; qc=0; } }
            else if (c=='"'||c=='\'') { inq=1; qc=c; }
            else if (c=='>') break;
            gt++;
        }
    }
    char idbuf[256]="", clsbuf[1024]="";
    char raw[8192]; size_t rn=0;
    {
        size_t blen = (size_t)(gt-tname) < sizeof(raw)-1 ? (size_t)(gt-tname) : sizeof(raw)-1;
        memcpy(raw, tname, blen); raw[blen]=0; rn=blen;
        const char *aw = p;
        while (aw < gt) {
            while (aw<gt && isspace((unsigned char)*aw)) aw++;
            const char *ks=aw;
            while (aw<gt && !isspace((unsigned char)*aw) && *aw!='=' && *aw!='>') aw++;
            size_t kl=(size_t)(aw-ks);
            if (kl==0) { aw++; continue; }
            if (aw<gt && *aw=='=') {
                aw++;
                while (aw<gt && isspace((unsigned char)*aw)) aw++;
                char qc=0;
                if (aw<gt && (*aw=='"'||*aw=='\'')) { qc=*aw; aw++; }
                const char *vs=aw;
                if (qc) { while (aw<gt && *aw!=qc) aw++; }
                else    { while (aw<gt && !isspace((unsigned char)*aw)) aw++; }
                size_t vl=(size_t)(aw-vs);
                if (qc && aw<gt) aw++;
                char key[64]; size_t kc=kl<63?kl:63; memcpy(key,ks,kc); key[kc]=0; my_tolower(key);
                char val[1200]; size_t vc=vl<1199?vl:1199; memcpy(val,vs,vc); val[vc]=0;
                char decv[1200]; decode_entities(val, strlen(val), decv);
                if (!strcmp(key,"id")) { size_t d=strlen(decv)<255?strlen(decv):255; memcpy(idbuf,decv,d); idbuf[d]=0; }
                else if (!strcmp(key,"class")) { size_t d=strlen(decv)<1023?strlen(decv):1023; memcpy(clsbuf,decv,d); clsbuf[d]=0; }
            }
        }
    }
    char tag[64]; size_t tl=tlen<63?tlen:63; memcpy(tag,tname,tl); tag[tl]=0; my_tolower(tag);
    ps->p = gt<ps->end?gt+1:ps->end;

    if (closing) { stack_pop_to(ps, tag); return; }
    NbNode *n = node_new(ps, tag);
    if (!n) return;
    if (idbuf[0]) n->id = str_dup(idbuf);
    if (clsbuf[0]) n->cls = str_dup(clsbuf);
    if (rn) n->attrs = str_dup_n(raw, rn);
    NbNode *parent = ps->depth>0 ? ps->stack[ps->depth-1] : ps->root;
    append_child(parent, n);
    if (is_rawtext(tag)) {
        const char *content = ps->p;
        const char *cend = ps->end;
        ps->p = skip_to_close(ps, tag, &cend);
        parser_add_raw_text(ps, n, tag, content, (size_t)(cend - content));
        return;
    }
    if (!is_void(tag)) {
        if (ps->depth < (int)(sizeof(ps->stack)/sizeof(ps->stack[0]))-1)
            ps->stack[ps->depth++] = n;
        else ps->err = 1;
    }
}

static void parser_add_text(Parser *ps) {
    const char *lt = ps->p;
    while (lt<ps->end && *lt!='<') lt++;
    if (lt==ps->p) return;
    size_t n = (size_t)(lt-ps->p);
    char *buf = xmalloc(n+1);
    memcpy(buf, ps->p, n); buf[n]=0;
    char *dec = xmalloc(n*4+1);
    size_t d = decode_entities(buf, n, dec);
    /* Keep decoded text verbatim (entities only, no whitespace folding):
     * extra/separator whitespace is a display concern, not data. Folding
     * here would irrecoverably join words across element boundaries. */
    if (ps->text_nodes) {
        /* Browser-shaped tree: the run becomes its own #text child so
         * childNodes / firstChild / *Sibling / Range see it. Text between
         * top-level tags belongs to the parse root (that is the fragment
         * innerHTML/insertAdjacentHTML hand back); depth is 0 after the
         * last close, so it cannot use the stack. The element's own text
         * field stays empty on this path. */
        if (d > 0 && ps->count < DOM_MAX_NODES) {
            NbNode *t = xmalloc(sizeof(*t));
            memset(t, 0, sizeof(*t));
            t->text = dec;      /* transfer: freed below only on the folded path */
            ps->count++;
            append_child(ps->depth>0 ? ps->stack[ps->depth-1] : ps->root, t);
            dec = NULL;
        }
    } else if (ps->depth>0) {
        NbNode *cur=ps->stack[ps->depth-1];
        size_t oldn=cur->text?strlen(cur->text):0;
        char *dst=xmalloc(oldn+d+1);
        if (cur->text) memcpy(dst,cur->text,oldn);
        memcpy(dst+oldn,dec,d);
        dst[oldn+d]=0;
        free(cur->text);
        cur->text=dst;
    }
    free(buf); free(dec);
    ps->p=lt;
}

/* Raw-text/RCDATA elements keep their source text instead of parsing it as
 * markup. The content is stored the same way ordinary character data is -
 * folded into n->text for the fetch path, a real #text child for the
 * script-facing path - so textContent, childNodes, clone and the wire
 * serializer all agree. title/textarea are RCDATA (entities decode);
 * script/style/noscript/xmp/iframe/noembed/noframes/plaintext are raw text
 * (they do not). */
static void parser_add_raw_text(Parser *ps, NbNode *n, const char *tag,
                                const char *s, size_t len) {
    if (!n || len == 0) return;
    char *txt = xmalloc(len + 1);
    memcpy(txt, s, len);
    txt[len] = 0;
    if (!strcasecmp(tag, "title") || !strcasecmp(tag, "textarea")) {
        char *dec = xmalloc(len * 4 + 1);
        decode_entities(txt, len, dec);
        free(txt);
        txt = dec;
    }
    if (ps->text_nodes) {
        if (ps->count >= DOM_MAX_NODES) { free(txt); return; }
        NbNode *t = xmalloc(sizeof(*t));
        memset(t, 0, sizeof(*t));
        t->text = txt;
        ps->count++;
        append_child(n, t);
    } else {
        size_t oldn = n->text ? strlen(n->text) : 0;
        size_t add = strlen(txt);
        char *dst = xmalloc(oldn + add + 1);
        if (oldn) memcpy(dst, n->text, oldn);
        memcpy(dst + oldn, txt, add + 1);
        free(n->text);
        free(txt);
        n->text = dst;
    }
}

static NbNode *parse_html_impl(const char *html, size_t len, int text_nodes) {
    Parser ps;
    memset(&ps, 0, sizeof(ps));
    ps.p=html; ps.end=html+len;
    ps.text_nodes=text_nodes;
    ps.root=xmalloc(sizeof(NbNode)); memset(ps.root,0,sizeof(*ps.root));
    ps.root->tag=str_dup("#document");
    while (ps.p<ps.end && !ps.err) {
        if (*ps.p=='<') parse_open(&ps);
        else parser_add_text(&ps);
    }
    return ps.root;
}

NbNode *nb_parse_html(const char *html, size_t len) {
    return parse_html_impl(html, len, 0);
}

/* Browser-shaped variant: character data between tags becomes real #text
 * children instead of being folded into the element's own text field. Used
 * by the script-facing parsing surfaces (innerHTML, insertAdjacentHTML,
 * DOMParser); the fetch.dom extraction path keeps the folded form because
 * the wire format and row projector were built around it. */
NbNode *nb_parse_html_nodes(const char *html, size_t len) {
    return parse_html_impl(html, len, 1);
}

static long serialize_rec(FILE *out, const NbNode *n, long *cnt) {
    (*cnt)++;
    size_t tlen = n->text ? strlen(n->text) : 0;
    size_t alen = n->attrs ? strlen(n->attrs) : 0;
    fprintf(out, "N|%s|%s|%s|%zu|%s|%zu|%s\n",
        n->tag?n->tag:"", n->id?n->id:"", n->cls?n->cls:"",
        tlen, n->text?n->text:"", alen, n->attrs?n->attrs:"");
    if (n->first_child) {
        fprintf(out, "D\n");
        for (const NbNode *c=n->first_child; c; c=c->next_sibling)
            serialize_rec(out, c, cnt);
        fprintf(out, "U\n");
    }
    return *cnt;
}

long nb_serialize(FILE *out, const NbNode *root) {
    long cnt=0;
    if (root) serialize_rec(out, root, &cnt);
    return cnt;
}

/* ---- load: rebuild a tree from nb_serialize's fetch.dom stream ----
 * The stream is character-based: N records are length-prefixed so text
 * and attr blobs may contain '|' and '\n', so D/U markers and field
 * delimiters are read byte-by-byte, never line-by-line. Mirrors
 * serialize_rec() exactly (wire format must read what we write). */

static int load_field_until(FILE *in, char **out, int delim) {
    size_t cap = 64, n = 0;
    char *buf = xmalloc(cap);
    int c;
    while ((c = getc(in)) != EOF && c != delim) {
        if (n + 1 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) abort(); buf = nb; }
        buf[n++] = (char)c;
    }
    if (c == EOF) { free(buf); return 0; }
    buf[n] = 0;
    *out = buf;
    return 1;
}

static long load_num_until(FILE *in, int *ok) {
    long v = 0; int c;
    while ((c = getc(in)) != EOF && c != '|') {
        if (c < '0' || c > '9') { *ok = 0; return 0; }
        v = v * 10 + (c - '0');
    }
    if (c == EOF) *ok = 0;
    return v;
}

static char *load_n_bytes(FILE *in, size_t n) {
    char *buf = xmalloc(n + 1);
    size_t got = 0;
    while (got < n) {
        int c = getc(in);
        if (c == EOF) { free(buf); return NULL; }
        buf[got++] = (char)c;
    }
    buf[got] = 0;
    return buf;
}

static NbNode *load_node(FILE *in, long *cnt, int *err) {
    if (*cnt >= DOM_MAX_NODES) { *err = 1; return NULL; }
    int c = getc(in);
    if (c == EOF) return NULL;
    if (c != 'N' || getc(in) != '|') { *err = 1; return NULL; }

    char *tag=NULL, *id=NULL, *cls=NULL, *text=NULL, *attrs=NULL;
    if (!load_field_until(in, &tag, '|')) { *err = 1; goto fail; }
    if (!load_field_until(in, &id,  '|')) { *err = 1; goto fail; }
    if (!load_field_until(in, &cls, '|')) { *err = 1; goto fail; }
    int ok = 1;
    long tlen = load_num_until(in, &ok);
    if (!ok) { *err = 1; goto fail; }
    text = load_n_bytes(in, (size_t)tlen);
    if (!text) { *err = 1; goto fail; }
    if (getc(in) != '|') { *err = 1; goto fail; }
    long alen = load_num_until(in, &ok);
    if (!ok) { *err = 1; goto fail; }
    attrs = load_n_bytes(in, (size_t)alen);
    if (!attrs) { *err = 1; goto fail; }
    if (getc(in) != '\n') { *err = 1; goto fail; }

    NbNode *n = xmalloc(sizeof(*n));
    memset(n, 0, sizeof(*n));
    n->tag = tag;
    if (id[0]) n->id = id; else { free(id); id = NULL; }
    if (cls[0]) n->cls = cls; else { free(cls); cls = NULL; }
    n->text = text;   /* keep even if empty string — null vs empty is
                         intentionally preserved for textContent */
    n->attrs = attrs;  /* empty raw blob is fine */
    (*cnt)++;

    /* children: 'D\n' ... 'U\n' */
    c = getc(in);
    if (c == 'D') {
        if (getc(in) != '\n') { *err = 1; goto fail; }
        for (;;) {
            c = getc(in);
            if (c == EOF) { *err = 1; goto fail; }
            if (c == 'U') { getc(in); break; }   /* consume '\n' after U */
            if (c == 'N') { ungetc(c, in); }
            else { *err = 1; goto fail; }
            NbNode *child = load_node(in, cnt, err);
            if (*err) goto fail;
            append_child(n, child);
        }
    } else {
        ungetc(c, in);   /* next sibling 'N', a closing 'U', or EOF */
    }
    return n;

fail:
    free(tag); free(id); free(cls); free(text); free(attrs);
    return NULL;
}

NbNode *nb_dom_load(FILE *in) {
    if (!in) return NULL;
    long cnt = 0;
    int err = 0;
    int c = getc(in);
    if (c == EOF) return NULL;
    if (c != 'N') return NULL;
    ungetc(c, in);
    /* The stream's first record is the root (the #document node the
     * serializer emitted from serialize_rec) — load it as-is; no extra
     * synthetic wrapper (would double the root). */
    NbNode *root = load_node(in, &cnt, &err);
    if (err || !root) { if (root) nb_node_free(root); return NULL; }
    return root;
}

const char *nb_attr_get(const NbNode *n, const char *name) {
    if (!n || !n->attrs || !name) return "";
    /* parse raw exactly like parse_open did */
    size_t nlen=strlen(name);
    const char *p=n->attrs;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *ks=p;
        while (*p && !isspace((unsigned char)*p) && *p!='=' && *p!='>') p++;
        size_t kl=(size_t)(p-ks);
        if (kl==0) { p++; continue; }
        char key[64]; size_t kc=kl<63?kl:63; memcpy(key,ks,kc); key[kc]=0; my_tolower(key);
        if (*p=='=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc=0;
            if (*p=='"'||*p=='\'') { qc=*p; p++; }
            const char *vs=p;
            if (qc) { while (*p && *p!=qc) p++; }
            else    { while (*p && !isspace((unsigned char)*p)) p++; }
            size_t vl=(size_t)(p-vs);
            if (qc && *p) p++;
            if (kl==nlen && !strncasecmp(ks,name,nlen)) {
                static char dec[1200];
                char vbuf[1200]; size_t vc=vl<1199?vl:1199; memcpy(vbuf,vs,vc); vbuf[vc]=0;
                decode_entities(vbuf, strlen(vbuf), dec);
                return dec;
            }
        }
    }
    return "";
}

/* Presence test for a bare (valueless) attribute. nb_attr_get() cannot answer
 * these: for `<option selected>` the raw blob holds `selected` with no `=`,
 * so the value scan never fires and it returns "". An empty string from
 * nb_attr_get() therefore means "absent OR present-but-empty", and for
 * boolean attributes (`selected`, `checked`, `disabled`, `multiple`, ...)
 * only presence carries the meaning. Walks the same key scan as above and
 * stops at the key, so `value="selected"` is NOT mistaken for the attribute. */
int nb_attr_has(const NbNode *n, const char *name) {
    if (!n || !n->attrs || !name) return 0;
    size_t nlen = strlen(name);
    const char *p = n->attrs;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *ks = p;
        while (*p && !isspace((unsigned char)*p) && *p!='=' && *p!='>') p++;
        size_t kl = (size_t)(p - ks);
        if (kl == 0) { p++; continue; }
        if (kl == nlen && !strncasecmp(ks, name, nlen)) return 1;
        if (*p == '=') {           /* skip the value so it cannot match a key */
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char qc = 0;
            if (*p=='"' || *p=='\'') { qc = *p; p++; }
            if (qc) { while (*p && *p != qc) p++; if (*p) p++; }
            else    { while (*p && !isspace((unsigned char)*p)) p++; }
        }
    }
    return 0;
}

void nb_node_free(NbNode *root) {
    if (!root) return;
    NbNode *c=root->first_child;
    while (c) {
        NbNode *nx=c->next_sibling;
        nb_node_free(c);
        c=nx;
    }
    free(root->tag); free(root->id); free(root->cls); free(root->attrs); free(root->text);
    free(root);
}

static long count_rec(const NbNode *n) {
    long c=1;
    for (const NbNode *x=n->first_child; x; x=x->next_sibling) c+=count_rec(x);
    return c;
}
long nb_count(const NbNode *root) {
    return root ? count_rec(root) : 0;
}
