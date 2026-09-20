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
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/file.h>

#include "nb_dom.h"

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
static char g_tmp_dom_path[PATH_BUF];
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
static char g_curl_cookie_path[PATH_BUF];  /* per-house Netscape jar shared by page/script/worker curls */
static int g_lock_fd = -1;                 /* single-instance flock fd (held for life) */
static char g_js_worker_path[PATH_BUF];
static char g_js_script_path[PATH_BUF];
static char g_js_style_path[PATH_BUF];   /* rung 7: concatenated page CSS */
static char g_media_op_path[PATH_BUF];
static char g_media_root[PATH_BUF];
static char g_video_player_path[PATH_BUF];
static char g_video_pump_path[PATH_BUF];
static char g_video_play_path[PATH_BUF];  /* V3 real-time op (+x/nb_video_play.+x) */
static char g_video_sess[PATH_BUF];   /* "" = no video running */
static int   g_video_pump_pid = -1;
static int   g_video_player_pid = -1;
static int   g_video_v3 = 0;   /* 1 = the running session is the V3 op */

/* One manager per house. flock(2) LOCK_EX|LOCK_NB on a lock file dies with
 * the process (no stale-pid handling needed) and is per-house, so separate
 * houses can still run separate managers. Prevents the racing-manager
 * pileup that corrupted live E2E runs. */
static void acquire_house_lock(const char *lock_path) {
    g_lock_fd = open(lock_path, O_WRONLY | O_CREAT, 0644);
    if (g_lock_fd < 0) return;             /* cannot lock: run anyway, don't wedged die */
    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr,
                "network_browser_manager: another instance already owns %s "
                "(kill it first) - exiting\n", lock_path);
        _exit(2);
    }
    if (ftruncate(g_lock_fd, 0) == 0) {
        char pid[32];
        int pn = snprintf(pid, sizeof(pid), "%ld\n", (long)getpid());
        (void)!write(g_lock_fd, pid, (size_t)pn);
    }
}

/* Browsers execute only classic-JS scripts (absent/empty type, or a
 * javascript MIME). Everything else — module, application/json,
 * application/ld+json, text/template, and any other custom type — is
 * skipped; the DOM parser already drops those nodes, so running them would
 * just emit WERR noise (and module syntax the engine can't parse anyway). */
static int script_type_skip(const char *tag, const char *tag_end) {
    const char *t = strcasestr_local(tag, "type=");
    if (!t || t >= tag_end) return 0;
    t += 5;
    char q = 0;
    if (*t == '"' || *t == '\'') { q = *t; t++; }
    const char *u = t;
    while (u < tag_end && *u && (q ? (*u != q)
                            : (*u != ' ' && *u != '\t' && *u != '>'))) u++;
    size_t n = (size_t)(u - t);
    if (n == 0) return 0;
    if (n >= 32) n = 32;
    char buf[64];
    memcpy(buf, t, n); buf[n] = 0;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    if (strstr(buf, "javascript")) return 0;
    return 1;
}

/* True if `tag` sits inside an unclosed <noscript> element. With scripting
 * enabled browsers neither render noscript content nor run its `<script>`
 * children; the DOM serializer already drops noscript outright. */
static int in_noscript_block(const char *root, const char *tag) {
    const char *p = root;
    int open = 0;
    while (p && p < tag) {
        const char *o = strcasestr_local(p, "<noscript");
        const char *c = strcasestr_local(p, "</noscript>");
        if (o && o >= tag) o = NULL;
        if (c && c >= tag) c = NULL;
        if (o && (!c || o < c)) { open = 1; p = o + 9; continue; }
        if (c && (!o || c < o)) { open = 0; p = c + 11; continue; }
        break;
    }
    return open;
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
        "curl -sL --max-time 8 -A 'Mozilla/5.0 (NNEST network-browser-hq)'"
        " -b '%s' -c '%s' -o '%s' -K '%s'",
        g_curl_cookie_path, g_curl_cookie_path, out_path, g_curl_url_path);
    return system(cmd) == 0;
}

/* Real SPAs ship a module graph of dozens of scripts (youtube home.html:
 * 42 <script> tags incl. the 10.8 MB kevlar_base bundle). The old 8-total /
 * 4-external caps truncated the graph before the app's own bootstrap ran, so
 * the page rendered only the static DOM. Raise them; the worker's page
 * loader (read_file_big) accepts up to 64 MB. */
#define NB_MAX_SCRIPTS     64
#define NB_MAX_EXT_SCRIPTS 48
static void collect_scripts(const char *html, const char *page_url, FILE *js_out, int *n_scripts) {
#define SCRIPT_BOUNDARY "/*nbjs-script-boundary*/"   /* per-script slices for the worker's document-order runner */
    const char *p = html;
    int n = 0, n_ext = 0;
    *n_scripts = 0;
    while (p && *p && n < NB_MAX_SCRIPTS) {
        const char *tag = strcasestr_local(p, "<script");
        if (!tag) break;
        if (tag[7] != '>' && tag[7] != ' ' && tag[7] != '\t' && tag[7] != '\n' && tag[7] != '/') {
            p = tag + 7;
            continue;
        }
        const char *gt = strchr(tag, '>');
        if (!gt) break;
        if (in_noscript_block(html, tag)) { p = tag + 7; continue; }
        const char *close = strcasestr_local(gt, "</script>");
        if (!close) break;
        if (script_type_skip(tag, gt)) {
            p = close + 9;
            continue;
        }
        const char *src = strcasestr_local(tag, "src=");
        if (src && src < gt) {
            if (n_ext >= NB_MAX_EXT_SCRIPTS) { p = close + 9; continue; }
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
                        fprintf(js_out, SCRIPT_BOUNDARY "\n");
                        while ((r = fread(buf, 1, sizeof(buf), ef)) > 0) fwrite(buf, 1, r, js_out);
                        fprintf(js_out, "\n");
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
            fprintf(js_out, SCRIPT_BOUNDARY "\n");
            fwrite(body, 1, (size_t)(close - body), js_out);
            fprintf(js_out, "\n");
            n++;
        }
        p = close + 9;
    }
    *n_scripts = n;
}

/* rung 7: concatenate the page's CSS into g_js_style_path — inline
 * <style> bodies in document order, then up to 4 <link rel="stylesheet">
 * resources resolved against the page URL (same cookie jar as page loads).
 * nb_css.c in the worker parses this; an empty file just yields no rules. */
static void collect_styles(const char *html, const char *page_url, FILE *css_out) {
    const char *p = html;
    while (p && *p) {
        const char *tag = strcasestr_local(p, "<style");
        if (!tag) break;
        if (tag[6] != '>' && tag[6] != ' ' && tag[6] != '\t' &&
            tag[6] != '\n' && tag[6] != '/') { p = tag + 6; continue; }
        const char *gt = strchr(tag, '>');
        if (!gt) break;
        if (in_noscript_block(html, tag)) { p = gt + 1; continue; }
        const char *close = strcasestr_local(gt, "</style>");
        if (!close) break;
        const char *body = gt + 1;
        if (close > body) fwrite(body, 1, (size_t)(close - body), css_out);
        fputc('\n', css_out);
        p = close + 8;
    }
    int n_link = 0;
    p = html;
    while (p && *p && n_link < 4) {
        const char *tag = strcasestr_local(p, "<link");
        if (!tag) break;
        if (tag[5] != '>' && tag[5] != ' ' && tag[5] != '\t' &&
            tag[5] != '\n' && tag[5] != '/') { p = tag + 5; continue; }
        const char *gt = strchr(tag, '>');
        if (!gt) break;
        const char *rel = strcasestr_local(tag, "rel=");
        if (!(rel && rel < gt)) { p = gt + 1; continue; }
        {
            const char *v = rel + 4;
            char q = 0;
            if (*v == '"' || *v == '\'') { q = *v; v++; }
            const char *vend = v;
            if (q) { while (*vend && *vend != q) vend++; }
            else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
            char rv[64];
            size_t rn = (size_t)(vend - v);
            if (rn >= sizeof(rv)) rn = sizeof(rv) - 1;
            memcpy(rv, v, rn);
            rv[rn] = 0;
            int is_sh = 0;
            const char *w = rv;
            while (*w && !is_sh) {
                const char *ww = w;
                while (*w && !isspace((unsigned char)*w)) w++;
                if ((size_t)(w - ww) == 10 && strncasecmp(ww, "stylesheet", 10) == 0)
                    is_sh = 1;
                while (*w && isspace((unsigned char)*w)) w++;
            }
            if (!is_sh) { p = gt + 1; continue; }
        }
        const char *href = strcasestr_local(tag, "href=");
        if (!(href && href < gt)) { p = gt + 1; continue; }
        const char *v = href + 5;
        char q = 0;
        if (*v == '"' || *v == '\'') { q = *v; v++; }
        const char *vend = v;
        if (q) { while (*vend && *vend != q) vend++; }
        else { while (*vend && !isspace((unsigned char)*vend) && *vend != '>') vend++; }
        char hs[PATH_BUF];
        size_t hn = (size_t)(vend - v);
        if (hn >= sizeof(hs)) hn = sizeof(hs) - 1;
        memcpy(hs, v, hn);
        hs[hn] = 0;
        html_decode_entities(hs);
        if (hs[0] && strncasecmp(hs, "data:", 5) != 0) {
            char resolved[PATH_BUF], ext[PATH_BUF];
            resolve_url(page_url, hs, resolved, sizeof(resolved));
            snprintf(ext, sizeof(ext), "%s.ext%d.css", g_js_style_path, n_link);
            if (curl_url_to_file(resolved, ext)) {
                FILE *ef = fopen(ext, "r");
                if (ef) {
                    char buf[4096];
                    size_t r;
                    fputc('\n', css_out);
                    while ((r = fread(buf, 1, sizeof(buf), ef)) > 0)
                        fwrite(buf, 1, r, css_out);
                    fputc('\n', css_out);
                    fclose(ef);
                    n_link++;
                }
            }
        }
        p = gt + 1;
    }
}

/* ---- NB-JS persistent worker lifecycle (worker plan §2B) ----
 * Step 2: the manager can spawn / LOAD / QUIT the resident worker, but
 * the page still renders through the existing one-shot eval + static
 * extractor. Spawn is lazy (only when a <script> exists); the worker is
 * QUIT+reaped on manager shutdown. Effects merge (step 4) later makes the
 * worker authoritative. RPC framing follows plan §4 (length-prefixed). */
static int g_worker_fd = -1;
static pid_t g_worker_pid = -1;
static char g_worker_render[65536];   /* step 4: last RENDER rows, or "" */
static char g_worker_err_path[PATH_BUF];  /* hygiene: worker stderr log */
static char g_console_path[PATH_BUF];     /* devtools console capture (NBW_CONSOLE) */
static long g_werr_tail = 0;              /* bytes of that log already surfaced */

/* rung-6 slice 2: the worker's pending NAV request, captured from a NAV
 * frame during worker_load and consumed by the main loop on the next tick
 * (kind: GO/REPLACE/RELOAD/BACK/FORWARD/ADDR; url for the navigations,
 * count is the step count for BACK/FORWARD). */
static char g_pending_nav_kind[16] = "";
static char g_pending_nav_url[PATH_BUF] = "";
static int  g_pending_nav_count = 1;

/* Step 4: overlay the worker's RENDER rows onto page.state.txt. The
 * post-JS DOM is authoritative, so content rows (TITLE/TEXT/LINK/IMG) are
 * replaced wholesale; non-content rows (URL|...) pass through unchanged.
 * A script with no DOM output leaves the static file intact. Returns 1
 * when the RENDER rows were actually merged. */
static int merge_render_rows(void) {
    if (!g_worker_render[0]) return 0;

    char tmp[PATH_BUF];
    FILE *pf = fopen(g_page_state_path, "r");
    FILE *wf = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!wf) {
        if (pf) fclose(pf);
        return 0;
    }
    if (pf) {
        char row[PATH_BUF];
        while (fgets(row, sizeof(row), pf)) {
            size_t L = strlen(row);
            while (L > 0 && (row[L-1]=='\n' || row[L-1]=='\r')) row[--L] = 0;
            if (strncmp(row, "TITLE|", 6) == 0 || strncmp(row, "TEXT|", 5) == 0 ||
                strncmp(row, "LINK|", 5) == 0 || strncmp(row, "IMG|", 4) == 0 ||
                strncmp(row, "MEDIA|", 6) == 0)
                continue;
            fprintf(wf, "%s\n", row);
        }
        fclose(pf);
    }
    fputs(g_worker_render, wf);
    if (g_worker_render[strlen(g_worker_render) - 1] != '\n') fputc('\n', wf);
    fclose(wf);
    atomic_commit(g_page_state_path, tmp);
    return 1;
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

/* D5 2026-09-11: when a media fetch/decode fails, still emit a tile so
 * the page doesn't silently blank. Writes a 64x64 sprite.csv grey tile
 * with a darker border (a visible "placeholder", never mistaken for the
 * real image). The <item label=> still carries the real alt text. */
static int write_placeholder_sprite(const char *out_dir) {
    mkdir_p_local(out_dir);
    char csv_path[PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", out_dir);
    FILE *f = fopen(csv_path, "w");
    if (!f) return 0;
    fprintf(f, "# resolution=64\n");
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            int edge = (x < 3 || y < 3 || x >= 61 || y >= 61);
            unsigned char r = edge ? 90 : 150, g = edge ? 90 : 150, b = edge ? 90 : 150, a = 255;
            if (!edge && ((x + y) % 32) < 8) { r = 135; g = 135; b = 135; }  /* subtle diagonal */
            fprintf(f, "%d,%d,%d,%d\n", r, g, b, a);
        }
    }
    fclose(f);
    struct stat st;
    return stat(csv_path, &st) == 0 && st.st_size > 20;
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
        if (!fetch_to_sprite(fetch_url, dir) && !write_placeholder_sprite(dir)) continue;
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

/* ---- Task C 2026-09-11: video-in-canvas V2 (wraith player) ----
 * One resident mechanism and no renderer C: nb_video_player (vendored
 * wraith) pre-extracts mp4 frames at 8fps and copies current_frame.png;
 * nb_video_pump converts the newest frame into m<N>/sprite.csv; the
 * renderer's hq_sprite() re-reads sprite.csv on mtime change, so the
 * existing VIDEO tile animates. V1 ffplay action stays as real-playback. */

static void video_reap(void) {
    if (g_video_player_pid > 0) {
        int st = 0;
        if (waitpid(g_video_player_pid, &st, WNOHANG) == g_video_player_pid) g_video_player_pid = -1;
    }
    if (g_video_pump_pid > 0) {
        int st = 0;
        if (waitpid(g_video_pump_pid, &st, WNOHANG) == g_video_pump_pid) g_video_pump_pid = -1;
    }
}

static void video_stop_all(void) {
    if (g_video_pump_pid < 0 && g_video_player_pid < 0 && !g_video_sess[0]) return;
    if (g_video_sess[0]) {
        /* tell the running session to stop. V2 path: player + pump write
         * into sess/session/video.control; V3 op (nb_video_play) reads
         * sess/video.control -- poke both, then reap. */
        {
            FILE *c = fopen(g_video_sess, "a");  /* ensure exists */
            if (c) fclose(c);
        }
        char cpath[PATH_BUF], cmd[PATH_BUF * 2];
        snprintf(cpath, sizeof(cpath), "%s/session/video.control", g_video_sess);
        {
            FILE *f = fopen(cpath, "w");
            if (f) { fprintf(f, "stop\n"); fclose(f); }
        }
        snprintf(cpath, sizeof(cpath), "%s/video.control", g_video_sess);
        {
            FILE *f = fopen(cpath, "w");
            if (f) { fprintf(f, "stop\n"); fclose(f); }
        }
        const char *poke = g_video_v3 && g_video_play_path[0] ? g_video_play_path : g_video_player_path;
        snprintf(cmd, sizeof(cmd), "'%s' --stop '%s' >/dev/null 2>&1", poke, g_video_sess);
        (void)system(cmd);
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_video_sess);
        (void)system(cmd);
    }
    if (g_video_pump_pid > 0) {
        kill(g_video_pump_pid, SIGTERM);
        video_reap();
    }
    if (g_video_player_pid > 0) {
        kill(g_video_player_pid, SIGTERM);
        video_reap();
    }
    g_video_v3 = 0;
}

static void video_start(const char *sprite_dir, const char *url) {
    video_stop_all();
    /* V3: single real-time op (libav decode + ALSA out). It resolves
     * youtube/http(s)/local itself and publishes surface.raw +
     * surface.receipt.txt into its own session dir; no pump, no ffplay
     * V2 pair. */
    if (g_video_play_path[0] && access(g_video_play_path, X_OK) == 0) {
        mkdir_p_local(g_video_sess);
        g_video_v3 = 1;
        g_video_player_pid = fork();
        if (g_video_player_pid == 0) {
            execl(g_video_play_path, g_video_play_path, url, g_video_sess, (char *)NULL);
            _exit(127);
        }
        if (g_video_player_pid < 0) { g_video_v3 = 0; g_video_player_pid = -1; return; }
        return;
    }
    /* V2 fallback: pre-extract + pump (only when the V3 op isn't built),
     * offline-specific sprite.csv 8fps path. */
    if (g_video_player_path[0] && g_video_pump_path[0] &&
        access(g_video_player_path, X_OK) == 0 && access(g_video_pump_path, X_OK) == 0) {
    } else {
        publish_status("error: video ops missing");
        return;
    }
    {
        struct stat st;
        if (stat(url, &st) != 0 || st.st_size < 100) {
            publish_status("error: video file missing");
            return;
        }
    }
    mkdir_p_local(g_video_sess);
    g_video_player_pid = fork();
    if (g_video_player_pid == 0) {
        execl(g_video_player_path, g_video_player_path, url, g_video_sess, (char *)NULL);
        _exit(127);
    }
    if (g_video_player_pid < 0) { g_video_player_pid = -1; return; }
    /* give the player a moment to create session before pump watches it */
    usleep(200000);
    g_video_pump_pid = fork();
    if (g_video_pump_pid == 0) {
        char fps[8];
        snprintf(fps, sizeof(fps), "8");
        execl(g_video_pump_path, g_video_pump_path, g_video_sess, sprite_dir, fps, (char *)NULL);
        _exit(127);
    }
    if (g_video_pump_pid < 0) g_video_pump_pid = -1;
}

static void video_start_if_page_has_video(void) {
    FILE *pf = fopen(g_page_state_path, "r");
    if (!pf) return;
    char sprite_dir[PATH_BUF] = "";
    char vurl[PATH_BUF] = "";
    char line[PATH_BUF + 512];
    while (fgets(line, sizeof(line), pf)) {
        /* VIDEO|<rel>|<url>|video */
        if (strncmp(line, "VIDEO|", 6) != 0) continue;
        char *rel = line + 6;
        char *b1 = strchr(rel, '|');
        if (!b1) continue;
        *b1 = 0;
        char *url = b1 + 1;
        char *b2 = strchr(url, '|');
        if (b2) *b2 = 0;
        if (!rel[0] || !url[0]) continue;
        snprintf(sprite_dir, sizeof(sprite_dir), "%s", rel);
        snprintf(vurl, sizeof(vurl), "%s", url);
        break;
    }
    fclose(pf);
    if (!sprite_dir[0] || !vurl[0]) { video_stop_all(); return; }
    /* V3 resolves local|http(s)|youtube itself; the V2 path stays
     * local-file-only as before. */
    char target[PATH_BUF];
    if (strncmp(vurl, "file://", 7) == 0) {
        snprintf(target, sizeof(target), "%s", vurl + 7);
    } else {
        snprintf(target, sizeof(target), "%s", vurl);
    }
    if (g_video_play_path[0] && access(g_video_play_path, X_OK) == 0) {
        video_start(sprite_dir, target);
        return;
    }
    if (strncmp(vurl, "http://", 7) == 0 || strncmp(vurl, "https://", 8) == 0) {
        video_stop_all();
        return;
    }
    video_start(sprite_dir, target);
}

#define WORKER_RECV_TIMEOUT_MS 3000   /* plan step 5: stall watchdog */

static void worker_err_tail(void);   /* defined below worker_close */
static int worker_send(const char *payload, size_t n) {
    if (g_worker_fd < 0) return 0;
    char lb[16];
    int ln = snprintf(lb, sizeof(lb), "%.6d\n", (int)n);
    if (write(g_worker_fd, lb, (size_t)ln) != ln) return 0;
    if (n && write(g_worker_fd, payload, n) != (ssize_t)n) return 0;
    return write(g_worker_fd, "\n", 1) == 1;
}

static int worker_recv_line_to(char *out, size_t cap, int timeout_ms) {
    if (g_worker_fd < 0) return 0;
    struct pollfd p = { g_worker_fd, POLLIN, 0 };
    int pr = poll(&p, 1, timeout_ms);
    if (pr <= 0) return 0;   /* worker stalled: caller closes + respawns */
    char lb[16]; size_t i = 0; char c;
    while (read(g_worker_fd, &c, 1) == 1) {
        if (c == '\n') break;
        if (i < sizeof(lb) - 1) lb[i++] = c;
    }
    lb[i] = 0;
    long n = strtol(lb, NULL, 10);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t got = 0;
    while (got < (size_t)n) {
        ssize_t r = read(g_worker_fd, out + got, (size_t)n - got);
        if (r <= 0) return 0;
        got += (size_t)r;
    }
    out[got] = 0;
    if (read(g_worker_fd, &c, 1) != 1) return 0;
    return 1;
}

static int worker_recv_line(char *out, size_t cap) {
    return worker_recv_line_to(out, cap, WORKER_RECV_TIMEOUT_MS);
}

/* A resident LOAD can spend a long quiet stretch evaluating one page slice
 * (the 11.9MB kevlar head alone is seconds) before its first RENDER/LIVE
 * frame; the interactive 3s watchdog would kill a healthy worker mid-LOAD.
 * The per-slice NB_EVAL_BUDGET (60s, set in worker_spawn) still bounds any
 * genuinely stuck slice, and a dead worker EOFs instantly rather than
 * timing out — so a generous LOAD quiet-budget is safe. */
#define WORKER_LOAD_QUIET_MS 90000

static void worker_close(void) {
    if (g_worker_fd >= 0) { close(g_worker_fd); g_worker_fd = -1; }
    if (g_worker_pid > 0) {
        kill(g_worker_pid, SIGKILL);          /* plan step 5: no strays */
        int st; waitpid(g_worker_pid, &st, 0);
        g_worker_pid = -1;
        /* hygiene: surface whatever the worker wrote to its stderr since
         * the last close (boot WERR| lines, page errors) on OUR stderr so
         * the module log carries the cause without a post-mortem hunt. */
        worker_err_tail();
    }
}

/* Print the worker's stderr log lines that haven't been surfaced yet,
 * one per [worker]-prefixed manager-stderr line. */
static void worker_err_tail(void) {
    if (!g_worker_err_path[0]) return;
    FILE *f = fopen(g_worker_err_path, "rb");
    if (!f) return;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
    long total = ftell(f);
    if (total <= g_werr_tail) { fclose(f); g_werr_tail = total; return; }
    if (fseek(f, g_werr_tail, SEEK_SET) != 0) { fclose(f); return; }
    static char buf[16384];
    size_t got = fread(buf, 1, sizeof(buf) - 1, f);
    long end = ftell(f);
    fclose(f);
    if (end > g_werr_tail) g_werr_tail = end;
    buf[got] = 0;
    char *line = buf;
    for (char *p = buf; *p; p++) {
        if (*p == '\n') {
            *p = 0;
            if (*line) fprintf(stderr, "[worker] %s\n", line);
            line = p + 1;
        }
    }
}

/* Spawn the worker on first need. Child keeps socketpair end as stdin/stdout. */
static void worker_spawn(void) {
    if (g_worker_fd >= 0) return;
    if (!g_js_worker_path[0]) return;
    FILE *probe = fopen(g_js_worker_path, "r");
    if (!probe) return;
    fclose(probe);

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(sv[1], STDIN_FILENO);
        dup2(sv[1], STDOUT_FILENO);
        close(sv[0]); close(sv[1]);
        /* hygiene: worker stderr → per-house log (append). The worker's
         * WERR| boot lines / page errors land here and are surfaced on
         * our stderr by worker_err_tail() when the worker closes. */
        int er = open(g_worker_err_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (er >= 0) { dup2(er, STDERR_FILENO); close(er); }
        /* rung-6 slice 2: hand the worker its own cookie jar under this
         * house's #.desktop, so cross-LOAD cookies persist per browser
         * (not the shared ~/.config/nbjs/ fallback). */
        char jar[PATH_BUF];
        snprintf(jar, sizeof(jar), "%s/#.desktop/nb_cookies.txt", g_house);
        setenv("NB_COOKIES_FILE", jar, 1);
        snprintf(jar, sizeof(jar), "%s/#.desktop/nb_localstorage.txt", g_house);
        setenv("NB_LOCALSTORAGE_FILE", jar, 1);
        snprintf(jar, sizeof(jar), "%s/#.desktop/nb_curl_cookies.txt", g_house);
        setenv("NB_CURL_COOKIES_FILE", jar, 1);
        /* devtools console EVAL: the worker streams console.* lines (and
         * the eval> result lines) into this capture file; the projection
         * renders its tail into the [console] panel. Append-mode on the
         * worker side keeps history across respawns. */
        char con[PATH_BUF];
        snprintf(con, sizeof(con), "%s/#.desktop/network_browser_console.txt", g_house);
        setenv("NBW_CONSOLE", con, 1);
        /* row-31 live incident (2026-09-19): the resident worker's page-slice
         * budget defaults to EVAL_BUDGET_SEC=2s; the real 10.8MB kevlar head
         * occasionally crosses it, sigalrm() _exit()s the whole worker
         * mid-LOAD, and the manager is left with a dead worker — static rows
         * merge, then "eval" reports "no page loaded". The one-shot row-31
         * path already overrides NB_EVAL_BUDGET for the same bundle; give the
         * RESIDENT browser a generous-but-bounded per-slice ceiling too. */
        setenv("NB_EVAL_BUDGET", "60", 1);
        execl(g_js_worker_path, g_js_worker_path, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) { close(sv[0]); close(sv[1]); return; }
    close(sv[1]);
    g_worker_fd = sv[0];
    g_worker_pid = pid;
}

/* Ask the worker to run a page. Reads the optional RENDER frame (step 4)
 * into g_worker_render[] then the STATUS frame. Returns 1 on "STATUS ok". */
static int worker_load(const char *js_path, const char *dom_path,
                       const char *href, const char *title,
                       const char *css_path) {
    worker_spawn();
    if (g_worker_fd < 0) return 0;
    char payload[8192];
    int n = snprintf(payload, sizeof(payload), "LOAD\n%s\n%s\n%s\n%s\n%s",
                     js_path ? js_path : "", dom_path ? dom_path : "",
                     href ? href : "", title ? title : "",
                     css_path ? css_path : "");
    if (!worker_send(payload, (size_t)n)) { worker_close(); return 0; }

    g_worker_render[0] = 0;
    g_pending_nav_kind[0] = 0; g_pending_nav_url[0] = 0; g_pending_nav_count = 1;
    char resp[65536];
    for (;;) {
        if (!worker_recv_line_to(resp, sizeof(resp), WORKER_LOAD_QUIET_MS)) { worker_close(); return 0; }
        if (strncmp(resp, "LIVE|", 5) == 0) continue;   /* drain keepalive */
        if (strncmp(resp, "RENDER\n", 7) == 0) {
            size_t rn = strlen(resp + 7);
            if (rn + 1 < sizeof(g_worker_render))
                memcpy(g_worker_render, resp + 7, rn + 1);
            continue;   /* wait for STATUS next */
        }
        if (strncmp(resp, "NAV\n", 4) == 0) {
            /* rung-6 slice 2: NAV\n<kind>\n<url-or-count>\n — the page asked
             * to navigate (history.forward() etc. or location.assign()).
             * Stash it; the main loop runs it through the same do_fetch /
             * back/forward stacks as a go:/back: request. */
            char *f1 = resp + 4;
            char *n1 = strchr(f1, '\n');
            if (n1) {
                size_t l = (size_t)(n1 - f1);
                if (l >= sizeof(g_pending_nav_kind)) l = sizeof(g_pending_nav_kind) - 1;
                memcpy(g_pending_nav_kind, f1, l);
                g_pending_nav_kind[l] = 0;
                if (n1[1]) {
                    char v[PATH_BUF];
                    size_t vl = strlen(n1 + 1);
                    if (vl >= sizeof(v)) vl = sizeof(v) - 1;
                    memcpy(v, n1 + 1, vl);
                    v[vl] = 0;
                    char *nl = strchr(v, '\n');
                    if (nl) *nl = 0;
                    if (strcmp(g_pending_nav_kind, "BACK") == 0 ||
                        strcmp(g_pending_nav_kind, "FORWARD") == 0) {
                        g_pending_nav_count = atoi(v);
                    } else {
                        snprintf(g_pending_nav_url, sizeof(g_pending_nav_url), "%s", v);
                    }
                }
            }
            continue;   /* keep reading until STATUS */
        }
        /* hygiene: worker-side JS/boot diagnostics (ERROR| rows) go to the
         * module log too, and don't abort the STATUS read mid-frame. */
        if (strncmp(resp, "ERROR|", 6) == 0) {
            fprintf(stderr, "[worker] %s\n", resp);
            continue;
        }
        return strncmp(resp, "STATUS ok", 9) == 0;
    }
}

/* devtools console EVAL: run <js> against the resident page heap. The
 * worker echoes source/result into the NBW_CONSOLE capture (the manager's
 * write_ui_projection renders its tail into the [console] panel), re-emits
 * RENDER rows (overlaid onto page.state.txt via merge_render_rows) and
 * stashes any NAV the snippet triggered (consumed next main-loop tick,
 * same as a page-triggered NAV). Returns 1 on "STATUS ok". */
static int worker_eval(const char *js) {
    worker_spawn();
    if (g_worker_fd < 0) { publish_status("error: no worker"); return 0; }
    char payload[8192];
    int n = snprintf(payload, sizeof(payload), "EVAL\n%s", js ? js : "");
    if (!worker_send(payload, (size_t)n)) { worker_close(); return 0; }

    char resp[65536];
    for (;;) {
        /* A console command can run the page's own handlers (input/click),
         * which may then drain a long quiet stretch — same generous budget
         * as LOAD. A dead worker still EOFs immediately. */
        if (!worker_recv_line_to(resp, sizeof(resp), WORKER_LOAD_QUIET_MS)) { worker_close(); return 0; }
        if (strncmp(resp, "LIVE|", 5) == 0) continue;   /* drain keepalive */
        if (strncmp(resp, "RENDER\n", 7) == 0) {
            size_t rn = strlen(resp + 7);
            if (rn + 1 < sizeof(g_worker_render))
                memcpy(g_worker_render, resp + 7, rn + 1);
            continue;   /* wait for STATUS next */
        }
        if (strncmp(resp, "NAV\n", 4) == 0) {
            char *f1 = resp + 4;
            char *n1 = strchr(f1, '\n');
            if (n1) {
                size_t l = (size_t)(n1 - f1);
                if (l >= sizeof(g_pending_nav_kind)) l = sizeof(g_pending_nav_kind) - 1;
                memcpy(g_pending_nav_kind, f1, l);
                g_pending_nav_kind[l] = 0;
                if (n1[1]) {
                    char v[PATH_BUF];
                    size_t vl = strlen(n1 + 1);
                    if (vl >= sizeof(v)) vl = sizeof(v) - 1;
                    memcpy(v, n1 + 1, vl);
                    v[vl] = 0;
                    char *nl = strchr(v, '\n');
                    if (nl) *nl = 0;
                    if (strcmp(g_pending_nav_kind, "BACK") == 0 ||
                        strcmp(g_pending_nav_kind, "FORWARD") == 0) {
                        g_pending_nav_count = atoi(v);
                    } else {
                        snprintf(g_pending_nav_url, sizeof(g_pending_nav_url), "%s", v);
                    }
                }
            }
            continue;
        }
        if (strncmp(resp, "ERROR|", 6) == 0) {
            fprintf(stderr, "[worker] %s\n", resp);
            continue;
        }
        return strncmp(resp, "STATUS ok", 9) == 0;
    }
}

static void worker_quit(void) {
    if (g_worker_fd >= 0) {
        worker_send("QUIT", 4);
        worker_close();
    }
}

static void run_page_scripts(const char *html, const char *url, const char *title) {
    if (!g_js_worker_path[0]) return;
    FILE *probe = fopen(g_js_worker_path, "r");
    if (!probe) return;
    fclose(probe);

    FILE *js = fopen(g_js_script_path, "w");
    if (!js) return;
    int n = 0;
    collect_scripts(html, url, js, &n);
    fclose(js);
    if (n <= 0) return;

    /* rung 7: ship the page's CSS to the worker (empty file = no rules,
     * the worker's nb_css_parse degrades gracefully). */
    FILE *sf = fopen(g_js_style_path, "w");
    if (sf) {
        collect_styles(html, url, sf);
        fclose(sf);
    }

    /* NB-JS worker authoritative: LOAD the page into the resident worker
     * and, when it reports RENDER rows, overlay them onto page.state.txt
     * (document.title=, el.textContent=, appendChild, ...). The legacy
     * one-shot nb_js_eval effects path is gone — the worker is the single
     * DOM writer. A worker that fails leaves the static DOM in place. */
    worker_load(g_js_script_path, g_tmp_dom_path, url, title, g_js_style_path);
    (void)merge_render_rows();
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

/* REAL FIX 2026-09-13, direct live report: "making new tab in network
 * populated search bar with same old address... cant we make sure it
 * populates blank? or populates with the address of last page in
 * event of loading history." Root cause, found live while testing
 * h-ai-lab's own cli_io fix: write_ui_projection()'s addr_label
 * always DOES carry the right value (blank on tab_new, the real
 * stored URL on tab_switch/history) - the manager's own tab state was
 * never wrong. But khtpm_core_render.c's kh_cli_io_reload() restores
 * a cli_io's input_buffer from <package_dir>/cli_io_state.txt
 * UNCONDITIONALLY on every reparse, keyed by target_id ("address"
 * here) - overwriting content="${addr_label}"'s seed with whatever
 * was last MANUALLY TYPED, forever, for the life of that file. Once
 * a human ever types a URL by hand, every future tab_new()/
 * tab_switch() keeps showing that same stale typed value, regardless
 * of what addr_label says, since the renderer's own restore always
 * runs after the seed and always wins. Real fix: the manager
 * proactively keeps cli_io_state.txt's own "address" key in sync with
 * g_current_url at the exact two real moments it changes for a
 * reason OTHER than the user typing (new tab, tab switch) - same
 * read-modify-write shape khtpm_core_render.c's own
 * default_cli_io_save() already uses for this exact file, so this
 * isn't a new convention, it's applying the existing one from the
 * other real writer's side. Typing a URL in the SAME tab needs no
 * sync call - the renderer's own per-keystroke save already keeps
 * that path consistent. */
static void sync_address_cli_io_state(const char *url) {
    if (!g_package_dir[0]) return;
    char path[PATH_BUF], tmp[PATH_BUF];
    snprintf(path, sizeof(path), "%s/cli_io_state.txt", g_package_dir);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    char lines[64][PATH_BUF];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        char line[PATH_BUF];
        while (n < 64 && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            if (strcmp(line, "address") == 0) continue; /* real value replaced below */
            snprintf(lines[n], sizeof(lines[n]), "%s=%s", line, eq + 1);
            n++;
        }
        fclose(f);
    }
    FILE *w = fopen(tmp, "w");
    if (!w) return;
    for (int i = 0; i < n; i++) fprintf(w, "%s\n", lines[i]);
    fprintf(w, "address=%s\n", url ? url : "");
    fclose(w);
    rename(tmp, path);
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
            sync_address_cli_io_state(g_current_url);
            return;
        }
        write_blank_page_state();
        g_current_url[0] = 0;
        publish_status("idle");
    } else {
        publish_status("ready");
    }
    sync_address_cli_io_state(g_current_url);
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
    sync_address_cli_io_state(g_current_url);
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
            sync_address_cli_io_state(g_current_url);
            return;
        }
        write_blank_page_state();
        g_current_url[0] = 0;
        publish_status("idle");
    } else {
        publish_status("ready");
    }
    sync_address_cli_io_state(g_current_url);
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
               "-b", g_curl_cookie_path, "-c", g_curl_cookie_path,
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

/* REAL, NEW 2026-09-11, direct live report ("history nav buttons were
 * supposed to delete on backspace, and have delete all"). g_visit_log_
 * path is a plain append-only, newest-LAST file; write_ui_projection()'s
 * own history block (real header comment right where h_%d_del_action
 * is published) walks it backwards to show newest-first at display
 * index 0 - `display_idx` here is that SAME index, translated back to
 * the real file line index the same way that loop does
 * (`hi = hn - 1 - display_idx`), so the entry actually removed is
 * exactly the one the user saw and backspaced. Read-all/skip-one/
 * rewrite-all, same real shape every other small state file in this
 * house uses for a delete (no temp-file atomicity here on purpose -
 * this file is already rewritten wholesale on every real edit
 * elsewhere in this codebase, e.g. default_cli_io_save()). */
static void history_delete_at(int display_idx) {
    if (display_idx < 0) return;
    char lines[256][PATH_BUF];
    int hn = 0;
    FILE *f = fopen(g_visit_log_path, "r");
    if (!f) return;
    while (hn < 256 && fgets(lines[hn], PATH_BUF, f)) {
        size_t L = strlen(lines[hn]);
        while (L > 0 && (lines[hn][L-1] == '\n' || lines[hn][L-1] == '\r')) lines[hn][--L] = 0;
        if (lines[hn][0]) hn++;
    }
    fclose(f);
    int hi = hn - 1 - display_idx;
    if (hi < 0 || hi >= hn) return; /* stale index (a concurrent visit shifted it) - safe no-op, not a crash */
    FILE *w = fopen(g_visit_log_path, "w");
    if (!w) return;
    for (int i = 0; i < hn; i++) if (i != hi) fprintf(w, "%s\n", lines[i]);
    fclose(w);
}

/* "delete all" - direct request, same turn as history_delete_at() above. */
static void history_clear_all(void) {
    FILE *w = fopen(g_visit_log_path, "w");
    if (w) fclose(w);
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

/* REAL, NEW 2026-09-12 (NETWORK-BROWSER-VIDEO-V3-DESIGN.md §3, V3-B
 * "YouTube URL" probe): a bare video URL - youtu.be/..., a /watch?v=,
 * a direct .mp4/.webm, or the yt: shortcut - never produces a <video>
 * tag in fetched HTML (YouTube's player is JS-driven, and a raw media
 * URL isn't HTML at all), so extract_and_publish() could never emit
 * the VIDEO| row video_start_if_page_has_video() keys on. The has_canvas
 * renderer path needed a real equivalent of the image-classifier ahead
 * of it in do_fetch() (looks_image_bytes()/publish_direct_image()<--the
 * exact pattern this mirrors): classify the URL FIRST, and when it is
 * one, publish a VIDEO|<sprite_dir>|<url>|video page.state row that has
 * no poster sprite (sprite_dir is only the V2 fallback's album art -
 * V3 ignores it, surface.raw is the real frame) and let the shared V3
 * start path run. No per-app hack: it is the same page-state contract
 * every <video> parse already fills. */
static int url_is_video(const char *url) {
    if (!url || !url[0]) return 0;
    if (strstr(url, "youtu.be/") != NULL) return 1;
    if (strstr(url, "youtube.com/watch") != NULL || strstr(url, "youtube.com/shorts") != NULL)
        return 1;
    if (strncmp(url, "yt:", 3) == 0) return 1;
    /* direct media: http(s) URL ending in a video extension (query string
     * allowed). Local file paths reach video_start the same way once the
     * page parser hands them a VIDEO row. */
    const char *q = strchr(url, '?');
    const char *sl = strrchr(url, '/');
    const char *dot = strrchr(url, '.');
    if (!dot || (q && dot > q)) return 0;
    if (sl && dot < sl) return 0;
    const char *ext = dot + 1;
    return strcasecmp(ext, "mp4") == 0 ||
           strcasecmp(ext, "webm") == 0 ||
           strcasecmp(ext, "m4v") == 0 ||
           strcasecmp(ext, "mov") == 0 ||
           strcasecmp(ext, "mkv") == 0;
}

static int publish_direct_video(const char *url) {
    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!out) return 0;
    const char *slash = strrchr(url, '/');
    const char *leaf = (slash && slash[1]) ? slash + 1 : "video";
    fprintf(out, "URL|%s\nTITLE|%s\n", url, leaf);
    fprintf(out, "VIDEO|%s/#.desktop/nb_sprites/video|%s|video\n", g_house, url);
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

/* Phase 1 step 1 (NB-JS worker plan §3/§7): parse the fetched HTML into
 * nb_dom.c's tolerant node tree and serialize it out to fetch.dom (the
 * future worker's input). Produced-but-unused this commit: no behavior
 * change, the manager still uses its linear extractor for rendering.
 * Writes atomically like the rest of the manager. Never blocks or fails
 * a fetch on parse trouble - if serialization fails we just leave the
 * previous fetch.dom (or none) in place. */
static void write_fetch_dom(const char *html, size_t n) {
    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_tmp_dom_path, tmp, sizeof(tmp));
    if (!out) return;
    NbNode *root = nb_parse_html(html, n);
    if (root) {
        long nodes = nb_serialize(out, root);
        fclose(out);
        if (nodes > 0) atomic_commit(g_tmp_dom_path, tmp);
        else unlink(tmp);
        nb_node_free(root);
    } else {
        fclose(out);
        unlink(tmp);
    }
}

/* =====================================================================
 * V4 2026-09-12: YouTube watch-page ingest.
 * "Make the watch page work as normal" - instead of the V3-B bare-video
 * fast path, a youtube.com/watch URL now fetches its page HTML and
 * parses the `ytInitialData` JSON the page itself renders from, the
 * same data YouTube uses to draw the watch page: the clean title, a
 * meta line (channel / subscribers / views / date), the real
 * description, and the end-screen related-video grid (the initial
 * HTML carries 12 endScreenVideoRenderer entries with videoId + thumb;
 * the related *sidebar* is NOT in the initial response).
 *
 * All rows ride the SHARED page.state contract the generic projection/
 * collection layers already handle - nothing renderer-specific, nothing
 * per-app:
 *   main video    MEDIA|V  -> collect_page_media -> VIDEO| (drives V3)
 *   related thumb MEDIA|I + following LINK row   (the chhtml + UI
 *   writers both turn an IMG tailed by a LINK into a tile whose action
 *   is the go:<watch url> - click plays the related video)
 * A related click lands back in do_fetch as go:watch?v=<id>, which is a
 * watch page again, so it renders its own metadata + video. Exactly
 * Chrome/Firefox behavior.
 * ===================================================================== */

static int youtube_watch_page(const char *url) {
    return url && strstr(url, "youtube.com/watch") != NULL;
}

/* ---- tiny path-based JSON reader over a span (no alloc, no tree) --
 * js_path_val() walks paths like
 *   contents.twoColumnWatchNextResults.results.results.contents[0]
 *     .videoPrimaryInfoRenderer.title.runs[0].text
 * (array index = suffix bracket on a key component) and returns the
 * START of the value at that path (NULL on miss). Objects/arrays are
 * scanned to the current level by skipping sibling values; quotes and
 * backslash escapes are honored inside strings. Assumes the same brace
 * matched JSON sub-span the walker is handed (the ytInitialData blob). */

static const char *js_ws(const char *p) {
    if (!p) return NULL;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *js_str_end(const char *p) {
    if (!p || *p != '"') return NULL;
    p++;
    while (*p) {
        if (*p == '\\') { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return NULL;
}

/* skip one JSON value starting at p (quote/depth aware); return AFTER it */
static const char *js_value_end(const char *p) {
    p = js_ws(p);
    if (!p) return NULL;
    if (*p == '{' || *p == '[') {
        int depth = 0;
        char q = 0;
        const char *s = p;
        while (*s) {
            if (q) {
                if (*s == q) q = 0;
                else if (*s == '\\') s++;
            } else if (*s == '"') {
                q = '"';
            } else if (*s == '{' || *s == '[') {
                depth++;
            } else if (*s == '}' || *s == ']') {
                depth--;
                if (depth <= 0) return s + 1;
            }
            s++;
        }
        return NULL;
    }
    if (*p == '"') return js_str_end(p);
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* inside object `{"...":val,...}` - value start for `key`, or NULL.
 * A non-matching key is skipped via js_value_end, so a duplicate key
 * at the same level just wins; nested same-named keys never match. */
static const char *js_obj_val(const char *obj, const char *key) {
    if (!obj || *obj != '{') return NULL;
    const char *p = obj + 1;
    for (;;) {
        p = js_ws(p);
        if (!p || !*p || *p == '}') return NULL;
        const char *ke = js_str_end(p);
        if (!ke) return NULL;
        size_t kl = strlen(key);
        if (kl == (size_t)(ke - p - 2) && memcmp(p + 1, key, kl) == 0) {
            const char *c = js_ws(ke);
            return (c && *c == ':') ? js_ws(c + 1) : NULL;
        }
        p = js_ws(ke);
        if (!p || *p != ':') return NULL;
        p = js_value_end(p + 1);
        if (!p) return NULL;
        if (*p == ',') { p++; continue; }
        return NULL;
    }
}

/* array: value start of element (0-based), or NULL when out of range */
static const char *js_arr_val(const char *arr, int idx) {
    if (!arr || *arr != '[') return NULL;
    const char *p = arr + 1;
    for (int n = 0;; n++) {
        p = js_ws(p);
        if (!p || !*p || *p == ']') return NULL;
        if (n == idx) return p;
        p = js_value_end(p);
        if (!p) return NULL;
        p = js_ws(p);
        if (*p == ',') { p++; continue; }
        return NULL;
    }
}

static const char *js_path_val(const char *v, const char *path) {
    const char *p = path;
    while (v && *p) {
        char key[64];
        int k = 0;
        int idx = -1;
        while (*p && *p != '.' && *p != '[') {
            if (k < (int)sizeof(key) - 1) key[k++] = *p;
            p++;
        }
        key[k] = 0;
        if (*p == '[') {
            idx = 0;
            p++;
            while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); p++; }
            if (*p == ']') p++;
        }
        if (*p == '.') p++;
        if (idx >= 0) {
            /* key component with an index suffix: obj[key] THEN [idx] */
            if (key[0]) { v = js_obj_val(v, key); if (!v) return NULL; v = js_ws(v); }
            v = js_arr_val(v, idx);
            if (!v) return NULL;
        } else {
            v = js_obj_val(v, key);
            if (!v) return NULL;
        }
        v = js_ws(v);
    }
    return v;
}

/* fetch a string value at path; unescape JSON escapes incl \uXXXX
 * (nbsp -> space, bullet -> '-'). Returns 1 on found+non-empty. */
static int js_get_str(const char *json, const char *path, char *out, size_t outsz) {
    out[0] = 0;
    const char *v = js_path_val(json, path);
    if (!v || *v != '"') return 0;
    const char *se = js_str_end(v);
    if (!se) return 0;
    const char *q = v + 1;
    size_t o = 0;
    while (q < se - 1 && o + 1 < outsz) {
        if (*q == '\\' && q + 1 < se) {
            q++;
            switch (*q) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                case 'r': out[o++] = '\r'; break;
                case 'b': out[o++] = '\b'; break;
                case 'f': out[o++] = '\f'; break;
                case '/': out[o++] = '/'; break;
                case '"': out[o++] = '"'; break;
                case '\\': out[o++] = '\\'; break;
                case 'u': {
                    unsigned u = 0;
                    int h;
                    for (h = 0; h < 4 && q + 1 < se - 1; h++) {
                        q++;
                        int d = 0;
                        if (*q >= '0' && *q <= '9') d = *q - '0';
                        else if (*q >= 'a' && *q <= 'f') d = *q - 'a' + 10;
                        else if (*q >= 'A' && *q <= 'F') d = *q - 'A' + 10;
                        else break;
                        u = (u << 4) | (unsigned)d;
                    }
                    if (u == 0xA0) u = 0x20;        /* nbsp -> space */
                    if (u == 0x2022) u = 0x2D;      /* bullet -> '-' */
                    if (u < 0x80) {
                        if (u >= 0x20) out[o++] = (char)u;
                    } else if (u < 0x800) {
                        out[o++] = (char)(0xC0 | (u >> 6));
                        out[o++] = (char)(0x80 | (u & 0x3F));
                    } else {
                        out[o++] = (char)(0xE0 | (u >> 12));
                        out[o++] = (char)(0x80 | ((u >> 6) & 0x3F));
                        out[o++] = (char)(0x80 | (u & 0x3F));
                    }
                    break;
                }
                default: out[o++] = *q; break;
            }
            q++;
        } else {
            out[o++] = *q++;
        }
    }
    out[o] = 0;
    return o > 0;
}

/* pull the `v=` watch id out of a youtube.com/watch URL */
static void yt_vid_from_url(const char *url, char *out, size_t outsz) {
    out[0] = 0;
    const char *q = strchr(url, '?');
    if (!q) return;
    const char *vp = strstr(q, "v=");
    if (!vp) return;
    vp += 2;
    size_t n = 0;
    while (vp[n] && vp[n] != '&') n++;
    if (n >= outsz) n = outsz - 1;
    memcpy(out, vp, n);
    out[n] = 0;
}

/* strip characters hostile to the page.state row format */
static void yt_sanitize(char *s) {
    char *w = s, *r = s;
    while (*r) {
        if (*r == '|') { r++; continue; }
        if (*r == '\n' || *r == '\r' || *r == '\t') *w++ = ' ';
        else *w++ = *r;
        r++;
    }
    *w = 0;
}

#define YT_WRAP 78
/* one paragraph (no stray \n) -> TEXT| rows, wrapped on spaces */
static void yt_emit_para(FILE *out, char *para, int *lines, int maxlines) {
    collapse_ws(para);
    if (!para[0]) return;
    char *s = para;
    while (*s && *lines < maxlines) {
        size_t L = strlen(s);
        if (L <= YT_WRAP) { fprintf(out, "TEXT|%s\n", s); (*lines)++; break; }
        size_t cut = YT_WRAP;
        while (cut > (size_t)YT_WRAP / 2 && s[cut] && s[cut] != ' ') cut--;
        if (s[cut] == ' ') {
            s[cut] = 0;
            fprintf(out, "TEXT|%s\n", s);
            s += cut + 1;
        } else {
            char save = s[YT_WRAP];
            s[YT_WRAP] = 0;
            fprintf(out, "TEXT|%s\n", s);
            s[YT_WRAP] = save;
            s += YT_WRAP;
        }
        (*lines)++;
    }
}

/* break js-decoded description (paragraphs on \n) into TEXT| rows */
static void yt_emit_desc(FILE *out, const char *desc) {
    static char buf[YT_WRAP * 200 + 8];
    size_t dn = strlen(desc);
    if (dn >= sizeof(buf) - 1) dn = sizeof(buf) - 1;
    memcpy(buf, desc, dn);
    buf[dn] = 0;
    int lines = 0;
    char *par = buf;
    char *p = buf;
    while (p && *p && lines < 120) {
        if (*p == '\n') {
            *p = 0;
            yt_emit_para(out, par, &lines, 120);
            par = p + 1;
        }
        p++;
    }
    if (par && *par) yt_emit_para(out, par, &lines, 120);
}

/* write the full watch-page row set; returns 1 when the fetched HTML
 * carried ytInitialData (a real watch page). */
static int ingest_youtube_watch(const char *html, const char *url, FILE *out) {
    const char *mark = strstr(html, "var ytInitialData");
    if (!mark) return 0;
    const char *eq = strchr(mark, '=');
    if (!eq) return 0;
    const char *j0 = eq + 1;
    while (*j0 == ' ' || *j0 == '\t') j0++;
    if (*j0 != '{') return 0;
    int depth = 0;
    char q = 0;
    const char *je = j0;
    for (; *je; je++) {
        if (q) {
            if (*je == q) q = 0;
            else if (*je == '\\') je++;
        } else if (*je == '"') {
            q = '"';
        } else if (*je == '{') {
            depth++;
        } else if (*je == '}') {
            depth--;
            if (depth == 0) { je++; break; }
        }
    }
    if (depth != 0) return 0;       /* malformed blob - fall back to generic */
    (void)je;                        /* the walker self-bounds on object/value closers */

    static const char *P =
        "contents.twoColumnWatchNextResults.results.results.contents";
    char title[600], chan[600], subs[300], views[300], date[300];
    char desc[YT_WRAP * 200];
    char pat[900];

    snprintf(pat, sizeof(pat), "%s[0].videoPrimaryInfoRenderer.title.runs[0].text", P);
    js_get_str(j0, pat, title, sizeof(title));
    snprintf(pat, sizeof(pat), "%s[0].videoPrimaryInfoRenderer.viewCount.videoViewCountRenderer.viewCount.simpleText", P);
    js_get_str(j0, pat, views, sizeof(views));
    snprintf(pat, sizeof(pat), "%s[0].videoPrimaryInfoRenderer.dateText.simpleText", P);
    js_get_str(j0, pat, date, sizeof(date));
    snprintf(pat, sizeof(pat), "%s[1].videoSecondaryInfoRenderer.owner.videoOwnerRenderer.title.runs[0].text", P);
    js_get_str(j0, pat, chan, sizeof(chan));
    snprintf(pat, sizeof(pat), "%s[1].videoSecondaryInfoRenderer.owner.videoOwnerRenderer.subscriberCountText.simpleText", P);
    js_get_str(j0, pat, subs, sizeof(subs));
    snprintf(pat, sizeof(pat), "%s[1].videoSecondaryInfoRenderer.attributedDescription.content", P);
    js_get_str(j0, pat, desc, sizeof(desc));

    char vid[32];
    yt_vid_from_url(url, vid, sizeof(vid));
    if (!vid[0] && !title[0] && !chan[0]) return 0;   /* not a watch page's shape */
    if (!title[0] && chan[0]) snprintf(title, sizeof(title), "%s", chan);

    yt_sanitize(title);
    yt_sanitize(chan);
    yt_sanitize(subs);
    yt_sanitize(views);
    yt_sanitize(date);

    fprintf(out, "URL|%s\n", url);
    if (vid[0]) {
        /* main video first: collect_page_media sprites the poster and
         * emits the VIDEO| row video_start_if_page_has_video() keys on */
        fprintf(out, "MEDIA|V|https://www.youtube.com/watch?v=%s|https://i.ytimg.com/vi/%s/hqdefault.jpg\n", vid, vid);
    }
    fprintf(out, "TITLE|%s\n", title[0] ? title : "YouTube");
    {
        char meta[900];
        size_t mo = 0;
        const char *parts[4] = { chan, subs, views, date };
        for (int pi = 0; pi < 4; pi++) {
            if (!parts[pi][0]) continue;
            if (mo) {
                if (mo < sizeof(meta) - 3) { meta[mo++] = 0xC2; meta[mo++] = 0xB7; meta[mo++] = ' '; }
            }
            size_t pl = strlen(parts[pi]);
            if (pl >= sizeof(meta) - mo) pl = sizeof(meta) - mo - 1;
            memcpy(meta + mo, parts[pi], pl);
            mo += pl;
        }
        meta[mo] = 0;
        if (meta[0]) fprintf(out, "TEXT|%s\n", meta);
    }
    yt_emit_desc(out, desc);

    /* end-screen related videos: 12 thumbnails from the player overlay */
    for (int i = 0; i < 12; i++) {
        char rp[900], rp2[1000];
        snprintf(rp, sizeof(rp),
            "playerOverlays.playerOverlayRenderer.endScreen.watchNextEndScreenRenderer.results[%d].endScreenVideoRenderer", i);
        char rvid[32], rtitle[700];
        snprintf(rp2, sizeof(rp2), "%s.videoId", rp);
        if (!js_get_str(j0, rp2, rvid, sizeof(rvid))) break;
        snprintf(rp2, sizeof(rp2), "%s.title.simpleText", rp);
        if (!js_get_str(j0, rp2, rtitle, sizeof(rtitle))) snprintf(rtitle, sizeof(rtitle), "Video");
        yt_sanitize(rtitle);
        char shortlab[48];
        snprintf(shortlab, sizeof(shortlab), "%s", rtitle);
        shortlab[40] = 0;
        if (shortlab[0]) {
            fprintf(out, "MEDIA|I|https://i.ytimg.com/vi/%s/hqdefault.jpg|%s\n", rvid, shortlab);
            fprintf(out, "LINK|https://www.youtube.com/watch?v=%s|%s\n", rvid, rtitle);
        }
    }

    return 1;
}

static void do_fetch(const char *url_in, int record_history) {
    char url[PATH_BUF];
    if (g_current_url[0]) resolve_url(g_current_url, url_in, url, sizeof(url));
    else snprintf(url, sizeof(url), "%s", url_in);

    publish_status("loading");
    write_chtpm_projection(); /* live X11 window must show loading before curl blocks */

    /* REAL, NEW 2026-09-12 (V3-B probe, NETWORK-BROWSER-VIDEO-V3-DESIGN.md
     * §3): a bare video URL - youtu.be/..., /watch?v=, direct .mp4/.webm,
     * yt: shortcut - never yields a <video> tag in fetched HTML (YouTube's
     * player is JS-driven; a raw media URL isn't HTML at all), so the page
     * parser could never emit the VIDEO| row that starts the V3 op.
     * Classify the URL itself and publish a poster-less VIDEO| row
     * (sprite_dir is only V2 fallback art; V3 ignores it and blits
     * surface.raw). Checked BEFORE curl so a multi-MB JS-heavy page is
     * never downloaded. Mirrors the image classifier's success shape. */
    if (url_is_video(url) && !youtube_watch_page(url)) {
        publish_status("ready");
        write_chtpm_projection();
        if (publish_direct_video(url)) {
            video_start_if_page_has_video();
            if (record_history && g_current_url[0] && strcmp(g_current_url, url) != 0)
                stack_push(g_back_path, g_current_url);
            snprintf(g_current_url, sizeof(g_current_url), "%s", url);
            visit_log_append(url);
            tab_after_fetch_ok(url);
            write_chtpm_projection();
            return;
        }
        publish_status("error: video start failed");
        return;
    }

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

    /* REAL, NEW 2026-09-12 (V3-B probe, NETWORK-BROWSER-VIDEO-V3-DESIGN.md
     * §3): a bare video URL - youtu.be/..., /watch?v=, direct .mp4/.webm,
     * yt: shortcut - never yields a <video> tag in fetched HTML, so the
     * generic page parser below could never emit the VIDEO| row that
     * starts the V3 op. Classify the URL itself and publish a poster-less
     * VIDEO| row (sprite_dir is only V2 fallback art; V3 ignores it).
     * Mirror the image path's commit/history/status shape exactly. */
    if (url_is_video(url) && !youtube_watch_page(url)) {
        if (publish_direct_video(url)) {
            video_start_if_page_has_video();
            if (record_history && g_current_url[0] && strcmp(g_current_url, url) != 0)
                stack_push(g_back_path, g_current_url);
            snprintf(g_current_url, sizeof(g_current_url), "%s", url);
            visit_log_append(url);
            tab_after_fetch_ok(url);
            publish_status("ready");
            write_chtpm_projection();
            return;
        }
    }

    /* V4 2026-09-12: youtube.com/watch page - parse ytInitialData into
     * CORE rows (main video + title + meta + desc), then let the shared
     * layers finish: collect_page_media turns the MEDIA rows into
     * sprite VIDEO|/IMG| rows (fetching the poster/related thumbs),
     * video_start_if_page_has_video() starts the V3 op on the main row. */
    if (youtube_watch_page(url)) {
        char tmpy[PATH_BUF];
        FILE *outy = atomic_open(g_page_state_path, tmpy, sizeof(tmpy));
        int yt_ok = 0;
        if (outy) {
            yt_ok = ingest_youtube_watch(html, url, outy);
            fclose(outy);
            if (yt_ok) {
                atomic_commit(g_page_state_path, tmpy);
                collect_page_media(html, url);
                video_start_if_page_has_video();
                goto do_fetch_publish_done;
            } else {
                unlink(tmpy);            /* bad/odd watch shape - generic parse */
            }
        }
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

    write_fetch_dom(html, n);

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
        video_start_if_page_has_video();
    }

    /* shared post-content tail (YT branch jumps here past the generic
     * parse; 4chan keeps its own inline copy) */
do_fetch_publish_done:
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

    /* devtools console EVAL: the address bar writes "go:<typed>", so a
     * typed "eval:<js>" arrives as "go:eval:<js>" here (nb_write_go.sh
     * keeps the go: prefix). Must be matched BEFORE the generic go: branch. */
    if (strncmp(line, "go:eval:", 8) == 0) {
        publish_status(worker_eval(line + 8) ? "ready" : "eval error");
        (void)merge_render_rows();
    } else if (strncmp(line, "eval:", 5) == 0) {
        publish_status(worker_eval(line + 5) ? "ready" : "eval error");
        (void)merge_render_rows();
    } else if (strncmp(line, "go:", 3) == 0) {
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
    } else if (strncmp(line, "delhist:", 8) == 0) {
        history_delete_at(atoi(line + 8));
    } else if (strcmp(line, "clearhist:") == 0 || strcmp(line, "clearhist") == 0) {
        history_clear_all();
    } else if (strncmp(line, "tab:", 4) == 0) {
        tab_switch(atoi(line + 4));
    } else if (strcmp(line, "newtab:") == 0 || strcmp(line, "newtab") == 0) {
        tab_new();
    } else if (strcmp(line, "closetab:") == 0 || strcmp(line, "closetab") == 0) {
        tab_close_current();
    } else if (strcmp(line, "video:stop") == 0) {
        video_stop_all();
        publish_status("video stopped");
    } else if (strcmp(line, "video:pause") == 0 && g_video_sess[0]) {
        char cmd[PATH_BUF * 2];
        const char *poke = g_video_v3 && g_video_play_path[0] ? g_video_play_path : g_video_player_path;
        snprintf(cmd, sizeof(cmd), "'%s' --pause '%s' >/dev/null 2>&1", poke, g_video_sess);
        (void)system(cmd);
        publish_status("video paused");
    } else if (strcmp(line, "video:resume") == 0 && g_video_sess[0]) {
        char cmd[PATH_BUF * 2];
        const char *poke = g_video_v3 && g_video_play_path[0] ? g_video_play_path : g_video_player_path;
        snprintf(cmd, sizeof(cmd), "'%s' --resume '%s' >/dev/null 2>&1", poke, g_video_sess);
        (void)system(cmd);
        publish_status("video playing");
    }
}

/* rung-6 slice 2: run the worker's pending NAV request (set when page JS
 * called location.assign/replace/reload, history.back/forward/go, or
 * pushState/replaceState). Duplicates the request-file contract so JS
 * navigation and toolbar navigation funnel through the SAME do_fetch /
 * back/forward stacks. GO = link-like (pushes current onto Back + clears
 * Forward, visits the log). REPLACE = navigate without a history entry.
 * RELOAD = re-fetch current. BACK/FORWARD walk the file stacks (count>1
 * for history.go(n)). ADDR = address-bar only, no fetch (pushState). */
static void consume_pending_nav(void) {
    if (!g_pending_nav_kind[0]) return;
    char kind[16], url[PATH_BUF];
    int count = g_pending_nav_count;
    snprintf(kind, sizeof(kind), "%s", g_pending_nav_kind);
    snprintf(url, sizeof(url), "%s", g_pending_nav_url);
    g_pending_nav_kind[0] = 0; g_pending_nav_url[0] = 0; g_pending_nav_count = 1;
    if (count < 1) count = 1;
    if (count > 8) count = 8;

    if (strcmp(kind, "GO") == 0) {
        if (!url[0]) return;
        stack_clear(g_forward_path);
        do_fetch(url, 1);
    } else if (strcmp(kind, "REPLACE") == 0) {
        if (!url[0]) return;
        do_fetch(url, 0);
    } else if (strcmp(kind, "RELOAD") == 0) {
        if (g_current_url[0]) do_fetch(g_current_url, 0);
        else publish_status("error: nothing to reload");
    } else if (strcmp(kind, "BACK") == 0) {
        for (int i = 0; i < count; i++) {
            char prev[PATH_BUF];
            if (!stack_pop(g_back_path, prev, sizeof(prev))) break;
            if (g_current_url[0]) stack_push(g_forward_path, g_current_url);
            do_fetch(prev, 0);
        }
    } else if (strcmp(kind, "FORWARD") == 0) {
        for (int i = 0; i < count; i++) {
            char next[PATH_BUF];
            if (!stack_pop(g_forward_path, next, sizeof(next))) break;
            if (g_current_url[0]) stack_push(g_back_path, g_current_url);
            do_fetch(next, 0);
        }
    } else if (strcmp(kind, "ADDR") == 0) {
        /* pushState/replaceState: update what the address bar shows, no
         * fetch and no history-stack change (the projection picks the new
         * URL up on this tick's write_chtpm_projection()). */
        if (!url[0]) return;
        snprintf(g_current_url, sizeof(g_current_url), "%s", url);
        if (g_tab_count > 0 && g_tab_current >= 0 && g_tab_current < g_tab_count)
            snprintf(g_tabs[g_tab_current].url, sizeof(g_tabs[g_tab_current].url), "%s", url);
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

/* devtools console: keep the NBW_CONSOLE capture file bounded. Trims to the
 * last <maxbytes> at a line boundary; called from write_ui_projection so the
 * projection loop itself enforces the cap without the worker knowing. */
static void trim_tail_file(const char *path, size_t maxbytes) {
    struct stat st;
    if (!path || stat(path, &st) != 0 || (size_t)st.st_size <= maxbytes) return;
    FILE *in = fopen(path, "r");
    if (!in) return;
    long skip = (long)(st.st_size - (long)maxbytes);
    if (fseek(in, skip, SEEK_SET) != 0) { fclose(in); return; }
    int c;
    while ((c = fgetc(in)) != EOF && c != '\n') {}   /* align to a line start */
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.trim", path);
    FILE *out = fopen(tmp, "w");
    if (!out) { fclose(in); return; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    rename(tmp, path);
}

/* REAL, NEW 2026-09-14 (video V4 "Nav row with play/pause + progress"
 * request) - read the V3 op's live playhead (surface.playhead.txt:
 * pos=<sec>\ndur=<sec>, rewritten by the op on every frame) and state
 * (video.state: playing/paused/stopped) so write_ui_projection() can
 * publish a real progress/play-pause strip that M OVES while the video
 * plays. Both are a plain fopen+scan of byte-small files every manager
 * tick (300ms) - cheap, and reparse-visible to the renderer immediately. */
static int video_read_playhead(const char *sess, double *pos, double *dur) {
    *pos = 0.0; *dur = 0.0;
    if (!sess || !sess[0]) return 0;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/surface.playhead.txt", sess);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "pos=", 4) == 0) *pos = atof(line + 4);
        else if (strncmp(line, "dur=", 4) == 0) *dur = atof(line + 4);
    }
    fclose(f);
    return 1;
}

static const char *video_read_state(const char *sess) {
    static char st[16];
    st[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/video.state", sess);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(st, sizeof(st), f)) {
            size_t L = strlen(st);
            while (L > 0 && (st[L-1] == '\n' || st[L-1] == '\r')) st[--L] = '\0';
        }
        fclose(f);
    }
    return st;
}

/* "0:07" / "0:18" mm:ss readout for the bar's centered time label. */
static void video_format_time(char *out, size_t n, double secs) {
    int s = (int)(secs + 0.5);
    if (s < 0) s = 0;
    snprintf(out, n, "%d:%02d", s / 60, s % 60);
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

    /* REVERTED 2026-09-11, direct instruction ("its echoing them twice
     * ... just treat this pipeline same as the other cli-io's clear on
     * new, 1 render only"). The 2026-09-11 fix just above this (now
     * removed) echoed cli_io_state.txt's own live-typed address= back
     * as addr_label, to fight the OLD destroy-and-rebuild reparse
     * wiping input_buffer on every tick. That's now unnecessary AND
     * actively harmful: khtpm_core_render.c's reparse_chtpm_if_changed()
     * (CHTPM-INCREMENTAL-REPARSE-DESIGN.md, live for every window as of
     * this same session) already preserves a cli_io's input_buffer
     * across reparse by never destroying the Elem in the first place -
     * no manager-side echo needed at all. Having BOTH the renderer's
     * own live-typed input_buffer AND the manager re-injecting the same
     * text via label= is two sources of truth for one field - the real
     * cause of the reported double-echo. Every other cli_io in the
     * house (open-hai's composer, chat-hai's, text-edit-hq's editor)
     * has a STATIC label/content, never manager-projected once armed -
     * this now matches that same one-source-of-truth shape: addr_label
     * is purely the loaded page's URL, exactly like it was before any
     * of this session's cli_io investigation started. */
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
            /* REAL, NEW 2026-09-11, direct live report ("history nav
             * buttons were supposed to delete on backspace, and have
             * delete all") - same real, generic backspace_action
             * capability every other deletable list row in the house
             * uses (open-hai's own session rows: "id like to add
             * backspace to delete... instead of making all those
             * delete spots"). Indexed by the DISPLAY position (shown,
             * 0=newest) - history_delete_at() below does the newest-
             * first-to-file-line-index translation, matching exactly
             * how this same loop walks hlines[] backwards to build
             * that same display order. */
            UI_PUT("h_%d_del_action='%s/ops/nb_write_delhist.sh' 'delhist' '%d'\n", shown, g_package_dir, shown);
            shown++;
        }
        UI_PUT("n_hist=%d\n", shown);
        UI_PUT("no_hist=%d\n", shown == 0 ? 1 : 0);
        /* "delete all" - direct request, same turn as backspace-to-
         * delete above. */
        UI_PUT("clear_hist_action='%s/ops/nb_write_clearhist.sh' 'clearhist'\n", g_package_dir);
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
            /* in-memory rows so an IMG depleted by an adjacent LINK (the
             * watch-page related-tile pattern) reads far enough ahead. */
            enum { NB_UI_ROWS_MAX = 128 };
            char (*rows)[PATH_BUF + 512] = malloc(sizeof(*rows) * NB_UI_ROWS_MAX);
            if (!rows) { fclose(pf); pf = 0; }
            if (!pf) { UI_PUT("content_count=0\ncontent_empty=1\nempty_msg=Ready - enter a URL above\n"); }
            else {
            int nrow = 0;
            while (nrow < NB_UI_ROWS_MAX && fgets(rows[nrow], sizeof(rows[0]), pf)) nrow++;
            fclose(pf);
            for (int ri = 0; ri < nrow && rc < 400; ri++) {
                char *line = rows[ri];
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
                    /* V4 2026-09-12: an IMG immediately tailed by a LINK
                     * row is the tile's action (watch-page related videos
                     * arrive as IMG+LINK pairs) - emit the go: and consume
                     * the LINK, mirroring the chhtml writer's img+link
                     * run pairing so both projections stay clickable. */
                    if (ri + 1 < nrow && strncmp(rows[ri + 1], "LINK|", 5) == 0) {
                        char *lnext = rows[ri + 1] + 5;
                        char *ubar = strchr(lnext, '|');
                        if (ubar) *ubar = 0;
                        char url_sq[PATH_BUF * 2];
                        shell_escape_squote(lnext, url_sq, sizeof(url_sq));
                        UI_PUT("c_%d_action='%s/ops/nb_write_go.sh' 'go' '%s'\n", rc, g_package_dir, url_sq);
                        ri++;
                    }
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
                    if (g_video_v3 && g_video_sess[0]) {
                        /* V3: live canvas row. The canvas sprite is the op's
                         * surface.raw path; kh_draw_canvas reads dims from the
                         * sibling surface.receipt.txt (frame_w/h). */
                        char raw_s[PATH_BUF];
                        snprintf(raw_s, sizeof(raw_s), "%s/surface.raw", g_video_sess);
                        UI_PUT("c_%d_kind=video\nc_%d_is_media=1\nc_%d_is_canvas=1\nc_%d_sprite=%s\nc_%d_label=%s\n", rc, rc, rc, rc, raw_s, rc, lab_s);
                        /* V4 (2026-09-14, "Nav row with play/pause +
                         * progress"): TWO extra content rows under the
                         * canvas, both show=-carried by the static xhtpm's
                         * own play/bar drop-in rows:
                         *   rc+1 = play/pause toggle (label = the ACTION
                         *          shown: pause when playing, play when
                         *          paused; nb_video_cmd.sh reads video.state
                         *          to decide which control to write)
                         *   rc+2 = generic <bar> progress strip - value/max
                         *          in centiseconds (player's float seconds
                         *          x100), centered mm:ss label, seek onClick
                         *          with a literal %FRAC the RENDERER replaces
                         *          with the 0.0..1.0 click fraction (generic
                         *          <bar> capability, see Elem's bar_max
                         *          comment in khtpm_render_core.c). */
                        double ph_pos = 0.0, ph_dur = 0.0;
                        video_read_playhead(g_video_sess, &ph_pos, &ph_dur);
                        const char *vst = video_read_state(g_video_sess);
                        int is_playing = (strcmp(vst, "playing") == 0);
                        /* ▌▌ = pause glyph (U+258C x2), ▶ = play (U+25B6):
                         * both in DejaVu Sans, the only text-glyph fallback
                         * the shared draw already uses. Holdings render the
                         * ACTION on the button (standard media convention:
                         * clicking the shown glyph does that thing). */
                        const char *play_label = is_playing
                            ? "\xE2\x96\x8C\xE2\x96\x8C"
                            : "\xE2\x96\xB6";
                        char toc_cmd[PATH_BUF * 2];
                        snprintf(toc_cmd, sizeof(toc_cmd),
                                 "'%s/ops/nb_video_cmd.sh' 'toggle' '%s'",
                                 g_package_dir, g_video_sess);
                        UI_PUT("c_%d_is_play=1\nc_%d_play_label=%s\nc_%d_play_action=%s\n",
                               rc + 1, rc + 1, play_label, rc + 1, toc_cmd);
                        int cs_pos = (int)(ph_pos * 100);
                        int cs_dur = (int)(ph_dur * 100);
                        char t1[16], t2[16], bar_text[48];
                        video_format_time(t1, sizeof(t1), ph_pos);
                        video_format_time(t2, sizeof(t2), ph_dur);
                        snprintf(bar_text, sizeof(bar_text), "%s / %s", t1, t2);
                        char seek_cmd[PATH_BUF * 2];
                        snprintf(seek_cmd, sizeof(seek_cmd),
                                 "'%s/ops/nb_video_cmd.sh' 'seek' '%s' %%FRAC",
                                 g_package_dir, g_video_sess);
                        UI_PUT("c_%d_is_bar=1\nc_%d_bar_value=%d\nc_%d_bar_max=%d\nc_%d_bar_text=%s\nc_%d_bar_action=%s\n",
                               rc + 2, rc + 2, cs_pos, rc + 2, cs_dur,
                               rc + 2, bar_text, rc + 2, seek_cmd);
                        rc += 2; /* consumed indexes rc+1 and rc+2; final
                                  * rc++ below closes at rc+3 (canvas + 2) */
                    } else {
                        char url_sq[PATH_BUF * 2];
                        shell_escape_squote(vurl, url_sq, sizeof(url_sq));
                        UI_PUT("c_%d_kind=video\nc_%d_is_media=1\nc_%d_sprite=%s\nc_%d_label=%s\n", rc, rc, rc, s1, rc, lab_s);
                        UI_PUT("c_%d_action=ffplay -autoexit -loglevel error '%s'\n", rc, url_sq);
                    }
                } else {
                    continue;
                }
                rc++;
            }
            free(rows);
            }
        }
        UI_PUT("content_count=%d\n", rc);
        UI_PUT("content_empty=%d\n", rc == 0 ? 1 : 0);
        UI_PUT("empty_msg=Ready - enter a URL above\n");
    }

    /* devtools console - the worker's NBW_CONSOLE capture tail (console.*
     * lines + eval: source/result echoes). Ring of the last 200 lines. */
    {
        if (g_console_path[0]) trim_tail_file(g_console_path, 262144);
        char *clines[200];
        int ci = 0;
        FILE *cf = g_console_path[0] ? fopen(g_console_path, "r") : NULL;
        if (cf) {
            char line[1400];
            while (fgets(line, sizeof(line), cf)) {
                size_t L = strlen(line);
                while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
                if (!line[0]) continue;
                char *d = strdup(line);
                if (!d) continue;
                if (ci == 200) {           /* drop oldest, keep last 199 */
                    free(clines[0]);
                    memmove(clines, clines + 1, sizeof(char *) * 199);
                    ci = 199;
                }
                clines[ci++] = d;
            }
            fclose(cf);
        }
        for (int i = 0; i < ci; i++) {
            char s[1500];
            uisan(clines[i], s, sizeof(s));
            UI_PUT("con_%d_text=%s\n", i, s);
            free(clines[i]);
        }
        UI_PUT("n_console=%d\n", ci);
        UI_PUT("no_console=%d\n", ci == 0 ? 1 : 0);
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
    signal(SIGPIPE, SIG_IGN);   /* plan step 5: dying worker writes must not kill us */
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
    path_join(g_worker_err_path, sizeof(g_worker_err_path), desktop, "network_browser_worker.err.log");
    path_join(g_console_path, sizeof(g_console_path), desktop, "network_browser_console.txt");
    {
        char lockpath[PATH_BUF];
        path_join(lockpath, sizeof(lockpath), desktop, "network_browser_manager.lock");
        acquire_house_lock(lockpath);
    }
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
    path_join(g_tmp_dom_path, sizeof(g_tmp_dom_path), tmpdir, "fetch.dom");
    path_join(g_curl_url_path, sizeof(g_curl_url_path), tmpdir, "curl.url.cfg");
    path_join(g_curl_cookie_path, sizeof(g_curl_cookie_path), desktop, "nb_curl_cookies.txt");
    path_join(g_fetch_pid_path, sizeof(g_fetch_pid_path), tmpdir, "fetch.pid");
    path_join(g_js_script_path, sizeof(g_js_script_path), tmpdir, "page.js");
    path_join(g_js_style_path, sizeof(g_js_style_path), tmpdir, "style.css");
    snprintf(g_js_worker_path, sizeof(g_js_worker_path), "%s/ops/+x/nb_js_worker.+x", g_package_dir);
    snprintf(g_media_op_path, sizeof(g_media_op_path), "%s/ops/+x/nb_media_to_sprite.+x", g_package_dir);
    path_join(g_media_root, sizeof(g_media_root), desktop, "nb_sprites");
    mkdir_p_local(g_media_root);
    snprintf(g_video_player_path, sizeof(g_video_player_path), "%s/ops/+x/nb_video_player.+x", g_package_dir);
    snprintf(g_video_pump_path, sizeof(g_video_pump_path), "%s/ops/+x/nb_video_pump.+x", g_package_dir);
    snprintf(g_video_play_path, sizeof(g_video_play_path), "%s/ops/+x/nb_video_play.+x", g_package_dir);
    snprintf(g_video_sess, sizeof(g_video_sess), "%s/&.hq-apps/network/tmp/nb_video0", g_house);
    video_stop_all(); /* stale session from a previous run */

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
        consume_pending_nav();
        write_chtpm_projection();

        if (!parent_still_alive()) {
            fprintf(stderr, "network_browser_manager: parent renderer is gone - exiting\n");
            video_stop_all();
            worker_quit();
            break;
        }
        video_reap();
        usleep(300000);
    }
    return 0;
}
