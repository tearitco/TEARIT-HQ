#define _POSIX_C_SOURCE 200809L
/* network_browser_manager.c - real manager for the "network browser"
 * HQ app, CENTROID_GOLD_STD.md's first real proof case (2026-08-31,
 * direct instruction: "i wanna start the centroid browser, all the way
 * to cli mirroring and it should have a manager to make sure all that
 * is cohesive, even if it just parses some simple html from a
 * webpage"). Shape copied directly from khtpm_hq_manager.c's own
 * proven contract (poll loop, atomic tmp+rename publish, one pending
 * action line consumed then cleared) - not invented fresh, per
 * CENTROID_GOLD_STD.md §3 rule 2 ("business logic lives in a real,
 * separate manager process").
 *
 * Real, deliberately scoped-down HTML handling: fetches a URL with
 * `curl` (already on this house's Linux/macOS legs, no new dependency)
 * and does a real, simple, manual (no regex, no libxml) text+link
 * extraction - title, visible text broken into lines at block-tag
 * boundaries, <a href> targets. This is NOT a real HTML/CSS renderer
 * (layout, images, JS - all explicitly out of scope) - just enough
 * real parsing of REAL fetched pages to prove the centroid pattern
 * (one manager, one real published projection, two symmetric
 * renderers) end to end with real content instead of a canned fixture.
 *
 * Publishes, atomically (tmp+rename, matching publish_common_events()'s
 * own convention), every time a fetch completes:
 *   #.desktop/network_browser_page.state.txt
 *     URL|<url actually fetched>
 *     TITLE|<page title, or empty>
 *     TEXT|<one visible text line>        (repeated, document order)
 *     LINK|<resolved href>|<link text>    (repeated, document order)
 *     IMG|<sprite_dir>|<alt>             (page <img>, house sprite.csv dir)
 *     VIDEO|<sprite_dir>|<url>|<alt>     (page <video>, poster sprite + ffplay)
 *   #.desktop/network_browser_status.state.txt
 *     one line: "idle" | "loading" | "ready" | "stopped" | "error: <detail>"
 *
 * Consumes #.desktop/network_browser_request.txt, one pending line at a
 * time, truncated back to empty after handling (same real contract as
 * khtpm_open_hai_manager.c's own request file):
 *   go:<url>   - fetch <url> (resolved against the current page's URL
 *                if it looks relative), publish the state files above;
 *                clears the forward stack
 *   back:      - push current URL onto forward stack, pop back stack, fetch
 *   forward:   - push current URL onto back stack, pop forward stack, fetch
 *   stop:      - if a curl child is running, kill it and publish "stopped"
 *   reload:    - fetch current URL again (no stack change)
 *   bookmark:  - append current URL to bookmarks table
 *   tab:<n>    - switch to tab n (save snapshot, load target, no curl
 *                unless that tab's snapshot is missing)
 *   newtab:    - next index under cap 8, blank page, switch to it
 *   closetab:  - close current tab (keep at least one); compact 0..n-1
 *
 * Tabs (cap 8) live in #.desktop/network_browser_tabs.txt
 *   TAB | <index> | <url> | <title> | current
 * Snapshots: #.desktop/nb_tabs/<index>/page.state.txt and url.txt
 * On manager start, if tabs.txt exists, the CURRENT tab snapshot is loaded
 * into page.state (no fetch). Missing tabs.txt = first-run blank Ready page.
 * Back/Forward/History stacks stay global (shared across tabs).
 *
 * Navigation files under #.desktop:
 *   network_browser_history.log.txt  append-only visit log (sidebar)
 *   network_browser_back.txt         Back stack (one URL per line)
 *   network_browser_forward.txt      Forward stack (one URL per line)
 *
 * REAL, NEW 2026-09-01 (khtpm-generic-dispatch-design.md's own real
 * conversion writeup): generates a real, live .chtpm projection from the
 * manager's own published state (current URL, page title/text/links,
 * status) every main-loop tick, using only generic tags (sidebar/panel/
 * scrolllist/item/text/cli_io) - zero new renderer C specific to this
 * app. The renderer (khtpm_core_render.+x) picks it up via its generic
 * reparse_chtpm_if_changed() capability. Follows the exact pattern
 * khtpm_open_hai_manager.c already proved.
 *
 * Usage: network_browser_manager.+x <house_root> [--data-root <dir>]
 */
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define PATH_BUF 4352

static char g_chtpm_output_path[PATH_BUF];
/* REAL, NEW 2026-09-03 - static-template port: when launched with argv "ui"
 * (the <module id="ui"> in network-browser-hq.xhtpm), the manager writes a
 * key=value UI file for the static template instead of regenerating the
 * whole .chtpm markup every tick. The old write_chtpm_projection() path
 * stays as rollback (old network-browser-hq.chtpm + button.sh, no "ui" arg). */
static char g_ui_output_path[PATH_BUF];
static int  g_mode_ui = 0;
static char g_package_dir[PATH_BUF];

/* Small local case-insensitive strstr - strcasestr isn't in strict C11
 * everywhere this house builds (macOS leg), so a real local copy avoids
 * a portability landmine rather than assuming glibc's extension. */
static const char *strcasestr_local(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    if (!nlen) return hay;
    for (; *hay; hay++) {
        if (strncasecmp(hay, needle, nlen) == 0) return hay;
    }
    return NULL;
}

static void mkdir_p_local(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}
#define PAGE_BUF_MAX (8 * 1024 * 1024) /* real, generous cap - real pages this manager will actually be pointed at are small; a huge page is truncated, not a crash */
#define MAX_LINES 4096

static char g_house[PATH_BUF];
static char g_request_path[PATH_BUF];
static char g_page_state_path[PATH_BUF];
static char g_status_path[PATH_BUF];
static char g_tmp_html_path[PATH_BUF];
static char g_current_url[PATH_BUF] = "";

static void path_join(char *out, size_t outsz, const char *a, const char *b) {
    snprintf(out, outsz, "%s/%s", a, b);
}

/* Atomic publish - same real tmp+rename shape as khtpm_hq_manager.c's
 * publish_common_events(), never a direct in-place write a reader
 * could see half-written. */
static FILE *atomic_open(const char *final_path, char *tmp_out, size_t tmp_out_sz) {
    snprintf(tmp_out, tmp_out_sz, "%s.tmp", final_path);
    return fopen(tmp_out, "w");
}
static void atomic_commit(const char *final_path, const char *tmp_path) {
    rename(tmp_path, final_path);
}

static void publish_status(const char *status) {
    char tmp[PATH_BUF];
    FILE *f = atomic_open(g_status_path, tmp, sizeof(tmp));
    if (!f) return;
    fprintf(f, "%s\n", status);
    fclose(f);
    atomic_commit(g_status_path, tmp);
}

/* ---------- real, simple, manual HTML extraction (no regex/libxml) ---------- */

static void html_decode_entities(char *s) {
    /* Real, small, in-place entity decode - the common real-world set,
     * not a full spec implementation (deliberately out of scope).
     * Numeric &#039; / &#39; / &#x27; too - catalog.json comments use them. */
    char *r = s, *w = s;
    while (*r) {
        if (*r == '&') {
            if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; continue; }
            if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; continue; }
            if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; continue; }
            if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; continue; }
            if (strncmp(r, "&apos;", 6) == 0) { *w++ = '\''; r += 6; continue; }
            if (strncmp(r, "&nbsp;", 6) == 0) { *w++ = ' '; r += 6; continue; }
            if (r[1] == '#') {
                const char *q = r + 2;
                int hex = 0;
                if (*q == 'x' || *q == 'X') { hex = 1; q++; }
                if (*q) {
                    char *end = NULL;
                    unsigned long cp = strtoul(q, &end, hex ? 16 : 10);
                    if (end && end > q && *end == ';') {
                        if (cp == 39 || cp == 8216 || cp == 8217) *w++ = '\'';
                        else if (cp == 34 || cp == 8220 || cp == 8221) *w++ = '"';
                        else if (cp == 160) *w++ = ' ';
                        else if (cp >= 32 && cp < 128) *w++ = (char)cp;
                        r = end + 1;
                        continue;
                    }
                }
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static void collapse_ws(char *s);

/* Sprite-grid item caption: longer than the old 22-char cut, entities
 * decoded (&gt; &lt; &amp; &#039; &quot; &nbsp; and numeric), raw http(s)
 * URL spam dropped (catalog sticky often concatenates title + URLs). One
 * line. Cap 44 so a 64px blit tile still has a usable word, not "KPO...".
 * page.state stores the decoded caption; xml_escape() still writes
 * well-formed chtpm label= (source keeps &amp;/&gt;). */
#define NB_SPRITE_LAB_MAX 44
static void fill_sprite_shortlab(const char *raw, char *out, size_t outsz) {
    char work[600];
    size_t n, cap;
    char *hp, *hp2;
    if (!out || outsz < 2) return;
    snprintf(work, sizeof(work), "%s", raw ? raw : "");
    html_decode_entities(work);
    collapse_ws(work);
    hp = strstr(work, "https://");
    hp2 = strstr(work, "http://");
    if (hp2 && (!hp || hp2 < hp)) hp = hp2;
    if (hp) {
        while (hp > work && (hp[-1] == ':' || hp[-1] == ' ')) hp--;
        *hp = '\0';
        collapse_ws(work);
    }
    if (!work[0]) {
        snprintf(out, outsz, " ");
        return;
    }
    cap = NB_SPRITE_LAB_MAX;
    if (cap + 1 > outsz) cap = outsz - 1;
    n = strlen(work);
    if (n > cap) {
        memcpy(out, work, cap);
        out[cap] = '\0';
    } else {
        memcpy(out, work, n + 1);
    }
}

static void collapse_ws(char *s) {
    char *r = s, *w = s;
    int last_space = 1; /* trim leading */
    while (*r) {
        unsigned char c = (unsigned char)*r;
        if (isspace(c)) {
            if (!last_space) { *w++ = ' '; last_space = 1; }
        } else {
            *w++ = (char)c;
            last_space = 0;
        }
        r++;
    }
    if (w > s && w[-1] == ' ') w--; /* trim trailing */
    *w = '\0';
}

static int is_block_tag(const char *name) {
    static const char *blocks[] = {
        "p", "div", "br", "li", "h1", "h2", "h3", "h4", "h5", "h6",
        "tr", "section", "article", "header", "footer", "ul", "ol",
        "table", "blockquote", NULL
    };
    for (int i = 0; blocks[i]; i++) if (strcasecmp(name, blocks[i]) == 0) return 1;
    return 0;
}

/* REAL, NEW 2026-09-01 - XML entity escaping for .chtpm projection
 * generation (ported from khtpm_open_hai_manager.c) */
static void xml_escape(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 6 < outsz; i++) {
        switch (in[i]) {
            case '&': o += snprintf(out + o, outsz - o, "&amp;"); break;
            case '<': o += snprintf(out + o, outsz - o, "&lt;"); break;
            case '>': o += snprintf(out + o, outsz - o, "&gt;"); break;
            case '"': o += snprintf(out + o, outsz - o, "&quot;"); break;
            default: if (o + 1 < outsz) out[o++] = in[i]; break;
        }
    }
    out[o] = '\0';
}

/* Shell single-quote escaping for .chtpm projection generation
 * (ported from khtpm_open_hai_manager.c) */
static void shell_escape_squote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        if (in[i] == '\'') {
            o += snprintf(out + o, outsz - o, "'\\''");
        } else {
            if (o + 1 < outsz) out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

/* Real, minimal URL join: absolute (has "://") passes through
 * unchanged; otherwise resolved against base's scheme+host (+ path
 * directory for a relative, non-rooted href). Real, deliberately
 * simplified - no ../ normalization (out of scope for a v1 proof). */
static void resolve_url(const char *base, const char *href, char *out, size_t outsz) {
    if (strstr(href, "://") || strncasecmp(href, "mailto:", 7) == 0 || strncasecmp(href, "tel:", 4) == 0) {
        snprintf(out, outsz, "%s", href);
        return;
    }
    const char *scheme_end = strstr(base, "://");
    if (href[0] == '/' && href[1] == '/' && scheme_end) {
        /* protocol-relative //upload.wikimedia.org/... */
        snprintf(out, outsz, "%.*s%s", (int)(scheme_end + 1 - base), base, href);
        return;
    }
    if (!scheme_end) { snprintf(out, outsz, "%s", href); return; }
    const char *host_start = scheme_end + 3;
    const char *path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - base) : strlen(base);
    if (href[0] == '/') {
        snprintf(out, outsz, "%.*s%s", (int)host_len, base, href);
        return;
    }
    /* relative to the current path's own directory */
    if (path_start) {
        const char *last_slash = strrchr(path_start, '/');
        size_t dir_len = last_slash ? (size_t)(last_slash - base + 1) : host_len;
        snprintf(out, outsz, "%.*s%s", (int)dir_len, base, href);
    } else {
        snprintf(out, outsz, "%.*s/%s", (int)host_len, base, href);
    }
}


static const char *skip_named_element(const char *p, const char *name) {
    size_t nlen = strlen(name);
    int depth = 0;
    const char *q = p;
    while (*q) {
        if (*q != '<') { q++; continue; }
        int closing = (q[1] == '/');
        const char *n = q + 1 + (closing ? 1 : 0);
        if (strncasecmp(n, name, nlen) == 0 && !isalnum((unsigned char)n[nlen]) && n[nlen] != '-') {
            const char *gt = strchr(q, '>');
            if (!gt) return q + 1;
            if (closing) {
                if (depth <= 1) return gt + 1;
                depth--;
                q = gt + 1;
                continue;
            }
            if (*(gt - 1) == '/') { q = gt + 1; continue; }
            depth++;
            q = gt + 1;
            continue;
        }
        q++;
    }
    return p + 1;
}

static int tag_is_chrome(const char *name, const char *tag, const char *tag_end) {
    if (!name[0] || !tag_end) return 0;
    if (strcasecmp(name, "nav") == 0 || strcasecmp(name, "footer") == 0 || strcasecmp(name, "aside") == 0)
        return 1;
    char buf[512];
    size_t n = (size_t)(tag_end - tag);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, tag, n);
    buf[n] = 0;
    for (char *c = buf; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    if (strstr(buf, "role=\"navigation\"") || strstr(buf, "role='navigation'") ||
        strstr(buf, "role=\"banner\"") || strstr(buf, "role='banner'") ||
        strstr(buf, "role=\"search\"") || strstr(buf, "role='search'") ||
        strstr(buf, "role=\"complementary\"") || strstr(buf, "role='complementary'"))
        return 1;
    if (strstr(buf, "mw-navigation") || strstr(buf, "vector-header") ||
        strstr(buf, "vector-main-menu") || strstr(buf, "vector-sitenotice") ||
        strstr(buf, "mw-jump-link") || strstr(buf, "vector-page-toolbar") ||
        strstr(buf, "noprint") || strstr(buf, "mw-hidden-catlinks") ||
        strstr(buf, "id=\"mw-head\"") || strstr(buf, "id=\"mw-panel\"") ||
        strstr(buf, "id=\"siteNotice\"") || strstr(buf, "class=\"mw-editsection\""))
        return 1;
    if (strstr(buf, "hidden") && (strstr(buf, "aria-hidden=\"true\"") || strstr(buf, " hidden") || strstr(buf, "hidden=")))
        return 1;
    if (strcasecmp(name, "header") == 0 &&
        (strstr(buf, "vector") || strstr(buf, "mw-") || strstr(buf, "site-") || strstr(buf, "page-header")))
        return 1;
    return 0;
}

static const char *page_body_start(const char *html) {
    const char *marks[] = {
        "id=\"mw-content-text\"", "id='mw-content-text'",
        "id=\"bodyContent\"", "id='bodyContent'",
        "role=\"main\"", "role='main'",
        "<main", "<article",
        "id=\"content\"", "id='content'",
        "id=\"main-content\"", "id='main-content'",
        "id=\"main\"", "id='main'",
        NULL
    };
    int i;
    for (i = 0; marks[i]; i++) {
        const char *hit = strcasestr_local(html, marks[i]);
        if (!hit) continue;
        const char *lt = hit;
        while (lt > html && *lt != '<') lt--;
        if (*lt == '<') return lt;
        return hit;
    }
    return html;
}

static int junk_visible_line(const char *s) {
    if (!s || !s[0]) return 1;
    if (strcasecmp(s, "Main menu") == 0) return 1;
    if (strcasecmp(s, "Navigation") == 0) return 1;
    if (strcasecmp(s, "Contribute") == 0) return 1;
    if (strcasecmp(s, "Tools") == 0) return 1;
    if (strcasecmp(s, "Personal tools") == 0) return 1;
    if (strcasecmp(s, "Appearance") == 0) return 1;
    if (strcasecmp(s, "hide") == 0) return 1;
    if (strcasecmp(s, "show") == 0) return 1;
    if (strcasestr_local(s, "move to sidebar")) return 1;
    if (strcasestr_local(s, "Jump to content")) return 1;
    if (strcasestr_local(s, "Jump to search")) return 1;
    if (strcasestr_local(s, "Toggle the table of contents")) return 1;
    if (strcasestr_local(s, "From Wikipedia, the free encyclopedia")) return 0;
    return 0;
}

/* Extracts TITLE/TEXT/LINK rows straight into the already-open state
 * file (streaming, so PAGE_BUF_MAX bounds memory, not output size). */
static void extract_and_publish(const char *html, const char *url, FILE *out) {
    fprintf(out, "URL|%s\n", url);

    const char *tstart = strcasestr_local(html, "<title");
    char title[512] = "";
    if (tstart) {
        const char *gt = strchr(tstart, '>');
        if (gt) {
            const char *tend = strcasestr_local(gt + 1, "</title>");
            if (tend) {
                size_t n = (size_t)(tend - (gt + 1));
                if (n >= sizeof(title)) n = sizeof(title) - 1;
                memcpy(title, gt + 1, n);
                title[n] = '\0';
                html_decode_entities(title);
                collapse_ws(title);
            }
        }
    }
    fprintf(out, "TITLE|%s\n", title);

    char line[2048];
    size_t linelen = 0;
    const char *p = page_body_start(html);
    int line_count = 0;

    #define TEXT_WRAP 88
    #define FLUSH_LINE() do { \
        if (linelen > 0) { \
            line[linelen] = '\0'; \
            html_decode_entities(line); \
            collapse_ws(line); \
            if (line[0] && title[0] && strcmp(line, title) == 0) { linelen = 0; } \
            else if (line[0] && junk_visible_line(line)) { linelen = 0; } \
            else if (line[0] && line_count < MAX_LINES) { \
                char *s = line; \
                while (*s && line_count < MAX_LINES) { \
                    size_t L = strlen(s); \
                    if (L <= TEXT_WRAP) { fprintf(out, "TEXT|%s\n", s); line_count++; break; } \
                    size_t cut = TEXT_WRAP; \
                    while (cut > TEXT_WRAP / 2 && s[cut] && s[cut] != ' ') cut--; \
                    if (s[cut] == ' ') { \
                        s[cut] = '\0'; fprintf(out, "TEXT|%s\n", s); s += cut + 1; \
                    } else { \
                        char save = s[TEXT_WRAP]; s[TEXT_WRAP] = '\0'; \
                        fprintf(out, "TEXT|%s\n", s); s[TEXT_WRAP] = save; s += TEXT_WRAP; \
                    } \
                    line_count++; \
                } \
                linelen = 0; \
            } else { linelen = 0; } \
        } \
    } while (0)

    while (*p) {
        if (*p == '<') {
            /* skip script/style/title/noscript bodies - title is TITLE| already */
            if (strncasecmp(p, "<script", 7) == 0 || strncasecmp(p, "<style", 6) == 0
                || strncasecmp(p, "<title", 6) == 0 || strncasecmp(p, "<noscript", 9) == 0) {
                const char *close = NULL;
                int adv = 0;
                if (strncasecmp(p, "<script", 7) == 0) { close = strcasestr_local(p, "</script>"); adv = 9; }
                else if (strncasecmp(p, "<style", 6) == 0) { close = strcasestr_local(p, "</style>"); adv = 8; }
                else if (strncasecmp(p, "<title", 6) == 0) { close = strcasestr_local(p, "</title>"); adv = 8; }
                else { close = strcasestr_local(p, "</noscript>"); adv = 11; }
                p = close ? close + adv : p + strlen(p);
                continue;
            }
            /* skip site chrome (nav/header/footer/aside + wiki vector chrome) */
            {
                const char *nameend = p + 1;
                if (*nameend == '/') nameend++;
                const char *ns = nameend;
                while (isalnum((unsigned char)*nameend) || *nameend == '-') nameend++;
                char tname[32] = "";
                size_t nl = (size_t)(nameend - ns);
                if (nl > 0 && nl < sizeof(tname)) { memcpy(tname, ns, nl); tname[nl] = 0; }
                const char *gt0 = strchr(p, '>');
                if (tname[0] && gt0 && tag_is_chrome(tname, p, gt0)) {
                    FLUSH_LINE();
                    p = skip_named_element(p, tname);
                    continue;
                }
            }
            if (strncasecmp(p, "<img", 4) == 0) {
                FLUSH_LINE();
                const char *tag_end = strchr(p, '>');
                if (!tag_end) { p++; continue; }
                char src[PATH_BUF] = "";
                char alt[256] = "";
                const char *href_kv = strcasestr_local(p, "src=");
                if (href_kv && href_kv < tag_end) {
                    const char *v = href_kv + 4;
                    char q = 0;
                    if (*v == '"' || *v == '\'') { q = *v; v++; }
                    const char *vend = v;
                    if (q) { while (*vend && *vend != q) vend++; }
                    else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
                    size_t n = (size_t)(vend - v);
                    if (n >= sizeof(src)) n = sizeof(src) - 1;
                    memcpy(src, v, n); src[n] = 0;
                    html_decode_entities(src);
                }
                const char *alt_kv = strcasestr_local(p, "alt=");
                if (alt_kv && alt_kv < tag_end) {
                    const char *v = alt_kv + 4;
                    char q = 0;
                    if (*v == '"' || *v == '\'') { q = *v; v++; }
                    const char *vend = v;
                    if (q) { while (*vend && *vend != q) vend++; }
                    else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
                    size_t n = (size_t)(vend - v);
                    if (n >= sizeof(alt)) n = sizeof(alt) - 1;
                    memcpy(alt, v, n); alt[n] = 0;
                    html_decode_entities(alt); collapse_ws(alt);
                }
                if (src[0] && strncasecmp(src, "data:", 5) != 0 && strncasecmp(src, "javascript:", 11) != 0) {
                    char resolved[PATH_BUF];
                    resolve_url(url, src, resolved, sizeof(resolved));
                    fprintf(out, "MEDIA|I|%s|%s\n", resolved, alt);
                }
                p = tag_end + 1;
                continue;
            }
            if (strncasecmp(p, "<video", 6) == 0) {
                FLUSH_LINE();
                const char *tag_end = strchr(p, '>');
                if (!tag_end) { p++; continue; }
                char src[PATH_BUF] = "";
                char poster[PATH_BUF] = "";
                const char *href_kv = strcasestr_local(p, "src=");
                if (href_kv && href_kv < tag_end) {
                    const char *v = href_kv + 4;
                    char q = 0;
                    if (*v == '"' || *v == '\'') { q = *v; v++; }
                    const char *vend = v;
                    if (q) { while (*vend && *vend != q) vend++; }
                    else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
                    size_t n = (size_t)(vend - v);
                    if (n >= sizeof(src)) n = sizeof(src) - 1;
                    memcpy(src, v, n); src[n] = 0;
                    html_decode_entities(src);
                }
                const char *po = strcasestr_local(p, "poster=");
                if (po && po < tag_end) {
                    const char *v = po + 7;
                    char q = 0;
                    if (*v == '"' || *v == '\'') { q = *v; v++; }
                    const char *vend = v;
                    if (q) { while (*vend && *vend != q) vend++; }
                    else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
                    size_t n = (size_t)(vend - v);
                    if (n >= sizeof(poster)) n = sizeof(poster) - 1;
                    memcpy(poster, v, n); poster[n] = 0;
                    html_decode_entities(poster);
                }
                const char *vendv = strcasestr_local(p, "</video>");
                const char *scan = tag_end + 1;
                const char *scan_end = vendv ? vendv : scan + 800;
                if (!src[0]) {
                    const char *src_tag = strcasestr_local(scan, "<source");
                    if (src_tag && src_tag < scan_end) {
                        const char *se = strchr(src_tag, '>');
                        const char *sv = se ? strcasestr_local(src_tag, "src=") : NULL;
                        if (sv && se && sv < se) {
                            const char *v = sv + 4;
                            char q = 0;
                            if (*v == '"' || *v == '\'') { q = *v; v++; }
                            const char *ve = v;
                            if (q) { while (*ve && *ve != q) ve++; }
                            else { while (*ve && !isspace((unsigned char)*ve) && *ve != '>') ve++; }
                            size_t n = (size_t)(ve - v);
                            if (n >= sizeof(src)) n = sizeof(src) - 1;
                            memcpy(src, v, n); src[n] = 0;
                            html_decode_entities(src);
                        }
                    }
                }
                if (src[0] || poster[0]) {
                    char rsrc[PATH_BUF] = "", rpost[PATH_BUF] = "";
                    if (src[0]) resolve_url(url, src, rsrc, sizeof(rsrc));
                    if (poster[0]) resolve_url(url, poster, rpost, sizeof(rpost));
                    fprintf(out, "MEDIA|V|%s|%s\n", rsrc[0] ? rsrc : rpost, rpost);
                }
                p = vendv ? vendv + 8 : tag_end + 1;
                continue;
            }
            if (strncasecmp(p, "<a ", 3) == 0 || strncasecmp(p, "<a\t", 3) == 0 || strncasecmp(p, "<a>", 3) == 0) {
                const char *href_kv = strcasestr_local(p, "href=");
                const char *tag_end = strchr(p, '>');
                char href[PATH_BUF] = "";
                if (href_kv && tag_end && href_kv < tag_end) {
                    const char *v = href_kv + 5;
                    char q = 0;
                    if (*v == '"' || *v == '\'') { q = *v; v++; }
                    const char *vend = q ? strchr(v, q) : v;
                    if (!q) { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
                    if (vend) {
                        size_t n = (size_t)(vend - v);
                        if (n >= sizeof(href)) n = sizeof(href) - 1;
                        memcpy(href, v, n);
                        href[n] = '\0';
                    }
                }
                const char *aend = strcasestr_local(p, "</a>");
                char text[512] = "";
                if (tag_end && aend && aend > tag_end) {
                    const char *tp = tag_end + 1;
                    size_t tw = 0;
                    while (tp < aend && tw < sizeof(text) - 1) {
                        if (*tp == '<') { const char *g = strchr(tp, '>'); tp = g ? g + 1 : tp + 1; continue; }
                        text[tw++] = *tp++;
                    }
                    text[tw] = '\0';
                    html_decode_entities(text);
                    collapse_ws(text);
                }
                /* Fold <a> into the current paragraph so a sentence stays
                 * one TEXT row. Standalone nav links (empty line so far,
                 * short-ish label) still become LINK rows. */
                if (linelen > 0 && text[0]) {
                    if (linelen < sizeof(line) - 1 && line[linelen - 1] != ' ') line[linelen++] = ' ';
                    size_t ti;
                    for (ti = 0; text[ti] && linelen < sizeof(line) - 1; ti++) line[linelen++] = text[ti];
                } else if (href[0] && href[0] != '#' && strncasecmp(href, "javascript:", 11) != 0 && strncasecmp(href, "mailto:", 7) != 0 && strncasecmp(href, "tel:", 4) != 0) {
                    char resolved[PATH_BUF];
                    resolve_url(url, href, resolved, sizeof(resolved));
                    fprintf(out, "LINK|%s|%s\n", resolved, text[0] ? text : resolved);
                } else if (text[0]) {
                    size_t ti;
                    for (ti = 0; text[ti] && linelen < sizeof(line) - 1; ti++) line[linelen++] = text[ti];
                }
                p = aend ? aend + 4 : (tag_end ? tag_end + 1 : p + 1);
                continue;
            }
            /* generic tag: flush accumulated text on a block boundary */
            const char *nameend = p + 1;
            int closing = (*nameend == '/');
            if (closing) nameend++;
            const char *ns = nameend;
            while (isalnum((unsigned char)*nameend)) nameend++;
            char tagname[32] = "";
            size_t nl = (size_t)(nameend - ns);
            if (nl > 0 && nl < sizeof(tagname)) { memcpy(tagname, ns, nl); tagname[nl] = '\0'; }
            if (tagname[0] && is_block_tag(tagname)) FLUSH_LINE();
            const char *gt = strchr(p, '>');
            p = gt ? gt + 1 : p + 1;
            continue;
        }
        if (linelen < sizeof(line) - 1) line[linelen++] = *p;
        p++;
    }
    FLUSH_LINE();
    #undef FLUSH_LINE
}

static void write_chtpm_projection(void);
static void write_ui_projection(void);
static void load_page_title(char *out, size_t outsz);
static void do_fetch(const char *url_in, int record_history);

static char g_curl_url_path[PATH_BUF];
static char g_js_eval_path[PATH_BUF];
static char g_js_script_path[PATH_BUF];
static char g_js_effects_path[PATH_BUF];
static char g_media_op_path[PATH_BUF];
static char g_media_root[PATH_BUF];

static int script_type_skip(const char *tag, const char *tag_end) {
    const char *t = strcasestr_local(tag, "type=");
    if (!t || t >= tag_end) return 0;
    t += 5;
    if (*t == '"' || *t == '\'') t++;
    if (strncasecmp(t, "module", 6) == 0) return 1;
    if (strcasestr_local(t, "json") && t < tag_end) return 1;
    if (strcasestr_local(t, "ld+json") && t < tag_end) return 1;
    return 0;
}

static int curl_url_to_file(const char *url, const char *out_path) {
    FILE *uf = fopen(g_curl_url_path, "w");
    if (!uf) return 0;
    fprintf(uf, "url = \"");
    for (const char *u = url; *u; u++) {
        if (*u == '"' || *u == '\\') fputc('\\', uf);
        fputc(*u, uf);
    }
    fprintf(uf, "\"\n");
    fclose(uf);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
        "curl -sL --max-time 8 -A 'Mozilla/5.0 (NNEST network-browser-hq)' -o '%s' -K '%s'",
        out_path, g_curl_url_path);
    return system(cmd) == 0;
}

static void collect_scripts(const char *html, const char *page_url, FILE *js_out, int *n_scripts) {
    const char *p = html;
    int n = 0, n_ext = 0;
    *n_scripts = 0;
    while (p && *p && n < 8) {
        const char *tag = strcasestr_local(p, "<script");
        if (!tag) break;
        if (tag[7] != '>' && tag[7] != ' ' && tag[7] != '\t' && tag[7] != '\n' && tag[7] != '/') {
            p = tag + 7;
            continue;
        }
        const char *gt = strchr(tag, '>');
        if (!gt) break;
        const char *close = strcasestr_local(gt, "</script>");
        if (!close) break;
        if (script_type_skip(tag, gt)) {
            p = close + 9;
            continue;
        }
        const char *src = strcasestr_local(tag, "src=");
        if (src && src < gt) {
            if (n_ext >= 4) { p = close + 9; continue; }
            const char *v = src + 4;
            char q = 0;
            if (*v == '"' || *v == '\'') { q = *v; v++; }
            const char *vend = v;
            if (q) { while (*vend && *vend != q) vend++; }
            else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
            char href[PATH_BUF];
            size_t hn = (size_t)(vend - v);
            if (hn >= sizeof(href)) hn = sizeof(href) - 1;
            memcpy(href, v, hn);
            href[hn] = 0;
            html_decode_entities(href);
            if (href[0] && strncasecmp(href, "javascript:", 11) != 0 && strncasecmp(href, "data:", 5) != 0) {
                char resolved[PATH_BUF], extpath[PATH_BUF];
                resolve_url(page_url, href, resolved, sizeof(resolved));
                snprintf(extpath, sizeof(extpath), "%s.ext%d.js", g_js_script_path, n_ext);
                if (curl_url_to_file(resolved, extpath)) {
                    FILE *ef = fopen(extpath, "r");
                    if (ef) {
                        char buf[4096];
                        size_t r;
                        fprintf(js_out, "\n;try{/* src %d */\n", n_ext);
                        while ((r = fread(buf, 1, sizeof(buf), ef)) > 0) fwrite(buf, 1, r, js_out);
                        fprintf(js_out, "\n}catch(_e){print('script error '+String(_e));}\n");
                        fclose(ef);
                        n++;
                        n_ext++;
                    }
                }
            }
            p = close + 9;
            continue;
        }
        const char *body = gt + 1;
        if (close > body) {
            fprintf(js_out, "\n;try{/* inline %d */\n", n);
            fwrite(body, 1, (size_t)(close - body), js_out);
            fprintf(js_out, "\n}catch(_e){print('script error '+String(_e));}\n");
            n++;
        }
        p = close + 9;
    }
    *n_scripts = n;
}

static void apply_js_effects(const char *page_title) {
    FILE *ef = fopen(g_js_effects_path, "r");
    if (!ef) return;
    char line[PATH_BUF];
    char new_title[512] = "";
    char extras[16][2048];
    int n_extra = 0;
    int ok = 0;
    while (fgets(line, sizeof(line), ef)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1]=='\n' || line[L-1]=='\r')) line[--L] = 0;
        if (strncmp(line, "OK|1", 4) == 0) ok = 1;
        else if (strncmp(line, "TITLE|", 6) == 0) {
            snprintf(new_title, sizeof(new_title), "%s", line + 6);
        } else if ((strncmp(line, "LOG|", 4) == 0 || strncmp(line, "TEXT|", 5) == 0) && n_extra < 16) {
            const char *payload = strchr(line, '|');
            if (payload) {
                snprintf(extras[n_extra], sizeof(extras[0]), "TEXT|js: %s", payload + 1);
                n_extra++;
            }
        }
    }
    fclose(ef);
    if (!ok && !new_title[0] && n_extra == 0) return;

    (void)page_title;
    char tmp[PATH_BUF];
    FILE *pf = fopen(g_page_state_path, "r");
    FILE *wf = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!pf || !wf) {
        if (pf) fclose(pf);
        if (wf) fclose(wf);
        return;
    }
    char row[PATH_BUF];
    while (fgets(row, sizeof(row), pf)) {
        size_t L = strlen(row);
        while (L > 0 && (row[L-1]=='\n' || row[L-1]=='\r')) row[--L] = 0;
        if (new_title[0] && strncmp(row, "TITLE|", 6) == 0)
            fprintf(wf, "TITLE|%s\n", new_title);
        else
            fprintf(wf, "%s\n", row);
    }
    fclose(pf);
    for (int i = 0; i < n_extra; i++) fprintf(wf, "%s\n", extras[i]);
    fclose(wf);
    atomic_commit(g_page_state_path, tmp);
}


#define MAX_MEDIA 24

static int html_attr(const char *tag, const char *tag_end, const char *name, char *out, size_t outsz) {
    char key[64];
    snprintf(key, sizeof(key), "%s=", name);
    const char *kv = strcasestr_local(tag, key);
    if (!kv || (tag_end && kv >= tag_end)) return 0;
    const char *v = kv + strlen(key);
    char q = 0;
    if (*v == '"' || *v == '\'') { q = *v; v++; }
    const char *vend = v;
    if (q) {
        while (*vend && *vend != q) vend++;
    } else {
        while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++;
    }
    size_t n = (size_t)(vend - v);
    if (n >= outsz) n = outsz - 1;
    memcpy(out, v, n);
    out[n] = 0;
    html_decode_entities(out);
    return out[0] != 0;
}

static int media_skip_url(const char *u) {
    if (!u || !u[0]) return 1;
    if (strncasecmp(u, "data:", 5) == 0) return 1;
    if (strncasecmp(u, "javascript:", 11) == 0) return 1;
    if (strncasecmp(u, "blob:", 5) == 0) return 1;
    if (strcasestr_local(u, "1x1") || strcasestr_local(u, "pixel") ||
        strcasestr_local(u, "doubleclick") || strcasestr_local(u, "analytics") ||
        strcasestr_local(u, "facebook.com/tr") || strcasestr_local(u, "/ads/"))
        return 1;
    return 0;
}

static void strip_pipes(char *s) {
    char *w = s, *r = s;
    while (*r) {
        if (*r != '|') *w++ = *r;
        r++;
    }
    *w = 0;
}

static int already_have_media(char got[][PATH_BUF], int n, const char *url) {
    int i;
    for (i = 0; i < n; i++) if (strcmp(got[i], url) == 0) return 1;
    return 0;
}

static int fetch_to_sprite(const char *abs_url, const char *out_dir) {
    mkdir_p_local(out_dir);
    char raw[PATH_BUF];
    snprintf(raw, sizeof(raw), "%s/raw.bin", out_dir);
    char cfg[PATH_BUF];
    snprintf(cfg, sizeof(cfg), "%s/curl.url.cfg", out_dir);
    FILE *uf = fopen(cfg, "w");
    if (!uf) return 0;
    fprintf(uf, "url = \"");
    for (const char *u = abs_url; *u; u++) {
        if (*u == '"' || *u == '\\') fputc('\\', uf);
        fputc(*u, uf);
    }
    fprintf(uf, "\"\n");
    fclose(uf);
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
        "curl -sL --max-time 8 -A 'Mozilla/5.0 (NNEST network-browser-hq)' -o '%s' -K '%s'",
        raw, cfg);
    if (system(cmd) != 0) return 0;
    struct stat st;
    if (stat(raw, &st) != 0 || st.st_size < 32) return 0;
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s'", g_media_op_path, raw, out_dir);
    if (system(cmd) != 0) return 0;
    char csv[PATH_BUF];
    snprintf(csv, sizeof(csv), "%s/sprite.csv", out_dir);
    return stat(csv, &st) == 0 && st.st_size > 20;
}

static void collect_page_media(const char *html, const char *page_url) {
    (void)html; (void)page_url;
    if (!g_media_op_path[0]) return;
    {
        FILE *probe = fopen(g_media_op_path, "r");
        if (!probe) return;
        fclose(probe);
    }
    mkdir_p_local(g_media_root);

    FILE *pf = fopen(g_page_state_path, "r");
    if (!pf) return;
    char tmp[PATH_BUF];
    FILE *wf = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!wf) { fclose(pf); return; }

    int media_i = 0;
    char line[PATH_BUF + 512];
    while (fgets(line, sizeof(line), pf)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1]=='\n' || line[L-1]=='\r')) line[--L] = 0;
        if (strncmp(line, "MEDIA|", 6) != 0) {
            if (line[0]) fprintf(wf, "%s\n", line);
            continue;
        }
        if (media_i >= MAX_MEDIA) continue;
        char *kind = line + 6;
        char *bar = strchr(kind, '|');
        if (!bar) continue;
        *bar = 0;
        char *rest = bar + 1;
        char *bar2 = strchr(rest, '|');
        char extra[PATH_BUF] = "";
        if (bar2) { *bar2 = 0; snprintf(extra, sizeof(extra), "%s", bar2 + 1); }
        char urlbuf[PATH_BUF];
        snprintf(urlbuf, sizeof(urlbuf), "%s", rest);
        if (media_skip_url(urlbuf) && media_skip_url(extra)) continue;
        char dir[PATH_BUF];
        snprintf(dir, sizeof(dir), "%s/m%d", g_media_root, media_i);
        const char *fetch_url = urlbuf;
        if (kind[0] == 'V' && extra[0] && !media_skip_url(extra)) fetch_url = extra;
        else if (media_skip_url(urlbuf) && extra[0]) fetch_url = extra;
        if (!fetch_to_sprite(fetch_url, dir)) continue;
        strip_pipes(urlbuf);
        strip_pipes(extra);
        char rel[PATH_BUF];
        snprintf(rel, sizeof(rel), "%s/#.desktop/nb_sprites/m%d", g_house, media_i);
        if (kind[0] == 'V')
            fprintf(wf, "VIDEO|%s|%s|video\n", rel, urlbuf[0] ? urlbuf : extra);
        else
            fprintf(wf, "IMG|%s|%s\n", rel, extra);
        media_i++;
    }
    fclose(pf);
    fclose(wf);
    atomic_commit(g_page_state_path, tmp);
}

static void run_page_scripts(const char *html, const char *url, const char *title) {
    if (!g_js_eval_path[0]) return;
    FILE *probe = fopen(g_js_eval_path, "r");
    if (!probe) return;
    fclose(probe);

    FILE *js = fopen(g_js_script_path, "w");
    if (!js) return;
    int n = 0;
    collect_scripts(html, url, js, &n);
    fclose(js);
    if (n <= 0) return;

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
        "timeout 3 '%s' '%s' '%s' '%s' '%s'",
        g_js_eval_path, g_js_script_path, g_js_effects_path, url, title ? title : "");
    int rc = system(cmd);
    (void)rc;
    apply_js_effects(title);
}


static char g_back_path[PATH_BUF];
static char g_forward_path[PATH_BUF];
static char g_visit_log_path[PATH_BUF];
static char g_fetch_pid_path[PATH_BUF];
static char g_bookmark_path[PATH_BUF];
static char g_tabs_path[PATH_BUF];
static char g_tabs_root[PATH_BUF];
#define MAX_TABS 8
typedef struct {
    char url[PATH_BUF];
    char title[512];
} TabRec;
static TabRec g_tabs[MAX_TABS];
static int g_tab_count = 0;
static int g_tab_current = 0;

static void stack_push(const char *path, const char *url) {
    if (!path || !url || !url[0]) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", url);
    fclose(f);
}

static int stack_pop(const char *path, char *out, size_t outsz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char lines[256][PATH_BUF];
    int n = 0;
    while (n < 256 && fgets(lines[n], PATH_BUF, f)) {
        size_t L = strlen(lines[n]);
        while (L > 0 && (lines[n][L-1]=='\n' || lines[n][L-1]=='\r')) lines[n][--L] = 0;
        if (lines[n][0]) n++;
    }
    fclose(f);
    if (n < 1) return 0;
    snprintf(out, outsz, "%s", lines[n-1]);
    FILE *w = fopen(path, "w");
    if (w) {
        for (int i = 0; i < n-1; i++) fprintf(w, "%s\n", lines[i]);
        fclose(w);
    }
    return 1;
}

static void stack_clear(const char *path) {
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

/* Sidebar visit log: append-only. Back/Forward never truncate this file. */
static void visit_log_append(const char *url) {
    if (!url || !url[0]) return;
    FILE *f = fopen(g_visit_log_path, "a");
    if (!f) return;
    fprintf(f, "%s\n", url);
    fclose(f);
}

static void copy_file_if_missing(const char *src, const char *dst) {
    struct stat st;
    if (stat(dst, &st) == 0) return;
    FILE *in = fopen(src, "r");
    if (!in) return;
    FILE *out = fopen(dst, "w");
    if (!out) { fclose(in); return; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
}


static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    char tmp[PATH_BUF];
    FILE *out = atomic_open(dst, tmp, sizeof(tmp));
    if (!out) { fclose(in); return 0; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    atomic_commit(dst, tmp);
    return 1;
}

static void pipe_sanitize(char *s) {
    if (!s) return;
    for (; *s; s++) if (*s == '|') *s = ' ';
}

static void write_blank_page_state(void) {
    char tmp[PATH_BUF];
    FILE *f = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!f) return;
    fprintf(f, "URL|\nTITLE|Network Browser\nTEXT|Ready - enter a URL above\n");
    fclose(f);
    atomic_commit(g_page_state_path, tmp);
}

static void tab_paths(int idx, char *dir, size_t dirsz, char *state, size_t statesz, char *urlp, size_t urlsz) {
    snprintf(dir, dirsz, "%s/%d", g_tabs_root, idx);
    if (state) snprintf(state, statesz, "%s/page.state.txt", dir);
    if (urlp) snprintf(urlp, urlsz, "%s/url.txt", dir);
}

static void tab_save_snapshot(int idx) {
    if (idx < 0 || idx >= MAX_TABS) return;
    char dir[PATH_BUF], state[PATH_BUF], urlp[PATH_BUF];
    tab_paths(idx, dir, sizeof(dir), state, sizeof(state), urlp, sizeof(urlp));
    mkdir_p_local(dir);
    copy_file(g_page_state_path, state);
    FILE *uf = fopen(urlp, "w");
    if (uf) {
        const char *u = (idx < g_tab_count && g_tabs[idx].url[0]) ? g_tabs[idx].url : g_current_url;
        fprintf(uf, "%s\n", u);
        fclose(uf);
    }
}

static void tab_snapshot_clear(int idx) {
    char dir[PATH_BUF], state[PATH_BUF], urlp[PATH_BUF];
    tab_paths(idx, dir, sizeof(dir), state, sizeof(state), urlp, sizeof(urlp));
    unlink(state);
    unlink(urlp);
    rmdir(dir);
}

static void tab_copy_snapshot(int src, int dst) {
    if (src == dst) return;
    char sdir[PATH_BUF], sstate[PATH_BUF], surl[PATH_BUF];
    char ddir[PATH_BUF], dstate[PATH_BUF], durl[PATH_BUF];
    tab_paths(src, sdir, sizeof(sdir), sstate, sizeof(sstate), surl, sizeof(surl));
    tab_paths(dst, ddir, sizeof(ddir), dstate, sizeof(dstate), durl, sizeof(durl));
    mkdir_p_local(ddir);
    copy_file(sstate, dstate);
    copy_file(surl, durl);
}

static int tab_load_snapshot(int idx) {
    char dir[PATH_BUF], state[PATH_BUF], urlp[PATH_BUF];
    tab_paths(idx, dir, sizeof(dir), state, sizeof(state), urlp, sizeof(urlp));
    struct stat st;
    int have = (stat(state, &st) == 0);
    if (have) copy_file(state, g_page_state_path);
    g_current_url[0] = 0;
    if (idx >= 0 && idx < g_tab_count && g_tabs[idx].url[0])
        snprintf(g_current_url, sizeof(g_current_url), "%s", g_tabs[idx].url);
    FILE *uf = fopen(urlp, "r");
    if (uf) {
        char line[PATH_BUF];
        if (fgets(line, sizeof(line), uf)) {
            size_t L = strlen(line);
            while (L > 0 && (line[L-1]=='\n' || line[L-1]=='\r')) line[--L] = 0;
            if (line[0]) snprintf(g_current_url, sizeof(g_current_url), "%s", line);
        }
        fclose(uf);
    }
    return have;
}

static void tabs_write(void) {
    char tmp[PATH_BUF];
    FILE *f = atomic_open(g_tabs_path, tmp, sizeof(tmp));
    if (!f) return;
    int i;
    for (i = 0; i < g_tab_count; i++) {
        char url[PATH_BUF], title[512];
        snprintf(url, sizeof(url), "%s", g_tabs[i].url);
        snprintf(title, sizeof(title), "%s", g_tabs[i].title[0] ? g_tabs[i].title : "Network Browser");
        pipe_sanitize(url);
        pipe_sanitize(title);
        fprintf(f, "TAB | %d | %s | %s | %s\n",
                i, url, title, (i == g_tab_current) ? "current" : "");
    }
    fclose(f);
    atomic_commit(g_tabs_path, tmp);
}

static void tabs_default_one(void) {
    memset(g_tabs, 0, sizeof(g_tabs));
    g_tab_count = 1;
    g_tab_current = 0;
    g_tabs[0].url[0] = 0;
    snprintf(g_tabs[0].title, sizeof(g_tabs[0].title), "Network Browser");
    tabs_write();
}

static void tabs_load(void);

/* On start: if tabs.txt exists, show the CURRENT tab snapshot (URL/TITLE/TEXT)
 * without fetching. Never overwrite live page.state / tabs / snapshots with
 * the blank Ready page. First run (no tabs.txt) still opens one blank tab. */
static void session_restore_on_start(void) {
    struct stat st_tabs, st_page, st_snap;
    int have_tabs = (stat(g_tabs_path, &st_tabs) == 0 && st_tabs.st_size > 0);
    int have_page = (stat(g_page_state_path, &st_page) == 0 && st_page.st_size > 0);
    char dir[PATH_BUF], state[PATH_BUF];

    tabs_load();
    tab_paths(g_tab_current, dir, sizeof(dir), state, sizeof(state), NULL, 0);
    int have_snap = (stat(state, &st_snap) == 0 && st_snap.st_size > 0);

    if (have_tabs && have_snap) {
        tab_load_snapshot(g_tab_current);
        publish_status(g_current_url[0] ? "ready" : "idle");
        return;
    }
    if (have_tabs) {
        if (g_tab_current >= 0 && g_tab_current < g_tab_count && g_tabs[g_tab_current].url[0])
            snprintf(g_current_url, sizeof(g_current_url), "%s", g_tabs[g_tab_current].url);
        if (!have_page)
            write_blank_page_state();
        tab_save_snapshot(g_tab_current);
        publish_status(g_current_url[0] ? "ready" : "idle");
        return;
    }
    /* First run: one blank tab. Do not create Ready if a page.state already exists. */
    if (!have_page)
        write_blank_page_state();
    tab_save_snapshot(g_tab_current);
}

static void tabs_load(void) {
    memset(g_tabs, 0, sizeof(g_tabs));
    g_tab_count = 0;
    g_tab_current = 0;
    FILE *f = fopen(g_tabs_path, "r");
    if (!f) { tabs_default_one(); return; }
    char line[PATH_BUF * 2];
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1]=='\n' || line[L-1]=='\r')) line[--L] = 0;
        if (!line[0]) continue;
        if (strncmp(line, "CURRENT|", 8) == 0) {
            int c = atoi(line + 8);
            if (c >= 0 && c < MAX_TABS) g_tab_current = c;
            continue;
        }
        char buf[PATH_BUF * 2];
        snprintf(buf, sizeof(buf), "%s", line);
        char *fields[8];
        int nf = 0;
        char *p = buf;
        fields[nf++] = p;
        while (nf < 8) {
            char *sep = strstr(p, " | ");
            if (!sep) break;
            *sep = 0;
            p = sep + 3;
            fields[nf++] = p;
        }
        if (nf < 2 || strcmp(fields[0], "TAB") != 0) continue;
        if (g_tab_count >= MAX_TABS) continue;
        int idx = atoi(fields[1]);
        if (idx < 0 || idx >= MAX_TABS) idx = g_tab_count;
        if (idx != g_tab_count) {
            /* compact on load: store sequentially */
            idx = g_tab_count;
        }
        snprintf(g_tabs[idx].url, sizeof(g_tabs[idx].url), "%s", nf > 2 ? fields[2] : "");
        snprintf(g_tabs[idx].title, sizeof(g_tabs[idx].title), "%s", nf > 3 ? fields[3] : "Network Browser");
        if (nf > 4 && strstr(fields[4], "current"))
            g_tab_current = idx;
        g_tab_count++;
    }
    fclose(f);
    if (g_tab_count < 1) tabs_default_one();
    if (g_tab_current < 0 || g_tab_current >= g_tab_count) g_tab_current = 0;
}

static void tab_after_fetch_ok(const char *url) {
    char title[512];
    load_page_title(title, sizeof(title));
    if (g_tab_current < 0 || g_tab_current >= g_tab_count) {
        if (g_tab_count < 1) tabs_default_one();
    }
    if (g_tab_current >= 0 && g_tab_current < g_tab_count) {
        snprintf(g_tabs[g_tab_current].url, sizeof(g_tabs[g_tab_current].url), "%s", url ? url : "");
        snprintf(g_tabs[g_tab_current].title, sizeof(g_tabs[g_tab_current].title), "%s",
                 title[0] ? title : (url && url[0] ? url : "Network Browser"));
        tabs_write();
        tab_save_snapshot(g_tab_current);
    }
}

static void tab_switch(int n) {
    if (n < 0 || n >= g_tab_count) {
        publish_status("error: no such tab");
        return;
    }
    if (n != g_tab_current)
        tab_save_snapshot(g_tab_current);
    g_tab_current = n;
    tabs_write();
    if (!tab_load_snapshot(n)) {
        if (g_tabs[n].url[0]) {
            do_fetch(g_tabs[n].url, 0);
            return;
        }
        write_blank_page_state();
        g_current_url[0] = 0;
        publish_status("idle");
    } else {
        publish_status("ready");
    }
    write_chtpm_projection();
}

static void tab_new(void) {
    if (g_tab_count >= MAX_TABS) {
        publish_status("error: tab cap");
        write_chtpm_projection();
        return;
    }
    tab_save_snapshot(g_tab_current);
    int n = g_tab_count;
    g_tabs[n].url[0] = 0;
    snprintf(g_tabs[n].title, sizeof(g_tabs[n].title), "Network Browser");
    g_tab_count++;
    g_tab_current = n;
    write_blank_page_state();
    g_current_url[0] = 0;
    tab_save_snapshot(n);
    tabs_write();
    publish_status("idle");
    write_chtpm_projection();
}

static void tab_close_current(void) {
    if (g_tab_count <= 1) {
        publish_status("error: last tab");
        write_chtpm_projection();
        return;
    }
    int closed = g_tab_current;
    int i;
    for (i = closed; i < g_tab_count - 1; i++) {
        g_tabs[i] = g_tabs[i + 1];
        tab_copy_snapshot(i + 1, i);
    }
    tab_snapshot_clear(g_tab_count - 1);
    g_tab_count--;
    if (g_tab_current >= g_tab_count)
        g_tab_current = g_tab_count - 1;
    tabs_write();
    if (!tab_load_snapshot(g_tab_current)) {
        if (g_tabs[g_tab_current].url[0]) {
            do_fetch(g_tabs[g_tab_current].url, 0);
            return;
        }
        write_blank_page_state();
        g_current_url[0] = 0;
        publish_status("idle");
    } else {
        publish_status("ready");
    }
    write_chtpm_projection();
}

static int request_line_is_stop(const char *line) {
    return strcmp(line, "stop:") == 0 || strcmp(line, "stop") == 0;
}

/* Peek+consume only stop requests so Stop can interrupt a live curl.
 * Other pending lines are left for handle_request(). */
static int consume_stop_request(void) {
    FILE *f = fopen(g_request_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int got = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    if (!got) return 0;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (!request_line_is_stop(line)) return 0;
    FILE *cf = fopen(g_request_path, "w");
    if (cf) fclose(cf);
    return 1;
}

static void write_fetch_pid(pid_t pid) {
    FILE *f = fopen(g_fetch_pid_path, "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)pid);
    fclose(f);
}

static void unlink_fetch_pid(void) {
    unlink(g_fetch_pid_path);
}

static void reap_or_kill(pid_t pid) {
    int status = 0;
    int i;
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    for (i = 0; i < 20; i++) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) return;
        usleep(50000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
}

/* 0 = curl ok, 1 = curl failed, 2 = Stop killed the child */
static int run_curl_interruptible(const char *out_path, const char *cfg_path) {
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        execlp("curl", "curl", "-sL", "--max-time", "12",
               "-A", "Mozilla/5.0 (NNEST network-browser-hq)",
               "-o", out_path, "-K", cfg_path, (char *)NULL);
        _exit(127);
    }
    write_fetch_pid(pid);
    for (;;) {
        int status = 0;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            unlink_fetch_pid();
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
            if (WIFSIGNALED(status)) return 1;
            return 1;
        }
        if (w < 0 && errno != EINTR) {
            unlink_fetch_pid();
            return 1;
        }
        if (consume_stop_request()) {
            reap_or_kill(pid);
            unlink_fetch_pid();
            return 2;
        }
        usleep(80000);
    }
}


static void bookmark_add(const char *url, const char *title) {
    if (!url || !url[0]) return;
    FILE *r = fopen(g_bookmark_path, "r");
    if (r) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), r)) {
            size_t L = strlen(line);
            while (L > 0 && (line[L-1]=='\n' || line[L-1]=='\r')) line[--L] = 0;
            if (strncmp(line, "BOOKMARK | ", 11) == 0) {
                const char *rest = line + 11;
                const char *bar = strstr(rest, " | ");
                const char *u = bar ? bar + 3 : rest;
                if (strcmp(u, url) == 0) { fclose(r); return; }
            }
        }
        fclose(r);
    }
    FILE *f = fopen(g_bookmark_path, "a");
    if (!f) { fprintf(stderr, "network_browser_manager: cannot append bookmarks\n"); return; }
    /* pipe-table row: SECTION | KEY | VALUE  — title is KEY, url is VALUE */
    char tbuf[512];
    const char *src = (title && title[0]) ? title : url;
    size_t i, j = 0;
    for (i = 0; src[i] && j + 1 < sizeof(tbuf); i++) {
        if (src[i] != '|') tbuf[j++] = src[i];
    }
    tbuf[j] = 0;
    fprintf(f, "BOOKMARK | %s | %s\n", tbuf[0] ? tbuf : url, url);
    fclose(f);
}

static void load_page_title(char *out, size_t outsz) {
    out[0] = 0;
    FILE *pf = fopen(g_page_state_path, "r");
    if (!pf) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), pf)) {
        if (strncmp(line, "TITLE|", 6) == 0) {
            size_t L = strlen(line + 6);
            while (L > 0 && (line[6+L-1]=='\n' || line[6+L-1]=='\r')) L--;
            if (L >= outsz) L = outsz - 1;
            memcpy(out, line + 6, L);
            out[L] = 0;
            break;
        }
    }
    fclose(pf);
}


static int looks_image_bytes(const unsigned char *b, size_t n) {
    if (n >= 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff) return 1;
    if (n >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') return 1;
    if (n >= 6 && ((!memcmp(b, "GIF87a", 6)) || !memcmp(b, "GIF89a", 6))) return 1;
    if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4)) return 1;
    return 0;
}

static int publish_direct_image(const char *url) {
    mkdir_p_local(g_media_root);
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/m0", g_media_root);
    mkdir_p_local(dir);
    if (!g_media_op_path[0]) return 0;
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s'", g_media_op_path, g_tmp_html_path, dir);
    if (system(cmd) != 0) return 0;
    char csv[PATH_BUF];
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    struct stat st;
    if (stat(csv, &st) != 0) return 0;
    const char *slash = strrchr(url, '/');
    const char *leaf = (slash && slash[1]) ? slash + 1 : "image";
    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!out) return 0;
    fprintf(out, "URL|%s\nTITLE|%s\n", url, leaf);
    fprintf(out, "IMG|%s/#.desktop/nb_sprites/m0|\n", g_house);
    fclose(out);
    atomic_commit(g_page_state_path, tmp);
    return 1;
}


#define CATALOG_MAX 24

static int json_get_str(const char *obj, const char *end, const char *key, char *out, size_t outsz) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *k = strstr(obj, pat);
    if (!k || (end && k >= end)) {
        snprintf(pat, sizeof(pat), "\"%s\": \"", key);
        k = strstr(obj, pat);
        if (!k || (end && k >= end)) { out[0] = 0; return 0; }
    }
    k = strchr(k + strlen(key) + 2, '"');
    if (!k) { out[0] = 0; return 0; }
    k++;
    size_t o = 0;
    while (*k && *k != '"' && o + 1 < outsz) {
        if (*k == '\\' && k[1]) { k++; }
        out[o++] = *k++;
    }
    out[o] = 0;
    return o > 0;
}

static long long json_get_ll(const char *obj, const char *end, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *k = strstr(obj, pat);
    if (!k || (end && k >= end)) return 0;
    k += strlen(pat);
    while (*k == ' ') k++;
    return strtoll(k, NULL, 10);
}

static void strip_html_tags(char *s) {
    char *r = s, *w = s;
    int in = 0;
    while (*r) {
        if (*r == '<') { in = 1; r++; continue; }
        if (*r == '>') { in = 0; r++; continue; }
        if (!in) *w++ = *r;
        r++;
    }
    *w = 0;
    html_decode_entities(s);
    collapse_ws(s);
}

static int parse_4chan_board(const char *url, char *board, size_t board_sz) {
    const char *p = strstr(url, "4chan.org/");
    if (!p) return 0;
    if (!strstr(url, "/catalog")) return 0;
    p += 10; /* 4chan.org/ */
    if (!strncmp(p, "www.", 4)) p += 4;
    const char *slash = strchr(p, '/');
    if (!slash || slash == p) return 0;
    size_t n = (size_t)(slash - p);
    if (n >= board_sz) n = board_sz - 1;
    memcpy(board, p, n);
    board[n] = 0;
    int i;
    for (i = 0; board[i]; i++) {
        if (!((board[i] >= 'a' && board[i] <= 'z') || (board[i] >= '0' && board[i] <= '9'))) return 0;
    }
    return board[0] != 0;
}

static int ingest_4chan_catalog(const char *page_url) {
    char board[32];
    if (!parse_4chan_board(page_url, board, sizeof(board))) return 0;
    char api[256];
    snprintf(api, sizeof(api), "https://a.4cdn.org/%s/catalog.json", board);
    char jsonpath[PATH_BUF];
    snprintf(jsonpath, sizeof(jsonpath), "%s/&.hq-apps/network/tmp/catalog.json", g_house);
    {
        FILE *uf = fopen(g_curl_url_path, "w");
        if (!uf) return 0;
        fprintf(uf, "url = \"%s\"\n", api);
        fclose(uf);
    }
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
        "curl -sL --max-time 12 -A 'Mozilla/5.0 (NNEST network-browser-hq)' -o '%s' -K '%s'",
        jsonpath, g_curl_url_path);
    if (system(cmd) != 0) return 0;
    FILE *jf = fopen(jsonpath, "r");
    if (!jf) return 0;
    static char json[PAGE_BUF_MAX];
    size_t n = fread(json, 1, sizeof(json) - 1, jf);
    json[n] = 0;
    fclose(jf);
    if (n < 8 || json[0] != '[') return 0;

    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!out) return 0;
    fprintf(out, "URL|%s\nTITLE|/%s/ catalog\n", page_url, board);

    mkdir_p_local(g_media_root);
    int count = 0;
    const char *p = json;
    while (count < CATALOG_MAX && (p = strstr(p, "\"tim\":"))) {
        const char *obj = p;
        while (obj > json && *obj != '{') obj--;
        const char *obj_end = strchr(p, '}');
        if (!obj_end) obj_end = p + 400;
        long long tim = json_get_ll(obj, obj_end, "tim");
        long long no = json_get_ll(obj, obj_end, "no");
        if (tim <= 0) { p += 6; continue; }
        char sub[256] = "", com[256] = "";
        json_get_str(obj, obj_end, "sub", sub, sizeof(sub));
        json_get_str(obj, obj_end, "com", com, sizeof(com));
        strip_html_tags(sub);
        strip_html_tags(com);
        strip_pipes(sub);
        strip_pipes(com);
        const char *lab = sub[0] ? sub : (com[0] ? com : "thread");
        char thumb[PATH_BUF];
        snprintf(thumb, sizeof(thumb), "https://i.4cdn.org/%s/%llds.jpg", board, tim);
        char dir[PATH_BUF];
        snprintf(dir, sizeof(dir), "%s/m%d", g_media_root, count);
        if (fetch_to_sprite(thumb, dir)) {
            fprintf(out, "IMG|%s/#.desktop/nb_sprites/m%d|%s\n", g_house, count, lab);
        } else {
            fprintf(out, "TEXT|%s\n", lab);
        }
        if (no > 0) {
            fprintf(out, "LINK|https://boards.4chan.org/%s/thread/%lld|open thread\n", board, no);
        }
        count++;
        p = obj_end;
    }
    fclose(out);
    atomic_commit(g_page_state_path, tmp);
    return count > 0;
}

static void do_fetch(const char *url_in, int record_history) {
    char url[PATH_BUF];
    if (g_current_url[0]) resolve_url(g_current_url, url_in, url, sizeof(url));
    else snprintf(url, sizeof(url), "%s", url_in);

    publish_status("loading");
    write_chtpm_projection(); /* live X11 window must show loading before curl blocks */

    {
        FILE *uf = fopen(g_curl_url_path, "w");
        if (!uf) { publish_status("error: could not write curl url file"); return; }
        fprintf(uf, "url = \"");
        for (const char *u = url; *u; u++) {
            if (*u == '"' || *u == '\\') fputc('\\', uf);
            fputc(*u, uf);
        }
        fprintf(uf, "\"\n");
        fclose(uf);
    }
    int rc = run_curl_interruptible(g_tmp_html_path, g_curl_url_path);
    if (rc == 2) {
        publish_status("stopped");
        write_chtpm_projection();
        return;
    }

    FILE *hf = fopen(g_tmp_html_path, "r");
    if (rc != 0 || !hf) {
        char st[600];
        snprintf(st, sizeof(st), "error: curl failed (rc=%d) for %s", rc, url);
        publish_status(st);
        if (hf) fclose(hf);
        return;
    }

    static char html[PAGE_BUF_MAX];
    size_t n = fread(html, 1, sizeof(html) - 1, hf);
    html[n] = '\0';
    fclose(hf);
    if (n == 0) {
        publish_status("error: empty fetch");
        return;
    }

    if (looks_image_bytes((const unsigned char *)html, n)) {
        if (!publish_direct_image(url)) {
            publish_status("error: image decode failed");
            return;
        }
        if (record_history && g_current_url[0] && strcmp(g_current_url, url) != 0)
            stack_push(g_back_path, g_current_url);
        snprintf(g_current_url, sizeof(g_current_url), "%s", url);
        visit_log_append(url);
        tab_after_fetch_ok(url);
        publish_status("ready");
        write_chtpm_projection();
        return;
    }

    if (ingest_4chan_catalog(url)) {
        if (record_history && g_current_url[0] && strcmp(g_current_url, url) != 0)
            stack_push(g_back_path, g_current_url);
        snprintf(g_current_url, sizeof(g_current_url), "%s", url);
        visit_log_append(url);
        tab_after_fetch_ok(url);
        publish_status("ready");
        write_chtpm_projection();
        return;
    }

    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!out) { publish_status("error: could not write page state"); return; }
    extract_and_publish(html, url, out);
    fclose(out);
    atomic_commit(g_page_state_path, tmp);

    {
        char title[512] = "";
        FILE *tf = fopen(g_page_state_path, "r");
        if (tf) {
            char line[PATH_BUF];
            while (fgets(line, sizeof(line), tf)) {
                if (strncmp(line, "TITLE|", 6) == 0) {
                    size_t L = strlen(line + 6);
                    while (L > 0 && (line[6+L-1]=='\n' || line[6+L-1]=='\r')) L--;
                    if (L >= sizeof(title)) L = sizeof(title) - 1;
                    memcpy(title, line + 6, L);
                    title[L] = 0;
                    break;
                }
            }
            fclose(tf);
        }
        run_page_scripts(html, url, title);
        collect_page_media(html, url);
    }

    if (record_history && g_current_url[0] && strcmp(g_current_url, url) != 0)
        stack_push(g_back_path, g_current_url);
    snprintf(g_current_url, sizeof(g_current_url), "%s", url);
    visit_log_append(url);
    tab_after_fetch_ok(url);
    publish_status("ready");
    write_chtpm_projection();
}

static void handle_request(void) {
    FILE *f = fopen(g_request_path, "r");
    if (!f) return;
    char line[PATH_BUF];
    int got = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    if (!got) return;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (!line[0]) return;

    /* clear immediately - same "truncate so it doesn't re-fire" contract
     * as khtpm_open_hai_manager.c's own handle_request(). */
    FILE *cf = fopen(g_request_path, "w");
    if (cf) fclose(cf);

    if (strncmp(line, "go:", 3) == 0) {
        stack_clear(g_forward_path);
        do_fetch(line + 3, 1);
    } else if (strcmp(line, "back:") == 0 || strcmp(line, "back") == 0) {
        char prev[PATH_BUF];
        if (stack_pop(g_back_path, prev, sizeof(prev))) {
            if (g_current_url[0]) stack_push(g_forward_path, g_current_url);
            do_fetch(prev, 0);
        } else publish_status("error: no history");
    } else if (strcmp(line, "forward:") == 0 || strcmp(line, "forward") == 0) {
        char next[PATH_BUF];
        if (stack_pop(g_forward_path, next, sizeof(next))) {
            if (g_current_url[0]) stack_push(g_back_path, g_current_url);
            do_fetch(next, 0);
        } else publish_status("error: no forward");
    } else if (request_line_is_stop(line)) {
        publish_status("stopped");
    } else if (strcmp(line, "reload:") == 0 || strcmp(line, "reload") == 0) {
        if (g_current_url[0]) do_fetch(g_current_url, 0);
        else publish_status("error: nothing to reload");
    } else if (strcmp(line, "bookmark:") == 0 || strcmp(line, "bookmark") == 0) {
        char title[512];
        load_page_title(title, sizeof(title));
        if (g_current_url[0]) { bookmark_add(g_current_url, title); publish_status("ready"); }
        else publish_status("error: nothing to bookmark");
    } else if (strncmp(line, "tab:", 4) == 0) {
        tab_switch(atoi(line + 4));
    } else if (strcmp(line, "newtab:") == 0 || strcmp(line, "newtab") == 0) {
        tab_new();
    } else if (strcmp(line, "closetab:") == 0 || strcmp(line, "closetab") == 0) {
        tab_close_current();
    }
}

/* REAL, NEW 2026-09-01 - write live .chtpm projection from manager state
 * (ported from khtpm_open_hai_manager.c's own pattern). Regenerates
 * the .chtpm file every main-loop tick from the manager's real published
 * state (current URL, page content, status), using only generic tags
 * (sidebar/panel/scrolllist/item/text) - zero new renderer C. The
 * renderer picks it up via reparse_chtpm_if_changed(). */
static void write_chtpm_projection(void) {
    if (g_mode_ui) { write_ui_projection(); return; }
    char *buf = malloc(262144);
    if (!buf) return;
    size_t cap = 262144, len = 0;
#define NB_APPEND(...) do { \
        int _n = snprintf(buf + len, cap - len, __VA_ARGS__); \
        if (_n > 0) len += (size_t)_n < cap - len ? (size_t)_n : cap - len - 1; \
    } while (0)

    NB_APPEND("<!-- network-browser-hq.chtpm - REAL, GENERATED PROJECTION.\n");
    NB_APPEND("     Written by network_browser_manager.c's own write_chtpm_projection()\n");
    NB_APPEND("     every real main-loop tick - DO NOT HAND-EDIT, changes are\n");
    NB_APPEND("     overwritten within ~300ms. See that function's own header\n");
    NB_APPEND("     comment for the real design this answers to. -->\n");
    NB_APPEND("<window label=\"Network Browser\" class=\"network-browser\">\n  <module src=\"&.hq-apps/network/+x/network_browser_manager.+x\"/>\n  <page name=\"main\">\n");

    /* REAL FIX 2026-09-01 (live report: "network browser has no x") -
     * layout_sidebar_panel() (khtpm_core_render.c) only adds the real
     * generic chrome X/! buttons when BOTH <sidebar> AND <panel> exist
     * in the page - a bare <page> with no sidebar (as this projection
     * used to emit) silently falls through to the flat-list layout
     * with zero chrome, by design (see that function's own early
     * `if (!sidebar || !panel) return 0;`). Real fix: give it a real,
     * minimal <sidebar> so it qualifies for the SAME generic mechanism
     * every other sidebar+panel window already gets - zero new C. */
    /* Sidebar: Bookmarks then History (visit log, not the Back stack).
     * Toolbar items live in the panel row so they sit at the top of
     * the content pane, not in the left column. class=quiet is kept
     * for look; nav [ ]N badges stay on the item (house rule). */
    NB_APPEND("    <sidebar>\n      <text label=\"Bookmarks\"/>\n");
    NB_APPEND("      <scrolllist>\n");
    {
        FILE *bf = fopen(g_bookmark_path, "r");
        int bi = 0;
        if (bf) {
            char bline[PATH_BUF];
            while (fgets(bline, sizeof(bline), bf) && bi < 32) {
                size_t L = strlen(bline);
                while (L > 0 && (bline[L-1]=='\n' || bline[L-1]=='\r')) bline[--L] = 0;
                if (strncmp(bline, "BOOKMARK | ", 11) != 0) continue;
                char *rest = bline + 11;
                char *sep = strstr(rest, " | ");
                if (!sep) continue;
                *sep = 0;
                char *burl = sep + 3;
                char lab_esc[600], url_sq[PATH_BUF * 2];
                xml_escape(rest[0] ? rest : burl, lab_esc, sizeof(lab_esc));
                shell_escape_squote(burl, url_sq, sizeof(url_sq));
                NB_APPEND("        <item id=\"bm%d\" label=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                          bi, lab_esc, g_package_dir, url_sq);
                bi++;
            }
            fclose(bf);
        }
        if (bi == 0)
            NB_APPEND("        <text label=\"No bookmarks yet\"/>\n");
    }
    NB_APPEND("      </scrolllist>\n");
    NB_APPEND("      <text label=\"History\"/>\n");
    NB_APPEND("      <scrolllist>\n");
    {
        /* File order is oldest-first (append). Show newest first, cap 32. */
        char hlines[256][PATH_BUF];
        int hn = 0;
        FILE *hf = fopen(g_visit_log_path, "r");
        if (hf) {
            while (hn < 256 && fgets(hlines[hn], PATH_BUF, hf)) {
                size_t L = strlen(hlines[hn]);
                while (L > 0 && (hlines[hn][L-1]=='\n' || hlines[hn][L-1]=='\r')) hlines[hn][--L] = 0;
                if (hlines[hn][0]) hn++;
            }
            fclose(hf);
        }
        int shown = 0;
        int hi;
        for (hi = hn - 1; hi >= 0 && shown < 32; hi--) {
            char lab_esc[600], url_sq[PATH_BUF * 2];
            xml_escape(hlines[hi], lab_esc, sizeof(lab_esc));
            shell_escape_squote(hlines[hi], url_sq, sizeof(url_sq));
            NB_APPEND("        <item id=\"hist%d\" label=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                      shown, lab_esc, g_package_dir, url_sq);
            shown++;
        }
        if (shown == 0)
            NB_APPEND("        <text label=\"No history yet\"/>\n");
    }
    NB_APPEND("      </scrolllist>\n    </sidebar>\n");
    NB_APPEND("    <panel>\n");
    /* Toolbar then address (cli_io class=top so the generic layout
     * pins it at y_cursor, not the bottom composer). */
    NB_APPEND("      <row class=\"toolbar\">\n");
    NB_APPEND("        <item id=\"nb-back\" class=\"quiet\" label=\"Back\" action=\"'%s/ops/nb_write_back.sh' 'back'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-fwd\" class=\"quiet\" label=\"Forward\" action=\"'%s/ops/nb_write_forward.sh' 'forward'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-stop\" class=\"quiet\" label=\"Stop\" action=\"'%s/ops/nb_write_stop.sh' 'stop'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-reload\" class=\"quiet\" label=\"Reload\" action=\"'%s/ops/nb_write_reload.sh' 'reload'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-home\" class=\"quiet\" label=\"Home\" action=\"'%s/ops/nb_write_go.sh' 'go' 'https://example.com'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-bm\" class=\"quiet\" label=\"Bookmark\" action=\"'%s/ops/nb_write_bookmark.sh' 'bookmark'\"/>\n", g_package_dir);
    NB_APPEND("        <item id=\"nb-close\" class=\"quiet\" label=\"Close tab\" action=\"'%s/ops/nb_write_closetab.sh' 'closetab'\"/>\n", g_package_dir);
    NB_APPEND("      </row>\n");
    /* Second toolbar row = tab strip. Same generic class=toolbar so the
     * existing renderer lays it as one ROW_H of equal-width items.
     * Numbered [ ]N badges stay visible (house rule). */
    NB_APPEND("      <row class=\"toolbar\">\n");
    {
        int ti;
        for (ti = 0; ti < g_tab_count; ti++) {
            const char *src = g_tabs[ti].title[0] ? g_tabs[ti].title
                : (g_tabs[ti].url[0] ? g_tabs[ti].url : "Network Browser");
            char shortlab[24];
            size_t sl = strlen(src);
            if (sl > 18) {
                memcpy(shortlab, src, 18);
                shortlab[18] = 0;
            } else {
                memcpy(shortlab, src, sl + 1);
            }
            if (ti == g_tab_current && shortlab[0] && strlen(shortlab) < 18) {
                /* mark current without hiding the label */
                char marked[24];
                snprintf(marked, sizeof(marked), "*%s", shortlab);
                snprintf(shortlab, sizeof(shortlab), "%s", marked);
            }
            char lab_esc[64];
            xml_escape(shortlab, lab_esc, sizeof(lab_esc));
            NB_APPEND("        <item id=\"tab%d\" class=\"quiet\" label=\"%s\" action=\"'%s/ops/nb_write_tab.sh' 'tab' '%d'\"/>\n",
                      ti, lab_esc, g_package_dir, ti);
        }
        NB_APPEND("        <item id=\"nb-newtab\" class=\"quiet\" label=\"New tab\" action=\"'%s/ops/nb_write_newtab.sh' 'newtab'\"/>\n", g_package_dir);
    }
    NB_APPEND("      </row>\n");

    /* Address bar input - generic <cli_io> mechanism.
     * REAL FIX 2026-09-01, found while packaging this app for the
     * co-work reference repo: this baked-in action path was missing
     * "ops/" - nb_write_go.sh has always lived at ops/nb_write_go.sh,
     * never directly under &.hq-apps/network/. Since this ran through
     * a real shell command with stderr redirected to /dev/null, the
     * wrong path failed completely silently - the address bar and
     * every content link have been non-functional since this file was
     * written, with zero visible symptom beyond "nothing happens."
     * REAL, NEW 2026-09-02 - class="top nb-address": top pins under
     * the toolbar; nb-address is the existing CSS color rule. */
    {
        const char *shown = g_current_url[0] ? g_current_url : "URL: ";
        char url_esc[PATH_BUF];
        xml_escape(shown, url_esc, sizeof(url_esc));
        NB_APPEND("      <cli_io id=\"address\" class=\"top nb-address\" target_id=\"address\" label=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                  url_esc, g_package_dir, g_chtpm_output_path);
    }

    /* Status line - read from status file */
    char status_line[256] = "idle";
    FILE *sf = fopen(g_status_path, "r");
    if (sf) {
        if (fgets(status_line, sizeof(status_line), sf)) {
            size_t n = strlen(status_line);
            while (n > 0 && (status_line[n-1] == '\n' || status_line[n-1] == '\r')) status_line[--n] = '\0';
        }
        fclose(sf);
    }
    char status_esc[300];
    xml_escape(status_line, status_esc, sizeof(status_esc));
    NB_APPEND("      <text id=\"status\" class=\"nb-status\" label=\"Status: %s\"/>\n", status_esc);

    /* Content area - scrollable list of page content */
    NB_APPEND("      <scrolllist class=\"from-top nb-content\">\n");

    /* Read page state, then project: consecutive IMG/VIDEO runs wrap
     * into ONE generic <row class="sprite-grid-row"> (renderer wraps
     * to fill width). Isolated single images stay a lone <item>.
     * Thread LINK after an IMG becomes that item's action=. */
#define NB_STATE_MAX 400
    {
        FILE *pf = fopen(g_page_state_path, "r");
        static char kinds[NB_STATE_MAX][8];
        static char f1[NB_STATE_MAX][1024];
        static char f2[NB_STATE_MAX][600];
        static char f3[NB_STATE_MAX][1024];
        int nst = 0;
        if (pf) {
            char line[PATH_BUF + 512];
            while (fgets(line, sizeof(line), pf) && nst < NB_STATE_MAX) {
                size_t n = strlen(line);
                while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
                char *bar = strchr(line, '|');
                if (!bar) continue;
                *bar = '\0';
                char *rest = bar + 1;
                snprintf(kinds[nst], sizeof(kinds[nst]), "%s", line);
                f1[nst][0] = f2[nst][0] = f3[nst][0] = '\0';
                if (strcmp(line, "TITLE") == 0 || strcmp(line, "TEXT") == 0) {
                    snprintf(f1[nst], sizeof(f1[nst]), "%s", rest);
                } else if (strcmp(line, "LINK") == 0 || strcmp(line, "IMG") == 0) {
                    char *bar2 = strchr(rest, '|');
                    if (bar2) { *bar2 = '\0'; snprintf(f2[nst], sizeof(f2[nst]), "%s", bar2 + 1); }
                    snprintf(f1[nst], sizeof(f1[nst]), "%s", rest);
                } else if (strcmp(line, "VIDEO") == 0) {
                    char *bar2 = strchr(rest, '|');
                    snprintf(f1[nst], sizeof(f1[nst]), "%s", rest);
                    if (bar2) {
                        *bar2 = '\0';
                        snprintf(f1[nst], sizeof(f1[nst]), "%s", rest);
                        char *vurl = bar2 + 1;
                        char *bar3 = strchr(vurl, '|');
                        if (bar3) { *bar3 = '\0'; snprintf(f3[nst], sizeof(f3[nst]), "%s", bar3 + 1); }
                        snprintf(f2[nst], sizeof(f2[nst]), "%s", vurl);
                    }
                } else {
                    continue;
                }
                nst++;
            }
            fclose(pf);
        }
        int row_count = 0;
        int i = 0;
        while (i < nst) {
            int is_media = (strcmp(kinds[i], "IMG") == 0 || strcmp(kinds[i], "VIDEO") == 0);
            if (!is_media) {
                char content_esc[600];
                if (strcmp(kinds[i], "TITLE") == 0) {
                    xml_escape(f1[i], content_esc, sizeof(content_esc));
                    NB_APPEND("      <text id=\"title%d\" class=\"page-title\" label=\"%s\"/>\n", row_count, content_esc);
                } else if (strcmp(kinds[i], "TEXT") == 0) {
                    xml_escape(f1[i], content_esc, sizeof(content_esc));
                    NB_APPEND("      <text id=\"text%d\" label=\"%s\"/>\n", row_count, content_esc);
                } else if (strcmp(kinds[i], "LINK") == 0) {
                    char link_text_esc[600], url_sq[PATH_BUF * 2];
                    xml_escape(f2[i][0] ? f2[i] : f1[i], link_text_esc, sizeof(link_text_esc));
                    shell_escape_squote(f1[i], url_sq, sizeof(url_sq));
                    NB_APPEND("      <item id=\"link%d\" label=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                              row_count, link_text_esc, g_package_dir, url_sq);
                }
                row_count++;
                i++;
                continue;
            }
            /* Collect a consecutive IMG/VIDEO run; a LINK immediately
             * after an IMG is that tile's action, not its own row. */
            int run_start = i;
            static int run_idx[NB_STATE_MAX];
            static char run_act[NB_STATE_MAX][1024];
            int nrun = 0;
            while (i < nst && (strcmp(kinds[i], "IMG") == 0 || strcmp(kinds[i], "VIDEO") == 0)) {
                run_idx[nrun] = i;
                run_act[nrun][0] = '\0';
                i++;
                if (i < nst && strcmp(kinds[run_idx[nrun]], "IMG") == 0 && strcmp(kinds[i], "LINK") == 0) {
                    snprintf(run_act[nrun], sizeof(run_act[nrun]), "%s", f1[i]);
                    i++;
                }
                nrun++;
            }
            (void)run_start;
            int wrap = (nrun >= 2);
            int t;
            if (wrap) NB_APPEND("      <row class=\"sprite-grid-row\">\n");
            for (t = 0; t < nrun; t++) {
                int ri = run_idx[t];
                char dir_esc[600], lab_esc[600];
                char shortlab[64];
                const char *rawlab;
                if (strcmp(kinds[ri], "VIDEO") == 0)
                    rawlab = f3[ri][0] ? f3[ri] : "play";
                else
                    rawlab = f2[ri][0] ? f2[ri] : " ";
                fill_sprite_shortlab(rawlab, shortlab, sizeof(shortlab));
                xml_escape(f1[ri], dir_esc, sizeof(dir_esc));
                xml_escape(shortlab, lab_esc, sizeof(lab_esc));
                if (strcmp(kinds[ri], "VIDEO") == 0) {
                    char url_sq[PATH_BUF * 2];
                    shell_escape_squote(f2[ri], url_sq, sizeof(url_sq));
                    NB_APPEND("        <item id=\"vid%d\" class=\"quiet\" label=\"%s\" sprite=\"%s\" action=\"ffplay -autoexit -loglevel error '%s'\"/>\n",
                              row_count, lab_esc, dir_esc, url_sq);
                } else if (run_act[t][0]) {
                    char url_sq[PATH_BUF * 2];
                    shell_escape_squote(run_act[t], url_sq, sizeof(url_sq));
                    NB_APPEND("        <item id=\"img%d\" class=\"quiet\" label=\"%s\" sprite=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                              row_count, lab_esc, dir_esc, g_package_dir, url_sq);
                } else {
                    NB_APPEND("        <item id=\"img%d\" class=\"quiet\" label=\"%s\" sprite=\"%s\"/>\n",
                              row_count, lab_esc, dir_esc);
                }
                row_count++;
            }
            if (wrap) NB_APPEND("      </row>\n");
        }
    }
#undef NB_STATE_MAX

    NB_APPEND("    </scrolllist>\n");
    NB_APPEND("  </panel>\n");
    NB_APPEND("  </page>\n</window>\n");
#undef NB_APPEND

    /* Only write when content actually changed - avoid needless reparse */
    static char *g_last_projection = NULL;
    if (g_last_projection && strcmp(g_last_projection, buf) == 0) { free(buf); return; }
    free(g_last_projection);
    g_last_projection = buf;

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_chtpm_output_path);
    FILE *wf = fopen(tmp_path, "w");
    if (!wf) {
        fprintf(stderr, "network_browser_manager: cannot write projection %s\n", tmp_path);
        return;
    }
    fputs(buf, wf);
    fclose(wf);
    rename(tmp_path, g_chtpm_output_path);
}

/* -------- static-template UI projection (g_mode_ui) ------------------
 * Same data as write_chtpm_projection(), emitted as key=value for
 * network-browser-hq.xhtpm instead of regenerated markup. Only written
 * when the content changes (the renderer also content-hashes it). */
static void uisan(const char *in, char *out, size_t outsz) {
    /* strip CR/LF, turn '|' into '/' (it is the frame-dump field
     * separator - an unescaped '|' in a label/action corrupts the
     * frame round trip) */
    size_t o = 0;
    for (const char *p = in ? in : ""; *p && o + 1 < outsz; p++) {
        char c = *p;
        if (c == '\n' || c == '\r') continue;
        if (c == '|') c = '/';
        out[o++] = c;
    }
    out[o] = '\0';
}
static void write_ui_projection(void) {
    char *buf = malloc(262144);
    if (!buf) return;
    size_t cap = 262144, len = 0;
#define UI_PUT(...) do { \
        int _n = snprintf(buf + len, cap - len, __VA_ARGS__); \
        if (_n > 0) len += (size_t)_n < cap - len ? (size_t)_n : cap - len - 1; \
    } while (0)

    /* fixed toolbar action strings (were baked into the markup before) */
    UI_PUT("act_back='%s/ops/nb_write_back.sh' 'back'\n", g_package_dir);
    UI_PUT("act_fwd='%s/ops/nb_write_forward.sh' 'forward'\n", g_package_dir);
    UI_PUT("act_stop='%s/ops/nb_write_stop.sh' 'stop'\n", g_package_dir);
    UI_PUT("act_reload='%s/ops/nb_write_reload.sh' 'reload'\n", g_package_dir);
    UI_PUT("act_home='%s/ops/nb_write_go.sh' 'go' 'https://example.com'\n", g_package_dir);
    UI_PUT("act_bm='%s/ops/nb_write_bookmark.sh' 'bookmark'\n", g_package_dir);
    UI_PUT("act_close='%s/ops/nb_write_closetab.sh' 'closetab'\n", g_package_dir);
    UI_PUT("act_newtab='%s/ops/nb_write_newtab.sh' 'newtab'\n", g_package_dir);
    UI_PUT("act_go='%s/ops/nb_write_go.sh' 'go' '%s'\n", g_package_dir, g_ui_output_path);

    {
        char shown[PATH_BUF], s[PATH_BUF];
        snprintf(shown, sizeof(shown), "%s", g_current_url[0] ? g_current_url : "URL: ");
        uisan(shown, s, sizeof(s));
        UI_PUT("addr_label=%s\n", s);
    }

    /* status */
    {
        char status_line[256] = "idle", s[300];
        FILE *sf = fopen(g_status_path, "r");
        if (sf) {
            if (fgets(status_line, sizeof(status_line), sf)) {
                size_t n = strlen(status_line);
                while (n > 0 && (status_line[n-1] == '\n' || status_line[n-1] == '\r')) status_line[--n] = 0;
            }
            fclose(sf);
        }
        uisan(status_line, s, sizeof(s));
        UI_PUT("status=Status: %s\n", s);
    }

    /* bookmarks */
    {
        int bi = 0;
        FILE *bf = fopen(g_bookmark_path, "r");
        if (bf) {
            char bline[PATH_BUF];
            while (fgets(bline, sizeof(bline), bf) && bi < 32) {
                size_t L = strlen(bline);
                while (L > 0 && (bline[L-1] == '\n' || bline[L-1] == '\r')) bline[--L] = 0;
                if (strncmp(bline, "BOOKMARK | ", 11) != 0) continue;
                char *rest = bline + 11;
                char *sep = strstr(rest, " | ");
                if (!sep) continue;
                *sep = 0;
                char *burl = sep + 3;
                char lab_s[600], url_sq[PATH_BUF * 2];
                uisan(rest[0] ? rest : burl, lab_s, sizeof(lab_s));
                shell_escape_squote(burl, url_sq, sizeof(url_sq));
                UI_PUT("bm_%d_label=%s\n", bi, lab_s);
                UI_PUT("bm_%d_action='%s/ops/nb_write_go.sh' 'go' '%s'\n", bi, g_package_dir, url_sq);
                bi++;
            }
            fclose(bf);
        }
        UI_PUT("n_bm=%d\n", bi);
        UI_PUT("no_bm=%d\n", bi == 0 ? 1 : 0);
    }

    /* history (newest first, cap 32) */
    {
        char hlines[256][PATH_BUF];
        int hn = 0;
        FILE *hf = fopen(g_visit_log_path, "r");
        if (hf) {
            while (hn < 256 && fgets(hlines[hn], PATH_BUF, hf)) {
                size_t L = strlen(hlines[hn]);
                while (L > 0 && (hlines[hn][L-1] == '\n' || hlines[hn][L-1] == '\r')) hlines[hn][--L] = 0;
                if (hlines[hn][0]) hn++;
            }
            fclose(hf);
        }
        int shown = 0;
        for (int hi = hn - 1; hi >= 0 && shown < 32; hi--) {
            char lab_s[600], url_sq[PATH_BUF * 2];
            uisan(hlines[hi], lab_s, sizeof(lab_s));
            shell_escape_squote(hlines[hi], url_sq, sizeof(url_sq));
            UI_PUT("h_%d_label=%s\n", shown, lab_s);
            UI_PUT("h_%d_action='%s/ops/nb_write_go.sh' 'go' '%s'\n", shown, g_package_dir, url_sq);
            shown++;
        }
        UI_PUT("n_hist=%d\n", shown);
        UI_PUT("no_hist=%d\n", shown == 0 ? 1 : 0);
    }

    /* tab strip */
    {
        int ti;
        for (ti = 0; ti < g_tab_count; ti++) {
            const char *src = g_tabs[ti].title[0] ? g_tabs[ti].title
                : (g_tabs[ti].url[0] ? g_tabs[ti].url : "Network Browser");
            char shortlab[24];
            size_t sl = strlen(src);
            if (sl > 18) { memcpy(shortlab, src, 18); shortlab[18] = 0; }
            else { memcpy(shortlab, src, sl + 1); }
            char marked[26], s[64];
            if (ti == g_tab_current && shortlab[0]) {
                snprintf(marked, sizeof(marked), "*%s", shortlab);
                uisan(marked, s, sizeof(s));
            } else {
                uisan(shortlab, s, sizeof(s));
            }
            UI_PUT("t_%d_label=%s\n", ti, s);
            UI_PUT("t_%d_action='%s/ops/nb_write_tab.sh' 'tab' '%d'\n", ti, g_package_dir, ti);
        }
        UI_PUT("n_tabs=%d\n", g_tab_count);
    }

    /* page content - one <repeat> row per state line; media rows are
     * NOT grouped into a sprite-grid-row here (first-cut limitation). */
    {
        FILE *pf = fopen(g_page_state_path, "r");
        int rc = 0;
        if (pf) {
            char line[PATH_BUF + 512];
            while (fgets(line, sizeof(line), pf) && rc < 400) {
                size_t n = strlen(line);
                while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
                char *bar = strchr(line, '|');
                if (!bar) continue;
                *bar = 0;
                char *rest = bar + 1;
                const char *kind = line;
                char t[1024], s1[1024], s2[700];

                if (strcmp(kind, "TITLE") == 0) {
                    uisan(rest, t, sizeof(t));
                    UI_PUT("c_%d_kind=title\nc_%d_is_title=1\nc_%d_text=%s\n", rc, rc, rc, t);
                } else if (strcmp(kind, "TEXT") == 0) {
                    uisan(rest, t, sizeof(t));
                    UI_PUT("c_%d_kind=text\nc_%d_is_text=1\nc_%d_text=%s\n", rc, rc, rc, t);
                } else if (strcmp(kind, "LINK") == 0) {
                    char *b2 = strchr(rest, '|');
                    if (b2) { *b2 = 0; snprintf(s2, sizeof(s2), "%s", b2 + 1); } else s2[0] = 0;
                    char url_sq[PATH_BUF * 2], lab_s[700];
                    uisan(s2[0] ? s2 : rest, lab_s, sizeof(lab_s));
                    shell_escape_squote(rest, url_sq, sizeof(url_sq));
                    UI_PUT("c_%d_kind=link\nc_%d_is_link=1\nc_%d_text=%s\n", rc, rc, rc, lab_s);
                    UI_PUT("c_%d_action='%s/ops/nb_write_go.sh' 'go' '%s'\n", rc, g_package_dir, url_sq);
                } else if (strcmp(kind, "IMG") == 0) {
                    char *b2 = strchr(rest, '|');
                    if (b2) { *b2 = 0; snprintf(s2, sizeof(s2), "%s", b2 + 1); } else s2[0] = 0;
                    uisan(rest, s1, sizeof(s1));        /* sprite dir */
                    char lab_s[700]; uisan(s2[0] ? s2 : " ", lab_s, sizeof(lab_s));
                    UI_PUT("c_%d_kind=img\nc_%d_is_media=1\nc_%d_sprite=%s\nc_%d_label=%s\n", rc, rc, rc, s1, rc, lab_s);
                } else if (strcmp(kind, "VIDEO") == 0) {
                    /* VIDEO|<sprite_dir>|<url>|<alt> */
                    char *b2 = strchr(rest, '|');
                    char vurl[1024] = "", valt[700] = "";
                    if (b2) {
                        *b2 = 0;
                        char *vu = b2 + 1;
                        char *b3 = strchr(vu, '|');
                        if (b3) { *b3 = 0; snprintf(valt, sizeof(valt), "%s", b3 + 1); }
                        snprintf(vurl, sizeof(vurl), "%s", vu);
                    }
                    uisan(rest, s1, sizeof(s1));
                    char lab_s[700]; uisan(valt[0] ? valt : "play", lab_s, sizeof(lab_s));
                    char url_sq[PATH_BUF * 2];
                    shell_escape_squote(vurl, url_sq, sizeof(url_sq));
                    UI_PUT("c_%d_kind=video\nc_%d_is_media=1\nc_%d_sprite=%s\nc_%d_label=%s\n", rc, rc, rc, s1, rc, lab_s);
                    UI_PUT("c_%d_action=ffplay -autoexit -loglevel error '%s'\n", rc, url_sq);
                } else {
                    continue;
                }
                rc++;
            }
            fclose(pf);
        }
        UI_PUT("content_count=%d\n", rc);
        UI_PUT("content_empty=%d\n", rc == 0 ? 1 : 0);
        UI_PUT("empty_msg=Ready - enter a URL above\n");
    }

#undef UI_PUT
    static char *g_last_ui = NULL;
    if (g_last_ui && strcmp(g_last_ui, buf) == 0) { free(buf); return; }
    free(g_last_ui);
    g_last_ui = buf;

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_ui_output_path);
    FILE *wf = fopen(tmp_path, "w");
    if (!wf) { fprintf(stderr, "network_browser_manager: cannot write %s\n", tmp_path); return; }
    fputs(buf, wf);
    fclose(wf);
    rename(tmp_path, g_ui_output_path);
}

/* REAL, NEW 2026-09-01 (ported from khtpm_open_hai_manager.c) - check
 * if the parent renderer process is still alive. Module processes
 * (launched via the renderer's generic <module> tag) should self-exit
 * when their parent dies. */
static int parent_still_alive(void) {
    if (!g_package_dir[0]) return 1;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/module_parent.pid", g_package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    int pid = 0;
    int got = fscanf(f, "%d", &pid);
    fclose(f);
    if (got != 1 || pid <= 0) return 1;
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno != ESRCH;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root> [pkg_dir] [ui]\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s/&.hq-apps/network", g_house);
    for (int ai = 2; ai < argc; ai++)
        if (strcmp(argv[ai], "ui") == 0) g_mode_ui = 1;

    char desktop[PATH_BUF];
    path_join(desktop, sizeof(desktop), g_house, "#.desktop");
    mkdir_p_local(desktop);
    mkdir_p_local(g_package_dir);
    path_join(g_request_path, sizeof(g_request_path), desktop, "network_browser_request.txt");
    path_join(g_page_state_path, sizeof(g_page_state_path), desktop, "network_browser_page.state.txt");
    path_join(g_status_path, sizeof(g_status_path), desktop, "network_browser_status.state.txt");
    path_join(g_back_path, sizeof(g_back_path), desktop, "network_browser_back.txt");
    path_join(g_forward_path, sizeof(g_forward_path), desktop, "network_browser_forward.txt");
    path_join(g_visit_log_path, sizeof(g_visit_log_path), desktop, "network_browser_history.log.txt");
    path_join(g_bookmark_path, sizeof(g_bookmark_path), desktop, "network_browser_bookmarks.txt");
    path_join(g_tabs_path, sizeof(g_tabs_path), desktop, "network_browser_tabs.txt");
    path_join(g_tabs_root, sizeof(g_tabs_root), desktop, "nb_tabs");
    mkdir_p_local(g_tabs_root);
    {
        char oldhist[PATH_BUF];
        struct stat st;
        path_join(oldhist, sizeof(oldhist), desktop, "network_browser_history.txt");
        if (stat(g_back_path, &st) != 0 && stat(oldhist, &st) == 0)
            rename(oldhist, g_back_path);
        copy_file_if_missing(g_back_path, g_visit_log_path);
    }

    /* REAL, NEW 2026-09-01 - .chtpm output path, same pattern as
     * khtpm_open_hai_manager.c's own g_chtpm_output_path setup */
    snprintf(g_chtpm_output_path, sizeof(g_chtpm_output_path), "%s/&.hq-apps/network/network-browser-hq.chtpm", g_house);
    snprintf(g_ui_output_path, sizeof(g_ui_output_path), "%s/#.desktop/network-browser-hq_ui.txt", g_house);

    char tmpdir[PATH_BUF];
    snprintf(tmpdir, sizeof(tmpdir), "%s/&.hq-apps/network/tmp", g_house);
    mkdir_p_local(tmpdir);
    path_join(g_tmp_html_path, sizeof(g_tmp_html_path), tmpdir, "fetch.html");
    path_join(g_curl_url_path, sizeof(g_curl_url_path), tmpdir, "curl.url.cfg");
    path_join(g_fetch_pid_path, sizeof(g_fetch_pid_path), tmpdir, "fetch.pid");
    path_join(g_js_script_path, sizeof(g_js_script_path), tmpdir, "page.js");
    path_join(g_js_effects_path, sizeof(g_js_effects_path), tmpdir, "js.effects.txt");
    snprintf(g_js_eval_path, sizeof(g_js_eval_path), "%s/ops/+x/nb_js_eval.+x", g_package_dir);
    snprintf(g_media_op_path, sizeof(g_media_op_path), "%s/ops/+x/nb_media_to_sprite.+x", g_package_dir);
    path_join(g_media_root, sizeof(g_media_root), desktop, "nb_sprites");
    mkdir_p_local(g_media_root);

    /* ensure the request file exists and is empty on startup - same
     * "never assume, always create" discipline khtpm_open_hai_manager.c uses. */
    { FILE *f = fopen(g_request_path, "a"); if (f) fclose(f); }
    publish_status("idle");

    /* Restore last tabs+snapshots if present. Never wipe live session files
     * (page.state, tabs.txt, nb_tabs/). Request file is only opened append so
     * a leftover go: is not invented; handle_request still consumes one line. */
    session_restore_on_start();

    write_chtpm_projection();

    for (;;) {
        handle_request();
        write_chtpm_projection();

        if (!parent_still_alive()) {
            fprintf(stderr, "network_browser_manager: parent renderer is gone - exiting\n");
            break;
        }
        usleep(300000);
    }
    return 0;
}
