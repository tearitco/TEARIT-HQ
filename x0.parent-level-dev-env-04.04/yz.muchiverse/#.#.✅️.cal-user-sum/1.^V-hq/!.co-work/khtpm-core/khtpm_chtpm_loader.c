/* khtpm_chtpm_loader.c — the actual .chtpm -> Elem-tree parser.
 *
 * Added 2026-09-01, doc-audit correction: earlier versions of this
 * package's own README claimed this loader lived in
 * khtpm_render_core.c ("see load_chtpm/parse_chtpm there") — it never
 * did. In the real house, `parse_chtpm()` lives in the app-level
 * renderer file itself, not the generic core, because it calls
 * `elem_new()`, which needs a real, app-owned Elem pool
 * (`g_pool[MAX_ELEMS]`/`g_n_elems`) sized for that app's own real UI.
 *
 * This file is that same real code, pulled out into its own generic,
 * text-includable piece (`#include "khtpm_chtpm_loader.c"`, same
 * text-include convention as khtpm_render_core.c/khtpm_draw_core.c —
 * see this package's own README on why this house never uses a shared
 * `.h` for this) so you don't have to write an XML-ish tag-tree parser
 * from scratch. Same real, minimal entity-decode (&quot;/&amp;/&gt;/
 * &lt; — no others, nothing else in this house's own real .chtpm files
 * ever needs more) and the same real `<!-- comment -->` handling.
 *
 * Requires (define/include before this file):
 *   - khtpm_render_core.c already included (for the Elem struct itself
 *     and CSS_MAX_CLASSES/MAX_CHILDREN)
 *   - #include <ctype.h>, <stdio.h>, <stdlib.h>, <string.h>
 *
 * Usage:
 *   Elem *root = parse_chtpm("your-window.chtpm");
 *   // root is your top-level <window>/whatever tag; walk it with
 *   // find_by_tag()/find_by_id() from khtpm_render_core.c.
 */

/* Real, app-owned Elem pool - size this for whatever your own UI
 * actually needs (512 is the real house's own current default). Every
 * elem_new() call takes the next free slot; nothing here ever frees
 * one, so a long-lived program that keeps rebuilding dynamic content
 * (a list, a scrolling log) should give ITS OWN dynamic content a
 * separate, reused, fixed-size array instead of calling elem_new()
 * every rebuild — see the real house's own reusable_slot() pattern if
 * you hit this. */
#define MAX_ELEMS 512
static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;

static Elem *elem_new(const char *tag) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

static void parse_attr_value(const char **p, char *out, size_t outsz) {
    skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') { if (n + 1 < outsz) out[n++] = **p; (*p)++; }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}

/* Real, minimal XML entity decode - ONLY these 4, the ones a real
 * shell-command action= string actually needs (quotes for "$0"-style
 * var quoting, > / < for redirects inside nested command
 * substitutions - a real house bug once shipped from missing &gt;, see
 * git history if curious). &amp; decoded LAST (standard HTML-entity
 * ordering) so a real "&amp;gt;" in source data isn't double-decoded. */
static void decode_entities(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; }
        else if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; }
        else if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; }
        else if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; }
        else *w++ = *r++;
    }
    *w = '\0';
}

/* Window-level drop_action (real XDND support, optional - only meaningful
 * if you wire up XDND yourself; harmless to ignore otherwise). */
static char g_drop_action[1024] = "";

/* Real, generic attribute table - covers every tag this package's own
 * example-dashboard.chtpm uses, plus the few extra fields Elem itself
 * carries. Trim or extend this yourself as your own .chtpm vocabulary
 * grows - this is meant to be edited, not treated as fixed. */
static void apply_attr(Elem *e, const char *name, const char *val) {
    if (strcmp(name, "id") == 0 || strcmp(name, "name") == 0) {
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (strcmp(name, "class") == 0) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", val);
        char *tok = strtok(tmp, " ");
        while (tok && e->n_classes < CSS_MAX_CLASSES) {
            snprintf(e->classes[e->n_classes], sizeof(e->classes[0]), "%s", tok);
            e->n_classes++;
            tok = strtok(NULL, " ");
        }
    } else if (strcmp(name, "label") == 0) {
        char decoded[sizeof(e->label)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->label, sizeof(e->label), "%s", decoded);
    } else if (strcmp(name, "action") == 0 || strcmp(name, "onClick") == 0 || strcmp(name, "onclick") == 0) {
        char decoded[sizeof(e->onclick)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->onclick, sizeof(e->onclick), "%s", decoded);
    } else if (strcmp(name, "sprite") == 0) {
        snprintf(e->sprite, sizeof(e->sprite), "%s", val);
    } else if (strcmp(name, "src") == 0) {
        /* Real house convention (<module src="...">) - reuses e->label,
         * same "module elements are never drawn, safe reuse" reasoning
         * every other repurposed field in this function already uses. */
        snprintf(e->label, sizeof(e->label), "%s", val);
    } else if (strcmp(name, "args") == 0) {
        /* Real house convention (<module args="...">) - optional extra
         * static argv for a module, reuses e->id (also never drawn). */
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (strcmp(name, "target_id") == 0) {
        snprintf(e->target_id, sizeof(e->target_id), "%s", val);
    } else if (strcmp(name, "backspace_action") == 0) {
        char decoded[sizeof(e->backspace_action)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(e->backspace_action, sizeof(e->backspace_action), "%s", decoded);
    } else if (strcmp(name, "rows") == 0) {
        e->rows = atoi(val);
    } else if (strcmp(name, "drop_action") == 0) {
        char decoded[sizeof(g_drop_action)];
        snprintf(decoded, sizeof(decoded), "%s", val);
        decode_entities(decoded);
        snprintf(g_drop_action, sizeof(g_drop_action), "%s", decoded);
    }
    /* Add your own attribute names here as your .chtpm vocabulary grows -
     * an unrecognized attribute is silently ignored, never an error. */
}

static const char *parse_element(const char *p, Elem *parent) {
    skip_ws(&p);
    if (*p != '<') return p;
    p++;
    if (*p == '!') {
        const char *end = strstr(p, "-->");
        return end ? end + 3 : p + strlen(p);
    }
    char tag[32]; size_t tn = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
        if (tn + 1 < sizeof(tag)) tag[tn++] = *p;
        p++;
    }
    tag[tn] = '\0';
    Elem *e = elem_new(tag);
    e->parent = parent;
    if (parent && parent->n_children < MAX_CHILDREN) parent->children[parent->n_children++] = e;

    for (;;) {
        skip_ws(&p);
        if (*p == '/' && p[1] == '>') { p += 2; return p; }
        if (*p == '>') { p++; break; }
        if (!*p) return p;
        char attr[32]; size_t an = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
            if (an + 1 < sizeof(attr)) attr[an++] = *p;
            p++;
        }
        attr[an] = '\0';
        skip_ws(&p);
        char val[1024] = "";
        if (*p == '=') { p++; parse_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) apply_attr(e, attr, val);
    }

    for (;;) {
        skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') {
            const char *end = strchr(p, '>');
            return end ? end + 1 : p + strlen(p);
        }
        p = parse_element(p, e);
    }
}

static Elem *parse_chtpm(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    const char *p = buf;
    Elem *root = NULL;
    while (*p) {
        skip_ws(&p);
        if (!*p) break;
        if (*p == '<' && p[1] == '!') { p = parse_element(p, NULL); continue; }
        if (*p != '<') break;
        if (!root) {
            root = elem_new("__root");
            const char *after = parse_element(p, root);
            p = after;
        } else {
            p = parse_element(p, root);
        }
    }
    free(buf);
    if (root && root->n_children > 0) return root->children[0];
    return root;
}
