#define _POSIX_C_SOURCE 200809L
/* network_browser_render_ascii.c - the real terminal/headless mirror
 * for the "network browser" HQ app, CENTROID_GOLD_STD.md §2 Stage 4 /
 * §3 rule 4's first concrete `ascii_draw_elem()`-class renderer
 * (2026-08-31, direct instruction: "i wanna start the centroid
 * browser, all the way to cli mirroring").
 *
 * Real symmetry with network_browser_render.c (the X11 renderer): BOTH
 * files read the exact same two real, manager-published state files
 * (network_browser_page.state.txt / network_browser_status.state.txt)
 * and write the exact same real action-file contract
 * (network_browser_action.txt, "go:<url>"). Neither file parses HTML,
 * neither owns any browsing logic (that's network_browser_manager.c's
 * whole job) - this is CENTROID_GOLD_STD.md's core rule applied
 * honestly to a document-flow app: for a scrolling list of TITLE/TEXT/
 * LINK rows, the manager's own published, ordered row list already IS
 * the single positioned "tree" (a flat, single-axis layout - each row
 * one line tall, in document order) - both renderers walk that SAME
 * real list, one drawing pixels, this one drawing text. No second,
 * independently-composed representation exists anywhere in this app.
 *
 * Real, interactive CLI loop (not a one-shot dump): prints the current
 * page inside a box-drawing frame (mirroring the X11 window's own
 * chrome, not just raw text), then reads one line from stdin:
 *   <a URL, or anything containing "://">  - go there
 *   <a number matching a shown [N] link>   - follow that link
 *   r / reload                             - re-fetch the current URL
 *   q / quit                               - exit
 *   (blank)                                - just redraw
 *
 * Usage: network_browser_render_ascii.+x <house_root> [start_url]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#define PATH_BUF 4352
#define URL_BUF 2048
#define MAX_ROWS 4096
#define TERM_W 100 /* real, fixed real-estate assumption - good enough for a v1 proof; could be ioctl(TIOCGWINSZ)-driven later */

typedef struct { char kind; char a[URL_BUF]; char b[512]; } PageRow;
static PageRow g_rows[MAX_ROWS];
static int g_n_rows = 0;
static char g_page_url[URL_BUF] = "";
static char g_status_line[256] = "idle";

static char g_house[PATH_BUF];
static char g_action_path[PATH_BUF];
static char g_page_state_path[PATH_BUF];
static char g_status_path[PATH_BUF];

static void trim_nl(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

static time_t mtime_of(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return (time_t)0;
    return st.st_mtime;
}

/* Real, exact mirror of network_browser_render.c's own reload_page_if_
 * changed() - same 3-line format, same URL|/TITLE|/TEXT|/LINK| tags -
 * the manager's real published contract, not re-derived independently. */
static void reload_page(void) {
    g_n_rows = 0;
    g_page_url[0] = '\0';
    FILE *f = fopen(g_page_state_path, "r");
    if (!f) return;
    char line[URL_BUF + 512];
    while (fgets(line, sizeof(line), f) && g_n_rows < MAX_ROWS) {
        trim_nl(line);
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        *p1 = '\0';
        char *rest = p1 + 1;
        if (strcmp(line, "URL") == 0) { snprintf(g_page_url, sizeof(g_page_url), "%s", rest); continue; }
        if (strcmp(line, "TITLE") == 0) { g_rows[g_n_rows].kind = 'T'; snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest); g_n_rows++; continue; }
        if (strcmp(line, "TEXT") == 0) { g_rows[g_n_rows].kind = 'X'; snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest); g_n_rows++; continue; }
        if (strcmp(line, "LINK") == 0) {
            char *p2 = strchr(rest, '|');
            if (!p2) continue;
            *p2 = '\0';
            g_rows[g_n_rows].kind = 'L';
            snprintf(g_rows[g_n_rows].a, sizeof(g_rows[g_n_rows].a), "%s", rest);
            snprintf(g_rows[g_n_rows].b, sizeof(g_rows[g_n_rows].b), "%s", p2 + 1);
            g_n_rows++;
            continue;
        }
    }
    fclose(f);
}

static void reload_status(void) {
    FILE *f = fopen(g_status_path, "r");
    if (!f) { snprintf(g_status_line, sizeof(g_status_line), "idle"); return; }
    if (!fgets(g_status_line, sizeof(g_status_line), f)) g_status_line[0] = '\0';
    trim_nl(g_status_line);
    fclose(f);
}

static void write_action(const char *action) {
    FILE *f = fopen(g_action_path, "w");
    if (!f) return;
    fprintf(f, "%s\n", action);
    fclose(f);
}

/* Real, deliberate wait-for-fetch - without this, a scripted or fast
 * typist submits a link/URL and the very next redraw() still shows the
 * OLD page (the manager hasn't published yet), which looks like the
 * click did nothing. Same real "loading" sentinel the X11 renderer
 * polls for asynchronously; this CLI renderer is interactive/blocking
 * by nature, so it waits synchronously instead - a real, deliberate
 * difference between the two renderers' own real I/O models, not a
 * missed case. */
static void wait_for_load(void) {
    printf("Loading...\n");
    fflush(stdout);
    for (int i = 0; i < 60; i++) {
        usleep(200000);
        reload_status();
        if (strcmp(g_status_line, "loading") != 0) break;
    }
}

static void hline(char ch) { for (int i = 0; i < TERM_W; i++) putchar(ch); putchar('\n'); }

static void wrap_print(const char *text, int width) {
    size_t len = strlen(text);
    size_t i = 0;
    while (i < len) {
        size_t take = (len - i > (size_t)width) ? (size_t)width : (len - i);
        if (take == (size_t)width && i + take < len) {
            size_t back = take;
            while (back > 0 && text[i + back] != ' ') back--;
            if (back > 0) take = back;
        }
        printf("| %.*s", (int)take, text + i);
        int pad = width - (int)take;
        for (int p = 0; p < pad; p++) putchar(' ');
        printf(" |\n");
        i += take;
        while (i < len && text[i] == ' ') i++;
    }
}

/* The real ASCII "draw_elem()" equivalent - walks the SAME g_rows list
 * network_browser_render.c walks, printing box-drawing chrome instead
 * of pixels. This is the concrete render half of the app; reload_page()
 * above is the concrete "read the shared centroid" half. */
static void redraw(int *link_map, int *n_links) {
    reload_page();
    reload_status();
    printf("\033[2J\033[H"); /* real terminal clear, same "redraw whole frame" contract as the X11 side's own XPutImage of the full buffer */
    hline('=');
    printf("| Network Browser (centroid CLI mirror)%*s|\n", TERM_W - 41, "");
    hline('-');
    printf("| URL> %-*s|\n", TERM_W - 8, g_page_url[0] ? g_page_url : "(none loaded)");
    hline('-');
    int content_w = TERM_W - 4;
    *n_links = 0;
    for (int i = 0; i < g_n_rows; i++) {
        if (g_rows[i].kind == 'T') {
            char buf[600]; snprintf(buf, sizeof(buf), "# %s", g_rows[i].a);
            wrap_print(buf, content_w);
        } else if (g_rows[i].kind == 'X') {
            wrap_print(g_rows[i].a, content_w);
        } else {
            int idx = ++(*n_links);
            link_map[idx] = i;
            char buf[600]; snprintf(buf, sizeof(buf), "[%d] %s", idx, g_rows[i].b);
            wrap_print(buf, content_w);
        }
    }
    hline('-');
    printf("| status: %-*s|\n", TERM_W - 11, g_status_line);
    hline('=');
    printf("Enter a URL, a link number, 'r' to reload, or 'q' to quit: ");
    fflush(stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <house_root> [start_url]\n", argv[0]); return 1; }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);

    char desktop[PATH_BUF];
    snprintf(desktop, sizeof(desktop), "%s/#.desktop", g_house);
    snprintf(g_action_path, sizeof(g_action_path), "%s/network_browser_action.txt", desktop);
    snprintf(g_page_state_path, sizeof(g_page_state_path), "%s/network_browser_page.state.txt", desktop);
    snprintf(g_status_path, sizeof(g_status_path), "%s/network_browser_status.state.txt", desktop);

    if (argc >= 3) {
        char action[URL_BUF + 8];
        snprintf(action, sizeof(action), "go:%s", argv[2]);
        write_action(action);
        wait_for_load();
    }

    int link_map[MAX_ROWS + 1];
    int n_links = 0;
    char line[URL_BUF];

    for (;;) {
        redraw(link_map, &n_links);
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_nl(line);
        if (line[0] == '\0') continue;
        if (strcasecmp(line, "q") == 0 || strcasecmp(line, "quit") == 0) break;
        if (strcasecmp(line, "r") == 0 || strcasecmp(line, "reload") == 0) {
            if (g_page_url[0]) {
                char action[URL_BUF + 8];
                snprintf(action, sizeof(action), "go:%s", g_page_url);
                write_action(action);
                wait_for_load();
            }
            continue;
        }
        int all_digits = 1;
        for (char *p = line; *p; p++) if (!isdigit((unsigned char)*p)) { all_digits = 0; break; }
        if (all_digits) {
            int idx = atoi(line);
            if (idx >= 1 && idx <= n_links) {
                int row = link_map[idx];
                char action[URL_BUF + 8];
                snprintf(action, sizeof(action), "go:%s", g_rows[row].a);
                write_action(action);
                wait_for_load();
            }
            continue;
        }
        /* treat anything else as a URL (bare "example.com" resolves
         * fine too - network_browser_manager.c's own resolve_url()
         * only special-cases already-absolute "://" hrefs, so a bare
         * host with no scheme is passed through to curl, which accepts
         * schemeless hosts as http by default - real, already-tested
         * behavior, not assumed). */
        char action[URL_BUF + 8];
        snprintf(action, sizeof(action), "go:%s", line);
        write_action(action);
        wait_for_load();
    }
    return 0;
}
