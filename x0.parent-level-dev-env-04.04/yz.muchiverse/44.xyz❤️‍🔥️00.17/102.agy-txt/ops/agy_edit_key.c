/* agy_edit_key - INTERACT canvas typing + piece.pdl METHOD dispatch.
 * Copied near-verbatim from 102.editor-📄️00.00/ops/editor_menu_input.c
 * (PLAN.md §2 - explicit reuse target, same linear cursor_pos model
 * kept on purpose rather than porting the TPMOS reference's own 2D
 * x,y model). do_fm()/find_widget_via_ledger() are left in place,
 * unmodified and harmless - agy-txt has no separate widget process to
 * relay a FILE MENU key to, so that code path simply never fires here
 * (no piece.pdl METHOD line will ever map to "FM" in this project).
 *
 * Usage: agy_edit_key.+x <keycode>
 *   key 0     = idle (no-op catch-up; house pal loop shape)
 *   printable = append at cursor (INTERACT path)
 *   127/8     = backspace
 *   10/13     = newline
 *   KEY:n via digit / raw method index = NEW / CLEAR / EXIT
 *
 * Buffer: pieces/system/editor_buffer.txt
 * State:  pieces/system/editor_state.txt  (cursor_pos, file_path, last_message)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_BUF 65536
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[160];
    char command[128];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
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

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[48][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 48 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static void bump_screen(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path),
             "%s/pieces/display/editor_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static size_t read_buffer(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_buffer.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, out_sz - 1, f);
    out[n] = '\0';
    fclose(f);
    return n;
}

static void write_buffer(const char *buf, size_t n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_buffer.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    if (n > 0) fwrite(buf, 1, n, f);
    fclose(f);
}

static int get_cursor(size_t buflen) {
    char path[PATH_BUF], raw[64];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_state.txt", project_root);
    read_kv_str(path, "cursor_pos", raw, sizeof(raw));
    if (!raw[0] || strcmp(raw, "-1") == 0) return (int)buflen;
    int p = atoi(raw);
    if (p < 0) p = 0;
    if ((size_t)p > buflen) p = (int)buflen;
    return p;
}

static void set_cursor(int pos) {
    char path[PATH_BUF], v[32];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_state.txt", project_root);
    snprintf(v, sizeof(v), "%d", pos);
    write_kv(path, "cursor_pos", v);
}

static void set_message(const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_state.txt", project_root);
    write_kv(path, "last_message", msg);
}

static int load_menu_items(MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path),
             "%s/projects/agy-txt/pieces/editor/piece.pdl", project_root);
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

static void do_new_file(void) {
    write_buffer("", 0);
    set_cursor(0);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_state.txt", project_root);
    write_kv(path, "file_path", "docs/untitled.txt");
    set_message("NEW FILE — empty buffer.");
}

static void do_clear(void) {
    write_buffer("", 0);
    set_cursor(0);
    set_message("CLEAR FILE — buffer cleared.");
}

static void do_exit(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/quit_flag.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) { fputs("1\n", f); fclose(f); }
    set_message("EXIT — quit flag set (Ctrl+C if still live).");
}

/* Read house_root.txt from the editor session to find the file-menu dir. */
static int read_house_root(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(out, (int)out_sz, f)) { fclose(f); return -1; }
    fclose(f);
    size_t ln = strlen(out);
    while (ln > 0 && (out[ln-1] == '\n' || out[ln-1] == '\r')) out[--ln] = '\0';
    return (out[0]) ? 0 : -1;
}

/* Find a file-menu widget session via the xyzfs runtime ledger.
 * Uses ledger_peers to discover active widgets, returns the first
 * widget's session root. Returns 0 if found, -1 if not. */
static int find_widget_via_ledger(char *out, size_t out_sz) {
    char house[PATH_BUF];
    if (read_house_root(house, sizeof(house)) != 0) return -1;
    char cmd_line[PATH_BUF * 2];
    snprintf(cmd_line, sizeof(cmd_line),
             "PRISC_PROJECT_ROOT='%s' '%s/&.widgits/file-menu/ops/+x/ledger_peers.+x' widget 2>/dev/null",
             project_root, house);
    FILE *fp = popen(cmd_line, "r");
    if (!fp) return -1;
    char line[MAX_LINE];
    int found = -1;
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        char *session_root = strtok(line, "|");
        if (session_root) {
            snprintf(out, out_sz, "%s", session_root);
            found = 0;
        }
    }
    pclose(fp);
    return found;
}

/* FM handler — bridge FILE MENU keypress to the file-menu widget.
 * Finds the widget via the xyzfs runtime ledger and writes key "1"
 * (menu activate) to its interact_relay.txt so the PAL loop processes it. */
static void do_fm(void) {
    char widget_root[PATH_BUF];
    if (find_widget_via_ledger(widget_root, sizeof(widget_root)) != 0) {
        set_message("FM — no widget session (start app with button.sh run)");
        return;
    }
    char relay[PATH_BUF];
    snprintf(relay, sizeof(relay), "%s/pieces/apps/player_app/interact_relay.txt", widget_root);
    FILE *rf = fopen(relay, "a");
    if (!rf) {
        set_message("FM — cannot reach widget relay");
        return;
    }
    fprintf(rf, "1\n");
    fclose(rf);
    set_message("FILE MENU — signal sent to widget");
}

/* Insert one byte at cursor. */
static void insert_char(char ch) {
    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    if (blen + 1 >= sizeof(buf)) return;
    int cur = get_cursor(blen);
    memmove(buf + cur + 1, buf + cur, blen - (size_t)cur);
    buf[cur] = ch;
    blen++;
    buf[blen] = '\0';
    write_buffer(buf, blen);
    set_cursor(cur + 1);
}

/* Insert a whole string at cursor — real UTF-8 bytes, no per-char
 * validation. See PASTE mode's own comment in main() for why this is
 * a genuinely separate mechanism from insert_char()'s one-keystroke-
 * per-int model, not a relaxation of it. */
static void insert_string(const char *s) {
    size_t slen = strlen(s);
    if (slen == 0) return;
    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    if (blen + slen + 1 >= sizeof(buf)) return;
    int cur = get_cursor(blen);
    memmove(buf + cur + slen, buf + cur, blen - (size_t)cur);
    memcpy(buf + cur, s, slen);
    blen += slen;
    buf[blen] = '\0';
    write_buffer(buf, blen);
    set_cursor(cur + (int)slen);
}

static void do_backspace(void) {
    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    int cur = get_cursor(blen);
    if (cur <= 0) return;
    memmove(buf + cur - 1, buf + cur, blen - (size_t)cur);
    blen--;
    buf[blen] = '\0';
    write_buffer(buf, blen);
    set_cursor(cur - 1);
}

/* Arrow keys while INTERACT: move cursor horizontally */
static void move_cursor(int delta) {
    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    int cur = get_cursor(blen) + delta;
    if (cur < 0) cur = 0;
    if ((size_t)cur > blen) cur = (int)blen;
    set_cursor(cur);
}

/* Up/down: same column on previous/next line (clamp to line length). */
static void move_cursor_line(int dir) {
    char buf[MAX_BUF];
    size_t blen = read_buffer(buf, sizeof(buf));
    int cur = get_cursor(blen);
    if (blen == 0) return;

    /* Line start of current line */
    int line_start = cur;
    while (line_start > 0 && buf[line_start - 1] != '\n') line_start--;
    int col = cur - line_start;

    if (dir < 0) {
        /* previous line */
        if (line_start == 0) return; /* already top */
        int prev_end = line_start - 1; /* the \n before current line */
        int prev_start = prev_end;
        while (prev_start > 0 && buf[prev_start - 1] != '\n') prev_start--;
        int prev_len = prev_end - prev_start;
        int new_col = col < prev_len ? col : prev_len;
        set_cursor(prev_start + new_col);
    } else {
        /* next line */
        int line_end = cur;
        while ((size_t)line_end < blen && buf[line_end] != '\n') line_end++;
        if ((size_t)line_end >= blen) return; /* already last line, no trailing \n past end */
        int next_start = line_end + 1;
        int next_end = next_start;
        while ((size_t)next_end < blen && buf[next_end] != '\n') next_end++;
        int next_len = next_end - next_start;
        int new_col = col < next_len ? col : next_len;
        set_cursor(next_start + new_col);
    }
}

/* 1 when chtpm has an engaged INTERACT/cli_io (active_index != -1).
 * Written by export_active_index() in chtpm_parser_pal. */
static int is_interact_typing(void) {
    char path[PATH_BUF], line[16];
    snprintf(path, sizeof(path),
             "%s/pieces/display/active_gui_is_typing.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    return atoi(line) != 0;
}

/* ---- Phase T4 additions (PLAN.md §4/§2) - real save/load. Reuses
 * agy_widget_cmds.c's own NEW/SAVE/SAVE_AS:<path>/LOAD:<path> inbox
 * unchanged (copied verbatim from editor_widget_cmds.c) - agy-txt has
 * no separate widget process to relay FROM, so these handlers write
 * directly into the same local inbox agy_widget_cmds.+x already drains
 * on every idle tick, then trigger an immediate drain instead of
 * waiting for the next idle tick (real menu commands should feel
 * instant, not laggy). ---- */

/* Which layout chtpm_parser_pal is currently showing - written fresh
 * every parse_chtm() call (confirmed real, chtpm_parser_pal.c). Lets
 * ONE shared key-dispatch op (this file, the only module every
 * agy-txt layout declares) behave differently per screen, same shape
 * as the TPMOS reference's own process_key()'s layout branching
 * (PLAN.md §1's own citation), just without a custom C daemon. */
static void read_current_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

/* Appends one command line to the local inbox, then immediately runs
 * agy_widget_cmds.+x to drain it - same real op agy_edit_key's own
 * idle branch already calls every tick, just triggered right away
 * instead of waited for. */
static void enqueue_and_drain(const char *cmd_line) {
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/pieces/system/widget_cmds", project_root);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", cmd_line); fclose(f); }

    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && ./ops/+x/agy_widget_cmds.+x 8 >/dev/null 2>&1",
             project_root);
    system(cmd);
}

/* file_menu.chtpm: KEY:1=NEW, KEY:2=SAVE (uses the CURRENT active
 * path, per agy_widget_cmds.c's own real SAVE behavior - errors with
 * "no active file" if none set yet, same as the reference project's
 * own real Save-with-no-path handling). Returns 1 if this layout
 * handled the key (caller should not fall through to editor dispatch). */
static int dispatch_file_menu(int key) {
    if (key == '1') { enqueue_and_drain("NEW"); bump_screen(); return 1; }
    if (key == '2') { enqueue_and_drain("SAVE"); bump_screen(); return 1; }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    /* PASTE mode (2026-07-30) — see fm_menu_input.c's own identical
     * addition for the full rationale: a real keypress is one int,
     * matching a physical keyboard's one-scancode-per-keystroke model
     * (key >= 32 && key <= 126 below can only ever be single-byte
     * ASCII, same as a real keyboard sends one scancode per press).
     * An emoji/multi-byte UTF-8 character was never one keystroke
     * either. This house's own directory tree has emoji path segments
     * that could never be typed character-by-character through
     * INTERACT for exactly this reason. PASTE takes the whole string
     * as one argv and inserts it verbatim (real UTF-8 bytes, no
     * per-character validation) at the cursor — only valid while
     * INTERACT is actually engaged (same real gate insert_char()'s
     * own call site already enforces), a no-op otherwise.
     * file_browser_save.chtpm/file_browser_load.chtpm no longer route
     * through this binary at all (PITFALL 65 rebuild) - they declare
     * <module>manager/+x/agy_browser_manager.+x</module> instead, a
     * separate native manager (see manager/agy_browser_manager.c). */
    char layout[256];
    read_current_layout(layout, sizeof(layout));

    if (argc >= 3 && strcmp(argv[1], "PASTE") == 0) {
        if (is_interact_typing()) {
            insert_string(argv[2]);
            bump_screen();
        }
        return 0;
    }

    int key = atoi(argv[1]);

    if (key == 0) {
        /* idle: drain widget cmd inbox (LOAD/SAVE/NEW) - covers the
         * case a command got enqueued but the immediate drain in
         * enqueue_and_drain() somehow didn't finish (defensive; the
         * immediate drain is the real path in normal use). */
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd),
                 "cd '%s' && ./ops/+x/agy_widget_cmds.+x 8 >/dev/null 2>&1",
                 project_root);
        system(cmd);
        return 0;
    }

    int typing = is_interact_typing();

    /* Phase T4 (PLAN.md §4): file_menu.chtpm gets its own dispatch,
     * checked BEFORE the editor's own piece.pdl-driven method dispatch
     * below - same "branch on which layout is active" shape the TPMOS
     * reference's own process_key() uses (PLAN.md §1), just one real
     * op instead of a custom daemon. file_browser_*.chtpm's own
     * dispatch is handled entirely above, before this point is ever
     * reached. */
    if (!typing) {
        if (strstr(layout, "file_menu.chtpm") != NULL) {
            if (dispatch_file_menu(key)) return 0;
        }
    }

    /* Method dispatch only when NOT in INTERACT (digits are text then).
     * Supports both ASCII digits ('2'-'5') and raw integers (2-5) as
     * 1-based method indices matching KEY:1-5 in piece.pdl. */
    if (!typing) {
        MenuItem items[MAX_MENU_ITEMS];
        int item_count = load_menu_items(items, MAX_MENU_ITEMS);
        int method_index = 0;

        if (key >= '1' && key <= '9')
            method_index = key - '0';
        else if (key >= 1 && key <= 99)
            method_index = key;

        if (method_index >= 1 && method_index <= item_count) {
            const char *cmd = items[method_index - 1].command;
            if (strcmp(cmd, "NEW") == 0) do_new_file();
            else if (strcmp(cmd, "CLEAR") == 0) do_clear();
            else if (strcmp(cmd, "EXIT") == 0) do_exit();
            else if (strcmp(cmd, "FM") == 0) do_fm();
            else set_message("Unknown command.");
            bump_screen();
            return 0;
        }
        return 0;
    }

    /* INTERACT canvas keys - editor_buffer.txt (T3's own real editing;
     * this point is only ever reached for editor.chtpm). */
    if (key >= 32 && key <= 126) {
        insert_char((char)key);
        bump_screen();
        return 0;
    }
    if (key == 127 || key == 8) {
        do_backspace();
        bump_screen();
        return 0;
    }
    if (key == 10 || key == 13) {
        insert_char('\n');
        bump_screen();
        return 0;
    }
    /* arrows: left/right char, up/down line (same column) - editor
     * canvas only. */
    if (key == 1000) { move_cursor(-1); bump_screen(); return 0; }      /* LEFT */
    if (key == 1001) { move_cursor(+1); bump_screen(); return 0; }      /* RIGHT */
    if (key == 1002) { move_cursor_line(-1); bump_screen(); return 0; } /* UP */
    if (key == 1003) { move_cursor_line(+1); bump_screen(); return 0; } /* DOWN */

    return 0;
}
