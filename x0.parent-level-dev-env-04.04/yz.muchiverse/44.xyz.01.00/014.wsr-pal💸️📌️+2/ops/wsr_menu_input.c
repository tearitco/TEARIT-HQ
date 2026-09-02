/* wsr_menu_input - piece.pdl METHOD-table-driven ACTION dispatch for
 * whichever wsr-pal screen is currently showing.
 *
 * REWRITTEN (xyzos-standards.txt sec. 18): screen SWITCHING is no longer
 * this op's job at all. Each wsr-pal screen (wsr_main_menu,
 * wsr_trade_menu, etc.) is now its own real `.chtpm` layout file, and
 * moving between them is a real `<button href="...">` - chtpm's own
 * native, proven mechanism (confirmed live in wraith-alpha's own
 * settings.chtpm), handled entirely inside chtpm_parser_pal.c's own
 * process_key(), synchronously, zero custom code. The `GOTO:<piece_id>`
 * command string this op used to hand-parse (and the `active_menu_piece`
 * app-state variable it used to mutate to fake a screen switch within
 * ONE shared layout) is retired for navigation entirely - piece.pdl
 * METHOD tables now hold ONLY real actions, never a navigation entry.
 *
 * WHICH SCREEN IS CURRENT is no longer tracked as separate mutable
 * state that something has to remember to keep in sync (the exact
 * class of bug that caused this rewrite in the first place - see
 * xyzos-standards.txt sec. 18's own account of the piece.pdl format
 * drift). It's derived fresh, every call, straight from
 * `pieces/display/current_layout.txt` - a real, existing chtpm export
 * (`parse_chtm()`'s own "EXPORT CURRENT LAYOUT FOR MODULE HEARTBEAT"
 * write, confirmed by direct read of chtpm_parser_pal.c) that already
 * fires on EVERY layout parse: the very first launch AND every real
 * `href` transition alike, always naming the ACTUAL file chtpm is
 * showing right now. `active_menu_piece` in wsr_menu/state.txt is kept
 * only as a plain display copy for `wsr_compose_frame.c` to read - this
 * op re-derives and re-writes it fresh on every single call, it is
 * never trusted as the source of truth for anything itself.
 *
 * cursor/digit_accum/arrow-key handling (`is_up`/`is_down`) are REMOVED
 * entirely - dead code, not a simplification-for-its-own-sake: chtpm's
 * own real nav mode (`chtpm_parser_pal.c`'s `focus_index`/`digit_accum`,
 * confirmed by direct read of `process_key()`) ALREADY resolves
 * arrow-key movement and multi-digit jump internally, firing
 * `send_command("KEY:n")` toward this op exactly ONCE, already fully
 * resolved - a raw arrow code or bare 'w'/'s' keystroke can never
 * actually reach this op through the real KEY:n relay path in the first
 * place, so the old `is_up`/`is_down` branches here were unreachable
 * even before this rewrite.
 *
 * Self-contained, no shared headers.
 * Usage: wsr_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#define access _access
#define F_OK 0
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#endif

#define MAX_LINE 512
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0 && access("projects", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

/* Same METHOD-line parsing shape as chtpm_parser.c's own
 * load_dynamic_methods() (strncmp "METHOD", split on '|' twice) - a
 * fresh, self-contained implementation here rather than including
 * that file, per this whole family's no-shared-headers doctrine. */
int load_menu_items(const char *project_root_, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/wsr-pal/pieces/%s/piece.pdl", project_root_, piece_id);
#pragma GCC diagnostic pop
    FILE *f = fopen(pdl_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max_items && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *label = trim(p1 + 1);
        char *command = trim(p2 + 1);
        snprintf(items[n].label, sizeof(items[n].label), "%s", label);
        snprintf(items[n].command, sizeof(items[n].command), "%s", command);
        n++;
    }
    fclose(f);
    return n;
}

static int count_corps(const char *project_root_) {
    /* Pure C (no shell) — same corp_* discovery as compose_frame. */
    int n = 0;
#ifdef _WIN32
    char pattern[PATH_BUF];
    snprintf(pattern, sizeof(pattern), "%s/projects/wsr-pal/pieces/*", project_root_);
    for (char *p = pattern; *p; p++) if (*p == '/') *p = '\\';
    struct _finddata_t fi;
    intptr_t h = _findfirst(pattern, &fi);
    if (h == -1) return 0;
    do {
        if (strncmp(fi.name, "corp_", 5) == 0) n++;
    } while (_findnext(h, &fi) == 0);
    _findclose(h);
#else
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root_);
    DIR *d = opendir(pieces_dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "corp_", 5) == 0) n++;
    }
    closedir(d);
#endif
    return n;
}

static void new_game(const char *project_root_) {
    char pieces_dir[PATH_BUF], template_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/wsr-pal/pieces", project_root_);
    snprintf(template_dir, sizeof(template_dir), "%s/projects/wsr-pal/pieces_template", project_root_);
    char cmd[(PATH_BUF + 32) * 2 + 64];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#ifdef _WIN32
    /* robocopy / MIR resets dest from template (Windows-native). */
    snprintf(cmd, sizeof(cmd),
             "cmd /c \"if exist \"%s\" rmdir /s /q \"%s\" & mkdir \"%s\" & xcopy /e /i /y /q \"%s\\*\" \"%s\\\" >nul\"",
             pieces_dir, pieces_dir, pieces_dir, template_dir, pieces_dir);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && cp -r '%s' '%s'", pieces_dir, template_dir, pieces_dir);
#endif
#pragma GCC diagnostic pop
    int rc = system(cmd);
    (void)rc;

    /* Reset history cursor so the next process restart doesn't replay
     * the wiped history.txt from an old file position. */
    char cursor_state[PATH_BUF];
    snprintf(cursor_state, sizeof(cursor_state), "%s/projects/wsr-pal/pieces/apps/player_app/state.txt", project_root_);
    FILE *f = fopen(cursor_state, "w");
    if (f) {
        fputs("history_cursor=0\n", f);
        fclose(f);
    }
}

/* Shell-out helper: Linux uses bash-style; Windows uses cmd /c.
 * Also rewrites Linux-style ./ops/+x/ paths so piece.pdl RUN: rows work. */
static void shell_cd_run(const char *project_root_, const char *rest, int capture, char *cap_out, size_t cap_sz) {
    char rest_buf[PATH_BUF * 2];
    snprintf(rest_buf, sizeof(rest_buf), "%s", rest ? rest : "");
#ifdef _WIN32
    /* Normalize common Linux op invocation forms */
    {
        char *p;
        while ((p = strstr(rest_buf, "./ops/+x/")) != NULL) {
            memmove(p, p + 2, strlen(p + 2) + 1); /* drop "./" */
        }
        while ((p = strstr(rest_buf, "ops/+x/")) != NULL) {
            /* leave forward slashes — cmd accepts them for MinGW bins */
            break;
        }
        /* scripts/tick_all.sh already rewritten at call sites */
    }
#endif
    char cmd[PATH_BUF * 3];
#ifdef _WIN32
    if (capture)
        snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && %s\"", project_root_, rest_buf);
    else
        snprintf(cmd, sizeof(cmd), "cmd /c \"cd /d \"%s\" && %s >nul 2>&1\"", project_root_, rest_buf);
#else
    if (capture)
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s 2>&1", project_root_, rest_buf);
    else
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s > /dev/null 2>&1", project_root_, rest_buf);
#endif
    if (capture && cap_out && cap_sz) {
        FILE *p = POPEN(cmd, "r");
        if (p) {
            if (!fgets(cap_out, (int)cap_sz, p)) cap_out[0] = '\0';
            else cap_out[strcspn(cap_out, "\r\n")] = '\0';
            PCLOSE(p);
        } else {
            cap_out[0] = '\0';
        }
    } else {
        int rc = system(cmd);
        (void)rc;
    }
}

static void startup_new_corp(const char *project_root_, char *message_out, size_t message_out_sz) {
    static int counter = 1;
    char corp_dir_check[PATH_BUF];
    char ticker[16];
    do {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(ticker, sizeof(ticker), "NEW%d", counter);
        snprintf(corp_dir_check, sizeof(corp_dir_check), "%s/projects/wsr-pal/pieces/corp_%s/state.txt", project_root_, ticker);
#pragma GCC diagnostic pop
        counter++;
    } while (access(corp_dir_check, F_OK) == 0 && counter < 1000);

    char op_cmd[PATH_BUF];
#ifdef _WIN32
    snprintf(op_cmd, sizeof(op_cmd), "ops\\+x\\corp_ipo.+x %s 200 >nul 2>&1", ticker);
#else
    snprintf(op_cmd, sizeof(op_cmd), "./ops/+x/corp_ipo.+x %s 200 > /dev/null 2>&1", ticker);
#endif
    shell_cd_run(project_root_, op_cmd, 0, NULL, 0);
    /* Probe success by presence of new state file */
    int ok = (access(corp_dir_check, F_OK) == 0);
    if (ok) {
        /* Real incorporation.c precedent (original source): founding a
         * corp makes the founder its 100% owner/shareholder - our
         * simplified equivalent is just setting owned_by, since we
         * don't track a full cap table. This was missing before - the
         * player could found a corp but never actually controlled it. */
        char owner_cmd[PATH_BUF];
#ifdef _WIN32
        snprintf(owner_cmd, sizeof(owner_cmd), "ops\\+x\\corp_set_owner.+x corp_%s player_you >nul 2>&1", ticker);
#else
        snprintf(owner_cmd, sizeof(owner_cmd), "./ops/+x/corp_set_owner.+x corp_%s player_you > /dev/null 2>&1", ticker);
#endif
        shell_cd_run(project_root_, owner_cmd, 0, NULL, 0);
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    if (ok) snprintf(message_out, message_out_sz, "Started corp_%s with $200M seed capital - you are the owner.", ticker);
    else snprintf(message_out, message_out_sz, "Startup failed.");
#pragma GCC diagnostic pop
}

/* Writes chtpm's own bridge fields (project_id/active_target_id) so
 * `${piece_methods}` resolves projects/wsr-pal/pieces/<piece_id>/piece.pdl
 * on the very next compose. */
static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=wsr-pal\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

/* Derives "which screen is current" straight from chtpm's own real
 * pieces/display/current_layout.txt export (see this file's own top
 * comment) - basename of the current .chtpm path, minus the extension.
 * Falls back to wsr_main_menu if the file is missing/unreadable (should
 * only happen before the very first parse_chtm() call ever runs). */
static void get_current_piece_id(const char *project_root_, char *out, size_t out_sz) {
    snprintf(out, out_sz, "wsr_main_menu");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root_);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        const char *slash = strrchr(line, '/');
        const char *base = slash ? slash + 1 : line;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/wsr-pal/pieces/wsr_menu/state.txt", project_root);

    int key = atoi(argv[1]);

    /* key=0 is the persistent loop's own re-derive-current-screen tick
     * (main_loop_chtpm.pal calls this every ~30ms - see that file's own
     * header comment), not a real user selection. REAL BUG, LIVE-CAUGHT:
     * an earlier version of this did the full seed write (and, in the
     * pal script, an unconditional compose_frame/hit_frame) on EVERY
     * one of those ticks regardless of whether anything had actually
     * changed - ~33 renders/sec with no real event behind most of them,
     * visible as constant flicker. Exit immediately, before touching
     * anything else, if the screen genuinely hasn't changed since the
     * last check - only a REAL screen change (a real href click) does
     * any write at all, or bumps wsr_screen_changed.txt (what the pal
     * script's own read_pos check gates compose_frame/hit_frame on). */
    if (key == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        int active_corp_index = read_kv_int(state_path, "active_corp_index", 0);
        int turn_number = read_kv_int(state_path, "turn_number", 0);
        int ticker_on = read_kv_int(state_path, "ticker_on", 0);
        FILE *f = fopen(state_path, "w");
        if (f) {
            fprintf(f, "active_corp_index=%d\n", active_corp_index);
            fprintf(f, "turn_number=%d\n", turn_number);
            fprintf(f, "ticker_on=%d\n", ticker_on);
            fprintf(f, "active_menu_piece=%s\n", derived);
            fprintf(f, "last_message=\n");
            fclose(f);
        }
        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/wsr_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    int active_corp_index = read_kv_int(state_path, "active_corp_index", 0);
    int turn_number = read_kv_int(state_path, "turn_number", 0);
    int ticker_on = read_kv_int(state_path, "ticker_on", 0);
    char active_menu_piece[128];
    get_current_piece_id(project_root, active_menu_piece, sizeof(active_menu_piece));

    /* If a wizard prompt is active (currently just Startup New Corp's
     * real 5-step flow - see wsr_wizard_input.c), EVERY key goes there
     * instead of normal menu dispatch, matching real cli_io's own
     * "active mode" exclusivity (researched directly against
     * chtpm_parser.c - once a cli_io element is active, key handling
     * branches entirely away from menu navigation until it exits). */
    int prompt_active = read_kv_int(state_path, "prompt_active", 0);
    if (prompt_active) {
        char wizard_cmd[PATH_BUF];
#ifdef _WIN32
        snprintf(wizard_cmd, sizeof(wizard_cmd), "ops\\+x\\wsr_wizard_input.+x %d", key);
#else
        snprintf(wizard_cmd, sizeof(wizard_cmd), "./ops/+x/wsr_wizard_input.+x %d", key);
#endif
        shell_cd_run(project_root, wizard_cmd, 0, NULL, 0);
        return 0;
    }

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_menu_piece, items, MAX_MENU_ITEMS);

    /* REAL BUG FIX (found live via wsr.chtpm's own ${piece_methods}
     * switch, researched against real chtpm_parser.c's own
     * inject_raw_key() - pieces/chtpm/plugins/chtpm_parser.c lines
     * 1263-1320): chtpm's own nav-mode digit_accum ALREADY resolves
     * multi-digit menu selection entirely inside chtpm_parser_pal.c,
     * before anything is ever written here - Enter then fires
     * send_command("KEY:13") for that ALREADY-RESOLVED item exactly
     * once. inject_raw_key() writes the raw decimal value verbatim
     * either way (send_command()'s own KEY: handling ASCII-encodes
     * indices 1-9 as '0'+k purely so small menus look like an ordinary
     * typed digit keystroke to older non-chtpm code paths; 10+ is
     * written as the bare integer). Either form arrives here as ONE
     * fully-resolved selection - no accumulation needed on this side. */
    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    /* Default to whatever last_message ALREADY was, not blank - this op
     * is now also called with key=0 on every persistent-loop iteration
     * (main_loop_chtpm.pal's own re-derive-current-screen step, see that
     * file's own header comment), not just on a real numbered
     * selection. Starting from "" unconditionally used to erase a real
     * action's own message within one loop tick (~30ms) of it being
     * set, before a player could ever actually read it - only a REAL
     * dispatched command below should ever overwrite it. */
    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;
        if (strncmp(cmd, "TICK_ALL:", 9) == 0) {
            char tick_cmd[PATH_BUF * 2];
#ifdef _WIN32
            snprintf(tick_cmd, sizeof(tick_cmd),
                     "powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\tick_all.ps1 %s >nul 2>&1",
                     cmd + 9);
#else
            snprintf(tick_cmd, sizeof(tick_cmd), "bash scripts/tick_all.sh %s > /dev/null 2>&1", cmd + 9);
#endif
            shell_cd_run(project_root, tick_cmd, 0, NULL, 0);
            turn_number++;
            snprintf(message, sizeof(message), "Turn advanced - the world moved forward.");
        } else if (strcmp(cmd, "CYCLE_CORP") == 0) {
            int corp_count = count_corps(project_root);
            if (corp_count > 0) active_corp_index = (active_corp_index + 1) % corp_count;
            snprintf(message, sizeof(message), "Switched active corporation.");
        } else if (strcmp(cmd, "TOGGLE_TICKER") == 0) {
            ticker_on = !ticker_on;
            snprintf(message, sizeof(message), "Ticker turned %s.", ticker_on ? "ON" : "OFF");
        } else if (strcmp(cmd, "NEW_GAME") == 0) {
            new_game(project_root);
            snprintf(active_menu_piece, sizeof(active_menu_piece), "wsr_main_menu");
            active_corp_index = 0;
            turn_number = 0;
            snprintf(message, sizeof(message), "New game started - world reset to initial data.");
        } else if (strcmp(cmd, "NEW_CORP") == 0) {
            startup_new_corp(project_root, message, sizeof(message));
        } else if (strcmp(cmd, "START_WIZARD:new_corp") == 0) {
            /* Real 5-step Startup New Corp flow (industry/country/
             * funding/name/ticker) - see wsr_wizard_input.c. Every
             * subsequent key goes there until the wizard finishes
             * or is cancelled (the prompt_active branch above). */
            FILE *sf = fopen(state_path, "w");
            if (sf) {
                fprintf(sf, "active_corp_index=%d\n", active_corp_index);
                fprintf(sf, "turn_number=%d\n", turn_number);
                fprintf(sf, "ticker_on=%d\n", ticker_on);
                fprintf(sf, "active_menu_piece=%s\n", active_menu_piece);
                fprintf(sf, "prompt_active=1\n");
                fprintf(sf, "prompt_step=1\n");
                fprintf(sf, "prompt_buffer=\n");
                fprintf(sf, "last_message=Enter the industry number for your new corporation.\n");
                fclose(sf);
            }
            return 0;
        } else if (strcmp(cmd, "STUB") == 0) {
            snprintf(message, sizeof(message), "Not yet available in this build.");
        } else if (strncmp(cmd, "RUN:", 4) == 0) {
            /* Real gameplay ops (trade/finance/management/loans/
             * derivatives) print their own one-line result (bought/
             * sold/insufficient cash/loan denied/etc) - capture it
             * as the menu message instead of the generic "Ran: X"
             * fallback below, so the player actually sees what
             * happened, not just that something ran. */
            char cap[MAX_LINE];
            shell_cd_run(project_root, cmd + 4, 1, cap, sizeof(cap));
            if (cap[0])
                snprintf(message, sizeof(message), "%s", cap);
            else
                snprintf(message, sizeof(message), "Ran: %s", items[resolved_item - 1].label);
        } else {
            /* Not a recognized special command - shell it directly,
             * matching chtpm_parser.c's own real fallback and
             * op-ed's bind_event()/trigger_event() precedent of
             * running whatever a METHOD row names. */
            shell_cd_run(project_root, cmd, 0, NULL, 0);
            snprintf(message, sizeof(message), "Ran: %s", items[resolved_item - 1].label);
        }
    }

    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "active_corp_index=%d\n", active_corp_index);
        fprintf(f, "turn_number=%d\n", turn_number);
        fprintf(f, "ticker_on=%d\n", ticker_on);
        fprintf(f, "active_menu_piece=%s\n", active_menu_piece);
        fprintf(f, "last_message=%s\n", message);
        fclose(f);
    }

    write_chtpm_bridge(active_menu_piece);
    return 0;
}
