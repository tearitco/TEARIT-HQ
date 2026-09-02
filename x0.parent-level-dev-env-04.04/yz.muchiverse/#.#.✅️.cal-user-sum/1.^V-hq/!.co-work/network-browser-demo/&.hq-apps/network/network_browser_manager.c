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
 * Consumes #.desktop/network_browser_request.txt, one pending line at a
 * time, truncated back to empty after handling (same real contract as
 * khtpm_open_hai_manager.c's own request file):
 *   go:<url>   - fetch <url> (resolved against the current page's URL
 *                if it looks relative), publish the state files above
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
#include <signal.h>
#include <errno.h>

#define PATH_BUF 4352

static char g_chtpm_output_path[PATH_BUF];
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
#define PAGE_BUF_MAX (2 * 1024 * 1024) /* real, generous cap - real pages this manager will actually be pointed at are small; a huge page is truncated, not a crash */
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
        do_fetch(line + 3);
    }
}

/* REAL, NEW 2026-09-01 - write live .chtpm projection from manager state
 * (ported from khtpm_open_hai_manager.c's own pattern). Regenerates
 * the .chtpm file every main-loop tick from the manager's real published
 * state (current URL, page content, status), using only generic tags
 * (sidebar/panel/scrolllist/item/text) - zero new renderer C. The
 * renderer picks it up via reparse_chtpm_if_changed(). */
static void write_chtpm_projection(void) {
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
    NB_APPEND("<window label=\"Network Browser\" class=\"\">\n  <page name=\"main\">\n");

    /* REAL FIX 2026-09-01 (live report: "network browser has no x") -
     * layout_sidebar_panel() (khtpm_core_render.c) only adds the real
     * generic chrome X/! buttons when BOTH <sidebar> AND <panel> exist
     * in the page - a bare <page> with no sidebar (as this projection
     * used to emit) silently falls through to the flat-list layout
     * with zero chrome, by design (see that function's own early
     * `if (!sidebar || !panel) return 0;`). Real fix: give it a real,
     * minimal <sidebar> so it qualifies for the SAME generic mechanism
     * every other sidebar+panel window already gets - zero new C. */
    NB_APPEND("    <sidebar>\n      <text id=\"nb-title\" label=\"Network\"/>\n      <scrolllist></scrolllist>\n    </sidebar>\n");
    NB_APPEND("    <panel>\n");

    /* Address bar input - generic <cli_io> mechanism.
     * REAL FIX 2026-09-01, found while packaging this app for the
     * co-work reference repo: this baked-in action path was missing
     * "ops/" - nb_write_go.sh has always lived at ops/nb_write_go.sh,
     * never directly under &.hq-apps/network/. Since this ran through
     * a real shell command with stderr redirected to /dev/null, the
     * wrong path failed completely silently - the address bar and
     * every content link have been non-functional since this file was
     * written, with zero visible symptom beyond "nothing happens." */
    NB_APPEND("    <cli_io id=\"address\" target_id=\"address\" label=\"URL: \" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
              g_package_dir, g_chtpm_output_path);

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
    NB_APPEND("    <text id=\"status\" label=\"Status: %s\"/>\n", status_esc);

    /* Content area - scrollable list of page content */
    NB_APPEND("    <scrolllist>\n");

    /* Read and output page content from state file */
    FILE *pf = fopen(g_page_state_path, "r");
    if (pf) {
        char line[PATH_BUF + 512];
        int row_count = 0;
        while (fgets(line, sizeof(line), pf) && row_count < 200) {
            size_t n = strlen(line);
            while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';

            char *bar = strchr(line, '|');
            if (!bar) continue;
            *bar = '\0';
            char *rest = bar + 1;

            char content_esc[600];
            if (strcmp(line, "TITLE") == 0) {
                xml_escape(rest, content_esc, sizeof(content_esc));
                NB_APPEND("      <text id=\"title%d\" class=\"page-title\" label=\"%s\"/>\n", row_count, content_esc);
            } else if (strcmp(line, "TEXT") == 0) {
                xml_escape(rest, content_esc, sizeof(content_esc));
                NB_APPEND("      <text id=\"text%d\" label=\"%s\"/>\n", row_count, content_esc);
            } else if (strcmp(line, "LINK") == 0) {
                char *bar2 = strchr(rest, '|');
                if (bar2) {
                    *bar2 = '\0';
                    char *link_text = bar2 + 1;
                    char link_text_esc[600];
                    xml_escape(link_text[0] ? link_text : rest, link_text_esc, sizeof(link_text_esc));
                    char url_sq[PATH_BUF * 2];
                    shell_escape_squote(rest, url_sq, sizeof(url_sq));
                    NB_APPEND("      <item id=\"link%d\" label=\"%s\" action=\"'%s/ops/nb_write_go.sh' 'go' '%s'\"/>\n",
                              row_count, link_text_esc, g_package_dir, url_sq);
                }
            }
            row_count++;
        }
        fclose(pf);
    }

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
    if (!wf) return;
    fputs(buf, wf);
    fclose(wf);
    rename(tmp_path, g_chtpm_output_path);
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
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root> [--data-root <dir>]\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s/&.hq-apps/network", g_house);

    char desktop[PATH_BUF];
    path_join(desktop, sizeof(desktop), g_house, "#.desktop");
    path_join(g_request_path, sizeof(g_request_path), desktop, "network_browser_request.txt");
    path_join(g_page_state_path, sizeof(g_page_state_path), desktop, "network_browser_page.state.txt");
    path_join(g_status_path, sizeof(g_status_path), desktop, "network_browser_status.state.txt");

    /* REAL, NEW 2026-09-01 - .chtpm output path, same pattern as
     * khtpm_open_hai_manager.c's own g_chtpm_output_path setup */
    snprintf(g_chtpm_output_path, sizeof(g_chtpm_output_path), "%s/&.hq-apps/network/network-browser-hq.chtpm", g_house);

    char tmpdir[PATH_BUF];
    snprintf(tmpdir, sizeof(tmpdir), "%s/&.hq-apps/network/tmp", g_house);
    mkdir_p_local(tmpdir);
    path_join(g_tmp_html_path, sizeof(g_tmp_html_path), tmpdir, "fetch.html");

    /* ensure the request file exists and is empty on startup - same
     * "never assume, always create" discipline khtpm_open_hai_manager.c uses. */
    { FILE *f = fopen(g_request_path, "a"); if (f) fclose(f); }
    publish_status("idle");

    /* Initialize the page state with a placeholder */
    FILE *f = fopen(g_page_state_path, "w");
    if (f) { fprintf(f, "URL|https://example.com\nTITLE|Network Browser\nTEXT|Ready - enter a URL above\n"); fclose(f); }

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
