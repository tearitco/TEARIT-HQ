#include "nb_css.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *strndup_local(const char *s, size_t n) {
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/* Strip C-style comments in place (they can hide braces/selectors). */
static void strip_comments(char *s) {
    char *w = s, *r = s;
    while (*r) {
        if (r[0] == '/' && r[1] == '*') {
            r += 2;
            while (*r && !(r[0] == '*' && r[1] == '/')) r++;
            if (*r) r += 2;
            continue;
        }
        *w++ = *r++;
    }
    *w = 0;
}

/* Consume a balanced { ... } block whose `{` is at p; returns the position
 * just past the matching `}` (or end if unterminated). Quotes respected. */
static const char *skip_block(const char *p, const char *end) {
    int depth = 1;
    while (p < end) {
        char c = *p;
        if (c == '"' || c == '\'') {
            char q = c;
            p++;
            while (p < end && *p != q) {
                if (*p == '\\' && p + 1 < end) p++;
                p++;
            }
            if (p < end) p++;
            continue;
        }
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) return p + 1;
        }
        p++;
    }
    return end;
}

static int is_token_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == '#';
}

/* Match one sanitized compound (e.g. "span.wide#hero", ".a.b", "#x", "*")
 * against an element. Pseudo-classes/attribute brackets are stripped by
 * the caller; we never see ':'/'['. Returns 1 on match. */
static int match_compound(const NbNode *el, const char *tok) {
    if (!tok || !*tok || !el) return 0;
    char clean[192];
    size_t cl = 0;
    const char *p = tok;
    for (; *p && cl < sizeof(clean) - 1; p++) {
        char c = *p;
        /* keep only chars we can compare (id/class/tag) */
        if (!is_token_char(c)) continue;
        if (c == ':' || c == '[' || c == ']') continue;
        clean[cl++] = c;
    }
    clean[cl] = 0;
    if (cl == 0) return 1;                       /* pseudo/attr-only selector */
    if (cl == 1 && clean[0] == '*') return 1;    /* universal */

    const char *q = clean;
    int fail = 0;
    while (*q && !fail) {
        if (*q == '#') {
            q++;
            const char *v = q;
            while (*q && *q != '.' && *q != '#') q++;
            size_t n = (size_t)(q - v);
            if (!el->id || strlen(el->id) != n || strncmp(el->id, v, n) != 0)
                fail = 1;
        } else if (*q == '.') {
            q++;
            const char *v = q;
            while (*q && *q != '.' && *q != '#') q++;
            size_t n = (size_t)(q - v);
            const char *w = el->cls;
            int found = 0;
            while (w && *w) {
                while (*w && is_ws(*w)) w++;
                if (!*w) break;
                const char *ww = w;
                while (*w && !is_ws(*w)) w++;
                size_t wn = (size_t)(w - ww);
                if (wn == n && strncmp(ww, v, n) == 0) { found = 1; break; }
            }
            if (!found) fail = 1;
        } else {
            const char *v = q;
            while (*q && *q != '.' && *q != '#') q++;
            size_t n = (size_t)(q - v);
            if (!el->tag || strlen(el->tag) != n ||
                strncasecmp(el->tag, v, n) != 0)
                fail = 1;
        }
    }
    return !fail;
}

/* Match one comma-part of a selector against el: last token must match el;
 * each earlier token matches some ancestor further up (descendant
 * combinator; `el>p` and `el+p` are tolerated by our token filtering). */
static int match_part(const NbNode *el, const char *part, size_t len) {
    const char *end = part + len;
    const char *p = end;
    while (p > part && is_ws(p[-1])) p--;
    if (p == part) return 0;
    const char *te = p;
    while (p > part && !is_ws(p[-1])) p--;
    const char *tok = p;
    size_t tl = (size_t)(te - tok);

    char tbuf[192];
    size_t tn = tl < sizeof(tbuf) - 1 ? tl : sizeof(tbuf) - 1;
    memcpy(tbuf, tok, tn);
    tbuf[tn] = 0;
    if (!match_compound(el, tbuf)) return 0;

    const char *rest = part;
    while (rest < tok && is_ws(*rest)) rest++;
    if (rest == tok) return 1;

    /* remaining tokens still hold a comma-free whitespace-separated list;
       they must all be satisfied climbing the ancestors, nearest first. */
    const char *r = tok;
    const char *tokens[8];
    long token_len[8];
    int ntok = 0;
    while (r > rest) {
        while (r > rest && is_ws(r[-1])) r--;
        const char *rte = r;
        while (r > rest && !is_ws(r[-1])) r--;
        if (ntok < 8) { tokens[ntok] = r; token_len[ntok] = (long)(rte - r); }
        ntok++;
    }
    const NbNode *node = el->parent;
    int idx = 0;
    while (node && idx < ntok) {
        char tb[192];
        long tn2 = token_len[idx] < (long)sizeof(tb) - 1 ? token_len[idx] : (long)sizeof(tb) - 1;
        memcpy(tb, tokens[idx], (size_t)tn2);
        tb[tn2] = 0;
        if (match_compound(node, tb)) idx++;
        node = node->parent;
    }
    return idx == ntok;
}

typedef struct { int n_id, n_cls, n_type; } SelInfo;

/* Specificity: max (id, class, type) triple across the comma parts. */
static void specificity(const char *sel, size_t len, SelInfo *out) {
    out->n_id = out->n_cls = out->n_type = 0;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && sel[j] != ',') j++;
        SelInfo p = {0, 0, 0};
        for (size_t k = i; k < j; k++) {
            if (sel[k] == '#') p.n_id++;
            else if (sel[k] == '.' || sel[k] == '[') p.n_cls++;
            else if (is_token_char(sel[k]) && isalpha((unsigned char)sel[k])) p.n_type++;
        }
        if (p.n_id > out->n_id) out->n_id = p.n_id;
        if (p.n_cls > out->n_cls) out->n_cls = p.n_cls;
        if (p.n_type > out->n_type) out->n_type = p.n_type;
        i = (j < len) ? j + 1 : j;
    }
}

NbCss *nb_css_parse(const char *text, size_t len) {
    NbCss *css = (NbCss *)calloc(1, sizeof(NbCss));
    if (!css) return NULL;
    if (!text || len == 0) return css;

    char *s = strndup_local(text, len);
    if (!s) return css;
    strip_comments(s);

    size_t slen = strlen(s);
    size_t i = 0;
    while (i < slen) {
        const char *head = s + i;
        const char *lb = head;
        while (lb < s + slen && *lb != '{' && *lb != ';') lb++;
        if (lb >= s + slen) break;
        if (*lb == ';') {                          /* @import etc.: skip */
            i = (size_t)(lb - s) + 1;
            continue;
        }
        const char *hstart = head;
        while (hstart < lb && is_ws(*hstart)) hstart++;
        if (hstart < lb && *hstart == '@') {       /* @media/@keyframes: skip body */
            const char *after = skip_block(lb + 1, s + slen);
            i = (size_t)(after - s);
            continue;
        }
        const char *in = lb + 1;
        const char *close = skip_block(in, s + slen);
        if (close == s + slen) break;              /* unterminated: stop */
        size_t hl = (size_t)(lb - hstart);         /* selector length */
        size_t dl = (size_t)(close - in - 1);      /* decl block length */
        if (hl == 0 || dl == 0) {
            i = (size_t)(close - s);
            continue;
        }
        if (css->nr == css->cap) {
            size_t nc = css->cap ? css->cap * 2 : 16;
            NbCssRule *rr = (NbCssRule *)realloc(css->r, nc * sizeof(NbCssRule));
            if (!rr) break;
            css->r = rr;
            css->cap = nc;
        }
        NbCssRule *rule = &css->r[css->nr];
        memset(rule, 0, sizeof(*rule));
        rule->selector = strndup_local(hstart, hl);
        rule->decls = strndup_local(in, dl);
        if (rule->decls && strstr(rule->decls, "!important")) rule->imp = 1;
        SelInfo si;
        specificity(hstart, hl, &si);
        rule->n_id = si.n_id; rule->n_cls = si.n_cls; rule->n_type = si.n_type;
        css->nr++;
        i = (size_t)(close - s);
    }
    free(s);
    return css;
}

/* ----------------------- cascade ----------------------- */

enum { PD_DISPLAY = 0, PD_VIS, PD_OP, PD_W, PD_H, PD_COUNT };

static int prop_id(const char *key) {
    if (strcmp(key, "display") == 0) return PD_DISPLAY;
    if (strcmp(key, "visibility") == 0) return PD_VIS;
    if (strcmp(key, "opacity") == 0) return PD_OP;
    if (strcmp(key, "width") == 0) return PD_W;
    if (strcmp(key, "height") == 0) return PD_H;
    return -1;
}

/* Trim a value string [v, v+vlen) into buf; returns buf. Strips a trailing
 * `!important` (sets important) exactly once. */
static const char *clean_value(const char *v, size_t vlen, char *buf, size_t cap,
                               int *important) {
    const char *s = v;
    size_t n = vlen;
    while (n && is_ws(s[0])) { s++; n--; }
    while (n && is_ws(s[n - 1])) n--;
    if (n > cap - 1) n = cap - 1;
    if (n >= 10 && strncmp(s + n - 10, "!important", 10) == 0) {
        *important = 1;
        n -= 10;
        while (n && is_ws(s[n - 1])) n--;
    }
    memcpy(buf, s, n);
    buf[n] = 0;
    return buf;
}

static void apply_decl(int pid, const char *val, size_t vlen, int important,
                       long long base, NbCssStyle *out, long long *scores) {
    char buf[64];
    int imp = important;
    const char *cv = clean_value(val, vlen, buf, sizeof(buf), &imp);
    if (!cv || !*cv) return;
    long long score = base + (imp ? 3000000000LL : 0);
    if (scores[pid] > score) return;
    if (scores[pid] == score && (pid == PD_W || pid == PD_H)) return;

    switch (pid) {
    case PD_DISPLAY:
        /* "none" hides; any other display keyword at a higher-or-equal
         * cascade score *clears* a previously computed none (a later
         * display:block/flex/inline must un-hide). Non-keyword junk is
         * treated as visible (the engine only models "none"). */
        if (strcasecmp(cv, "none") == 0) {
            if (score >= scores[pid]) {
                snprintf(out->display, sizeof(out->display), "none");
                scores[pid] = score;
            }
        } else if (score >= scores[pid]) {
            out->display[0] = 0;
            scores[pid] = score;
        }
        break;
    case PD_VIS:
        if (strcasecmp(cv, "hidden") == 0 || strcasecmp(cv, "collapse") == 0) {
            if (score >= scores[pid]) {
                snprintf(out->visibility, sizeof(out->visibility), "%s", cv);
                scores[pid] = score;
            }
        } else if (score >= scores[pid]) {
            out->visibility[0] = 0;
            scores[pid] = score;
        }
        break;
    case PD_OP: {
        char *ep = NULL;
        double dv = strtod(cv, &ep);
        if (ep && ep != cv && *ep == 0 && dv >= 0.0 && dv <= 1.0) {
            if (score >= scores[pid]) {
                snprintf(out->opacity, sizeof(out->opacity), "%g", dv);
                scores[pid] = score;
            }
        }
        break;
    }
    case PD_W:
    case PD_H: {
        char tmp[64];
        int i2 = 0;
        const char *v2 = clean_value(val, vlen, tmp, sizeof(tmp), &i2);
        char *ep = NULL;
        double dv = strtod(v2, &ep);
        while (ep && is_ws(*ep)) ep++;
        int ok = 0;
        if (ep && ep != v2) {
            if (*ep == 0) ok = 1;                       /* bare number */
            else if (ep[0] == 'p' && ep[1] == 'x' && ep[2] == 0) ok = 1;
            else if (ep[0] == 'p' && ep[1] == 'x' && (ep[2] == 0)) ok = 1;
        }
        if (ok) {
            if (dv < 0) break;
            if (score >= scores[pid]) {
                if (pid == PD_W) { out->width = dv; out->has_width = 1; }
                else { out->height = dv; out->has_height = 1; }
                scores[pid] = score;
            }
        }
        break;
    }
    }
}

static void apply_decls(const char *decls, int important_pass,
                        long long base, NbCssStyle *out, long long *scores) {
    if (!decls || !*decls) return;
    const char *p = decls;
    while (*p) {
        while (*p && (is_ws(*p) || *p == ';')) p++;
        if (!*p) break;
        const char *cs = p;
        while (*p && *p != ';') p++;
        const char *colon = memchr(cs, ':', (size_t)(p - cs));
        if (colon) {
            size_t kl = (size_t)(colon - cs);
            while (kl && is_ws(cs[kl - 1])) kl--;
            if (kl > 0 && kl < 32) {
                char kbuf[32];
                for (size_t k = 0; k < kl; k++)
                    kbuf[k] = (char)tolower((unsigned char)cs[k]);
                kbuf[kl] = 0;
                int pid = prop_id(kbuf);
                if (pid >= 0)
                    apply_decl(pid, colon + 1, (size_t)(p - (colon + 1)),
                               important_pass, base, out, scores);
            }
        }
    }
}

void nb_css_resolve(const NbCss *css, const NbNode *el,
                    const char *inline_style, NbCssStyle *out) {
    memset(out, 0, sizeof(*out));
    if (!el) return;
    long long scores[PD_COUNT];
    for (int i = 0; i < PD_COUNT; i++) scores[i] = -1;

    if (css) {
        for (size_t i = 0; i < css->nr; i++) {
            const NbCssRule *rule = &css->r[i];
            if (!rule->selector || !rule->decls) continue;
            size_t sl = strlen(rule->selector);
            size_t j = 0;
            int matched = 0;
            while (j < sl) {
                while (j < sl && is_ws(rule->selector[j])) j++;
                if (j >= sl) break;
                const char *part = rule->selector + j;
                while (j < sl && rule->selector[j] != ',') j++;
                size_t pl = (size_t)((rule->selector + j) - part);
                while (pl && is_ws(part[pl - 1])) pl--;
                if (pl > 0 && match_part(el, part, pl)) { matched = 1; break; }
                if (j < sl && rule->selector[j] == ',') j++;
            }
            if (!matched) continue;
            long long base = (long long)rule->n_id * 1000000LL +
                             (long long)rule->n_cls * 10000LL +
                             (long long)rule->n_type * 100LL +
                             (long long)i;
            apply_decls(rule->decls, rule->imp, base, out, scores);
        }
    }
    if (inline_style && *inline_style)
        apply_decls(inline_style, 0, 4000000000LL, out, scores);
}

int nb_css_hidden(const NbCss *css, const NbNode *el) {
    for (const NbNode *n = el; n; n = n->parent) {
        NbCssStyle st;
        nb_css_resolve(css, n, nb_attr_get(n, "style"), &st);
        if (strcmp(st.display, "none") == 0) return 1;
        if (strcmp(st.visibility, "hidden") == 0) return 1;
        if (strcmp(st.visibility, "collapse") == 0) return 1;
    }
    return 0;
}

void nb_css_free(NbCss *css) {
    if (!css) return;
    for (size_t i = 0; i < css->nr; i++) {
        free(css->r[i].selector);
        free(css->r[i].decls);
    }
    free(css->r);
    free(css);
}