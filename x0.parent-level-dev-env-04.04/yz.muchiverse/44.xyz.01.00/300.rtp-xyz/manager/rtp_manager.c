/* rtp_manager - persistent daemon for rtp-xyz's own save/load screen.
 * Modeled directly on slop-ed-dev_manager.c's real pattern (TPMOS
 * reference tree, read in full 2026-07-31 - see dox/rtp-xyz-
 * architecture.md §2a for the citation): is_active_layout() gate (this
 * daemon only does real work while project_menu.chtpm is the current
 * layout, otherwise idles - the real answer to "how does a module hand
 * back control," no explicit handoff protocol needed), poll loop on
 * history.txt for real "COMMAND: " lines, real cp -r save/load, no
 * shared memory, no shared headers - matches every other op/daemon in
 * this house.
 *
 * SAVE LOCATION - direct instruction (2026-07-31): "real house, but
 * create concessions for default/dev." rtp-xyz has NO login system of
 * its own (confirmed - no current_login.txt, no xyzfs reference
 * anywhere in this project's own code before this file). Real
 * resolution order:
 *   1. Walk up from project_root to find <house_root>/xyzfs/users/ -
 *      if found, take the first real user directory (dev-mode
 *      default - no real per-project login flow exists yet to pick a
 *      SPECIFIC user, so "first available" is the honest concession).
 *      Target: <that_user>/home/projects/rtp-xyz/<save-name>/ -
 *      matches rpg-xyz-plan.md's own "Decisions" table AND a real,
 *      already-pre-created empty folder found at exactly this path.
 *   2. If no xyzfs/users/ is reachable at all (standalone/dev run, no
 *      house context) - fall back to a project-local
 *      pieces/saves_local/<save-name>/ dir, so save/load still works
 *      with zero house dependency.
 *
 * Reuses the EXISTING real save infrastructure's own shape
 * (ops/save_game.c's auto-numbered-slot + save_meta.txt convention)
 * rather than duplicating it - this daemon's own save/load targets a
 * different root (xyzfs project dir instead of pieces/saves/), same
 * real mechanism otherwise (cp -r, turn/timestamp metadata).
 *
 * Usage: launched via project_menu.chtpm's own <module> tag, matching
 * slop-ed-dev_manager's own real invocation shape - see that layout's
 * own header. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH 4096
#define MAX_LINE 1024
#define MAX_ITEMS 64

static volatile sig_atomic_t g_shutdown = 0;
static void handle_sigint(int sig) { (void)sig; g_shutdown = 1; }

static char project_root[MAX_PATH] = ".";
static char save_root[MAX_PATH] = "";   /* resolved once at startup */
static int using_xyzfs = 0;             /* for the status line */

static void resolve_project_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int is_dir(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Real resolution, per this file's own header comment - walk up from
 * project_root looking for xyzfs/users/, take the first real user dir
 * found. rtp-xyz sits directly under the house root (confirmed:
 * project_root/../xyzfs exists), so one level up is enough - not
 * hardcoding a fixed number of ".." in case this project is ever
 * relocated, real directory-existence checks instead. */
static void resolve_save_root(void) {
    char candidate[MAX_PATH];
    snprintf(candidate, sizeof(candidate), "%s/../xyzfs/users", project_root);
    DIR *d = opendir(candidate);
    if (d) {
        struct dirent *e;
        char first_user[256] = "";
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char user_dir[MAX_PATH];
            snprintf(user_dir, sizeof(user_dir), "%s/%s", candidate, e->d_name);
            if (is_dir(user_dir)) { snprintf(first_user, sizeof(first_user), "%s", e->d_name); break; }
        }
        closedir(d);
        if (first_user[0]) {
            snprintf(save_root, sizeof(save_root), "%s/%s/home/projects/rtp-xyz", candidate, first_user);
            char cmd[MAX_PATH + 16];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", save_root);
            if (system(cmd) == 0) { using_xyzfs = 1; return; }
        }
    }
    /* Concession for default/dev: no real xyzfs user reachable. */
    snprintf(save_root, sizeof(save_root), "%s/pieces/saves_local", project_root);
    char cmd[MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", save_root);
    (void)!system(cmd);
    using_xyzfs = 0;
}

static int run_command(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static void transition_to_layout(const char *layout_path) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/layout_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", layout_path); fclose(f); }
}

static void hit_frame_marker(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

/* REAL, LIVE-CAUGHT BUG (2026-07-31): there is NO generic "recompose
 * whatever layout is active" tool - ops/compose_frame.+x is
 * mutaclysm's own GAME-SPECIFIC frame composer. Its own
 * write_panel_gui_state() (ops/compose_frame.c) unconditionally
 * rewrites the EXACT SAME pieces/apps/player_app/manager/gui_state.txt
 * this daemon writes to, with ITS OWN craft_panel_items/
 * inventory_panel_items content - confirmed live: calling it here
 * clobbered this daemon's own save_list_markup/save_root_kind fields
 * within the same tick, every time, silently (both writers share one
 * file, neither preserves the other's fields). REMOVED - the real,
 * generic mechanism to make chtpm_parser_pal.c recompose the CURRENT
 * frame (whatever layout that is) with fresh gui_state is its own
 * internal compose_frame() function, triggered by hit_frame_marker()'s
 * frame_changed.txt pulse alone - no separate binary needed, matching
 * how every other <interact>-driven layout in this house already
 * signals a redraw. Do not re-add a trigger_render() that shells out
 * to a mutaclysm-specific op. */

static void log_message(const char *msg) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

static int is_active_layout(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int res = 0;
    if (fgets(line, sizeof(line), f)) {
        char *cur = trim(line);
        if (strstr(cur, "project_menu.chtpm") != NULL) res = 1;
    }
    fclose(f);
    return res;
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

/* One save = one real subdirectory under save_root containing world_01/,
 * registry/, project.pdl - and its own save_meta.txt (turn, saved_at),
 * same shape ops/save_game.c already establishes for pieces/saves/. */
static int next_save_serial(void) {
    DIR *d = opendir(save_root);
    int max_n = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            int n;
            if (sscanf(e->d_name, "save_%d", &n) == 1 && n > max_n) max_n = n;
        }
        closedir(d);
    }
    return max_n + 1;
}

/* REAL, LIVE-CAUGHT DATA-LOSS BUG (2026-07-31): the original version of
 * both functions below chained every step with `&&` in ONE shell -c
 * string, checked only the FINAL combined exit code, and - worse, in
 * do_load() - ran `rm -rf` on the LIVE pieces/registry BEFORE
 * confirming the save actually had a valid registry/ to restore.
 * Real consequence, confirmed live: this project's own pieces/
 * registry/ (containing this whole de-theme's own real slime/
 * slime_pup content) was silently destroyed - the ORIGINAL save
 * (`save_1`) turned out to have never actually received a registry/
 * copy at all (root cause of THAT specific silent failure not fully
 * chased down - a shell-chain step failed invisibly, since
 * run_command() redirects both stdout AND stderr to /dev/null in the
 * child, matching real_command()'s own header - no error ever
 * surfaced anywhere), and the later Load's own blind `rm -rf` had
 * nothing valid to restore from, permanently deleting the live copy
 * with zero recovery path. Rewritten below: every step's own real
 * exit code is checked separately (no more single combined chain
 * hiding which step actually failed), and do_load() NEVER touches the
 * live directories until the SOURCE (the save's own world_01/registry)
 * is confirmed to exist first - if the source is missing or
 * incomplete, the load is refused outright, live data is left
 * completely untouched. */
static int dir_nonempty(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        found = 1;
        break;
    }
    closedir(d);
    return found;
}

static void do_save_new(void) {
    int serial = next_save_serial();
    char dst[MAX_PATH];
    snprintf(dst, sizeof(dst), "%s/save_%d", save_root, serial);

    char cmd[MAX_PATH * 2];
    int ok = 1;

    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dst);
    if (run_command(cmd) != 0) ok = 0;

    if (ok) {
        snprintf(cmd, sizeof(cmd), "cp -r '%s/pieces/world_01' '%s/'", project_root, dst);
        if (run_command(cmd) != 0) ok = 0;
    }

    if (ok) {
        snprintf(cmd, sizeof(cmd), "cp -r '%s/pieces/registry' '%s/'", project_root, dst);
        if (run_command(cmd) != 0) ok = 0;
    }

    /* project.pdl is real but optional - a missing one should not fail
     * the whole save (matches the original behavior's own intent),
     * but is checked separately rather than silently folded into a
     * combined chain. */
    if (ok) {
        char src_pdl[MAX_PATH];
        snprintf(src_pdl, sizeof(src_pdl), "%s/project.pdl", project_root);
        if (is_dir(src_pdl) == 0) { /* not a dir check, just existence via access below */ }
        snprintf(cmd, sizeof(cmd), "[ -f '%s/project.pdl' ] && cp '%s/project.pdl' '%s/'", project_root, project_root, dst);
        run_command(cmd); /* best-effort, not gated on ok */
    }

    /* Verify what actually landed, not just trust exit codes - a `cp`
     * can exit 0 while copying an EMPTY source directory (not an
     * error, just nothing to copy), which is exactly how the original
     * incident's own registry/ ended up silently absent. */
    if (ok) {
        char world_dst[MAX_PATH], reg_dst[MAX_PATH];
        snprintf(world_dst, sizeof(world_dst), "%s/world_01", dst);
        snprintf(reg_dst, sizeof(reg_dst), "%s/registry", dst);
        if (!dir_nonempty(world_dst) || !dir_nonempty(reg_dst)) ok = 0;
    }

    char turn_path[MAX_PATH];
    snprintf(turn_path, sizeof(turn_path), "%s/pieces/world_01/map_start/state.txt", project_root);
    int turn = read_kv_int(turn_path, "turn", 0);

    if (ok) {
        char meta_path[MAX_PATH + 32];
        snprintf(meta_path, sizeof(meta_path), "%s/save_meta.txt", dst);
        FILE *mf = fopen(meta_path, "w");
        if (mf) {
            fprintf(mf, "turn=%d\n", turn);
            fprintf(mf, "saved_at=%ld\n", (long)time(NULL));
            fclose(mf);
        }
    } else {
        /* Don't leave a half-written, misleading save slot behind. */
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dst);
        run_command(cmd);
    }

    char msg[160];
    if (ok) snprintf(msg, sizeof(msg), "Saved to save_%d (turn %d, %s).", serial, turn, using_xyzfs ? "xyzfs" : "local dev");
    else snprintf(msg, sizeof(msg), "Save failed - nothing was written (see this daemon's own header comment for the real incident this guards against).");
    log_message(msg);
}

static void do_load(const char *save_name) {
    char src[MAX_PATH];
    snprintf(src, sizeof(src), "%s/%s", save_root, save_name);
    if (!is_dir(src)) { log_message("Load failed: save not found."); return; }

    /* REAL SAFETY GATE - confirm the source has valid content BEFORE
     * touching the live directories at all. This is the fix for the
     * real incident this file's own header comment documents. */
    char src_world[MAX_PATH], src_reg[MAX_PATH];
    snprintf(src_world, sizeof(src_world), "%s/world_01", src);
    snprintf(src_reg, sizeof(src_reg), "%s/registry", src);
    if (!dir_nonempty(src_world)) {
        log_message("Load refused: this save has no real world_01/ content - live data left untouched.");
        return;
    }
    if (!dir_nonempty(src_reg)) {
        log_message("Load refused: this save has no real registry/ content - live data left untouched.");
        return;
    }

    char cmd[MAX_PATH * 2];
    int ok = 1;

    snprintf(cmd, sizeof(cmd), "rm -rf '%s/pieces/world_01'", project_root);
    if (run_command(cmd) != 0) ok = 0;
    if (ok) {
        snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s/pieces/'", src_world, project_root);
        if (run_command(cmd) != 0) ok = 0;
    }

    if (ok) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s/pieces/registry'", project_root);
        if (run_command(cmd) != 0) ok = 0;
    }
    if (ok) {
        snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s/pieces/'", src_reg, project_root);
        if (run_command(cmd) != 0) ok = 0;
    }

    if (ok) {
        snprintf(cmd, sizeof(cmd), "[ -f '%s/project.pdl' ] && cp '%s/project.pdl' '%s/'", src, src, project_root);
        run_command(cmd); /* best-effort, optional */
    }

    char msg[160];
    if (ok) snprintf(msg, sizeof(msg), "Loaded %s.", save_name);
    else snprintf(msg, sizeof(msg), "Load partially failed - live pieces/world_01 or pieces/registry may be incomplete, check manually.");
    log_message(msg);
    transition_to_layout("pieces/chtpm/layouts/game.chtpm");
}

static void write_gui_state(void) {
    /* Path CORRECTED after direct code read (2026-07-31): chtpm_parser_
     * pal.c's own resolve_project_gui_state_path() (projects/<id>/
     * manager/gui_state.txt) is the WRAITH-ALPHA-embedded-sub-project
     * shape, wrong here. 102.agy-txt's own agy_browser_manager.c
     * documents the real gotcha directly in its own header comment:
     * pieces/apps/player_app/manager/gui_state.txt is only auto-loaded
     * when project_id is EMPTY (confirmed: rtp-xyz sets no project_id
     * anywhere - this is a standalone top-level project, not an
     * embedded sub-project) - load_vars() calls
     * load_state_file("pieces/apps/player_app/manager/gui_state.txt",
     * NULL) unconditionally (chtpm_parser_pal.c:1191, confirmed by
     * direct read) for exactly this case. */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/manager/gui_state.txt", project_root);
    char mkdir_cmd[MAX_PATH + 32];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s/pieces/apps/player_app/manager'", project_root);
    (void)!system(mkdir_cmd);

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "module_path=manager/+x/rtp_manager.+x\n");
    fprintf(f, "active_layout_id=project_menu.chtpm\n");
    fprintf(f, "save_root_kind=%s\n", using_xyzfs ? "xyzfs (real house)" : "local dev fallback");

    /* Real save list, one <button onClick="LOAD:name"> per real save
     * subdirectory - digit index starts at 2 (item 1 reserved,
     * matching fo-menu-sys.md's own house-wide convention). */
    char list_markup[8192] = "";
    int idx = 2;
    DIR *d = opendir(save_root);
    if (d) {
        struct dirent *entries[MAX_ITEMS];
        int n = 0;
        struct dirent *e;
        while ((e = readdir(d)) != NULL && n < MAX_ITEMS) {
            if (e->d_name[0] == '.') continue;
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", save_root, e->d_name);
            if (!is_dir(full)) continue;
            entries[n] = malloc(sizeof(struct dirent));
            memcpy(entries[n], e, sizeof(struct dirent));
            n++;
        }
        closedir(d);
        for (int i = 0; i < n; i++) {
            char meta_path[MAX_PATH + 32];
            snprintf(meta_path, sizeof(meta_path), "%s/%s/save_meta.txt", save_root, entries[i]->d_name);
            int turn = read_kv_int(meta_path, "turn", -1);
            char label[256];
            if (turn >= 0) snprintf(label, sizeof(label), "%s (turn %d)", entries[i]->d_name, turn);
            else snprintf(label, sizeof(label), "%s", entries[i]->d_name);
            char btn[512];
            snprintf(btn, sizeof(btn), "<button label=\"%s\" onClick=\"SET_LOAD:%s\" /><br/>", label, entries[i]->d_name);
            if (strlen(list_markup) + strlen(btn) < sizeof(list_markup) - 64) strcat(list_markup, btn);
            idx++;
            free(entries[i]);
        }
    }
    (void)idx;
    if (list_markup[0] == '\0') strcpy(list_markup, "<text label=\"(no saves yet)\" /><br/>");
    /* REAL, LIVE-CAUGHT BUG (2026-07-31): a plain custom gui_state.txt
     * variable is NOT enough to make raw markup (bare ${var} outside
     * any <text label="..."> wrapper) actually re-render - confirmed
     * by direct read of chtpm_parser_pal.c's own compose_frame():
     * it only re-runs parse_chtm() (the pass that redoes raw markup
     * substitution) when piece_methods/active_target_id/input_text
     * specifically change - any OTHER gui_state.txt variable is
     * substituted into memory (get_var/set_var) but the ALREADY-
     * PARSED XML tree is not rebuilt, so a bare ${save_list_markup}
     * injection point stays frozen at whatever it resolved to at the
     * very first parse (usually empty, before this daemon's first
     * write). Fix: publish the real save-list markup AS piece_methods
     * itself - already wired to force a reparse on change, and
     * project_menu.chtpm has no active_target_id/hero of its own to
     * fight over that variable with (load_dynamic_methods() only
     * fires when active_target_id is non-empty, which it isn't here).
     * project_menu.chtpm's own ${save_list_markup} reference was
     * updated to ${piece_methods} accordingly - keep them in sync if
     * you ever rename this again. */
    fprintf(f, "piece_methods=%s\n", list_markup);

    char raw_resp[MAX_LINE] = "";
    char resp_path[MAX_PATH];
    snprintf(resp_path, sizeof(resp_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *rf = fopen(resp_path, "r");
    if (rf) {
        char line[MAX_LINE], last[MAX_LINE] = "";
        while (fgets(line, sizeof(line), rf)) snprintf(last, sizeof(last), "%s", line);
        fclose(rf);
        snprintf(raw_resp, sizeof(raw_resp), "%s", trim(last));
    }
    fprintf(f, "last_message=%s\n", raw_resp);

    fclose(f);
}

static void handle_command(const char *cmd) {
    /* SET_ prefix is REQUIRED - chtpm_parser_pal.c's own send_command()
     * silently drops any onClick string that doesn't start with SET_/
     * OP:/MP3:/PROJECT_ACTION:/SETTINGS_PAGE:/LAUNCH:/LOAD_PROJECT:/
     * KEY: before ever reaching inject_command() - a real, documented
     * pitfall (#99 in the house-wide pitfalls doc: "Commands... will
     * be SILENTLY IGNORED. Always prefix project commands with SET_"),
     * confirmed by direct read of send_command()'s own prefix checks
     * this session. Do not rename these without keeping SET_. */
    if (strncmp(cmd, "SET_LOAD:", 9) == 0) { do_load(cmd + 9); return; }
    if (strcmp(cmd, "SET_SAVE_NEW") == 0) { do_save_new(); return; }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    resolve_project_root();
    resolve_save_root();

    /* MUST match project_menu.chtpm's own <interact src="..."> exactly -
     * chtpm_parser_pal.c's inject_command()/inject_raw_key() both route
     * through that per-layout global path (interact_history_path).
     * Reuses the SAME pieces/apps/player_app/interact_relay.txt
     * game.chtpm's own module already uses - matches the real,
     * established precedent (102.agy-txt's own agy_browser_manager.c +
     * file_browser_save.chtpm both reuse this exact shared path too,
     * confirmed by direct read this session) rather than inventing a
     * dedicated relay file - safe since only one module is ever the
     * active layout at a time. */
    char hist_path[MAX_PATH];
    snprintf(hist_path, sizeof(hist_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    long last_pos = 0;
    struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    /* Real, live-caught bug (2026-07-31): without this, gui_state.txt
     * is never written until the FIRST command is processed, so
     * project_menu.chtpm's own ${save_root_kind}/${save_list_markup}
     * render empty on first arrival - matches slop-ed-dev_manager.c's
     * own real main() calling write_editor_state()/trigger_render()/
     * hit_frame_marker() once unconditionally at startup, not only
     * inside the history-diff branch. */
    write_gui_state();
    hit_frame_marker();

    while (!g_shutdown) {
        if (!is_active_layout()) { usleep(100000); continue; }

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                int processed = 0;
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmdp = strstr(line, "COMMAND: ");
                        if (cmdp) { handle_command(trim(cmdp + 9)); processed = 1; }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
                if (processed) { write_gui_state(); hit_frame_marker(); }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        usleep(16667);
    }
    return 0;
}
