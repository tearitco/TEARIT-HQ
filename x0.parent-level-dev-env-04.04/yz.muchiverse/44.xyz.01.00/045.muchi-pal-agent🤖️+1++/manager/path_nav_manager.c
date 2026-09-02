/* path_nav_manager - persistent live-typing path completion for chat.chtpm.
 *
 * User request (2026-07-20 handoff chain, see #.haiku/sonnet/): "*" typed
 * anywhere in the chat message is meant to collide with ordinary chat text
 * containing a literal "*" - same tradeoff as gem-dev's own completion
 * trigger, confirmed acceptable by direct instruction ("it is what it
 * is"), not a bug to guard against.
 *
 * Pattern ported from gem-dev's manager/gem-dev_manager.c
 * (update_completions/handle_choose_path) and its ops/src/complete_path.c,
 * but this project has no persistent AI-query-loop manager of its own
 * (send_message.c/check_response.c are one-shot ops instead - see that
 * file's own header comment), so this is a NEW, narrowly-scoped
 * persistent process, launched as a second <module> line alongside the
 * existing `system/prisc+x pal/main_loop_chtpm.pal` one. Confirmed safe
 * to coexist: main_loop_chtpm.pal's own read_history of interact_relay.txt
 * only ever branches on key==13 (Enter); every digit key this process
 * injects for file selection (0-9) falls through to that script's own
 * no_key/sleep branch untouched.
 *
 * Tree building is the real difference from gem-dev: gem-dev's picker is
 * flat (one directory level per query, re-invoked as the user retypes a
 * deeper path). This one recursively pre-builds nested <button> markup
 * (matched dir open "[-]", its own children pre-listed but collapsed
 * "[+]") because chtpm_parser_pal.c's native fold support (search
 * "[+]"/"[-]" in a label, see its own header comment sec. on
 * fold_&lt;id&gt;) only toggles VISIBILITY of already-parsed children - it
 * does not re-invoke anything on expand, so any level the user might
 * expand has to already be present in the generated markup up front.
 * Depth is capped (MAX_DEPTH) to bound both output size and directory
 * walk cost. Style verified directly against
 * 1.TPMOS_c_+rmmp.0102.0028/projects/+-demo's own hand-written example of
 * this exact nested-button/fold shape.
 *
 * Selection dispatch: only files are selectable (onClick="KEY:0".."KEY:9",
 * capped at 10 live picks across the whole rendered tree - the same
 * digit-injection budget gem-dev's own 5-slot picker used, just doubled
 * since chtpm_parser_pal.c's inject_raw_key() only single-key-injects for
 * KEY:0..KEY:9, see its own send_command()). Directories are never
 * KEY-bound - expand/collapse is entirely the native fold click, no
 * dispatch code needed on this end at all. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PATH_LEN 4096
#define MAX_MARKUP 65536
/* Raised 10 -> 30 (2026-07-21, quick visibility test - direct user
 * request) to fix files being silently dropped once the GLOBAL pick
 * budget was exhausted by top-level files alone, before recursion ever
 * reached any subdirectory (jul-21-session-handoff-FINAL.txt has the
 * full trace). KNOWN LIMITATION, not fixed by this change alone:
 * chtpm_parser_pal.c's own send_command() (system/chtpm_parser_pal.c
 * line ~2004-2010) only converts k=0..9 into a real digit-character
 * keypress (`inject_raw_key('0'+k)`) - for k>=10 it falls through to
 * `inject_raw_key(k)`, injecting the RAW INTEGER as a key code instead
 * (e.g. onClick="KEY:13" on the 14th pick would inject raw code 13,
 * which is Enter/CR in this same codebase, not "select item 13"). So
 * this raise fixes VISIBILITY (files no longer silently vanish) but
 * picks beyond index 9 are NOT safely selectable yet - that needs a
 * matching fix on the send_command() side (decompose k>=10 into a real
 * multi-keystroke sequence, or a different selection mechanism
 * entirely) before being considered actually fixed end-to-end. */
#define MAX_PICKS 30
#define MAX_DEPTH 6
#define MAX_ENTRIES_PER_DIR 40

static volatile sig_atomic_t g_shutdown = 0;
static void handle_sig(int s) { (void)s; g_shutdown = 1; }

static bool g_nav_mode = false;
static char g_last_typed[2048] = "";
static char g_nav_menu[MAX_MARKUP] = "";
static char g_pick_paths[MAX_PICKS][MAX_PATH_LEN];
static int g_pick_count = 0;
static long g_relay_last_pos = -1;
static int g_last_typing_state = -1; /* mirrors active_gui_is_typing.txt; -1 = not read yet */
/* Position (not a bare bool - see below) of the '*' that was dismissed by
 * REACTIVATING (poll_reactivate_closes_picker()), or -1 if nothing is
 * suppressed. Without this, the leftover '*' still sitting in the buffer
 * keeps looking like a fresh trigger on every subsequent keystroke (each
 * one is a new g_last_typed value, strrchr still finds the SAME old '*'),
 * reopening + re-deactivating the picker right as the user is mid-type -
 * live-reported as flicker.
 *
 * A bare "suppress until no '*' anywhere in the text" flag (the first cut
 * at this fix) was ALSO wrong, live-caught: typing " hi how are u *" after
 * dismissing left the ORIGINAL '*' still present earlier in the string, so
 * suppression never lifted even for a genuinely NEW, later '*' the user
 * deliberately typed - "didn't retrigger picker" was the exact complaint.
 * Tracking the DISMISSED star's own position and comparing it against
 * strrchr's current result fixes both: same position (user just kept
 * typing past the one they dismissed) stays suppressed, any different
 * position (a fresh '*' appeared, earlier OR later) is a new occurrence
 * and triggers normally. */
static long g_suppressed_star_offset = -1;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

/* Both pulses are required, same as gem-dev's own update_completions()/
 * handle_choose_path() (pulse_state_changed() + trigger_render(), never
 * just one): state_changed.txt growth is what makes chtpm_parser_pal.c's
 * own main loop call load_vars()+parse_chtm() again (re-reading nav_menu
 * from gui_state.txt and rebuilding the element tree); frame_changed.txt
 * growth only triggers a redraw of whatever tree is ALREADY parsed. Pulsing
 * only the render marker (an earlier version of this function did exactly
 * that) redraws the stale startup-time tree forever - nav_menu updates
 * never appear on screen even though gui_state.txt has the right value. */
static void pulse_render(void) {
    FILE *f = fopen("pieces/apps/player_app/state_changed.txt", "a");
    if (f) { fputs("N\n", f); fclose(f); }
    f = fopen("pieces/display/frame_changed.txt", "a");
    if (f) { fputs("N\n", f); fclose(f); }
}

/* MERGE, never truncate-and-replace: chtpm_parser_pal.c's own
 * save_cli_io_gui_state() (called on every keystroke, see its own
 * resolve_cli_io_project_id()/save_to_gui_state_impl() chain) writes the
 * live "message_input=..." value into this EXACT SAME file - it's the
 * shared per-project gui_state.txt, not something private to this
 * manager. An earlier version of this function opened the file with "w"
 * and wrote ONLY nav_menu=, wiping out message_input= (and anything else
 * already there) on every single picker update - the parser's own next
 * read-back would then restore a stale/empty value into the cli_io's
 * on-screen text, which is exactly the "picked path doesn't appear in
 * the Message field" bug this was chasing. Read the existing file first,
 * keep every OTHER key byte-for-byte untouched, only replace (or add)
 * nav_menu=. */
/* input_override: NULL preserves whatever message_input= is already in
 * the file untouched (the normal case - update_nav_menu()/exit_nav_mode()
 * use this). Non-NULL replaces it - used ONLY by choose_path() after a
 * splice, because chtpm_parser_pal.c's own sync_cli_input_from_gui_state()
 * re-reads message_input= from this file on every render and force-
 * overwrites the in-memory cli_io buffer with it (see that function's own
 * comment on pal-forum's free-text cli_io fix) - without this, the OLD
 * pre-splice text (still sitting in the preserved message_input= line)
 * would keep winning over the newly-picked path forever, which is exactly
 * the "picked path never appears in the Message field" bug this chases. */
static void write_gui_state(const char *input_override) {
    char kept[MAX_MARKUP];
    size_t kept_len = 0;
    kept[0] = '\0';

    FILE *rf = fopen("projects/muchi-pal-agent/manager/gui_state.txt", "r");
    if (rf) {
        char line[MAX_MARKUP];
        while (fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "nav_menu=", 9) == 0) continue;
            if (input_override && strncmp(line, "message_input=", 14) == 0) continue;
            size_t llen = strlen(line);
            if (kept_len + llen + 1 >= sizeof(kept)) continue;
            memcpy(kept + kept_len, line, llen);
            kept_len += llen;
            kept[kept_len] = '\0';
        }
        fclose(rf);
    }

    FILE *f = fopen("projects/muchi-pal-agent/manager/gui_state.txt", "w");
    if (!f) return;
    fputs(kept, f);
    if (input_override) fprintf(f, "message_input=%s\n", input_override);
    fprintf(f, "nav_menu=%s\n", g_nav_menu);
    fclose(f);
}

/* Escapes a path for safe use inside a double-quoted chtpm label/attribute:
 * chtpm attribute parsing splits on '"', so a literal '"' in a filename
 * would break the generated markup - strip anything that isn't a plain
 * path character rather than risk malformed tags from untrusted dirents. */
static void sanitize_for_markup(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        char c = in[i];
        if (c == '"' || c == '<' || c == '>' || c == '&') continue;
        out[j++] = c;
    }
    out[j] = '\0';
}

static void sanitize_id(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        out[j++] = isalnum((unsigned char)in[i]) ? in[i] : '_';
    }
    out[j] = '\0';
}

/* Appends one directory's contents as nested <button> markup into buf.
 * dir_path is relative to cwd (the session root); rel_path is what's
 * shown to the user (same value here - no separate sandbox root in this
 * project). depth 0 is the top matched level (rendered open, "[-]");
 * deeper levels default collapsed ("[+]") per +-demo's own convention. */
static void append_dir_markup(char *buf, size_t buf_sz, const char *dir_path,
                               const char *filter_prefix, int depth) {
    DIR *d = opendir(dir_path[0] ? dir_path : ".");
    if (!d) return;

    struct dirent *entries[MAX_ENTRIES_PER_DIR];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < MAX_ENTRIES_PER_DIR) {
        if (e->d_name[0] == '.') continue;
        if (filter_prefix[0] && strncmp(e->d_name, filter_prefix, strlen(filter_prefix)) != 0) continue;
        /* readdir's own buffer is reused per-call, so copy the entry out
         * now rather than storing the dirent pointer past the next call. */
        struct dirent *copy = malloc(sizeof(struct dirent));
        if (copy) { memcpy(copy, e, sizeof(struct dirent)); entries[n++] = copy; }
    }
    closedir(d);

    /* Simple insertion sort by name - n is capped small (MAX_ENTRIES_PER_DIR). */
    for (int i = 1; i < n; i++) {
        struct dirent *key = entries[i];
        int j = i - 1;
        while (j >= 0 && strcmp(entries[j]->d_name, key->d_name) > 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }

    for (int i = 0; i < n; i++) {
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s%s%s", dir_path, dir_path[0] ? "/" : "", entries[i]->d_name);
        struct stat st;
        bool is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        char safe_name[300], safe_id[300];
        sanitize_for_markup(entries[i]->d_name, safe_name, sizeof(safe_name));
        sanitize_id(full, safe_id, sizeof(safe_id));

        /* No literal '\n' anywhere in this generated markup: gui_state.txt is a
         * flat, line-delimited key=value store (see write_gui_state()) - a raw
         * newline byte inside the nav_menu value would truncate it at the first
         * line when read back by the parser's own line-based get_var(), the same
         * way gem-dev's own g_menu_area concatenates <button ...><br/> fragments
         * with no '\n' between them and relies purely on <br/> for line breaks. */
        char line[1024];
        if (is_dir) {
            const char *marker = (depth == 0) ? "[-]" : "[+]";
            snprintf(line, sizeof(line), "<button label=\"%s/ %s\" id=\"nav_%s\"><br/>",
                     safe_name, marker, safe_id);
            strncat(buf, line, buf_sz - strlen(buf) - 1);

            if (depth + 1 < MAX_DEPTH) {
                append_dir_markup(buf, buf_sz, full, "", depth + 1);
            }
            strncat(buf, "</button><br/>", buf_sz - strlen(buf) - 1);
        } else if (g_pick_count < MAX_PICKS) {
            int idx = g_pick_count;
            snprintf(g_pick_paths[idx], MAX_PATH_LEN, "%s", full);
            g_pick_count++;
            /* NAV_PICK:%d, not KEY:%d (2026-07-21) - KEY:n only encodes
             * a single ASCII digit correctly (chtpm_parser_pal.c's own
             * send_command()); MAX_PICKS is now 30, so indices 10-29
             * need their own real multi-digit command, not the raw
             * integer-as-key-code that KEY:n falls back to past 9 (see
             * inject_raw_line()'s own header comment for the exact
             * collision - KEY:13 would inject Enter/CR, not "pick 13"). */
            snprintf(line, sizeof(line), "<button label=\"%s\" onClick=\"NAV_PICK:%d\" id=\"nav_%s\" /><br/>",
                     safe_name, idx, safe_id);
            strncat(buf, line, buf_sz - strlen(buf) - 1);
        }
        if (strlen(buf) + 512 >= buf_sz) break;
    }
    for (int i = 0; i < n; i++) free(entries[i]);
}

/* Auto-activation (gem-dev's own update_completions()/handle_choose_path()
 * pattern: write pieces/display/active_gui_index.txt to jump keyboard focus
 * onto the suggestions panel the instant matches appear, then restore focus
 * back to the input field on exit - so the user doesn't have to manually
 * navigate down into the picker or back out of it). gem-dev's own version
 * has to read-and-remember the CALLER's focus index first, because its
 * Suggestions header isn't at a fixed position in that layout. This project
 * doesn't need that: chat.chtpm was deliberately reordered so <cli_io
 * id="message_input"> is the ONLY interactive element before ${nav_menu},
 * making it permanently interactive index 1 - so "restore" is always just
 * "write 1 back", no read-first-remember-later needed. First nav button (if
 * any match exists) always lands at index 2 for the same reason. */
static void set_active_gui_index(int idx) {
    FILE *f = fopen("pieces/display/active_gui_index.txt", "w");
    if (f) { fprintf(f, "%d\n", idx); fclose(f); }
}

/* Asks chtpm_parser_pal.c to force active_index = -1 (see its own
 * last_force_deactivate_size declaration comment and the force_deactivate_ch
 * check in main()) - direct instruction: the picker should un-activate the
 * cli_io itself the instant it appears ("[^]" -> "[>]" on the Message line),
 * not require the user to press ESC by hand first. gem-dev never did this
 * (still requires manual ESC there), a deliberate improvement here, not a
 * gap being closed. Growth-tracked marker file, same idiom as pulse_render(). */
static void request_deactivate(void) {
    FILE *f = fopen("pieces/display/force_deactivate.txt", "a");
    if (f) { fputs("D\n", f); fclose(f); }
}

/* Splits "dir/prefix" into the directory to scan and the name-prefix to
 * filter on, same convention as gem-dev's own complete_path.c: everything
 * after the last '/' is the filter, everything before it is the dir (no
 * slash at all -> scan "." with the whole string as filter). */
static void update_nav_menu(const char *path_query) {
    bool was_empty = (g_nav_menu[0] == '\0');
    g_pick_count = 0;
    g_nav_menu[0] = '\0';

    char dir_path[MAX_PATH_LEN] = "";
    const char *filter = path_query;
    const char *last_slash = strrchr(path_query, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - path_query);
        if (dlen == 0) snprintf(dir_path, sizeof(dir_path), "/");
        else { size_t cp = dlen < sizeof(dir_path) - 1 ? dlen : sizeof(dir_path) - 1; memcpy(dir_path, path_query, cp); dir_path[cp] = '\0'; }
        filter = last_slash + 1;
    }

    append_dir_markup(g_nav_menu, sizeof(g_nav_menu), dir_path, filter, 0);
    /* Both only on the empty->populated edge, not every re-filter keystroke:
     * forcing focus back to item 2 (and re-pulsing render) on every single
     * keystroke while the tree is already open was visibly flickering the
     * focus indicator during continued typing - live-reported. */
    if (g_nav_menu[0] != '\0' && was_empty) {
        set_active_gui_index(2);
        request_deactivate();
    }
    write_gui_state(NULL);
    pulse_render();
}

/* input_override forwarded straight to write_gui_state() - see that
 * function's own comment. choose_path() passes the newly-spliced text
 * (message_input= must move forward to it, or the parser's own
 * sync_cli_input_from_gui_state() re-asserts the stale pre-splice value
 * on the very next render); the plain '*' vanished from the typed line
 * case (star no longer present at all - user backspaced or retyped) has
 * nothing new to assert, so it passes NULL and leaves message_input=
 * exactly as the parser itself last wrote it. */
static void exit_nav_mode(const char *input_override) {
    g_nav_mode = false;
    g_nav_menu[0] = '\0';
    g_pick_count = 0;
    set_active_gui_index(1);
    write_gui_state(input_override);
    pulse_render();
}

/* Splices the chosen file's path into the live input line, replacing
 * everything from the trigger '*' onward - mirrors gem-dev's own
 * handle_choose_path() truncate-at-star-then-append approach. */
static void choose_path(int idx) {
    if (idx < 0 || idx >= g_pick_count) { exit_nav_mode(NULL); return; }

    char before[1024] = "";
    char *star = strrchr(g_last_typed, '*');
    if (star) {
        size_t blen = (size_t)(star - g_last_typed);
        if (blen >= sizeof(before)) blen = sizeof(before) - 1;
        memcpy(before, g_last_typed, blen);
        before[blen] = '\0';
    }

    char new_line[2048];
    snprintf(new_line, sizeof(new_line), "%s%s", before, g_pick_paths[idx]);

    FILE *f = fopen("pieces/apps/player_app/cli_buffers.txt", "a");
    if (f) { fprintf(f, "m%s\n", new_line); fclose(f); }

    snprintf(g_last_typed, sizeof(g_last_typed), "%s", new_line);
    exit_nav_mode(new_line);
}

/* Reads the last "m..."-prefixed line from cli_buffers.txt - the live
 * typed content of the message_input cli_io (prefix is the first char of
 * its id, "message_input" -> 'm', per chtpm_parser_pal.c's own generic
 * per-keystroke fallback append). Small file, re-scanned whole each tick
 * rather than tracked incrementally - simpler and cheap at this size. */
static bool read_last_typed(char *out, size_t out_sz) {
    FILE *f = fopen("pieces/apps/player_app/cli_buffers.txt", "r");
    if (!f) return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'm') {
            char *content = line + 1;
            content[strcspn(content, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", content);
            found = true;
        }
    }
    fclose(f);
    return found;
}

/* Polls interact_relay.txt (appended to by chtpm_parser_pal.c's own
 * NAV_PICK:n onClick dispatch, see send_command()'s "NAV_PICK:" branch
 * and inject_raw_line()'s own header comment) for newly appended pick
 * lines, from this process's own tracked offset - same incremental-tail
 * idea as gem-dev's g_buf_last_pos, just against the relay file instead
 * of the input-buffer log.
 *
 * HISTORY (2026-07-21): this used to match bare "KEY:n" dispatch
 * (n=0..9 only, single ASCII digit only) via a raw decimal-ASCII-code
 * comparison here. Raised MAX_PICKS above 10 needed real multi-digit
 * selection, which KEY:n's own encoding can't express (KEY:13 would
 * inject raw code 13 - Enter/CR in this same codebase - not "pick 13").
 * Switched the whole pipeline (onClick markup + this parser) to a
 * distinctly-tagged NAV_PICK:n line instead of trying to decompose/
 * reassemble multi-digit key sequences through the single-key path. */
static void poll_relay_for_picks(void) {
    const char *path = "pieces/apps/player_app/interact_relay.txt";
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (g_relay_last_pos < 0) {
        fseek(f, 0, SEEK_END);
        g_relay_last_pos = ftell(f);
        fclose(f);
        return;
    }
    fseek(f, g_relay_last_pos, SEEK_SET);
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        if (g_nav_mode) {
            char *t = trim(line);
            if (strncmp(t, "NAV_PICK:", 9) == 0) {
                choose_path(atoi(t + 9));
            }
        }
    }
    g_relay_last_pos = ftell(f);
    fclose(f);
}

/* Second half of the request: "if activated again, close picker" - direct
 * instruction. chtpm_parser_pal.c writes pieces/display/active_gui_is_typing.txt
 * on every export_active_index() call (1 = a cli_io is genuinely active/
 * typing right now, 0 = focus-only - see that file's own header comment
 * where it's written). request_deactivate() above already forces this to 0
 * the instant the picker appears; if the user then presses Enter on the
 * still-focused Message field again (the engine's own normal cli_io-Enter-
 * activates behavior, untouched), this flips back to 1. Catching that
 * SPECIFIC 0->1 edge while the picker is still open is exactly "user asked
 * to type again while the picker was up" - treated as a close request,
 * same as backspacing the trigger '*' away. No engine change needed for
 * this half: active_gui_is_typing.txt was already being written, just
 * never read by anything until now. */
static void poll_reactivate_closes_picker(void) {
    FILE *f = fopen("pieces/display/active_gui_is_typing.txt", "r");
    if (!f) return;
    char line[16] = "";
    int state = fgets(line, sizeof(line), f) ? atoi(line) : -1;
    fclose(f);
    if (state < 0) return;

    if (g_last_typing_state == 0 && state == 1 && g_nav_mode) {
        char *star = strrchr(g_last_typed, '*');
        g_suppressed_star_offset = star ? (long)(star - g_last_typed) : -1;
        exit_nav_mode(NULL);
    }
    g_last_typing_state = state;
}

int main(void) {
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    while (!g_shutdown) {
        char typed[1024] = "";
        bool have_line = read_last_typed(typed, sizeof(typed));

        if (have_line && strcmp(typed, g_last_typed) != 0) {
            snprintf(g_last_typed, sizeof(g_last_typed), "%s", typed);
            char *star = strrchr(g_last_typed, '*');
            long star_offset = star ? (long)(star - g_last_typed) : -1;
            if (star_offset != g_suppressed_star_offset) {
                g_suppressed_star_offset = -1; /* different (or no) '*' - any old suppression no longer applies */
            }
            if (star && g_suppressed_star_offset < 0) {
                /* Poll-timing race, live-caught: this poll loop only checks
                 * active_gui_is_typing.txt every 40-150ms, coarse enough that
                 * the ORIGINAL "user activated the cli_io and started typing"
                 * 0->1 transition can be missed entirely and only observed
                 * LATER, after nav_mode is already true (once this trigger
                 * fires) - poll_reactivate_closes_picker() then sees that
                 * late-arriving "1" as if it were a FRESH reactivation while
                 * the picker was open, closing it immediately (confirmed via
                 * manager_debug.log: EXIT_NAV firing right after the very
                 * update_nav_menu() call that just opened it). We KNOW the
                 * user was actively typing right up to this trigger - sync
                 * the baseline to 1 explicitly here so the next
                 * poll_reactivate_closes_picker() call doesn't mistake
                 * catching up on stale history for a real user action. */
                g_last_typing_state = 1;
                g_nav_mode = true;
                update_nav_menu(star + 1);
            } else if (g_nav_mode) {
                exit_nav_mode(NULL);
            }
        }

        poll_relay_for_picks();
        poll_reactivate_closes_picker();

        usleep(g_nav_mode ? 40000 : 150000);
    }
    return 0;
}
