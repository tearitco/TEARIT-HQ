/* title_input - one verb, one binary, no shared headers.
 * The title screen's own small digit/Enter accumulator - same preview-
 * then-commit model as ops/choice.c (see that file's header comment
 * for the full citation of where this pattern was ported from:
 * chtmp_parser.c in real 1.TPMOS), but deliberately a SEPARATE, simpler
 * op rather than reusing choice.c directly: there's no hero piece.pdl
 * to dispatch against yet at title time (New Game hasn't created one),
 * and this screen's whole option list is just "New Game" + one row per
 * existing save - a flat 1-based list, no piece.pdl-index-based
 * 2-based scheme needed. Up/down arrows also move the cursor by one row
 * (clamped, not wrapped, at either end) - an addition alongside the
 * original digit-jump input, not a replacement for it; both remain live
 * at once, same as gameplay's own wasd-or-arrows dual input.
 *
 * State persisted in pieces/system/title_state.txt (cursor,
 * digit_accum) - same reasoning as hero/state.txt's action_cursor/
 * digit_accum: every keypress here is a fresh short-lived process, so
 * the accumulator has to live in a file, not in memory.
 *
 * Commit action (Enter):
 *   - cursor 1 (New Game): reset the live pieces/world_01/ from the
 *     pristine pieces/world_01_template/ (rm -rf + cp -r, matching
 *     real 1.TPMOS op-ed's own save_game_to_path()/load_game_from_path()
 *     shell-out precedent - see dox/01-cdda-architecture.md and
 *     platform-vision.txt for the citation).
 *   - cursor 2..N+1 (Load <save>): same rm -rf + cp -r, but source is
 *     the chosen pieces/saves/save_N/world_01/ instead of the template.
 * Either way, on success this appends a sentinel keycode (999 - outside
 * any real key's range, including the 1000-1003 arrow codes) to
 * pieces/apps/player_app/history.txt. pal/main_loop.pal's title-phase
 * loop reads history.txt every tick the same way it always does
 * (read_history + beq - prisc+x has NO opcode that can check an
 * arbitrary file's existence/size by path; read_pos/read_layout are
 * both hardcoded to their own specific files, confirmed by reading
 * system/prisc+x.c's parser directly rather than assumed) and jumps
 * into the real gameplay loop the instant it sees 999, on the very
 * next tick after this op ran. This is the exact same synthetic-
 * keycode-injection mechanism already proven out (and later reverted
 * for its original use case) earlier in this project's numbered-choice
 * work - see nav-refactor-2.txt. Single-process hand-off, not a
 * restart: the same prisc+x/keyboard_input/renderer trio keeps running
 * straight through the title->gameplay transition.
 *
 * Usage: title_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_SAVES 32

/* Same ARROW_* sentinel values keyboard_input.c/gl_mirror.c/move_player.c
 * already use everywhere else in this project - up/down move the cursor
 * by one row, clamped (not wrapped) at either end of the list. */
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
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

/* Directory-order scan of pieces/saves/ - same order
 * compose_title_frame.c's renderer uses, so the row a save is drawn at
 * and the row this op resolves a digit to can never drift (same
 * anti-drift reasoning as every other panel/list in this project). */
static int load_save_names(char names[][64], int max_names) {
    char saves_dir[PATH_BUF];
    snprintf(saves_dir, sizeof(saves_dir), "%s/pieces/saves", project_root);
    DIR *d = opendir(saves_dir);
    if (!d) return 0;
    struct dirent *entry;
    int n = 0;
    while (n < max_names && (entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        /* save_N names are genuinely short despite dirent's d_name
         * being declared unbounded - same class of warning already
         * suppressed narrowly elsewhere in this project. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(names[n], 64, "%s", entry->d_name);
#pragma GCC diagnostic pop
        n++;
    }
    closedir(d);
    return n;
}

static void reset_world(const char *src_world_dir) {
    char world_dst[PATH_BUF];
    snprintf(world_dst, sizeof(world_dst), "%s/pieces/world_01", project_root);

    char cmd[(PATH_BUF + 32) * 2 + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && cp -r '%s' '%s'", world_dst, src_world_dir, world_dst);
    int rc = system(cmd);
    (void)rc;

    /* Sentinel keycode 999 tells pal/main_loop.pal's title-phase loop
     * to jump into gameplay on its very next tick - see this file's
     * header comment. */
    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *f = fopen(history_path, "a");
    if (f) { fprintf(f, "999\n"); fclose(f); }
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/title_state.txt", project_root);
    int cursor = read_kv_int(state_path, "cursor", 1);
    int digit_accum = read_kv_int(state_path, "digit_accum", 0);

    char save_names[MAX_SAVES][64];
    int save_count = load_save_names(save_names, MAX_SAVES);
    int total = save_count + 1; /* + New Game */

    int is_digit = (key >= '0' && key <= '9');
    int is_enter = (key == 10 || key == 13);

    if (key == ARROW_UP) {
        if (cursor > 1) cursor--;
        digit_accum = 0;
    } else if (key == ARROW_DOWN) {
        if (cursor < total) cursor++;
        digit_accum = 0;
    } else if (is_digit) {
        int d = key - '0';
        int new_val = digit_accum * 10 + d;
        if (new_val >= 1 && new_val <= total) {
            digit_accum = new_val;
            cursor = new_val;
        } else if (d >= 1 && d <= total) {
            digit_accum = d;
            cursor = d;
        } else {
            digit_accum = 0;
        }
    } else if (is_enter) {
        if (cursor == 1) {
            char tpl_dir[PATH_BUF];
            snprintf(tpl_dir, sizeof(tpl_dir), "%s/pieces/world_01_template", project_root);
            reset_world(tpl_dir);
        } else if (cursor >= 2 && cursor <= total) {
            char save_world[PATH_BUF];
            snprintf(save_world, sizeof(save_world), "%s/pieces/saves/%s/world_01", project_root, save_names[cursor - 2]);
            reset_world(save_world);
        }
        digit_accum = 0;
    } else {
        digit_accum = 0;
    }

    FILE *f = fopen(state_path, "w");
    if (f) {
        fprintf(f, "cursor=%d\n", cursor);
        fprintf(f, "digit_accum=%d\n", digit_accum);
        fclose(f);
    }
    return 0;
}
