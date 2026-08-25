/* list_processes - one verb, one binary, no shared headers.
 * The "monitor of all running processes" - scans
 * pieces/world_01/map_lobby/<pet_id>/window.pid (the same per-pet marker
 * menu_input.c's spawn_egg_window already writes/checks for its
 * duplicate-window guard) and reports which pets currently have a live
 * egg_window process. Doesn't have to run continuously - it's an
 * on-demand snapshot, refreshed whenever the terminal's Processes screen
 * is opened, not a background daemon. The real continuous record is
 * pieces/system/master_ledger.txt (egg_window.c's append_window_ledger
 * writes window_opened/window_closed there); this op is just a live
 * "what's alive right now" view on top of the same window.pid markers,
 * a deliberately independent, on-demand check rather than a persistent
 * process registry of its own - matching the "independent for now, but
 * modular enough to couple later" principle the whole registry is built
 * around (see dox/01-architecture.md).
 *
 * Usage: list_processes.+x [ignored-arg]
 * Writes pieces/system/process_list.txt (one line per tracked pet:
 * pet_id|pid|alive(0/1)) for compose_menu.c to render, and prints one
 * summary line to stdout for menu_input.c's last_message. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Same check as menu_input.c's own is_pid_alive - duplicated per the
 * project's "no shared header" convention rather than factored out. */
static int is_pid_alive(long pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    int alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
#else
    return kill((pid_t)pid, 0) == 0;
#endif
}

int main(void) {
    resolve_root();

    char map_path[PATH_BUF];
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/map_lobby", project_root);

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/system/process_list.txt", project_root);
    FILE *out = fopen(out_path, "w");
    if (!out) { printf("Could not write process list.\n"); return 1; }

    DIR *d = opendir(map_path);
    int tracked = 0, alive_count = 0;
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

            char pid_path[PATH_BUF + 32];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(pid_path, sizeof(pid_path), "%s/%s/window.pid", map_path, ent->d_name);
#pragma GCC diagnostic pop

            FILE *pf = fopen(pid_path, "r");
            if (!pf) continue;
            long pid = 0;
            int got = fscanf(pf, "%ld", &pid) == 1;
            fclose(pf);
            if (!got) continue;

            int alive = is_pid_alive(pid);
            fprintf(out, "%s|%ld|%d\n", ent->d_name, pid, alive);
            tracked++;
            if (alive) alive_count++;
        }
        closedir(d);
    }
    fclose(out);

    printf("%d window(s) tracked, %d currently alive.\n", tracked, alive_count);
    return 0;
}
