/* khtpm_hq_manager.c — db-hq's MANAGER binary (Stage 2d shell/manager
 * split, au11-hq/khtpm-merge-how2.md + local-2do-15.txt's own "Stage 2d,
 * REDONE correctly" entry, 2026-08-16). First real execution of that
 * plan - db-hq picked as the simplest target (no live-updating feed, no
 * multi-instance concurrency to design around yet, per that entry's own
 * recommended order).
 *
 * Real local precedent this is copied FROM (not invented): khtpm_strip_
 * parser.c (shell) + khtpm_taskbar_manager_main.c (a SEPARATE binary
 * that computes state and writes strip_state.txt/strip_history.txt for
 * the shell to poll) - the SAME shell+manager split wraith_parser_alpha.
 * c's own <module> convention uses (see that file's launch_module():
 * real fork()+execv(), not shared code).
 *
 * Owns EVERYTHING that used to be khtpm_hq_render.c's own business
 * logic (load_common_events()/open_in_editor(), moved here verbatim):
 *   - scans <house_root>/common_events/ every poll, publishes the
 *     sorted list to #.desktop/db_hq_common_events.state.txt (atomic
 *     tmp-write-then-rename, same convention khtpm_taskbar_manager.c's
 *     own registry writes already use - not invented here).
 *   - polls #.desktop/db_hq_action.txt for a pending "open:<name>"
 *     request the shell writes when the user activates "Open in
 *     Editor"; on seeing one, does the actual events-hq spawn (the
 *     exact system() call open_in_editor() used to do in-process) and
 *     clears the request file.
 *
 * The shell (khtpm_hq_render.c) now does NEITHER of these directly -
 * it only reads the state file and writes the action-request file. No
 * shared code between this file and the shell; they're independent
 * standalone binaries, launched/killed together by open_db_hq.sh,
 * exactly matching the taskbar's own manager+shell pairing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* macOS leg (2026-08-22): no `setsid` binary on macOS — drop the prefix
 * there (nohup+& already detaches for this pattern); Linux byte-identical. */
#ifdef __APPLE__
#define KTB_SETSID ""
#else
#define KTB_SETSID "setsid "
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PATH_BUF 4096
#define MAX_EVENTS 128

static char g_house_root[PATH_BUF];
static char g_events_state_path[PATH_BUF];
static char g_action_path[PATH_BUF];

static void publish_common_events(void) {
    char ce_root[PATH_BUF];
    snprintf(ce_root, sizeof(ce_root), "%s/common_events", g_house_root);
    if (access(ce_root, F_OK) != 0) mkdir(ce_root, 0755);

    char names[MAX_EVENTS][64];
    int n = 0;
    DIR *d = opendir(ce_root);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) && n < MAX_EVENTS) {
            if (de->d_name[0] == '.') continue;
            char ep[PATH_BUF];
            snprintf(ep, sizeof(ep), "%s/%s", ce_root, de->d_name);
            struct stat st;
            if (stat(ep, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(names[n], sizeof(names[0]), "%s", de->d_name);
            n++;
        }
        closedir(d);
    }
    /* same sort load_common_events() used to do - stable, alphabetical */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char t[64]; snprintf(t, sizeof(t), "%s", names[i]);
                snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                snprintf(names[j], sizeof(names[j]), "%s", t);
            }

    /* Atomic publish - write-to-tmp-then-rename, same convention
     * khtpm_taskbar_manager.c's own registry writes already use. */
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_events_state_path);
    FILE *f = fopen(tmp_path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", names[i]);
    fclose(f);
    rename(tmp_path, g_events_state_path);
}

/* REAL, dynamic path discovery (2026-08-17, direct instruction: "we
 * dont hardcode, see how tpmos's button.sh does dynamic path
 * discovery" - live report after muchi-pet/livedesk-clock moved out of
 * xyzfs/bin/ and every hardcoded "%s/xyzfs/bin/<app>/..." string
 * silently broke). Same real precedent as this house's own
 * play_event.sh (searches upward for a "101.mutaclsym... / system"
 * landmark instead of assuming a fixed depth) and
 * livedesk_build_toys_menu()'s own toys_scan_one_root() (scans known
 * root dirs for an app by name) - not invented fresh. Scans a short
 * list of known real app-root directories under house_root for a
 * subdirectory whose name contains app_name, so the NEXT time an app
 * moves between *.monads/&.widgits/&.hq-apps/@.apps this call site
 * doesn't need a source edit at all. */
static int find_app_dir(const char *house_root, const char *app_name, char *out, size_t outsz) {
    static const char *roots[] = { "*.monads", "&.widgits", "&.hq-apps", "@.apps", NULL };
    for (int i = 0; roots[i]; i++) {
        char parent[PATH_BUF];
        snprintf(parent, sizeof(parent), "%s/%s", house_root, roots[i]);
        DIR *d = opendir(parent);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strstr(ent->d_name, app_name)) {
                snprintf(out, outsz, "%s/%s", parent, ent->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    out[0] = '\0';
    return 0;
}

/* Real action this used to be open_in_editor()'s own system() call,
 * moved here verbatim - the shell no longer spawns child processes as
 * its own business action, that's the manager's job now. */
static void handle_action_request(void) {
    FILE *f = fopen(g_action_path, "r");
    if (!f) return;
    char line[256] = "";
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
    if (len == 0) return; /* already-handled/empty request, nothing to do */

    if (strncmp(line, "open:", 5) == 0) {
        const char *name = line + 5;
        char ce_path[PATH_BUF];
        snprintf(ce_path, sizeof(ce_path), "%s/common_events/%s", g_house_root, name);
        char muchi_pet_dir[PATH_BUF];
        find_app_dir(g_house_root, "muchi-pet", muchi_pet_dir, sizeof(muchi_pet_dir));
        char sh[PATH_BUF * 3];
        snprintf(sh, sizeof(sh),
            KTB_SETSID "nohup sh -c 'sh \"%s/ops/open_event_ez.sh\" \"%s\" \"%s\"' >/dev/null 2>&1 &",
            muchi_pet_dir, ce_path, g_house_root);
        int rc = system(sh);
        (void)rc;
    }
    /* clear the request so it doesn't re-fire next poll */
    FILE *w = fopen(g_action_path, "w");
    if (w) fclose(w);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "khtpm_hq_manager: usage: <house_root>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_events_state_path, sizeof(g_events_state_path), "%s/#.desktop/db_hq_common_events.state.txt", g_house_root);
    snprintf(g_action_path, sizeof(g_action_path), "%s/#.desktop/db_hq_action.txt", g_house_root);

    /* start clean - no stale action carried over from a previous run */
    FILE *w = fopen(g_action_path, "w");
    if (w) fclose(w);

    for (;;) {
        publish_common_events();
        handle_action_request();
        usleep(400000); /* 400ms poll - fast enough to feel live, cheap enough to idle */
    }
    return 0;
}
