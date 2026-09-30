/* khtpm_desktop_trigger_watcher - real PERSISTENT bridge-watcher
 * daemon for the DESKTOP-entity trigger layer (EVENT-TRIGGER-LAYER-
 * PLAN.md/PLAY-MODE-ENTITY-HARNESS-DESIGN.md's own desktop extension).
 * Direct build sequence, same session: "we will use move / pathfinding
 * event program to make cursword move to castle" -> "it should use
 * the same master ledger mechanism displayed in lpns."
 *
 * Same real shape as piececraft-hq's own pc_trigger_watcher.c (that
 * file's own header comment documents the pattern in full - this is
 * its desktop-side twin, not a new design): tails
 * #.desktop/master_ledger.txt (khtpm_core_render.c's own
 * desktop_ledger_append(), written by desktop_check_touch_trigger()
 * on every cursword move while Play Mode is on) from its own launch-
 * time EOF (never replays old data on a fresh start). For every new
 * `touched_npc` line, shells out to events-hq's real play_event.sh
 * against the TARGET entity's own real pal directory (parsed out of
 * the ledger row's own `target:<name>` detail field) with
 * trigger=player-touch - the exact same call the manual Play button/
 * castle's own cmd_1.sh already makes, so this doesn't reimplement
 * anything about how an event actually runs, only WHEN one starts.
 *
 * Standalone for now, matching pc_trigger_watcher.c's own original
 * build order (proven manually first, auto-launch wiring into the
 * manager's own startup is real, deliberate, separate follow-up work
 * - not done here, given today's earlier taskbar-manager incident;
 * see BUG-LOG.md).
 *
 * Self-contained, no shared headers.
 * Usage: khtpm_desktop_trigger_watcher.+x <house_root> (no other args,
 * runs forever until SIGTERM/SIGINT) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_LINE 1024
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char g_house_root[MAX_PATH] = ".";
static volatile sig_atomic_t g_should_exit = 0;

static void handle_signal(int signo) {
    (void)signo;
    g_should_exit = 1;
}

/* Reads whatever NEW complete lines exist past g_ledger_offset - same
 * "wait for the trailing newline, leave a partial write for next
 * poll" tolerance palnet_peer.c's own read_outbox_new_lines() and
 * pc_trigger_watcher.c's own read_ledger_new_lines() already use. */
static long g_ledger_offset = -1;
static int read_ledger_new_lines(const char *ledger_path, char out_lines[][MAX_LINE], int max_lines) {
    FILE *f = fopen(ledger_path, "r");
    if (!f) return 0;

    if (g_ledger_offset < 0) {
        fseek(f, 0, SEEK_END);
        g_ledger_offset = ftell(f);
        fclose(f);
        return 0;
    }

    if (fseek(f, g_ledger_offset, SEEK_SET) != 0) {
        g_ledger_offset = 0;
        fseek(f, 0, SEEK_SET);
    }

    int n = 0;
    long consumed = g_ledger_offset;
    char line[MAX_LINE];
    while (n < max_lines && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') break;
        line[strcspn(line, "\r\n")] = '\0';
        consumed += (long)len;
        if (line[0] == '\0') continue;
        snprintf(out_lines[n], MAX_LINE, "%s", line);
        n++;
    }
    g_ledger_offset = consumed;
    fclose(f);
    return n;
}

/* Ledger row shape: timestamp|turn|actor|action_type|details
 * (desktop_ledger_append()'s own format, matching 101.lpns+map+4's
 * proven shape, RMMV-EVENT-ARCHITECTURE-LEARNINGS.md §7). Extracts the
 * action_type (field 3, 0-indexed) and, if it's touched_npc, the
 * details field's own `target:<name>` token. */
static int ledger_line_is_touched_npc(const char *line, char *target_out, size_t target_sz) {
    const char *fields[5];
    int nf = 0;
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", line);
    char *p = copy;
    fields[nf++] = p;
    while (nf < 5 && (p = strchr(p, '|')) != NULL) {
        *p = '\0';
        p++;
        fields[nf++] = p;
    }
    if (nf < 5) return 0;
    if (strcmp(fields[3], "touched_npc") != 0) return 0;
    const char *details = fields[4];
    const char *t = strstr(details, "target:");
    if (!t) return 0;
    t += 7;
    const char *end = strchr(t, ',');
    size_t len = end ? (size_t)(end - t) : strlen(t);
    if (len >= target_sz) len = target_sz - 1;
    memcpy(target_out, t, len);
    target_out[len] = '\0';
    return 1;
}

/* Finds <house_root>/xyzfs/users/<uuid>/home/livedesk/pals/<name> -
 * same real pal-directory shape every entity already lives under,
 * mirroring desktop_check_touch_trigger()'s own derivation
 * (khtpm_core_render.c). Only one real user dir exists today; this
 * still scans properly rather than hardcoding the one UUID seen live
 * this session. */
static int find_pal_dir(const char *house_root, const char *pal_name, char *out, size_t out_sz) {
    char users_root[PATH_BUF];
    snprintf(users_root, sizeof(users_root), "%s/xyzfs/users", house_root);
    DIR *d = opendir(users_root);
    if (!d) return 0;
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char cand[PATH_BUF];
        snprintf(cand, sizeof(cand), "%s/%s/home/livedesk/pals/%s", users_root, ent->d_name, pal_name);
        struct stat st;
        if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(out, out_sz, "%s", cand);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <house_root>\n", argv[0]);
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    char ledger_path[PATH_BUF];
    snprintf(ledger_path, sizeof(ledger_path), "%s/#.desktop/master_ledger.txt", g_house_root);

    fprintf(stderr, "khtpm_desktop_trigger_watcher: watching %s\n", ledger_path);

    while (!g_should_exit) {
        char new_lines[16][MAX_LINE];
        int n = read_ledger_new_lines(ledger_path, new_lines, 16);
        for (int i = 0; i < n; i++) {
            char target[128];
            if (!ledger_line_is_touched_npc(new_lines[i], target, sizeof(target))) continue;

            char pal_dir[PATH_BUF];
            if (!find_pal_dir(g_house_root, target, pal_dir, sizeof(pal_dir))) {
                fprintf(stderr, "khtpm_desktop_trigger_watcher: no pal dir for '%s'\n", target);
                continue;
            }

            char cmd[PATH_BUF * 3];
            snprintf(cmd, sizeof(cmd),
                     "'%s/&.widgits/events-hq/ops/play_event.sh' '%s' '%s' player-touch >/dev/null 2>&1",
                     g_house_root, pal_dir, g_house_root);
            fprintf(stderr, "khtpm_desktop_trigger_watcher: firing trigger for %s\n", target);
            int rc = system(cmd);
            (void)rc;
        }
        usleep(300000);
    }

    return 0;
}
