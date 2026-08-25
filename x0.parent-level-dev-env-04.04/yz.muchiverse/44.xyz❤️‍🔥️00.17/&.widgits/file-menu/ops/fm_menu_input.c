#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *in = fopen(path, "r");
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    int found = 0;
    if (in) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                fprintf(out, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    if (!found) fprintf(out, "%s=%s\n", key, val);
    fclose(out);
    rename(tmp, path);
}

/* resolve_xyzfs_home/documents - unchanged real logic, see the
 * pre-rebuild version of this file for the full save-bug.txt/PITFALL
 * account (this is the real, hard jail boundary for the browser). */
static int resolve_xyzfs_home(char *out, size_t out_sz) {
    char house_root_path[PATH_BUF];
    snprintf(house_root_path, sizeof(house_root_path), "%s/pieces/system/house_root.txt", project_root);
    char house_root[MAX_PATH] = "";
    FILE *f = fopen(house_root_path, "r");
    if (!f) return 0;
    if (!fgets(house_root, sizeof(house_root), f)) { fclose(f); return 0; }
    fclose(f);
    house_root[strcspn(house_root, "\r\n")] = '\0';
    if (!house_root[0]) return 0;

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
    char xyzfs[MAX_PATH] = "";
    read_kv(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));
    if (!xyzfs[0]) return 0;

    snprintf(out, out_sz, "%s/%s/home", house_root, xyzfs);
    return 1;
}

static int resolve_xyzfs_documents(char *out, size_t out_sz) {
    char home[PATH_BUF];
    if (!resolve_xyzfs_home(home, sizeof(home))) return 0;
    snprintf(out, out_sz, "%s/documents", home);
    char mkcmd[PATH_BUF + 32];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", out);
    { int _rc = system(mkcmd); (void)_rc; }
    return 1;
}

static int has_focus(char *session_root, size_t sr_sz) {
    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/pieces/system/focus.txt", project_root);
    if (access(focus, F_OK) != 0) return 0;
    char inbox[MAX_PATH] = "";
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    read_kv(focus, "session_root", session_root, sr_sz);
    if (!inbox[0] || !session_root[0]) return 0;
    if (access(session_root, F_OK) != 0) return 0;
    return 1;
}

#define FM_WIDGET_STATE_SUBDIR "pieces/system"

static void enqueue_cmd(const char *cmd) {
    char cmd_path[PATH_BUF], wdir[PATH_BUF];
    snprintf(cmd_path, sizeof(cmd_path), "%s/ops/+x/fm_enqueue_cmd.+x", project_root);
    snprintf(wdir, sizeof(wdir), "%s/%s", project_root, FM_WIDGET_STATE_SUBDIR);
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = { cmd_path, wdir, (char *)cmd, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static void enqueue_cmd_with_path(const char *cmd, const char *path_arg) {
    char cmd_path[PATH_BUF], wdir[PATH_BUF];
    snprintf(cmd_path, sizeof(cmd_path), "%s/ops/+x/fm_enqueue_cmd.+x", project_root);
    snprintf(wdir, sizeof(wdir), "%s/%s", project_root, FM_WIDGET_STATE_SUBDIR);
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = { cmd_path, wdir, (char *)cmd, (char *)path_arg, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static void set_focus(const char *session_root_target) {
    char focus_path[PATH_BUF];
    snprintf(focus_path, sizeof(focus_path), "%s/ops/+x/fm_set_focus.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        char wdir[PATH_BUF];
        snprintf(wdir, sizeof(wdir), "%s/pieces/system", project_root);
        char *args[] = { focus_path, wdir, (char *)session_root_target, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static int scan_entry_count(const char *dir, const char *filter) {
    char cmd[MAX_PATH + 64];
    if (filter[0])
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\" \"%s\" 2>/dev/null | wc -l",
                 project_root, dir, filter);
    else
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\" 2>/dev/null | wc -l",
                 project_root, dir);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char buf[64];
    if (!fgets(buf, sizeof(buf), p)) { pclose(p); return 0; }
    pclose(p);
    return atoi(buf);
}

static void read_entry_at_index(const char *dir, const char *filter, int idx,
                                 char *name, size_t name_sz, int *is_dir) {
    name[0] = '\0';
    *is_dir = 0;
    char cmd[MAX_PATH + 64];
    if (filter[0])
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\" \"%s\" 2>/dev/null",
                 project_root, dir, filter);
    else
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\" 2>/dev/null",
                 project_root, dir);
    FILE *p = popen(cmd, "r");
    if (!p) return;
    char line[MAX_LINE];
    int i = 0;
    while (fgets(line, sizeof(line), p)) {
        if (i == idx) {
            char kind[16] = "";
            sscanf(line, "%15[^|]|%511[^|]", kind, name);
            *is_dir = (strcmp(kind, "DIR") == 0);
            break;
        }
        i++;
    }
    pclose(p);
    (void)name_sz;
}

static void bump_screen(void) {
    char marker[PATH_BUF];
    snprintf(marker, sizeof(marker), "%s/pieces/display/fm_screen_changed.txt", project_root);
    FILE *mf = fopen(marker, "a");
    if (mf) { fprintf(mf, "X\n"); fclose(mf); }
}

/* ============================================================
 * 2026-07-30 REBUILD (PITFALL 65 fix, editor-widget-app-refactor-j30.txt).
 * fm_compose_frame.c now emits real <button onClick="KEY:n"> markup -
 * chtpm_parser_pal.c owns ALL navigation/focus/highlighting itself
 * (arrows, digit-jump, Enter-to-activate). This file no longer hand-
 * tracks cursor_pos for navigation at all - it ONLY ever receives a
 * KEY:n integer for a button the user has ALREADY activated (a real
 * chtpm-native click or Enter-on-focused-button), decodes it into a
 * string action at ONE clearly-marked boundary per mode (same real
 * split as 102.agy-txt/manager/agy_browser_manager.c's own
 * decode_key_to_action()/dispatch_action(), built this same session),
 * and runs the SAME real business logic this file always had (xyzfs
 * jail, quoting fixes, click-to-load-in-LOAD-not-SAVE, empty-FILE-
 * field Enter-engages convention) - none of that changes, only HOW a
 * final action gets identified changes.
 *
 * SEARCH/FILE are now real <cli_io> elements in file-menu.chtpm
 * (visibility-gated on ${fm_browser_active}) instead of hand-tracked
 * fields inside this op's own cursor_pos state - their content is
 * read fresh from cli_buffers.txt ONLY at the moment CONFIRM fires
 * (matches TPMOS's own agy-text-editor_manager.c real precedent,
 * confirmed this same session: read_file_path_input()/read_search_
 * query_input() are called at discrete dispatch points, never
 * continuously polled and echoed back - a continuous echo races
 * against chtpm_parser_pal.c's own sync_cli_input_from_gui_state()
 * restore pass and stomps the live-typed buffer, confirmed/fixed in
 * agy_browser_manager.c this same session).
 * ============================================================ */

static void read_cli_field(const char *fm_path_unused, char prefix, char *out, size_t out_sz) {
    (void)fm_path_unused;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/cli_buffers.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == prefix) {
            char *v = line + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
        }
    }
    fclose(f);
}

/* execute_menu_option - 2026-07-30 CORRECTED (direct instruction:
 * "always use href to go to another page... that was the whole point
 * of the refactor"): options 3/4/5/6 (SAVE AS/LOAD/CHANGE FOCUS/PAIR
 * FROM FILE) are real screen transitions now - real
 * <button href="..."> in fm_compose_frame.c's own build_main_menu_
 * markup(), handled entirely by chtpm_parser_pal.c itself, never
 * reaching this op at all. Only NEW(1)/SAVE(2) remain real onClick=
 * "KEY:n" actions (they fire a command and stay on THIS screen). */
static void execute_menu_option(const char *fm_path, int option) {
    char session_root[MAX_PATH] = "";
    int focused = has_focus(session_root, sizeof(session_root));

    if (option == 1) {
        if (focused) {
            enqueue_cmd("NEW");
            write_kv(fm_path, "status_line", "NEW sent");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot NEW");
        }
    } else if (option == 2) {
        if (focused) {
            enqueue_cmd("SAVE");
            write_kv(fm_path, "status_line", "SAVE sent");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot SAVE");
        }
    }
}

/* dispatch_main_menu - KEY:1/2 map to execute_menu_option(1/2), real
 * chtpm-native button activation. */
static void dispatch_main_menu(const char *fm_path, int key) {
    if (key == 1 || key == 2) execute_menu_option(fm_path, key);
}

/* active_gui_index - real, confirmed mechanism (direct read of
 * chtpm_parser_pal.c's own cli_io Enter handler, 2026-07-30): pressing
 * Enter while a cli_io is actively engaged AND non-empty is a real,
 * house-native "Send" trigger - the parser itself saves the full typed
 * value to gui_state.txt, clears its own input_buffer, and calls
 * inject_raw_key(13) ("Trigger the Send action by dispatching KEY:13"
 * - its own comment). That raw 13 lands in the SAME interact_relay.txt
 * this file's own main() reads, indistinguishable by VALUE ALONE from
 * a coincidental KEY:n button match (13 could easily collide with a
 * real select_N/CONFIRM integer depending on entry_count) - matches
 * TPMOS's own agy-text-editor_manager.c's own process_key(), which
 * resolves the identical ambiguity by checking get_active_gui_index()
 * ==2 (its own FILE field) before ever treating a raw Enter as
 * SET_SAVE_ACTION/SET_LOAD_ACTION. Same real fix applied and confirmed
 * in 102.agy-txt/manager/agy_browser_manager.c this same session.
 * SEARCH is always interactive_idx 1 and FILE always 2 in every real
 * browser layout this project declares (both statically declared
 * first, before the dynamically-injected button block). */
static int active_gui_index(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_is_typing.txt", project_root);
    char typing[8] = "";
    FILE *f = fopen(path, "r");
    if (f) { if (fgets(typing, sizeof(typing), f)) {} fclose(f); }

    /* REAL RACE, live-caught 2026-07-30 (102.agy-txt/manager/
     * agy_browser_manager.c's own identical copy of this function, same
     * session): chtpm_parser_pal.c writes active_gui_index.txt via a
     * plain fopen(path,"w") + fprintf (not an atomic tmp+rename), so a
     * reader arriving between the truncate and the write sees 0 bytes.
     * A short bounded retry clears this narrow race without masking a
     * genuinely different (non-transient) empty-file state. */
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_index.txt", project_root);
    char idx_str[16] = "";
    for (int attempt = 0; attempt < 5; attempt++) {
        idx_str[0] = '\0';
        f = fopen(path, "r");
        if (f) { if (fgets(idx_str, sizeof(idx_str), f)) {} fclose(f); }
        if (idx_str[0] != '\0') break;
        usleep(2000);
    }

    if (typing[0] != '1') return 0;
    return atoi(idx_str);
}

/* dispatch_file_browser - real KEY:n decode: 1=BACK,
 * 2..(1+entry_count)=select_N, (2+entry_count)=CONFIRM. CANCEL is a
 * real href now (fm_compose_frame.c's own build_file_browser_markup())
 * - never reaches this op. Must match that function's own numbering
 * exactly. browse_mode (save_as/load/pair) is inferred from
 * current_layout.txt (matches 102.agy-txt's own is_save inference for
 * file_browser_save.chtpm/file_browser_load.chtpm), not a separate
 * stored state key - a real href to a DIFFERENT layout file IS the
 * mode switch now, same real mechanism agy-txt already proved this
 * session. 2026-07-30 CORRECTED: LOAD-success/PAIR-success used to
 * write mode=main_menu to auto-return there - a real op cannot trigger
 * a layout transition by writing a file (confirmed this session,
 * chtpm_parser_pal.c's own href handling only ever fires from a real
 * button's own activation) - matches agy_browser_manager.c's own real
 * behavior for the identical case: show "Command sent"/"Paired!" and
 * stay on this screen, the user clicks the real CANCEL href
 * themselves. */
static void dispatch_file_browser(const char *fm_path, const char *browse_mode, int key) {
    char browse_dir[PATH_BUF], path_buffer[MAX_LINE], search_text[MAX_LINE];
    read_kv(fm_path, "browse_dir", browse_dir, sizeof(browse_dir));
    read_kv(fm_path, "path_buffer", path_buffer, sizeof(path_buffer));
    read_kv(fm_path, "search_text", search_text, sizeof(search_text));

    int entry_count = 0;
    if (browse_dir[0]) entry_count = scan_entry_count(browse_dir, search_text);

    int back_key = 1;
    int first_entry_key = 2;
    int confirm_key = 2 + entry_count;

    /* Real Enter-from-cli_io "Send" relay (see active_gui_index()'s own
     * header comment) - a raw 10/13 while the FILE field is actively
     * engaged means "submit directly", matching TPMOS's own real
     * convention; while SEARCH is engaged it's a real cli_io-internal
     * event with no separate action here (search has no live-filter-
     * commit wired up) - swallow it rather than risk it coincidentally
     * matching an unrelated select_N/CONFIRM key purely by numeric
     * accident. Checked BEFORE the generic key-range logic below so it
     * can never collide with it. */
    if (key == 10 || key == 13) {
        int agi = active_gui_index();
        if (agi == 2) key = confirm_key;
        else if (agi == 1) return;
    }

    if (key == back_key) {
        char xyzfs_home[PATH_BUF];
        int have_root = resolve_xyzfs_home(xyzfs_home, sizeof(xyzfs_home));
        if (have_root && strcmp(browse_dir, xyzfs_home) == 0) return; /* at jail root */
        char *last = strrchr(browse_dir, '/');
        if (last && last != browse_dir) *last = '\0';
        else if (last == browse_dir) browse_dir[1] = '\0';
        if (have_root) {
            size_t home_len = strlen(xyzfs_home);
            if (strncmp(browse_dir, xyzfs_home, home_len) != 0 ||
                (browse_dir[home_len] != '\0' && browse_dir[home_len] != '/')) {
                snprintf(browse_dir, sizeof(browse_dir), "%s", xyzfs_home);
            }
        }
        write_kv(fm_path, "browse_dir", browse_dir);
        return;
    }

    if (key >= first_entry_key && key < confirm_key) {
        int entry_idx = key - first_entry_key;
        char name[512] = "";
        int is_dir = 0;
        read_entry_at_index(browse_dir, search_text, entry_idx, name, sizeof(name), &is_dir);
        if (!name[0]) return;
        if (is_dir) {
            char new_dir[PATH_BUF];
            snprintf(new_dir, sizeof(new_dir), "%s/%s", browse_dir, name);
            write_kv(fm_path, "browse_dir", new_dir);
            return;
        }
        char full_path[PATH_BUF];
        snprintf(full_path, sizeof(full_path), "%s/%s", browse_dir, name);
        if (strcmp(browse_mode, "pair") == 0) {
            if (access(full_path, F_OK) == 0) {
                set_focus(full_path);
                write_kv(fm_path, "status_line", "Paired! Click CANCEL to go back.");
            } else {
                write_kv(fm_path, "browse_dir", full_path);
                write_kv(fm_path, "status_line", "Not a session root; try parent");
            }
        } else if (strcmp(browse_mode, "load") == 0) {
            /* Real click-to-load (PITFALL 64, still real, still true) -
             * one real activation on a listed file loads it directly.
             * SAVE mode's own identical-looking selection stays
             * pre-fill-then-confirm below (protects against an
             * accidental overwrite). */
            enqueue_cmd_with_path("LOAD", full_path);
            write_kv(fm_path, "path_buffer", "");
            write_kv(fm_path, "status_line", "Command sent");
        } else {
            write_kv(fm_path, "path_buffer", full_path);
        }
        return;
    }

    if (key == confirm_key) {
        /* Real content comes from the cli_io field, read fresh right
         * here (matches TPMOS's own Enter/SET_SAVE_ACTION-time read -
         * NOT a continuously-polled value, see this file's own header
         * comment). A click-to-load selection already wrote path_buffer
         * directly above without touching the FILE cli_io at all, so
         * check that first; otherwise fall back to whatever's actually
         * typed in the FILE field right now. */
        char cli_val[MAX_LINE] = "";
        read_cli_field(fm_path, 'f', cli_val, sizeof(cli_val));
        const char *use_path = path_buffer[0] ? path_buffer : cli_val;

        if (strcmp(browse_mode, "pair") == 0) {
            if (use_path[0]) {
                set_focus(use_path);
                write_kv(fm_path, "status_line", "Paired! Click CANCEL to go back.");
            } else {
                write_kv(fm_path, "status_line", "No path selected for pairing");
            }
        } else if (use_path[0]) {
            const char *prefix = (strcmp(browse_mode, "load") == 0) ? "LOAD" : "SAVE_AS";
            enqueue_cmd_with_path(prefix, use_path);
            write_kv(fm_path, "path_buffer", "");
            write_kv(fm_path, "status_line", "Command sent");
        } else {
            write_kv(fm_path, "status_line", "No path selected");
        }
        return;
    }
}

/* dispatch_peer_list - real KEY:1..peer_count=select. CANCEL is a real
 * href now (fm_compose_frame.c's own build_peer_list_markup()) - never
 * reaches this op. Same real "op can't trigger a layout transition"
 * fix as dispatch_file_browser() above - Focused!/Focus failed stays
 * on this screen, real CANCEL href takes the user back. */
static void dispatch_peer_list(const char *fm_path, int key) {
    char count_cmd[MAX_PATH + 64];
    snprintf(count_cmd, sizeof(count_cmd),
             "(\"%s/ops/+x/ledger_peers.+x\" editor 2>/dev/null; "
             "\"%s/ops/+x/ledger_peers.+x\" app 2>/dev/null; "
             "\"%s/ops/+x/ledger_peers.+x\" widget 2>/dev/null) | wc -l",
             project_root, project_root, project_root);
    int peer_count = 0;
    FILE *p = popen(count_cmd, "r");
    if (p) { char buf[64]; if (fgets(buf, sizeof(buf), p)) peer_count = atoi(buf); pclose(p); }

    if (key >= 1 && key <= peer_count) {
        char peer_cmd[MAX_PATH + 64];
        snprintf(peer_cmd, sizeof(peer_cmd),
                 "(\"%s/ops/+x/ledger_peers.+x\" editor 2>/dev/null; "
                 "\"%s/ops/+x/ledger_peers.+x\" app 2>/dev/null; "
                 "\"%s/ops/+x/ledger_peers.+x\" widget 2>/dev/null) | head -%d | tail -1 | cut -d'|' -f1",
                 project_root, project_root, project_root, key);
        char sr_buf[MAX_PATH] = "";
        FILE *sp = popen(peer_cmd, "r");
        if (sp) {
            if (fgets(sr_buf, sizeof(sr_buf), sp)) {
                sr_buf[strcspn(sr_buf, "\n")] = '\0';
                if (sr_buf[0]) set_focus(sr_buf);
            }
            pclose(sp);
        }
        write_kv(fm_path, "status_line", sr_buf[0] ? "Focused! Click CANCEL to go back." : "Focus failed");
    }
}

/* PASTE mode - unchanged real mechanism, but now only ever targets the
 * FILE cli_io's own real backing store (cli_buffers.txt), matching
 * chtpm_parser_pal.c's own real per-keystroke sync format for a real
 * <cli_io id="file_path_input"> element (id[0]='f' prefix). SEARCH is
 * a real cli_io too now (id[0]='s'); this project's own harness should
 * paste into whichever field is actually focused via the same real
 * interact_relay.txt/cli_io channel every other real cli_io in this
 * house uses - this op no longer owns that state at all, so it has
 * nothing left to do for PASTE and is kept only as a documented no-op
 * for any caller still invoking it this way. */
static void handle_paste(const char *fm_path, const char *text) {
    (void)fm_path;
    (void)text;
}

int main(int argc, char **argv) {
    resolve_root();

    if (argc >= 3 && strcmp(argv[1], "PASTE") == 0) {
        char fm_path[PATH_BUF];
        snprintf(fm_path, sizeof(fm_path), "%s/pieces/system/fm_state.txt", project_root);
        handle_paste(fm_path, argv[2]);
        return 0;
    }

    int key = (argc > 1) ? atoi(argv[1]) : 0;
    /* REAL BUG, live-caught 2026-07-30: chtpm_parser_pal.c's own
     * send_command()'s KEY:n handling (confirmed via direct source
     * read) is NOT a plain integer relay for n in 0-9 - it calls
     * inject_raw_key('0' + k), i.e. it sends the ASCII CODE of the
     * digit (e.g. onClick="KEY:3" relays the byte 51, not 3) - a
     * real button activation is designed to look EXACTLY like the
     * user having typed that digit as a real keystroke, matching how
     * a PAL script's own raw digit-comparison would already expect to
     * see it. Only for n >= 10 does it send the raw integer unshifted.
     * decode_key_to_action() in every dispatch_*() function below
     * expects the ORIGINAL small integer (1=back, 2=first entry, ...) -
     * un-shift here, once, before any of them ever see the value.
     * Real, confirmed, reproducible: without this, EVERY single-digit
     * KEY:n button in this file (the entire main_menu, BACK, the first
     * 9 directory entries, CONFIRM/CANCEL whenever entry_count < 8)
     * silently never dispatched - digit-jump nav still worked (that's
     * chtpm_parser_pal's own internal focus logic, unrelated to this
     * relay), but Enter-to-activate did nothing. */
    if (key >= '0' && key <= '9') key -= '0';
    if (key != 0) {
        char fm_path[PATH_BUF];
        snprintf(fm_path, sizeof(fm_path), "%s/pieces/system/fm_state.txt", project_root);

        /* 2026-07-30 CORRECTED: dispatch on the REAL current layout
         * (chtpm_parser_pal.c's own pieces/display/current_layout.txt)
         * instead of a hand-tracked "mode" key - matches
         * 102.agy-txt/manager/agy_browser_manager.c's own layout-based
         * dispatch exactly (direct instruction: real hrefs are the
         * whole point of this refactor, not an internal mode switch). */
        char layout_path[PATH_BUF], layout[256] = "";
        snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
        FILE *lf = fopen(layout_path, "r");
        if (lf) {
            if (fgets(layout, sizeof(layout), lf)) layout[strcspn(layout, "\r\n")] = '\0';
            fclose(lf);
        }

        if (strstr(layout, "file_menu_browser_save.chtpm")) {
            dispatch_file_browser(fm_path, "save_as", key);
        } else if (strstr(layout, "file_menu_browser_load.chtpm")) {
            dispatch_file_browser(fm_path, "load", key);
        } else if (strstr(layout, "file_menu_browser_pair.chtpm")) {
            dispatch_file_browser(fm_path, "pair", key);
        } else if (strstr(layout, "file_menu_peers.chtpm")) {
            dispatch_peer_list(fm_path, key);
        } else {
            dispatch_main_menu(fm_path, key);
        }
    }

    bump_screen();
    return 0;
}
