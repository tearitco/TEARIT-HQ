/* bookmarks_manager.c — bookmarks' real MANAGER binary (2026-08-25, real
 * TPMOS-compliant rebuild — au11-hq/TPMOS-COMPLIANCE-DEBT.md's own
 * standing rule, added the same day: don't patch around a missing
 * manager with renderer-side workarounds when the compliant pattern has
 * already been built for a sibling app. That sibling is stats_hq_
 * manager.c — this file copies its exact init/poll-loop/atomic-publish
 * shape, not reinvented).
 *
 * Real business logic owned here (moved out of bm_menu.sh entirely):
 * parses <pal>/bookmarks.pdl's real `BOOKMARK | name | path` rows and
 * publishes one `name<TAB>path` line per bookmark into
 * <pal>/bookmarks_state.txt (mtime-gated, atomic tmp-then-rename, same
 * convention every manager in this house uses). bm_menu.sh's own
 * `do_add` still owns WRITING bookmarks.pdl (add/New+) — this manager
 * only reacts to it changing, exactly like stats_hq_manager.c reacts to
 * session-stats files changing. The renderer's own dbhq_load_bookmark_
 * state()/dbhq_inject_bookmark_items() (khtpm_core_render.c,
 * 2026-08-25) reads this file and injects real <button> rows — no bash
 * XML generation, no chtpm-live-reload hack (that workaround is deleted
 * along with this file landing).
 *
 * Per-pal, unlike stats_hq_manager.c's house-wide scan: argv[2] is the
 * pal dir, passed automatically by dbhq_launch_module()'s own <module>
 * launch (extended 2026-08-25 to pass g_package_dir as argv[2] for any
 * consumer that needs it — harmless/ignored by house-wide managers that
 * only read argv[1]). */
#define _DEFAULT_SOURCE /* usleep() under -std=c11 strict mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_BUF 4096
#define MAX_BOOKMARKS 128

static char g_pal_dir[PATH_BUF];
static char g_pdl_path[PATH_BUF];
static char g_state_path[PATH_BUF];
static time_t g_pdl_mtime = 0;

/* trims leading/trailing whitespace in place, same rule bm_menu.sh's own
 * pdl_rows() used (a plain sed trim of leading/trailing blanks). */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) s[--len] = '\0';
    return s;
}

static void publish_bookmarks(void) {
    struct stat st;
    if (stat(g_pdl_path, &st) != 0) {
        /* no bookmarks.pdl yet (fresh pal) - publish an empty state file
         * once so the renderer's own load doesn't just stall forever. */
        if (g_pdl_mtime != (time_t)-1) {
            FILE *f = fopen(g_state_path, "w");
            if (f) fclose(f);
            g_pdl_mtime = (time_t)-1;
        }
        return;
    }
    if (st.st_mtime == g_pdl_mtime) return;
    g_pdl_mtime = st.st_mtime;

    FILE *in = fopen(g_pdl_path, "r");
    if (!in) return;

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return; }

    char line[PATH_BUF];
    int n = 0;
    while (n < MAX_BOOKMARKS && fgets(line, sizeof(line), in)) {
        if (strncmp(line, "BOOKMARK", 8) != 0) continue;
        char *rest = line + 8;
        char *bar1 = strchr(rest, '|');
        if (!bar1) continue;
        char *name_start = bar1 + 1;
        char *bar2 = strchr(name_start, '|');
        if (!bar2) continue;
        *bar2 = '\0';
        char *path_start = bar2 + 1;
        char *name = trim(name_start);
        char *path = trim(path_start);
        if (!name[0] || !path[0]) continue;
        fprintf(out, "%s\t%s\n", name, path);
        n++;
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, g_state_path);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "bookmarks_manager: usage: <house_root> <pal_dir>\n"); return 1; }
    snprintf(g_pal_dir, sizeof(g_pal_dir), "%s", argv[2]);
    snprintf(g_pdl_path, sizeof(g_pdl_path), "%s/bookmarks.pdl", g_pal_dir);
    snprintf(g_state_path, sizeof(g_state_path), "%s/bookmarks_state.txt", g_pal_dir);

    for (;;) {
        publish_bookmarks();
        usleep(500000); /* 0.5s poll - New+ commits should show up snappy, not stats-hq's own 1s */
    }
    return 0;
}
