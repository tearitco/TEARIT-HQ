/* livedesk_nav_debug - real diagnostic "receipt" for the live nav/open
 * state, 2026-08-06, direct request ("can u see receipts of the view
 * that is rendering for debug/ if not we need to add functionality for
 * that"). Prints every row of livedesk_open.txt and
 * livedesk_nav_claims.txt with a real, live PID-liveness check next to
 * each one (kill(pid,0) - the same probe tp_desktop_window.c's own
 * nav_claim_rows()/livedesk_registry_add() now self-heal with), so a
 * stale/corrupt entry is visible directly instead of only inferred from
 * a mismatched on-screen number. Read-only - never edits either file
 * (both self-heal on their own now every time a popup opens or a window
 * registers).
 *
 * Usage: livedesk_nav_debug.+x <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define PATH_BUF 4352

static int pid_is_alive(int pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
}

static void dump_file(const char *path, const char *title) {
    printf("=== %s (%s) ===\n", title, path);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  (missing)\n\n");
        return;
    }
    char line[PATH_BUF];
    int n = 0, dead = 0, malformed = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        n++;
        char *p = strstr(line, "PID=");
        int pid = p ? atoi(p + 4) : 0;
        const char *status;
        if (!p) { status = "MALFORMED (no PID=)"; malformed++; }
        else if (!pid_is_alive(pid)) { status = "DEAD"; dead++; }
        else { status = "alive"; }
        printf("  [%-4s] %s\n", status, line);
    }
    fclose(f);
    printf("  -- %d entries, %d dead, %d malformed --\n\n", n, dead, malformed);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: livedesk_nav_debug.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];
    char open_path[PATH_BUF], claims_path[PATH_BUF], lock_path[PATH_BUF];
    snprintf(open_path, sizeof(open_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk-nav-claims/livedesk_nav_claims.txt", house_root);
    snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_popup.lock", house_root);

    dump_file(open_path, "livedesk_open.txt");
    dump_file(claims_path, "livedesk_nav_claims.txt");

    FILE *lf = fopen(lock_path, "r");
    printf("=== popup lock (%s) ===\n  %s\n", lock_path, lf ? "exists (lock state itself is held in-kernel, not visible from file content)" : "not yet created (no popup has opened this session)");
    if (lf) fclose(lf);
    return 0;
}
