/* ledger_append.c — Append lifecycle event to house runtime ledger.
 * Ported from &.widgits/file-menu/ops/ledger_append.c (the house's own
 * real, proven widget-discovery mechanism - see @.apps/BOARD_WIDGET_
 * ARCHITECTURE.md §4's own "discovery via the xyzfs runtime ledger"
 * line, never actually built until now - board-viewer's own button.sh
 * used to explicitly omit these calls, see that file's own removed
 * "LEDGER CALLS DELIBERATELY OMITTED" comment for the history).
 *
 * REAL FIX vs file-menu's own copy (direct user instruction 2026-08-03,
 * "login can be optional, but use 'default'"): file-menu's own version
 * hard-exits(1) if 0.user-pal👤️/00.login-signup/current_login.txt's
 * own current_xyzfs field is empty (no active login) - which is why
 * board-viewer's button.sh never called it at all until now. This copy
 * falls back to the literal bucket "default" instead of erroring, so
 * the SAME real ledger mechanism (same file, same schema, same path
 * shape under <house>/<xyzfs>/home/runtime/ledger.txt) works whether
 * or not anyone is logged in. Once a project in this house DOES log in
 * for real (current_xyzfs gets a real value), this same code picks up
 * the real per-user ledger automatically - no separate mechanism, just
 * a graceful default bucket instead of a hard error.
 *
 * Usage: ledger_append <event> <type> <project_id> <session_root> <pid> <display_name> <inbox_path>
 *   event        ONLINE | OFFLINE
 *   type         editor | widget | app | daemon
 *   project_id   civ-txt | tactics-txt | board-viewer | etc
 *   session_root full path to session dir
 *   pid          process ID ($$)
 *   display_name human-readable label
 *   inbox_path   relative to session_root (e.g. pieces/system/widget_cmds/inbox.txt)
 *
 * Schema: timestamp|event|type|project_id|session_root|pid|display_name|inbox_path
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 4096
#define MAX_PATH 4096

static char house_root[MAX_PATH] = "";

static void resolve_house_root(void) {
    const char *prisc_root = getenv("PRISC_PROJECT_ROOT");
    if (!prisc_root || !prisc_root[0]) {
        fprintf(stderr, "Error: PRISC_PROJECT_ROOT not set\n");
        exit(1);
    }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", prisc_root);
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot read house_root.txt (%s)\n", path);
        exit(1);
    }
    if (!fgets(house_root, sizeof(house_root), f)) {
        fclose(f);
        fprintf(stderr, "Error: empty house_root.txt\n");
        exit(1);
    }
    fclose(f);
    size_t ln = strlen(house_root);
    while (ln > 0 && (house_root[ln-1] == '\n' || house_root[ln-1] == '\r')) house_root[--ln] = '\0';
    if (!house_root[0]) {
        fprintf(stderr, "Error: empty house_root.txt\n");
        exit(1);
    }
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void resolve_ledger_path(char *out, size_t out_sz) {
    resolve_house_root();

    /* Read current_login.txt for current_xyzfs - "default" if nobody's
     * logged in (see this file's own header comment). */
    char login_path[MAX_PATH];
    snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
    char xyzfs[MAX_PATH];
    read_kv(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));
    if (!xyzfs[0]) snprintf(xyzfs, sizeof(xyzfs), "default");

    snprintf(out, out_sz, "%s/%s/home/runtime/ledger.txt", house_root, xyzfs);

    /* Ensure runtime/ directory exists (mkdir -p for full chain) */
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/%s/home/runtime", house_root, xyzfs);
    char mkcmd[MAX_PATH + 32];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    { int _rc = system(mkcmd); (void)_rc; }
}

int main(int argc, char **argv) {
    if (argc < 8) {
        fprintf(stderr, "Usage: ledger_append <event> <type> <project_id> <session_root> <pid> <display_name> <inbox_path>\n");
        return 1;
    }

    const char *event       = argv[1];
    const char *type        = argv[2];
    const char *project_id  = argv[3];
    const char *session_root= argv[4];
    const char *pid_str     = argv[5];
    const char *display_name= argv[6];
    const char *inbox_path  = argv[7];

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);

    char ledger_path[MAX_PATH];
    resolve_ledger_path(ledger_path, sizeof(ledger_path));

    FILE *fp = fopen(ledger_path, "a");
    if (!fp) {
        fprintf(stderr, "Error: cannot open ledger for append: %s\n", ledger_path);
        return 1;
    }

    fprintf(fp, "%s|%s|%s|%s|%s|%s|%s|%s\n",
            timestamp, event, type, project_id, session_root, pid_str, display_name, inbox_path);

    fclose(fp);
    return 0;
}
