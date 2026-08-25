/* xlector_input - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/,
 * see shared-ops-refactor-plan.txt for the why).
 *
 * THE STANDARD REFERENCE IMPLEMENTATION of the real xlector active-
 * target pattern, ported faithfully from real 1.TPMOS's
 * `projects/fuzz-op/manager/fuzz-op_manager.c` (`route_input()`, read
 * in full - not the mutaclsym adaptation from an earlier pass, which
 * simplified several things this file deliberately restores). Any
 * pal/prisc+x project that wants xlector consumes THIS file directly
 * from shared-ops/ (compiled into that project's own ops/+x/, see
 * shared-ops-refactor-plan.txt §4 for the mechanics) - see
 * z0.zoo_0000's own dox/xlector-standard.md for the full write-up of
 * what's genuinely project-agnostic here (all of it, after the
 * genericization below) vs. what a project might still need to add
 * itself (giving its own pieces a `select` METHOD row).
 *
 * THE CORE IDEA: there is no "player" by default. A single global
 * cursor piece, `xlector`, is the DEFAULT thing wasd/arrows and method
 * hotkeys act on - not a hero, not any specific entity. Enter, while
 * controlling xlector, scans the current map for a piece sitting at
 * xlector's own tile and - if that piece's own piece.pdl has a
 * `select` METHOD row (the per-piece, per-game authorization gate:
 * "depending on if the game .pdl dictates that in this particular
 * game xlector is allowed to control those entities", direct user
 * instruction, not a hardcoded name-pattern exclusion like real
 * fuzz-op's own `strstr(name, "zombie")` check - piece.pdl-driven
 * exclusion is more correct AND more agnostic across different games)
 * - retargets control to it. '9' or Escape (27) is checked FIRST,
 * unconditionally, every single tick, before anything else: if
 * `active_target_id != "xlector"`, snap back to `xlector` immediately
 * and do nothing else that tick. This exact ordering is real
 * fuzz-op's own `route_input()` structure (line ~550 in that file),
 * not a reinterpretation.
 *
 * THE MENU DIFFERS BY WHOEVER IS CURRENTLY CONTROLLED, ENTIRELY FOR
 * FREE: method hotkeys (2-9) always resolve against
 * `active_target_id`'s OWN piece.pdl (via pdl_reader.+x, exactly the
 * same generic call real fuzz-op's own method_key handler makes -
 * confirmed at that file's line ~716). Xlector itself is just another
 * piece with its own (mostly empty) piece.pdl - when it's the active
 * target, the menu is whatever XLECTOR's piece.pdl says (typically
 * nothing beyond move/select, i.e. genuinely no numbered options);
 * once something else is selected, the exact same code path now reads
 * THAT piece's piece.pdl instead, and its real methods appear. Zero
 * special-casing anywhere in this file for "is this xlector's menu or
 * an entity's menu" - it's the same one lookup, parameterized by
 * whichever id currently sits in active_target_id.
 *
 * SHADOW XLECTOR SYNC: whenever the currently-controlled entity moves,
 * xlector's OWN state.txt position is updated to match (real fuzz-op's
 * own documented "SHADOW XLECTOR SYNC" comment, line ~813) - so
 * whenever control is later relinquished (`9`/Escape), the cursor is
 * sitting exactly where you left off, not wherever it was before you
 * selected something.
 *
 * GENERICIZED (this file originally had a `#define MAP_ID` hardcoded
 * to one project's own single map - that's what made it
 * un-shareable). Now resolves xlector's own directory (and from it,
 * its containing map) via a dynamic world_NN/map_NN/xlector/ scan, the
 * same pattern pdl_reader.c's own resolve_piece_pdl_path() and
 * move_entity.c's own resolve_piece_dir() already use - see either
 * file for the identical shape. This also means cross-map xlector
 * following now works for free the moment a project has more than one
 * map (real fuzz-op's own xlector inherits the selected entity's
 * map_id across maps - the old hardcoded-single-map version explicitly
 * named this as deferred; the dynamic scan removes the reason it was
 * deferred, since active_target's own directory is now resolved
 * fresh every call rather than assumed to live under one fixed map).
 *
 * OUT OF SCOPE THIS PASS (named, not silently skipped): joystick
 * keycodes (real fuzz-op maps 2000-2009/2100-2103 range codes too -
 * this file only handles the keyboard's own bare-decimal/ANSI-arrow
 * codes; add them the same way if a joystick reader ever exists for a
 * project using this standard).
 *
 * Usage: xlector_input.+x <keycode>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Same pattern as move_entity.c's own resolve_piece_dir()/
 * pdl_reader.c's own resolve_piece_pdl_path() - scan
 * world_NN/map_NN/<piece_id>/ for the piece's own directory, falling
 * back to a flat pieces/<piece_id>/ layout. */
static void resolve_piece_dir(const char *piece_id, char *out, size_t out_sz) {
    char pieces_dir[PATH_BUF];
    snprintf(pieces_dir, sizeof(pieces_dir), "%s/pieces", project_root);

    DIR *worlds = opendir(pieces_dir);
    if (worlds) {
        struct dirent *w;
        while ((w = readdir(worlds)) != NULL) {
            if (strncmp(w->d_name, "world_", 6) != 0) continue;
            char maps_dir[PATH_BUF + 256];
            snprintf(maps_dir, sizeof(maps_dir), "%s/%s", pieces_dir, w->d_name);
            DIR *maps = opendir(maps_dir);
            if (!maps) continue;
            struct dirent *m;
            while ((m = readdir(maps)) != NULL) {
                if (strncmp(m->d_name, "map_", 4) != 0) continue;
                char candidate[PATH_BUF + 512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(candidate, sizeof(candidate), "%s/%s/%s", maps_dir, m->d_name, piece_id);
#pragma GCC diagnostic pop
                struct stat st;
                if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
                    closedir(maps);
                    closedir(worlds);
                    snprintf(out, out_sz, "%s", candidate);
                    return;
                }
            }
            closedir(maps);
        }
        closedir(worlds);
    }

    snprintf(out, out_sz, "%s/pieces/%s", project_root, piece_id);
}

static void piece_state_path(const char *piece_id, char *out, size_t out_sz) {
    char piece_dir[PATH_BUF + 512];
    resolve_piece_dir(piece_id, piece_dir, sizeof(piece_dir));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out, out_sz, "%s/state.txt", piece_dir);
#pragma GCC diagnostic pop
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

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { snprintf(out, out_sz, "%s", eq + 1); break; }
    }
    fclose(f);
}

/* Generic find-or-append single-key writer, reusable across any of
 * this op's several target files (xlector's own state, or whichever
 * piece is currently active_target_id) and any key. Safe against the
 * real newline-in-place-mutation bug found in mutaclsym (see that
 * project's dox/00-HANDOFF.md) - values are always written fresh via
 * snprintf, never stripped-in-place from an aliased pointer. */
static void set_kv_str(const char *path, const char *key, const char *value) {
    char lines[64][MAX_LINE];
    int nlines = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    int found = 0;
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < nlines; i++) {
        if (!found && strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
            continue;
        }
        fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void set_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    set_kv_str(path, key, buf);
}

/* Checks whether `piece_id` may be xlector-selected - real 1.TPMOS's
 * own fuzz-op excludes by hardcoded name pattern ("Can't select
 * zombies"); this standard instead asks the piece's own piece.pdl,
 * per direct instruction: selectability is a per-game, per-piece
 * authorization the piece.pdl itself grants (a `select` METHOD row),
 * not a name convention every adopting game would have to keep in
 * sync by hand. */
static int piece_is_selectable(const char *piece_id) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' %s has_method select", project_root, piece_id);
#pragma GCC diagnostic pop
    FILE *pf = popen(cmd, "r");
    if (!pf) return 0;
    char line[16] = "0";
    if (!fgets(line, sizeof(line), pf)) line[0] = '0';
    pclose(pf);
    return line[0] == '1';
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    int key = atoi(argv[1]);
    resolve_root();

    char xlector_path[PATH_BUF];
    piece_state_path("xlector", xlector_path, sizeof(xlector_path));

    char active_target[64];
    read_kv_str(xlector_path, "active_target_id", active_target, sizeof(active_target), "xlector");
    int xlector_x = read_kv_int(xlector_path, "pos_x", 0);
    int xlector_y = read_kv_int(xlector_path, "pos_y", 0);

    int is_enter = (key == 10 || key == 13);
    int is_reset = (key == '9' || key == 27);
    int is_move = (key == 'w' || key == 'a' || key == 's' || key == 'd' ||
                   key == ARROW_UP || key == ARROW_DOWN || key == ARROW_LEFT || key == ARROW_RIGHT);

    /* '9'/Escape ALWAYS checked first, unconditionally - real
     * fuzz-op_manager.c's own route_input() ordering, confirmed at its
     * line ~550. Returns immediately either way (even if already on
     * xlector, matching that file's own early-return shape), so
     * nothing below this block ever also fires on the same keypress. */
    if (is_reset) {
        if (strcmp(active_target, "xlector") != 0) {
            set_kv_str(xlector_path, "active_target_id", "xlector");
        }
        set_kv_int(xlector_path, "last_key", key);
        return 0;
    }

    /* ASCII<->emoji display toggle - same 'e'/'E' key and pure-
     * display-preference reasoning as mutaclsym's own ops/choice.c
     * (see that project's dox/00-HANDOFF.md for the full writeup).
     * Stored on xlector's own state.txt since it's the one piece that
     * always exists in this standard - a global display preference,
     * not tied to whichever entity happens to be selected. Checked
     * early/unconditionally, same priority as is_reset above, and
     * touches nothing else (no digit_accum/action_cursor reset) -
     * flipping how tiles are drawn shouldn't cancel whatever the
     * player was doing. */
    if (key == 'e' || key == 'E') {
        int emoji_mode = read_kv_int(xlector_path, "emoji_mode", 0);
        set_kv_int(xlector_path, "emoji_mode", !emoji_mode);
        set_kv_int(xlector_path, "last_key", key);
        return 0;
    }

    /* Enter while controlling xlector scans every OTHER piece on
     * xlector's OWN CURRENT MAP (resolved dynamically, not a hardcoded
     * MAP_ID - see this file's own header comment) for one sitting at
     * xlector's own tile - first selectable match wins (real fuzz-op
     * takes the first match in directory order too, no closest-match/
     * priority logic). */
    if (strcmp(active_target, "xlector") == 0 && is_enter) {
        char xlector_dir[PATH_BUF + 512];
        resolve_piece_dir("xlector", xlector_dir, sizeof(xlector_dir));
        char map_dir[PATH_BUF + 512];
        snprintf(map_dir, sizeof(map_dir), "%s", xlector_dir);
        char *slash = strrchr(map_dir, '/');
        if (slash) *slash = '\0';

        DIR *d = opendir(map_dir);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
                char cand_path[PATH_BUF + 768];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(cand_path, sizeof(cand_path), "%s/%s/state.txt", map_dir, entry->d_name);
#pragma GCC diagnostic pop
                FILE *cf = fopen(cand_path, "r");
                if (!cf) continue;
                fclose(cf);
                int cx = read_kv_int(cand_path, "pos_x", -999);
                int cy = read_kv_int(cand_path, "pos_y", -999);
                if (cx == xlector_x && cy == xlector_y && piece_is_selectable(entry->d_name)) {
                    set_kv_str(xlector_path, "active_target_id", entry->d_name);
                    strncpy(active_target, entry->d_name, sizeof(active_target) - 1);
                    active_target[sizeof(active_target) - 1] = '\0';
                    break;
                }
            }
            closedir(d);
        }
        set_kv_int(xlector_path, "last_key", key);
        return 0;
    }

    /* Movement - always acts on active_target_id, generic, whether
     * that's xlector itself (a free cursor with no piece.pdl-declared
     * collision exemption - see move_entity.c's own header comment for
     * why it's fully generic too, matching real fuzz-op's own
     * behavior: xlector moves through the SAME collision-checked
     * move_entity op as any other piece, not a special free-roam
     * mode) or a selected pet. */
    if (is_move) {
        const char *dir = "up";
        if (key == 'w' || key == ARROW_UP) dir = "up";
        else if (key == 's' || key == ARROW_DOWN) dir = "down";
        else if (key == 'a' || key == ARROW_LEFT) dir = "left";
        else if (key == 'd' || key == ARROW_RIGHT) dir = "right";

        char cmd[PATH_BUF + 128];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/move_entity.+x' %s %s", project_root, active_target, dir);
        int rc = system(cmd);
        (void)rc; /* move_entity's own exit status isn't consulted here, matching every other op-calling-op site in this project family */

        /* SHADOW XLECTOR SYNC - real fuzz-op_manager.c's own name for
         * this, line ~813: whoever is being controlled just moved, so
         * copy their new position onto xlector's OWN state too, so
         * relinquishing control later (9/Escape) leaves the cursor
         * sitting exactly where you left off. */
        if (strcmp(active_target, "xlector") != 0) {
            char active_path[PATH_BUF];
            piece_state_path(active_target, active_path, sizeof(active_path));
            int nx = read_kv_int(active_path, "pos_x", xlector_x);
            int ny = read_kv_int(active_path, "pos_y", xlector_y);
            set_kv_int(xlector_path, "pos_x", nx);
            set_kv_int(xlector_path, "pos_y", ny);
        }
        set_kv_int(xlector_path, "last_key", key);
        return 0;
    }

    /* Method hotkeys (2-9) - generic dispatch against whichever
     * piece.pdl active_target_id currently names. This is the actual
     * mechanic behind "xlector shows a different menu than a selected
     * entity": there is no menu-switching code at all, just one lookup
     * parameterized by active_target_id, exactly like real
     * fuzz-op_manager.c's own method_key handler (that file, line
     * ~708-788) - reproduced here in the same shape mutaclsym's own
     * ops/choice.c already established for this project family (digit
     * accumulator persisted in a state.txt field since every keypress
     * is a fresh short-lived process), just made generic over WHICH
     * piece's state.txt/piece.pdl are in play. */
    int is_digit = (key >= '0' && key <= '9');
    char active_path[PATH_BUF];
    piece_state_path(active_target, active_path, sizeof(active_path));

    if (is_digit || is_enter) {
        int digit_accum = read_kv_int(active_path, "digit_accum", 0);
        int action_cursor = read_kv_int(active_path, "action_cursor", -1);

        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' %s list_methods", project_root, active_target);
        FILE *pf = popen(cmd, "r");
        char names[32][MAX_LINE];
        int total = 0;
        if (pf) {
            while (total < 32 && fgets(names[total], MAX_LINE, pf)) {
                names[total][strcspn(names[total], "\r\n")] = '\0';
                total++;
            }
            pclose(pf);
        }

        if (is_digit) {
            int d = key - '0';
            int new_val = digit_accum * 10 + d;
            if (new_val >= 2 && new_val < total) { digit_accum = new_val; action_cursor = new_val; }
            else if (d >= 2 && d < total) { digit_accum = d; action_cursor = d; }
            else { digit_accum = 0; action_cursor = -1; }
        } else { /* is_enter */
            if (action_cursor >= 2 && action_cursor < total) {
                char get_cmd[PATH_BUF];
                snprintf(get_cmd, sizeof(get_cmd), "'%s/ops/+x/pdl_reader.+x' %s get_method %s",
                         project_root, active_target, names[action_cursor]);
                FILE *gf = popen(get_cmd, "r");
                if (gf) {
                    char handler[MAX_LINE] = "";
                    if (fgets(handler, sizeof(handler), gf)) handler[strcspn(handler, "\r\n")] = '\0';
                    pclose(gf);
                    if (handler[0]) {
                        char exec_cmd[PATH_BUF];
                        snprintf(exec_cmd, sizeof(exec_cmd), "'%s/%s' %s", project_root, handler, active_target);
                        int rc = system(exec_cmd);
                        (void)rc; /* the handler op's own exit status isn't consulted here, matching every other op-calling-op site in this project family */
                    }
                }
            }
            digit_accum = 0;
            action_cursor = -1;
        }

        set_kv_int(active_path, "digit_accum", digit_accum);
        set_kv_int(active_path, "action_cursor", action_cursor);
        set_kv_int(active_path, "last_key", key);
        return 0;
    }

    /* Any other key: abandon a pending digit sequence, same "don't
     * leave stale accumulator state for several turns" reasoning
     * mutaclsym's own ops/choice.c already documents. */
    set_kv_int(active_path, "digit_accum", 0);
    set_kv_int(active_path, "action_cursor", -1);
    set_kv_int(xlector_path, "last_key", key);
    return 0;
}
