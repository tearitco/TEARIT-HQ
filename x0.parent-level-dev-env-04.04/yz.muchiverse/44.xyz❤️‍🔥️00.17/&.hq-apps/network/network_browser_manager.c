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
 *   #.desktop/network_browser_status.state.txt
 *     one line: "idle" | "loading" | "ready" | "error: <detail>"
 *
 * Consumes #.desktop/network_browser_action.txt, one pending line at a
 * time, truncated back to empty after handling (same real contract as
 * khtpm_hq_manager.c's own action file):
 *   go:<url>   - fetch <url> (resolved against the current page's URL
 *                if it looks relative), publish the state files above
 *
 * Usage: network_browser_manager.+x <house_root>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define PATH_BUF 4352

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
#define PAGE_BUF_MAX (2 * 1024 * 1024) /* real, generous cap - real pages this manager will actually be pointed at are small; a huge page is truncated, not a crash */
#define MAX_LINES 4096

static char g_house[PATH_BUF];
static char g_action_path[PATH_BUF];
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
     * not a full spec implementation (deliberately out of scope). */
    char *r = s, *w = s;
    while (*r) {
        if (*r == '&') {
            if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; continue; }
            if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; continue; }
            if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; continue; }
            if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; continue; }
            if (strncmp(r, "&#39;", 5) == 0 || strncmp(r, "&apos;", 6) == 0) {
                *w++ = '\''; r += (r[2] == '3') ? 5 : 6; continue;
            }
            if (strncmp(r, "&nbsp;", 6) == 0) { *w++ = ' '; r += 6; continue; }
        }
        *w++ = *r++;
    }
    *w = '\0';
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
    const char *p = html;
    int line_count = 0;

    #define FLUSH_LINE() do { \
        if (linelen > 0) { \
            line[linelen] = '\0'; \
            html_decode_entities(line); \
            collapse_ws(line); \
            if (line[0] && line_count < MAX_LINES) { fprintf(out, "TEXT|%s\n", line); line_count++; } \
            linelen = 0; \
        } \
    } while (0)

    while (*p) {
        if (*p == '<') {
            /* skip script/style bodies entirely - never visible text */
            if (strncasecmp(p, "<script", 7) == 0 || strncasecmp(p, "<style", 6) == 0) {
                const char *close = strcasestr_local(p, strncasecmp(p, "<script", 7) == 0 ? "</script>" : "</style>");
                p = close ? close + (strncasecmp(p, "<script", 7) == 0 ? 9 : 8) : p + strlen(p);
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
                if (href[0] && href[0] != '#' && strncasecmp(href, "javascript:", 11) != 0) {
                    char resolved[PATH_BUF];
                    resolve_url(url, href, resolved, sizeof(resolved));
                    fprintf(out, "LINK|%s|%s\n", resolved, text[0] ? text : resolved);
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

static void do_fetch(const char *url_in) {
    char url[PATH_BUF];
    if (g_current_url[0]) resolve_url(g_current_url, url_in, url, sizeof(url));
    else snprintf(url, sizeof(url), "%s", url_in);

    publish_status("loading");

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
        "curl -sL --max-time 12 -A 'Mozilla/5.0 (NNEST network-browser-hq)' -o '%s' '%s'",
        g_tmp_html_path, url);
    int rc = system(cmd);

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

    char tmp[PATH_BUF];
    FILE *out = atomic_open(g_page_state_path, tmp, sizeof(tmp));
    if (!out) { publish_status("error: could not write page state"); return; }
    extract_and_publish(html, url, out);
    fclose(out);
    atomic_commit(g_page_state_path, tmp);

    snprintf(g_current_url, sizeof(g_current_url), "%s", url);
    publish_status("ready");
}

static void handle_action_request(void) {
    FILE *f = fopen(g_action_path, "r");
    if (!f) return;
    char line[PATH_BUF];
    int got = (fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    if (!got) return;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (!line[0]) return;

    /* clear immediately - same "truncate so it doesn't re-fire" contract
     * as khtpm_hq_manager.c's own handle_action_request(). */
    FILE *cf = fopen(g_action_path, "w");
    if (cf) fclose(cf);

    if (strncmp(line, "go:", 3) == 0) {
        do_fetch(line + 3);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root>\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);

    char desktop[PATH_BUF];
    path_join(desktop, sizeof(desktop), g_house, "#.desktop");
    path_join(g_action_path, sizeof(g_action_path), desktop, "network_browser_action.txt");
    path_join(g_page_state_path, sizeof(g_page_state_path), desktop, "network_browser_page.state.txt");
    path_join(g_status_path, sizeof(g_status_path), desktop, "network_browser_status.state.txt");

    char tmpdir[PATH_BUF];
    snprintf(tmpdir, sizeof(tmpdir), "%s/&.hq-apps/network/tmp", g_house);
    mkdir_p_local(tmpdir);
    path_join(g_tmp_html_path, sizeof(g_tmp_html_path), tmpdir, "fetch.html");

    /* ensure the action file exists and is empty on startup - same
     * "never assume, always create" discipline khtpm_hq_manager.c uses. */
    { FILE *f = fopen(g_action_path, "a"); if (f) fclose(f); }
    publish_status("idle");

    for (;;) {
        handle_action_request();
        usleep(300000);
    }
    return 0;
}
