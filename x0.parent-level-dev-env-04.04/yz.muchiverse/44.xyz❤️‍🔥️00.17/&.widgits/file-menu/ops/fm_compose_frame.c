#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MARKUP_BUF 16384

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

/* set_gui_var - real-modify-write single key into this project's own
 * gui_state.txt (2026-07-30 rebuild, PITFALL 65 fix). Path confirmed
 * via direct read of chtpm_parser_pal.c's own load_project_gui_state():
 * project_id=file-menu (button.sh's own state.txt) routes load_vars()
 * through a project-scoped path, not the generic pieces/apps/
 * player_app/manager/ one - same real gotcha 102.agy-txt's own manager
 * hit this same session (see that project's own BROWSER_CONTRACT.md §0
 * and manager/agy_browser_manager.c's own gui_state_path() comment).
 * load_project_gui_state() checks candidates in order and uses the
 * FIRST that exists; "projects/<id>/manager/gui_state.txt" is checked
 * BEFORE "pieces/apps/<id>/manager/gui_state.txt" - and button.sh
 * already creates projects/file-menu/manager/ (pre-existing
 * infrastructure, confirmed by direct read), so that's the one this
 * writes to. */
static void gui_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/projects/file-menu/manager/gui_state.txt", project_root);
}

/* pulse_state_changed - REAL, confirmed against TPMOS's own reference
 * (1.TPMOS_c_+rmmp.0103.0001/projects/agy-text-editor/manager/
 * agy-text-editor_manager.c's own trigger_render(), re-read 2026-07-30
 * specifically to verify this before adding it - it pulses BOTH
 * pieces/display/frame_changed.txt AND pieces/apps/player_app/
 * state_changed.txt). This op's own fm_screen_changed.txt marker
 * (written by fm_menu_input.c's own bump_screen()) only drives THIS
 * project's own main_loop_chtpm.pal into calling fm_compose_frame
 * again - it does NOT tell chtpm_parser_pal.c's own main loop to
 * re-run parse_chtm(), which is the step that actually re-substitutes
 * the now-bare ${game_map} line into fresh <button>/<text> elements
 * (same real mechanism confirmed this session for 102.agy-txt's own
 * ${directory_browser_markup}). Without this, view.txt gets the
 * correct fresh markup but the rendered frame never picks it up. */
static void pulse_state_changed(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }
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

/* escape_xml - real, needed for the FIRST time this project injects
 * user-controlled text (filenames/dirnames, peer display names) as raw
 * markup text instead of a plain hand-drawn string - an unescaped '<',
 * '>', '&', or '"' in a filename would otherwise break the XML the
 * moment substitute_vars_naked() re-parses it (confirmed mechanism,
 * chtpm_parser_pal.c ~line 2297-2335). Real files with these
 * characters are rare but not impossible (this house's own directory
 * trees already have emoji/unusual segments - save-bug.txt), so this
 * is a real correctness fix, not defensive-for-no-reason. */
static void escape_xml(const char *in, char *out, size_t out_sz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 6 < out_sz; p++) {
        switch (*p) {
            case '<': memcpy(out + o, "&lt;", 4); o += 4; break;
            case '>': memcpy(out + o, "&gt;", 4); o += 4; break;
            case '&': memcpy(out + o, "&amp;", 5); o += 5; break;
            case '"': memcpy(out + o, "&quot;", 6); o += 6; break;
            default: out[o++] = (char)*p; break;
        }
    }
    out[o] = '\0';
}

static void format_size(long bytes, char *out, size_t out_sz) {
    if (bytes < 1024)
        snprintf(out, out_sz, "%ldB", bytes);
    else
        snprintf(out, out_sz, "%.0fKB", bytes / 1024.0);
}

static int has_focus(char *session_root, size_t sr_sz,
                     char *state_path, size_t sp_sz,
                     char *kind, size_t k_sz,
                     char *disp, size_t d_sz) {
    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/pieces/system/focus.txt", project_root);
    if (access(focus, F_OK) != 0) return 0;
    read_kv(focus, "session_root", session_root, sr_sz);
    read_kv(focus, "state_path", state_path, sp_sz);
    read_kv(focus, "kind", kind, k_sz);
    read_kv(focus, "display_name", disp, d_sz);
    if (!session_root[0] || access(session_root, F_OK) != 0) return 0;
    return 1;
}

/* build_main_menu_markup - real <button> per option, matching
 * mutaclysm's own write_panel_gui_state() shape (101.mutaclsym
 * ops/compose_frame.c) - the in-house proof this pattern is correct
 * and already shipping. 2026-07-30 CORRECTED (direct instruction:
 * "always use href to go to another page... that was the whole point
 * of the refactor"): SAVE AS/LOAD/CHANGE FOCUS/PAIR FROM FILE are real
 * screen transitions now - real <button href="..."> into their own
 * separate layout files (matching 102.agy-txt's own file_menu.chtpm
 * -> file_browser_save.chtpm/file_browser_load.chtpm exactly), NOT an
 * internal mode write. Only NEW FILE/SAVE FILE stay real onClick=
 * "KEY:n" actions (they fire a command and stay on THIS screen - no
 * navigation involved). Real href also fixes a live-caught bug for
 * free: a same-layout mode switch left chtpm_parser_pal.c's own
 * focus_index pointing at a stale, unrelated element in the new
 * structure - a real href forces cleanup_module()/parse_chtm()/
 * initialize_focus() to run fresh (confirmed via direct read of its
 * own href-handling code). */
static void build_main_menu_markup(char *out, size_t out_sz, int focused) {
    size_t o = 0;
    int n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"%s\" onClick=\"KEY:1\" /><br/>",
                  focused ? "NEW FILE" : "(no focus)");
    if (n > 0) o += (size_t)n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"%s\" onClick=\"KEY:2\" /><br/>",
                  focused ? "SAVE FILE" : "(no focus)");
    if (n > 0) o += (size_t)n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"%s\" href=\"pieces/chtpm/layouts/file_menu_browser_save.chtpm\" /><br/>",
                  focused ? "SAVE AS..." : "(no focus)");
    if (n > 0) o += (size_t)n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"%s\" href=\"pieces/chtpm/layouts/file_menu_browser_load.chtpm\" /><br/>",
                  focused ? "LOAD FILE..." : "(no focus)");
    if (n > 0) o += (size_t)n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"%s\" href=\"pieces/chtpm/layouts/file_menu_peers.chtpm\" /><br/>",
                  focused ? "CHANGE FOCUS" : "CHANGE FOCUS / PAIR");
    if (n > 0) o += (size_t)n;
    n = snprintf(out + o, out_sz - o,
                  "<button label=\"PAIR FROM FILE...\" href=\"pieces/chtpm/layouts/file_menu_browser_pair.chtpm\" /><br/>");
    if (n > 0) o += (size_t)n;
}

/* build_file_browser_markup - real BACK/entry/CONFIRM buttons + a real
 * CANCEL href back to file_menu_main.chtpm (2026-07-30 corrected, see
 * build_main_menu_markup()'s own header comment - was onClick="KEY:n"
 * writing mode=main_menu internally, now a real href matching
 * 102.agy-txt's own file_browser_save/load.chtpm CANCEL exactly).
 * SEARCH/FILE are real <cli_io> elements declared statically in each
 * layout file itself (a dynamically-injected ${var} can't carry its
 * OWN further ${var} reference and have it re-substituted - confirmed
 * this session building agy-txt's own manager). Real KEY:n numbering:
 * 1=BACK, 2..(1+entry_count)=select_N, then CONFIRM - fm_menu_input.c's
 * own decode must match this exactly. */
static void build_file_browser_markup(char *out, size_t out_sz,
                                       const char *browse_dir, const char *search_text,
                                       const char *confirm_label, int *entry_count_out) {
    size_t o = 0;
    int key_n = 1;

    {
        int n = snprintf(out + o, out_sz - o,
                          "<button label=\"&lt;- BACK\" onClick=\"KEY:%d\" /><br/>", key_n++);
        if (n > 0) o += (size_t)n;
    }

    int entry_count = 0;
    if (browse_dir[0]) {
        char scan_cmd[MAX_PATH + 64];
        if (search_text[0])
            snprintf(scan_cmd, sizeof(scan_cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\" \"%s\"",
                     project_root, browse_dir, search_text);
        else
            snprintf(scan_cmd, sizeof(scan_cmd), "\"%s/ops/+x/fm_scan_dir.+x\" \"%s\"",
                     project_root, browse_dir);
        FILE *scan = popen(scan_cmd, "r");
        if (scan) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), scan)) {
                line[strcspn(line, "\n")] = '\0';
                char kind_entry[16] = "", name[512] = "";
                long size = 0;
                int is_dir = 0;
                sscanf(line, "%15[^|]|%511[^|]|%ld|%d", kind_entry, name, &size, &is_dir);
                char name_esc[1024];
                escape_xml(name, name_esc, sizeof(name_esc));
                char sz[32] = "";
                if (!is_dir && size > 0) format_size(size, sz, sizeof(sz));
                int n = snprintf(out + o, out_sz - o,
                                  "<button label=\"[%s] %s%s%s%s\" onClick=\"KEY:%d\" /><br/>",
                                  is_dir ? "DIR" : "FIL", name_esc,
                                  sz[0] ? " (" : "", sz, sz[0] ? ")" : "", key_n++);
                if (n > 0) o += (size_t)n;
                entry_count++;
            }
            pclose(scan);
        }
    }

    {
        int n = snprintf(out + o, out_sz - o,
                          "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>", confirm_label, key_n++);
        if (n > 0) o += (size_t)n;
    }
    {
        int n = snprintf(out + o, out_sz - o,
                          "<button label=\"CANCEL\" href=\"pieces/chtpm/layouts/file_menu_main.chtpm\" /><br/>");
        if (n > 0) o += (size_t)n;
    }

    *entry_count_out = entry_count;
}

/* build_peer_list_markup - real KEY:1..peer_count select buttons + a
 * real CANCEL href back to file_menu_main.chtpm (2026-07-30 corrected,
 * same real fix as build_file_browser_markup() above). */
static void build_peer_list_markup(char *out, size_t out_sz, int *peer_count_out) {
    size_t o = 0;
    int key_n = 1;
    int peer_count = 0;

    char scan_cmd[MAX_PATH + 64];
    snprintf(scan_cmd, sizeof(scan_cmd), "\"%s/ops/+x/ledger_peers.+x\" editor 2>/dev/null; "
             "\"%s/ops/+x/ledger_peers.+x\" app 2>/dev/null; "
             "\"%s/ops/+x/ledger_peers.+x\" widget 2>/dev/null",
             project_root, project_root, project_root);
    FILE *peers = popen(scan_cmd, "r");
    if (peers) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), peers)) {
            line[strcspn(line, "\n")] = '\0';
            char sr[MAX_PATH] = "", inbox[MAX_PATH] = "", dn[256] = "", pid[64] = "", pi[128] = "";
            sscanf(line, "%4095[^|]|%4095[^|]|%255[^|]|%127[^|]|%63s",
                   sr, inbox, dn, pi, pid);
            const char *show = dn[0] ? dn : (pi[0] ? pi : sr);
            char show_esc[512];
            escape_xml(show, show_esc, sizeof(show_esc));
            int n = snprintf(out + o, out_sz - o,
                              "<button label=\"%s (PID %s)\" onClick=\"KEY:%d\" /><br/>",
                              show_esc, pid, key_n++);
            if (n > 0) o += (size_t)n;
            peer_count++;
        }
        pclose(peers);
    }
    {
        int n = snprintf(out + o, out_sz - o,
                          "<button label=\"CANCEL\" href=\"pieces/chtpm/layouts/file_menu_main.chtpm\" /><br/>");
        if (n > 0) o += (size_t)n;
    }
    *peer_count_out = peer_count;
}

/* emit_text_line - real <text label="..." /><br/> element, replacing
 * every plain fprintf(f, "...\n") this file used to do. REAL BUG,
 * caught live 2026-07-30 during this rebuild's own first smoke test:
 * ${game_map} is now a BARE top-level line in file-menu.chtpm (not
 * wrapped in <text label="${game_map}">), so substitute_vars_naked()
 * re-parses its ENTIRE content as raw XML (same real mechanism
 * confirmed for 102.agy-txt's ${directory_browser_markup}) - plain,
 * unwrapped text in that blob would either fail to tokenize correctly
 * or render literally instead of as real content. Every line this
 * file writes into view.txt must itself be a real, valid element. */
static void emit_text_line(FILE *f, const char *text) {
    char esc[MAX_LINE];
    escape_xml(text, esc, sizeof(esc));
    fprintf(f, "<text label=\"%s\" /><br/>", esc);
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

/* resolve_xyzfs_documents - same real chain fm_menu_input.c's own copy
 * uses (xyzfs jail default browse_dir). Duplicated here (not linked)
 * matching this project's own existing pattern of small per-op copies
 * of this logic, same as it already had before this rebuild. */
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

int main(void) {
    resolve_root();
    char fm_path[PATH_BUF], browse_dir[PATH_BUF];
    char path_buffer[MAX_LINE], search_text[MAX_LINE], status_line[MAX_LINE];
    char last_layout[256];
    snprintf(fm_path, sizeof(fm_path), "%s/pieces/system/fm_state.txt", project_root);
    read_kv(fm_path, "browse_dir", browse_dir, sizeof(browse_dir));
    read_kv(fm_path, "path_buffer", path_buffer, sizeof(path_buffer));
    read_kv(fm_path, "search_text", search_text, sizeof(search_text));
    read_kv(fm_path, "status_line", status_line, sizeof(status_line));
    read_kv(fm_path, "last_layout", last_layout, sizeof(last_layout));

    char layout[256];
    read_current_layout(layout, sizeof(layout));
    int is_browser = strstr(layout, "file_menu_browser_") != NULL;
    int is_peers = strstr(layout, "file_menu_peers.chtpm") != NULL;

    /* Real reset-on-entry, same trigger 102.agy-txt's own manager uses
     * (reset_for_layout()) - a real href landing on this exact file is
     * detected by comparing to the last layout THIS op itself last
     * rendered, since (unlike a native manager) this op has no
     * persistent in-memory state between invocations. */
    if (strcmp(layout, last_layout) != 0) {
        if (is_browser) {
            char docs_dir[PATH_BUF];
            if (resolve_xyzfs_documents(docs_dir, sizeof(docs_dir)))
                snprintf(browse_dir, sizeof(browse_dir), "%s", docs_dir);
            search_text[0] = '\0';
            path_buffer[0] = '\0';
            status_line[0] = '\0';

            /* REAL BUG, live-caught 2026-07-30: chtpm_parser_pal.c's
             * own cli_io typing handler writes the LIVE buffer into
             * gui_state.txt under the element's own target_id DIRECTLY
             * ("file_path_input"/"search_query" - confirmed via direct
             * read of save_cli_io_gui_state()'s own call site) - a
             * COMPLETELY SEPARATE key from this file's own
             * "file_path_input_val"/"search_query_val" (which only
             * ever feeds the LABEL prefix text, a display-only
             * concern). sync_cli_input_from_gui_state()'s own restore
             * pass (run on every fresh parse_chtm(), including every
             * real href transition) reads the target_id-keyed value,
             * NOT the "_val" one - clearing only "_val" left the REAL
             * one ("hi", from an earlier abandoned typing session)
             * permanently stuck in gui_state.txt, silently prepending
             * itself onto every subsequent typed filename
             * ("hihi2-....txt") for the rest of the session. Both
             * keys must be cleared on every fresh browser entry. */
            set_gui_var("file_path_input", "");
            set_gui_var("search_query", "");
        }
    }

    char session_root[MAX_PATH] = "", state_path[MAX_PATH] = "";
    char kind[128] = "", disp[128] = "";
    int focused = has_focus(session_root, sizeof(session_root),
                            state_path, sizeof(state_path),
                            kind, sizeof(kind), disp, sizeof(disp));

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/apps/player_app/view.txt", project_root);
    FILE *f = fopen(out_path, "w");
    if (!f) return 1;

    if (is_browser) {
        const char *label = "LOAD";
        int is_pair = strstr(layout, "file_menu_browser_pair.chtpm") != NULL;
        int is_save = strstr(layout, "file_menu_browser_save.chtpm") != NULL;
        if (is_save) label = "SAVE AS";
        else if (is_pair) label = "PAIR WITH PROJECT";
        char line[MAX_LINE];
        if (is_pair)
            emit_text_line(f, "MODE: PAIR FROM FILE");
        else {
            snprintf(line, sizeof(line), "MODE: %s FILE", label);
            emit_text_line(f, line);
        }
        snprintf(line, sizeof(line), "DIR: %s", browse_dir[0] ? browse_dir : ".");
        emit_text_line(f, line);
        emit_text_line(f, "");

        if (focused && state_path[0]) {
            char active[MAX_LINE] = "";
            read_kv(state_path, "file_path", active, sizeof(active));
            if (!active[0]) read_kv(state_path, "document", active, sizeof(active));
            snprintf(line, sizeof(line), "ACTIVE: %s", active[0] ? active : "(none)");
            emit_text_line(f, line);
            emit_text_line(f, "");
        }

        emit_text_line(f, "DIRECTORY CONTENTS:");
        const char *confirm = is_pair ? "PAIR" : label;
        int entry_count = 0;
        char markup[MARKUP_BUF];
        build_file_browser_markup(markup, sizeof(markup), browse_dir, search_text, confirm, &entry_count);
        fprintf(f, "%s", markup);
        emit_text_line(f, "");
        emit_text_line(f, status_line[0] ? status_line : "");

        set_gui_var("search_query_val", search_text);
        set_gui_var("file_path_input_val", path_buffer);

    } else if (is_peers) {
        emit_text_line(f, "");
        emit_text_line(f, "FOCUS TARGETS:");
        emit_text_line(f, "");
        int peer_count = 0;
        char markup[MARKUP_BUF];
        build_peer_list_markup(markup, sizeof(markup), &peer_count);
        fprintf(f, "%s", markup);
        if (peer_count > 0) {
            emit_text_line(f, "");
            emit_text_line(f, "Select a target to pair.");
        } else {
            emit_text_line(f, "No active projects found.");
            emit_text_line(f, "");
            emit_text_line(f, "CANCEL above to go back.");
        }

    } else {
        char line[MAX_LINE];
        emit_text_line(f, "");
        if (focused) {
            snprintf(line, sizeof(line), "ACTIVE: %s", disp[0] ? disp : "(project)");
            emit_text_line(f, line);
            if (state_path[0]) {
                char active[MAX_LINE] = "";
                read_kv(state_path, "file_path", active, sizeof(active));
                if (!active[0]) read_kv(state_path, "document", active, sizeof(active));
                if (active[0]) {
                    snprintf(line, sizeof(line), "FILE: %s", active);
                    emit_text_line(f, line);
                }
            }
        } else {
            emit_text_line(f, ">> NO PROJECT FOCUSED <<");
        }
        emit_text_line(f, "");
        char markup[MARKUP_BUF];
        build_main_menu_markup(markup, sizeof(markup), focused);
        fprintf(f, "%s", markup);
        emit_text_line(f, "");
        emit_text_line(f, status_line[0] ? status_line : (focused ? "Ready." : "Select CHANGE FOCUS or PAIR FROM FILE to pair with a project."));
    }

    write_kv(fm_path, "browse_dir", browse_dir);
    write_kv(fm_path, "search_text", search_text);
    write_kv(fm_path, "path_buffer", path_buffer);
    write_kv(fm_path, "status_line", status_line);
    write_kv(fm_path, "last_layout", layout);

    pulse_state_changed();
    fclose(f);
    return 0;
}
