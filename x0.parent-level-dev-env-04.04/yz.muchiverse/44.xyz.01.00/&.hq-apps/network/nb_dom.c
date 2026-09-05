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
static size_t decode_entities(const char *p, size_t n, char *out) {
    size_t o = 0, i = 0;
    while (i < n) {
        if (p[i] == '&' && i + 1 < n) {
            const char *semi = memchr(p + i + 1, ';', n - (i + 1));
            if (semi) {
                size_t body = (size_t)(semi - (p + i) - 1);
                size_t off = (size_t)((semi - p) + 1);
                if (body >= 2 && p[i + 1] == '#') {
                    long cp = -1;
                    const char *dig = p + i + 2;
                    if (body >= 2 && (dig[0] == 'x' || dig[0] == 'X')) {
                        const char *d = dig + 1; long v = 0;
                        while (d < semi && isxdigit((unsigned char)*d)) { v = v * 16 + (isdigit((unsigned char)*d) ? *d-'0' : (tolower((unsigned char)*d)-'a'+10)); d++; }
                        if (d == semi) cp = v;
                    } else {
                        const char *d = dig; long v = 0;
                        while (d < semi && isdigit((unsigned char)*d)) { v = v*10 + (*d-'0'); d++; }
                        if (d == semi) cp = v;
                    }
                    if (cp >= 0x20 && cp < 0x100) { out[o++] = (char)cp; i = off; continue; }
                    if (cp > 0) { out[o++] = '\xEF'; i = off; continue; }
                } else if (body >= 2) {
                    char name[16];
                    size_t bl = body < sizeof(name)-1 ? body : sizeof(name)-1;
                    memcpy(name, p + i + 1, bl); name[bl] = 0;
                    char ch = 0;
                    if      (!strcasecmp(name,"amp"))   ch = '&';
                    else if (!strcasecmp(name,"lt"))    ch = '<';
                    else if (!strcasecmp(name,"gt"))    ch = '>';
                    else if (!strcasecmp(name,"quot"))  ch = '"';
                    else if (!strcasecmp(name,"apos"))  ch = '\'';
                    else if (!strcasecmp(name,"nbsp"))  ch = ' ';
                    else if (!strcasecmp(name,"copy"))  ch = 0xA9;
                    else if (!strcasecmp(name,"reg"))   ch = 0xAE;
                    else if (!strcasecmp(name,"times")) ch = 0xD7;
                    else if (!strcasecmp(name,"divide"))ch = 0xF7;
                    else if (!strcasecmp(name,"mdash")) ch = 0x97;
                    else if (!strcasecmp(name,"ndash")) ch = 0x96;
                    else if (!strcasecmp(name,"hellip"))ch = 0x85;
                    if (ch) { out[o++] = ch; i = off; continue; }
                }
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
static int is_skip(const char *tag) {
    return !strcmp(tag,"script") || !strcmp(tag,"style")
        || !strcmp(tag,"title") || !strcmp(tag,"noscript")
        || !strcmp(tag,"head");
}

typedef struct {
    const char *p, *end;
    NbNode *root;
    NbNode *stack[256];
    int depth;
    long count;
    int err;
} Parser;

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
static const char *skip_to_close(Parser *ps, const char *tag) {
    size_t tl = strlen(tag);
    const char *scan = ps->p;
    while (scan < ps->end) {
        const char *lx = scan;
        while (lx < ps->end && *lx != '<') lx++;
        if (lx >= ps->end) return ps->end;
        /* match "</" then tag then ">" */
        if ((size_t)(ps->end - lx) >= tl + 3 && lx[1] == '/'
            && strncasecmp(lx + 2, tag, tl) == 0 && lx[tl + 2] == '>') {
            return lx + tl + 3;
        }
        scan = lx + 1;
    }
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
    if (is_skip(tag)) { ps->p = skip_to_close(ps, tag); return; }
    NbNode *n = node_new(ps, tag);
    if (!n) return;
    if (idbuf[0]) n->id = str_dup(idbuf);
    if (clsbuf[0]) n->cls = str_dup(clsbuf);
    if (rn) n->attrs = str_dup_n(raw, rn);
    NbNode *parent = ps->depth>0 ? ps->stack[ps->depth-1] : ps->root;
    append_child(parent, n);
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
    if (ps->depth>0) {
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

NbNode *nb_parse_html(const char *html, size_t len) {
    Parser ps;
    memset(&ps, 0, sizeof(ps));
    ps.p=html; ps.end=html+len;
    ps.root=xmalloc(sizeof(NbNode)); memset(ps.root,0,sizeof(*ps.root));
    ps.root->tag=str_dup("#document");
    while (ps.p<ps.end && !ps.err) {
        if (*ps.p=='<') parse_open(&ps);
        else parser_add_text(&ps);
    }
    return ps.root;
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
