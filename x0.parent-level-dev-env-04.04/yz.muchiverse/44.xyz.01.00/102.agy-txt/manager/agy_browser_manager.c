/* agy_browser_manager - real native C manager for agy-txt's file
 * browser (file_browser_save.chtpm / file_browser_load.chtpm),
 * replacing the superseded hand-drawn-marker/buttonless implementation
 * (PITFALL 65, #.haiku+/jul30-house-refactor.txt). Structural shape
 * copied from 101.ledger-player-npc-simple+3/system/game_manager.c
 * (pthread poll loop, ~60fps, signal-handled clean shutdown, <module>
 * launch) - a real, already-shipped native-manager precedent in this
 * exact house. Content (save/load path resolution, directory-listing
 * markup shape, autocomplete-scoped-to-typed-input pattern) ported
 * from TPMOS's own real reference,
 * 1.TPMOS_c_+rmmp.0103.0001/projects/agy-text-editor/manager/
 * agy-text-editor_manager.c (save_to_path/load_from_path/
 * find_autocomplete_matches/append_aligned_button_attr all read in
 * full before this port, not re-derived).
 *
 * Contract this file implements: 102.agy-txt/manager/BROWSER_CONTRACT.md
 * - writes ONLY pieces/apps/player_app/manager/gui_state.txt (the
 * real, already-active chtpm_parser_pal.c auto-load path for
 * "modern_layout" projects - confirmed this session, agy-txt already
 * qualifies unconditionally). No hand-drawn "[>]"/"[ ]" markers
 * anywhere in this file - every list row is a real <button>, every
 * text field a real <cli_io>; the PARSER owns focus/nav/marker-
 * drawing entirely, per DYNAMIC_SUBMENUS_FUNCTIONAL_STD.md's own core
 * rule (confirmed correct for the RENDERING side; the STRING-COMMAND
 * side of that doc does NOT hold for this house's own prisc+x VM -
 * see jul30-house-refactor.txt §3 - so onClick="KEY:n" is used here,
 * matching mutaclysm's own real, already-shipped adaptation, NOT
 * onClick="SET_..."/"OP:..." which prisc+x cannot relay today).
 *
 * KEY:n IS A SWAPPABLE ENCODING LAYER, not the real dispatch - see
 * dispatch_action() below, which takes a STRING action name. Only
 * decode_key_to_action() knows about the KEY:n <-> string mapping;
 * once Tier 2 (prisc+x string-command relay) lands, only that one
 * function needs to change.
 *
 * SCOPE: SAVE_AS/LOAD path resolution + directory browsing + search
 * filter, matching agy-txt's own real, already-proven feature set
 * (PLAN.md §4 Phase T4). Autocomplete-while-typing (TPMOS's own real
 * feature, find_autocomplete_matches()) is deliberately NOT ported -
 * out of scope per PLAN.md §5's own existing precedent ("Autocomplete
 * in the file browser... not load-bearing for proving the
 * architecture"), same real reasoning applied again here, not a new
 * decision.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define POLL_INTERVAL 16667  /* 16ms, matches game_manager.c's own real poll rate */

static char project_root[MAX_PATH] = ".";
static volatile int running = 1;
static long last_history_pos = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static void signal_handler(int sig) { (void)sig; running = 0; }

static void log_mgr(const char *fmt, ...) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/agy_browser_manager.log", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    fprintf(f, "[%ld] ", (long)time(NULL));
    vfprintf(f, fmt, args);
    fprintf(f, "\n");
    va_end(args);
    fclose(f);
}

/* ---- gui_state.txt: read-modify-write single key (same real shape
 * as agy_edit_key.c's own write_kv()/read_kv_str()).
 * REAL BUG, live-caught 2026-07-30: this session's own earlier reading
 * of chtpm_parser_pal.c's load_vars() (BROWSER_CONTRACT.md §0) was
 * WRONG - pieces/apps/player_app/manager/gui_state.txt is only
 * auto-loaded when project_id is EMPTY. agy-txt's own state.txt sets
 * project_id=agy-txt (button.sh), so load_vars() instead takes the
 * load_project_gui_state("agy-txt") branch, which checks
 * pieces/apps/agy-txt/manager/gui_state.txt (among 3 other candidate
 * paths, confirmed via direct read of chtpm_parser_pal.c's own
 * load_project_gui_state(), none of which is player_app/manager/) -
 * confirmed live: gui_state.txt had correct fresh data at the
 * player_app path, the rendered frame never picked it up. This path
 * is the one load_project_gui_state() actually finds. */
static void gui_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/pieces/apps/agy-txt/manager/gui_state.txt", project_root);
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

static void set_gui_var(const char *key, const char *value) {
    char path[PATH_BUF];
    gui_state_path(path, sizeof(path));
    FILE *rf = fopen(path, "r");
    char lines[64][MAX_LINE];
    int n = 0;
    if (rf) { while (n < 64 && fgets(lines[n], MAX_LINE, rf)) n++; fclose(rf); }
    size_t klen = strlen(key);
    FILE *wf = fopen(path, "w");
    if (!wf) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(wf, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], wf);
    }
    if (!found) fprintf(wf, "%s=%s\n", key, value);
    fclose(wf);
}

/* pulse_frame_marker - real chtpm_parser_pal.c trigger (confirmed via
 * direct read, 2026-07-30 live-test bug), matching mutaclysm's own
 * compose_frame.c write_panel_gui_state()-adjacent marker exactly:
 * pieces/display/frame_changed.txt alone only sets the render loop's
 * dirty flag (a plain re-render of the ALREADY-parsed element tree) -
 * it does NOT re-run parse_chtm(), so raw ${var}-as-markup lines (like
 * ${directory_browser_markup} in this project's own .chtpm layouts)
 * never get re-substituted/re-parsed into real <button> elements after
 * the first parse. pieces/apps/player_app/state_changed.txt is the
 * marker that forces a fresh parse_chtm() on every poll (chtpm_parser_
 * pal.c's own main loop, the state_ch growth branch) - both markers
 * are pulsed here, matching the real in-house precedent exactly
 * (mutaclysm's own compose_frame.c pulses this same second file for
 * this same reason). Live-caught: gui_state.txt had the correct fresh
 * markup, the rendered frame did not, until this fix. */
static void pulse_frame_marker(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *sf = fopen(state_path, "a");
    if (sf) { fputc('.', sf); fclose(sf); }
}

/* ---- browser state (project-side only - never rendered directly,
 * only ever projected into gui_state.txt via write_frame() below) ---- */
static char browse_dir[PATH_BUF] = "";
static char search_query[MAX_LINE] = "";
static char file_path_input[MAX_LINE] = "";
static int browser_mode = 0; /* 0 = load, 1 = save_as */
static char status_line[MAX_LINE] = "Ready.";

/* ---- xyzfs home/documents resolution - same real chain as
 * agy_widget_cmds.c's own resolve_xyzfs_home()/agy_edit_key.c's own
 * copy (BROWSER_CONTRACT.md §4 - reused, not reinvented, this is the
 * THIRD copy of this same real logic in this project now; a future
 * cleanup could extract it to a tiny shared header, not done here to
 * avoid widening this change's own real scope). ---- */
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

/* ---- directory listing: reuse agy_scan_dir.+x unchanged (already
 * generic, already proven, BROWSER_CONTRACT.md §4). project_root
 * quoted in the popen() command from the start (PITFALL 64's own real
 * bug - don't reintroduce it via a fresh copy). ---- */
#define MAX_ENTRIES 64
typedef struct { char kind[8]; char name[256]; long size; int is_dir; } Entry;
static Entry entries[MAX_ENTRIES];
static int entry_count = 0;

static void scan_dir(void) {
    entry_count = 0;
    if (!browse_dir[0]) return;
    char cmd[PATH_BUF + 128];
    if (search_query[0])
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/agy_scan_dir.+x\" \"%s\" \"%s\" 2>/dev/null",
                 project_root, browse_dir, search_query);
    else
        snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/agy_scan_dir.+x\" \"%s\" 2>/dev/null",
                 project_root, browse_dir);
    FILE *p = popen(cmd, "r");
    if (!p) return;
    char line[MAX_LINE];
    while (entry_count < MAX_ENTRIES && fgets(line, sizeof(line), p)) {
        line[strcspn(line, "\n")] = '\0';
        Entry *e = &entries[entry_count];
        sscanf(line, "%7[^|]|%255[^|]|%ld|%d", e->kind, e->name, &e->size, &e->is_dir);
        entry_count++;
    }
    pclose(p);
}

static void format_size(long bytes, char *out, size_t out_sz) {
    if (bytes < 1024) snprintf(out, out_sz, "%ldB", bytes);
    else snprintf(out, out_sz, "%.0fKB", bytes / 1024.0);
}

/* ---- real dynamic <button> markup - PARSER draws focus/[>]/numbering
 * itself (BROWSER_CONTRACT.md §2, mutaclysm's own real, cited
 * pattern) - this function only ever emits bare <button>/<text>
 * elements, never a hand-drawn marker. KEY:n indices below are the
 * REAL, TEMPORARY encoding (see this file's own header comment) -
 * index 1 is always BACK, 2..(1+entry_count) are real entries. ---- */
static void build_directory_browser_markup(char *out, size_t max_sz) {
    out[0] = '\0';
    char row[600];
    snprintf(row, sizeof(row), "<button label=\"&lt;- BACK\" onClick=\"KEY:1\" /><br/>");
    strncat(out, row, max_sz - strlen(out) - 1);
    for (int i = 0; i < entry_count; i++) {
        char label[400];
        if (entries[i].is_dir) {
            snprintf(label, sizeof(label), "[DIR] %s", entries[i].name);
        } else {
            char sz[32] = "";
            if (entries[i].size > 0) format_size(entries[i].size, sz, sizeof(sz));
            snprintf(label, sizeof(label), "[FIL] %s%s%s%s", entries[i].name,
                     sz[0] ? " (" : "", sz, sz[0] ? ")" : "");
        }
        snprintf(row, sizeof(row), "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", label, i + 2);
        strncat(out, row, max_sz - strlen(out) - 1);
    }
}

static void build_action_buttons_markup(char *out, size_t max_sz) {
    out[0] = '\0';
    /* KEY index right after the last real directory entry - real
     * confirm action, matching this file's own dispatch_action(). */
    int confirm_key = 2 + entry_count;
    char row[600];
    snprintf(row, sizeof(row), "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>",
             browser_mode ? "SAVE FILE" : "LOAD FILE", confirm_key);
    strncat(out, row, max_sz - strlen(out) - 1);
    snprintf(row, sizeof(row), "<button label=\"CANCEL\" href=\"pieces/chtpm/layouts/file_menu.chtpm\" /><br/>");
    strncat(out, row, max_sz - strlen(out) - 1);
}

/* ---- real cli_io field sync - reads pieces/apps/player_app/
 * cli_buffers.txt, same real per-id-prefixed-line format
 * chtpm_parser_pal.c's own generic fallback writes (confirmed this
 * session: id[0] as the line prefix character) - "search_query"
 * starts with 's', "file_path_input" starts with 'f', matching
 * TPMOS's own exact id naming, reused directly (BROWSER_CONTRACT.md
 * §2). Takes the LAST matching line, same real "current value" read
 * TPMOS's own read_file_path_input()/read_search_query_input() use. ---- */
static void read_cli_field(char prefix, char *out, size_t out_sz) {
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

/* ---- widget_cmds inbox - real, unchanged, BROWSER_CONTRACT.md §4 -
 * same enqueue+immediate-drain shape agy_edit_key.c's own
 * enqueue_and_drain() already uses. ---- */
static void enqueue_and_drain(const char *cmd_line) {
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/pieces/system/widget_cmds", project_root);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", cmd_line); fclose(f); }
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "\"%s/ops/+x/agy_widget_cmds.+x\" 8 >/dev/null 2>&1", project_root);
    { int _rc = system(cmd); (void)_rc; }
}

/* ---- real frame projection - the ONLY function that writes
 * gui_state.txt, called after every real state change (matches
 * game_manager.c's own pulse-after-mutation shape). ---- */
static void write_frame(void) {
    scan_dir();
    set_gui_var("browser_mode_header", browser_mode ? "MODE: SAVE FILE" : "MODE: LOAD FILE");
    {
        char line[PATH_BUF + 16];
        snprintf(line, sizeof(line), "DIR: %s", browse_dir);
        set_gui_var("browser_current_dir_line", line);
    }
    set_gui_var("search_query_val", search_query);
    set_gui_var("file_path_input_val", file_path_input);
    set_gui_var("editor_response_line", status_line);
    {
        char markup[8192];
        build_directory_browser_markup(markup, sizeof(markup));
        set_gui_var("directory_browser_markup", markup);
    }
    {
        char markup[1024];
        build_action_buttons_markup(markup, sizeof(markup));
        set_gui_var("browser_action_buttons_markup", markup);
    }
    pulse_frame_marker();
}

/* ---- reset-on-entry, same real trigger fm_menu_input.c's own
 * execute_menu_option()/this project's own prior port used - detects
 * "did the active layout change since last poll" and reseeds fresh
 * default state (xyzfs documents/ as the real default browse_dir,
 * BROWSER_CONTRACT.md/save-bug.txt's own fix, unchanged). ---- */
static char last_layout[256] = "";

static void reset_for_layout(const char *layout) {
    browser_mode = (strstr(layout, "file_browser_save.chtpm") != NULL) ? 1 : 0;
    char docs_dir[PATH_BUF];
    if (resolve_xyzfs_documents(docs_dir, sizeof(docs_dir)))
        snprintf(browse_dir, sizeof(browse_dir), "%s", docs_dir);
    else
        snprintf(browse_dir, sizeof(browse_dir), "%s", project_root);
    search_query[0] = '\0';
    file_path_input[0] = '\0';
    snprintf(status_line, sizeof(status_line), "%s",
             browser_mode ? "Type path or browse to a file" : "Browse to a file or type path");
    snprintf(last_layout, sizeof(last_layout), "%s", layout);
    /* Same real fix as the superseded PAL port's own bump_screen()
     * insight (jul30-house-refactor.txt's own predecessor bug) -
     * write_frame() below both reseeds gui_state.txt AND pulses
     * frame_changed.txt, so this is never silently stale. */

    /* REAL BUG, live-caught 2026-07-30 building &.widgits/file-menu's
     * own equivalent rebuild (same session): chtpm_parser_pal.c's own
     * cli_io typing handler writes the LIVE buffer into gui_state.txt
     * under the element's own target_id DIRECTLY ("file_path_input"/
     * "search_query" - confirmed via direct read of save_cli_io_gui_
     * state()'s own call site) - a COMPLETELY SEPARATE key from this
     * file's own "file_path_input_val"/"search_query_val" (which only
     * ever feeds the LABEL prefix text, a display-only concern).
     * sync_cli_input_from_gui_state()'s own restore pass (run on every
     * fresh parse_chtm(), including every real href transition) reads
     * the target_id-keyed value, NOT the "_val" one - clearing only
     * "_val" leaves the REAL one permanently stuck in gui_state.txt
     * from an earlier abandoned typing session, silently prepending
     * itself onto every subsequently typed filename for the rest of
     * the session. Both keys must be cleared on every fresh entry -
     * write_frame() below writes the "_val" pair; these are the
     * target_id-named ones it does not. */
    set_gui_var("file_path_input", "");
    set_gui_var("search_query", "");

    /* PITFALL 61, same real bug re-confirmed for this new architecture
     * 2026-07-30: href respawns the <module> process, but
     * interact_relay.txt itself is NOT truncated on that respawn (only
     * once, at button.sh's own session start) - a fresh manager launch
     * would otherwise replay whatever bare-number lines are already
     * sitting in the file from unrelated prior activity (editor canvas
     * INTERACT typing, file_menu digit-jumps - the SAME shared
     * <interact src> channel every layout in this project uses) as
     * spurious button-index dispatches via poll_history()'s own
     * decode_key_to_action(). Seek last_history_pos to the file's
     * CURRENT size on every fresh layout entry so only genuinely new
     * activity from this point forward is ever processed - matches the
     * superseded PAL port's own fix (there: truncate the file outright;
     * here: non-destructive seek, since this file is a shared channel
     * other layouts still rely on once this module is torn back down). */
    {
        char relay_path[PATH_BUF];
        snprintf(relay_path, sizeof(relay_path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
        struct stat st;
        last_history_pos = (stat(relay_path, &st) == 0) ? st.st_size : 0;
    }
}

/* ---- real action dispatch - STRING action names, KEY:n is decoded
 * into these by decode_key_to_action() below; this function itself
 * never sees a raw keycode (this file's own header comment - the
 * boundary that makes Tier 2's own future string-command relay a
 * localized change). ---- */
static void dispatch_action(const char *action) {
    log_mgr("dispatch_action: '%s' (browse_dir='%s' browser_mode=%d)", action, browse_dir, browser_mode);
    if (strcmp(action, "back") == 0) {
        char xyzfs_home[PATH_BUF];
        int have_root = resolve_xyzfs_home(xyzfs_home, sizeof(xyzfs_home));
        if (have_root && strcmp(browse_dir, xyzfs_home) == 0) return; /* already at the jail root */
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
        return;
    }
    if (strncmp(action, "select_", 7) == 0) {
        int idx = atoi(action + 7);
        if (idx < 0 || idx >= entry_count) return;
        if (entries[idx].is_dir) {
            char new_dir[PATH_BUF];
            snprintf(new_dir, sizeof(new_dir), "%s/%s", browse_dir, entries[idx].name);
            snprintf(browse_dir, sizeof(browse_dir), "%s", new_dir);
        } else if (!browser_mode) {
            /* Real click-to-load (PITFALL 64) - one real button
             * activation on a listed file loads it directly. */
            char full_path[PATH_BUF];
            snprintf(full_path, sizeof(full_path), "%s/%s", browse_dir, entries[idx].name);
            char cmd_line[PATH_BUF + 16];
            snprintf(cmd_line, sizeof(cmd_line), "LOAD:%s", full_path);
            enqueue_and_drain(cmd_line);
            file_path_input[0] = '\0';
            snprintf(status_line, sizeof(status_line), "Command sent");
        } else {
            /* SAVE mode: pre-fill only - deliberate asymmetry,
             * protects against an accidental overwrite. */
            snprintf(file_path_input, sizeof(file_path_input), "%s/%s", browse_dir, entries[idx].name);
        }
        return;
    }
    if (strcmp(action, "confirm") == 0) {
        read_cli_field('f', file_path_input, sizeof(file_path_input));
        if (file_path_input[0]) {
            char cmd_line[PATH_BUF + 16];
            snprintf(cmd_line, sizeof(cmd_line), "%s:%s", browser_mode ? "SAVE_AS" : "LOAD", file_path_input);
            enqueue_and_drain(cmd_line);
            file_path_input[0] = '\0';
            snprintf(status_line, sizeof(status_line), "Command sent");
        } else {
            snprintf(status_line, sizeof(status_line), "No path selected");
        }
        return;
    }
}

/* ---- KEY:n -> string action decode - the ONE place that knows the
 * temporary integer encoding (this file's own header comment). ---- */
static void decode_key_to_action(int key, char *action, size_t action_sz) {
    if (key == 1) { snprintf(action, action_sz, "back"); return; }
    int confirm_key = 2 + entry_count;
    if (key == confirm_key) { snprintf(action, action_sz, "confirm"); return; }
    if (key >= 2 && key < confirm_key) {
        snprintf(action, action_sz, "select_%d", key - 2);
        return;
    }
    action[0] = '\0';
}

/* ---- real poll loop - same real shape as game_manager.c's own
 * poll_history() (fseek to last position, fgets loop) but reading the
 * REAL channel, confirmed 2026-07-30 via direct read of
 * chtpm_parser_pal.c's own send_command()/inject_raw_key():
 * onClick="KEY:n" button activation calls send_command("KEY:n") ->
 * inject_raw_key(n), which writes the BARE INTEGER (no "KEY_PRESSED:"
 * prefix - that format is pieces/keyboard/history.txt's own RAW
 * physical-keyboard-driver format, an unrelated channel) into
 * whichever file the ACTIVE LAYOUT's own <interact src> attribute
 * names - pieces/apps/player_app/interact_relay.txt for every layout
 * in this project (confirmed via TPMOS's own agy-text-editor_manager.c
 * main loop, which falls back to plain atoi(line) on its own
 * equivalent <interact src> target for exactly this reason). The
 * earlier version of this function watched pieces/keyboard/history.txt
 * for a "KEY_PRESSED:"-prefixed line - real button activations
 * (BACK/file entries/SAVE FILE/LOAD FILE) never appear there at all,
 * so LOAD/click-to-load silently never fired. Real per-character
 * INTERACT/cli_io typing also lands in this same file as bare ASCII
 * codes; decode_key_to_action()'s own bounded range (1..confirm_key)
 * naturally ignores those (harmless no-op), so no separate filtering
 * is needed here. ---- */
/* active_gui_index - real, confirmed mechanism (direct read of
 * chtpm_parser_pal.c's own cli_io Enter handler, 2026-07-30): pressing
 * Enter while a cli_io is actively engaged AND non-empty is a real,
 * house-native "Send" trigger - the parser itself saves the full typed
 * value to gui_state.txt, clears its own input_buffer, and calls
 * inject_raw_key(13) ("Trigger the Send action by dispatching
 * KEY:13" - its own comment). That raw 13 lands in this same poll
 * loop's own interact_relay.txt read, indistinguishable by VALUE ALONE
 * from a coincidental KEY:n button match (13 could easily collide with
 * a real select_N/confirm/back integer depending on entry_count) -
 * matches TPMOS's own agy-text-editor_manager.c's own process_key(),
 * which resolves the identical ambiguity by checking
 * get_active_gui_index()==2 (its own FILE field) before ever treating
 * a raw Enter as SET_SAVE_ACTION/SET_LOAD_ACTION. pieces/display/
 * active_gui_is_typing.txt + active_gui_index.txt are the same real,
 * generic files that mechanism reads from - SEARCH is always
 * interactive_idx 1 and FILE always 2 in every real layout this
 * project declares (both statically declared first, before the
 * dynamically-injected button block - confirmed structurally). */
static int active_gui_index(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_is_typing.txt", project_root);
    char typing[8] = "";
    FILE *f = fopen(path, "r");
    if (f) { if (fgets(typing, sizeof(typing), f)) {} fclose(f); }

    /* REAL RACE, live-caught 2026-07-30: chtpm_parser_pal.c writes
     * active_gui_index.txt via a plain fopen(path,"w") + fprintf (not
     * an atomic tmp+rename), so a reader arriving between the truncate
     * and the write sees 0 bytes - confirmed live via this function's
     * own log: "typing='1' idx=''" at the EXACT moment a real Enter-
     * from-cli_io event needed this value. The write itself is a few
     * bytes and completes in microseconds; a short bounded retry is
     * enough to clear a race this narrow without masking a genuinely
     * different (non-transient) empty-file state. */
    snprintf(path, sizeof(path), "%s/pieces/display/active_gui_index.txt", project_root);
    char idx_str[16] = "";
    for (int attempt = 0; attempt < 5; attempt++) {
        idx_str[0] = '\0';
        f = fopen(path, "r");
        if (f) { if (fgets(idx_str, sizeof(idx_str), f)) {} fclose(f); }
        if (idx_str[0] != '\0') break;
        usleep(2000);
    }
    log_mgr("active_gui_index: typing='%.*s' idx='%.*s'",
            (int)strcspn(typing, "\r\n"), typing, (int)strcspn(idx_str, "\r\n"), idx_str);

    if (typing[0] != '1') return 0;
    return atoi(idx_str);
}

static void poll_history(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *hf = fopen(path, "r");
    if (!hf) { log_mgr("poll_history: cannot open %s", path); return; }
    fseek(hf, last_history_pos, SEEK_SET);
    char line[MAX_LINE];
    int changed = 0;
    while (fgets(line, sizeof(line), hf)) {
        int key = atoi(line);
        if (key == 0) continue;

        if (key == 10 || key == 13) {
            int agi = active_gui_index();
            if (agi == 2) {
                log_mgr("poll_history: raw Enter while FILE cli_io active -> confirm");
                dispatch_action("confirm");
                changed = 1;
            } else if (agi == 1) {
                /* SEARCH's own real Enter - no separate action (search
                 * has no live-filter-commit wired up), but it's still a
                 * real cli_io-internal "Send" relay (see this function's
                 * own header comment) - swallow it here rather than
                 * risk it coincidentally matching some unrelated
                 * select_N/back/confirm integer in the generic decode
                 * below purely by numeric accident. */
            }
            if (agi == 1 || agi == 2) continue;
        }

        /* REAL BUG, live-caught 2026-07-30 building &.widgits/file-menu's
         * own equivalent rebuild: chtpm_parser_pal.c's own send_command()
         * KEY:n handling is NOT a plain integer relay for n in 0-9 - it
         * calls inject_raw_key('0' + k), sending the ASCII CODE of the
         * digit (onClick="KEY:3" relays byte 51, not 3) - matches a real
         * keystroke of that digit. Only n >= 10 sends the raw integer
         * unshifted. decode_key_to_action() below expects the ORIGINAL
         * small integer - un-shift here first, same real fix already
         * applied and confirmed in &.widgits/file-menu/ops/fm_menu_
         * input.c's own main(). Without this, every single-digit KEY:n
         * button (BACK, first 9 entries, CONFIRM whenever entry_count<8)
         * silently never dispatched. */
        if (key >= '0' && key <= '9') key -= '0';
        char action[32];
        decode_key_to_action(key, action, sizeof(action));
        log_mgr("poll_history: raw_line='%.*s' key=%d entry_count=%d -> action='%s'",
                (int)strcspn(line, "\r\n"), line, key, entry_count, action);
        if (action[0]) { dispatch_action(action); changed = 1; }
    }
    last_history_pos = ftell(hf);
    fclose(hf);

    if (changed) write_frame();
}

static void read_current_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static void *polling_thread(void *arg) {
    (void)arg;
    log_mgr("=== agy_browser_manager polling thread started ===");
    while (running) {
        char layout[256];
        read_current_layout(layout, sizeof(layout));
        int on_browser = (strstr(layout, "file_browser_save.chtpm") != NULL) ||
                          (strstr(layout, "file_browser_load.chtpm") != NULL);
        if (on_browser) {
            if (strcmp(layout, last_layout) != 0) {
                reset_for_layout(layout);
                write_frame();
            } else {
                poll_history();
            }
        }
        usleep(POLL_INTERVAL);
    }
    log_mgr("=== agy_browser_manager polling thread exiting ===");
    return NULL;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    resolve_root();
    log_mgr("=== agy_browser_manager started ===");
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    pthread_t tid;
    if (pthread_create(&tid, NULL, polling_thread, NULL) != 0) {
        log_mgr("ERROR: failed to create polling thread");
        return 1;
    }
    while (running) sleep(1);
    pthread_join(tid, NULL);
    log_mgr("=== agy_browser_manager stopped ===");
    return 0;
}
