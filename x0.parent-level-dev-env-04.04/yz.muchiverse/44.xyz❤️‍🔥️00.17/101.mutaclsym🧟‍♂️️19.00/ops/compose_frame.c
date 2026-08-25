/* compose_frame - one verb, one binary, no shared headers.
 * Renders current game state (map + furniture + ground items + hero +
 * inventory + turn) into a plain text frame at
 * pieces/display/current_frame.txt. Does NOT touch a terminal itself -
 * rendering is the renderer process's job, reading the file this op
 * writes. Self-contained: own root resolution, own constants, own
 * copies of the map/state reading logic (same pattern already used by
 * move_player.c / end_turn.c / pickup.c). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)
/* VIEWPORT_W/VIEWPORT_H are what actually gets printed - the
 * terminal-visible window, unchanged at the old fixed 40x16. MAX_MAP_W/
 * MAX_MAP_H are generous compile-time buffer-size caps for the FULL
 * map (which can now be bigger than the viewport), not the real
 * per-map dimensions - those come from each map's own state.txt at
 * runtime. See dox/01-cdda-architecture.md §5a - this file is the
 * actual camera implementation for that blocking prerequisite. */
#define VIEWPORT_W 40
#define VIEWPORT_H 16
#define MAX_MAP_W 256
#define MAX_MAP_H 256
#define MAX_RECIPES_DISPLAY 16
#define LOG_TAIL 4
#define BOX_TEXT_W 48 /* wider than VIEWPORT_W on purpose - the draw loop clips to the grid anyway */
/* Room for the longest emoji this project's registries actually use -
 * a base codepoint plus a VS16 variation selector (e.g. "⬇️"/"🗡️") is
 * at most 4+3=7 UTF-8 bytes, +1 for the terminating NUL - 12 leaves
 * real margin without being wasteful at MAX_MAP_H*MAX_MAP_W cells. */
#define EMOJI_BUF 12

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

/* Looks up one field of an item registry row by item_id. field_index:
 * 0=name, 2=glyph (fields are id|name|category|glyph|weight|power). */
static void item_registry_field(const char *item_id, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/items/items.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(item_id) || strncmp(line, item_id, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Counts how many inventory pieces have the given item_id - same logic
 * as ops/craft.c's own count_in_inventory(), duplicated here (not
 * shared) so the crafting panel can show accurate "ready"/"missing"
 * status without actually consuming anything - this op only reads. */
static int count_in_inventory(const char *item_id) {
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/hero_01/inventory", project_root);
    DIR *d = opendir(inventory_dir);
    if (!d) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char id[64];
        read_kv_str(state_path, "item_id", id, sizeof(id), "?");
        if (strcmp(id, item_id) == 0) count++;
    }
    closedir(d);
    return count;
}

/* Reads the last LOG_TAIL lines of pieces/display/message_log.txt (a
 * plain append-only event log every message-producing op writes to now
 * instead of overwriting a single hero/state.txt msg field - that
 * single-field version silently clobbered same-turn messages from
 * other ops, e.g. a zombie's attack message erasing a pickup message
 * from earlier the same turn). Reads the whole file each render (fine
 * for this project's file sizes, matches the simple-over-clever
 * approach already used throughout) into a small circular buffer so
 * only the tail is kept regardless of how long the log has grown -
 * same "keep a full audit trail, only ever tail-display it" shape as
 * frame_history.txt already established. */
static int read_message_log_tail(char lines_out[][256]) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[LOG_TAIL][MAX_LINE];
    int total = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(buf[total % LOG_TAIL], sizeof(buf[0]), "%s", line);
        total++;
    }
    fclose(f);
    int n = total < LOG_TAIL ? total : LOG_TAIL;
    for (int i = 0; i < n; i++) {
        int idx = (total - n + i) % LOG_TAIL;
        snprintf(lines_out[i], 256, "%s", buf[idx]);
    }
    return n;
}

/* field_index: 2=glyph (fields are id|name|glyph|hp|damage). */
static void monster_registry_field(const char *monster_type, int field_index, char *out, size_t out_sz, const char *def) {
    snprintf(out, out_sz, "%s", def);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *field = p1 + 1;
        for (int i = 1; i < field_index && field; i++) {
            field = strchr(field, '|');
            if (field) field++;
        }
        if (!field) break;
        char *end = strchr(field, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", field);
        break;
    }
    fclose(f);
}

/* Same registry-scan shape as ops/compose_rgb_frame.c's own
 * glyph_rgb_top(), but extracting the row's unicode field (glyph|id|
 * name|walkable|rgb_top|unicode|rgb_top_emoji) instead of rgb_top -
 * the ASCII<->emoji toggle's terminal-side lookup for terrain/
 * furniture glyphs. See GRAND-ARCHITECTURE.md §0a for the full
 * research (real op-ed/fuzz-op toggle mechanism, ported the doc's
 * recommended registry-field way instead of repeating their two
 * disagreeing hardcoded tables) and this op's emoji_mode plumbing
 * below for how it's wired in. */
static int glyph_unicode_lookup(const char *registry_rel_path, char glyph, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Same '#' vs '#|...' (t_wall) comment-check as
         * compose_rgb_frame.c's own glyph_rgb_top(). */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        line[strcspn(line, "\n")] = '\0';
        char *p = strchr(line, '|'); if (!p) continue; /* -> id */
        p = strchr(p + 1, '|'); if (!p) continue;      /* -> name */
        p = strchr(p + 1, '|'); if (!p) continue;      /* -> walkable */
        p = strchr(p + 1, '|'); if (!p) continue;      /* -> rgb_top */
        p = strchr(p + 1, '|'); if (!p) continue;      /* -> unicode */
        char *end = strchr(p + 1, '|'); /* -> rgb_top_emoji, if present */
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", p + 1);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

/* Terrain and furniture glyph sets are disjoint by construction (same
 * assumption the grid-build loop below already relies on when merging
 * the two layers), so trying terrain then furniture by character alone
 * is unambiguous between just those two - unlike items, which DO share
 * glyphs with terrain in three real cases ('=', '~', '%' - scrap_metal/
 * rag/jerky vs pavement/shallow-water/rubble), which is exactly why
 * item/monster glyphs are resolved by their own item_id/monster_type at
 * the point they're placed onto the grid below, never by re-deriving
 * identity from a shared character afterward. */
static void terrain_or_furniture_unicode(char glyph, char *out, size_t out_sz) {
    if (glyph_unicode_lookup("pieces/registry/terrain/terrain_types.txt", glyph, out, out_sz)) return;
    if (glyph_unicode_lookup("pieces/registry/furniture/furniture_types.txt", glyph, out, out_sz)) return;
    snprintf(out, out_sz, "%c", glyph);
}

/* Builds the bracket-cursor choice legend straight from hero's own
 * piece.pdl (via ops/pdl_reader.+x, the same op-to-op popen pattern
 * ops/choice.c already uses to dispatch it) instead of a hand-typed
 * static string - so the digits/highlight shown here and what
 * ops/choice.c actually dispatches can never drift apart. Real 1.TPMOS
 * had exactly this class of bug: get_piece_methods_op.c (renders the
 * number) and pdl_reader.c/route_input() (dispatches the number) used
 * two independently-written skip-lists that could disagree - one shared
 * source of truth here avoids that entirely.
 *
 * Format matches real 1.TPMOS's chtmp_parser.c EXACTLY: "[marker] N.
 * [Label]" where marker is "[>]" for the entry action_cursor currently
 * points at (a pending, not-yet-committed digit sequence - see
 * ops/choice.c) and "[ ]" otherwise - confirmed live in a captured
 * frame ("[>] 1. [+-demo]" / "[ ] 2. [404-test]" ...). Rows 0/1
 * (move/end_turn) are skipped, numbering starts at 2, matching
 * choice.c's own convention. Quit is Ctrl+C only - no key-based quit
 * in the PAL script or any op.
 *
 * interact_mode (the real xlector active-target pattern, adapted - see
 * ops/choice.c's own header comment for the full writeup) replaces the
 * WHOLE footer with a cursor-mode-specific hint while active, rather
 * than showing the numbered action list - those digits are genuine
 * no-ops in this mode (there's no action-bar menu to jump around while
 * controlling the cursor, see ops/choice.c), so showing them would be
 * misleading. '[i] Interact' is shown as a plain, always-visible legend
 * entry (not a numbered/dispatched item) when interact_mode=0. */
/* REAL FIX (direct user request, "get rid of lower duplicates"): the
 * numbered Pickup/Drop/Eat/... list used to be enumerated here too,
 * duplicating what chtpm mode's own top-level nav now shows via a
 * real `${piece_methods}` (see pieces/chtpm/layouts/game.chtpm) - not
 * just visual clutter, the two lists together made this line long
 * enough to get truncated by chtpm_rgb_render.c's own 640px-wide GL
 * text rasterization (confirmed live). Regular "run" mode (no chtpm
 * wrapper, no top-level nav to show the list instead) loses the
 * printed enumeration here, but action_cursor/digit-jump dispatch
 * itself is unaffected - only the discoverability hint shrinks. */
static void build_choice_footer(char *out, size_t out_sz, int action_cursor, int interact_mode) {
    (void)action_cursor;
    if (interact_mode) {
        snprintf(out, out_sz, "[wasd/arrows] Look  [enter] Examine  [t] Throw  [esc] Back  [e] Emoji\n");
        return;
    }
    snprintf(out, out_sz, "[wasd/arrows] Move  [i] Interact  [e] Emoji\n");
}

/* Overwrites a sub-rectangle of the VIEWPORT (screen-space, not map-
 * space - the panel is glued to the visible window's top-left corner,
 * not the map's absolute origin, so it stays put regardless of where
 * the camera has scrolled to) with a bordered, titled box - shared by
 * every overlay panel type (craft, inventory) now that a second real
 * consumer exists. Extends any short row with spaces first (same
 * technique the hero glyph placement uses) so the box's own spaces
 * genuinely blank out the map underneath rather than leaving old
 * glyphs peeking through past the overlay text's length. */
/* egrid, when non-NULL (emoji_mode=1 only - see main()'s call sites),
 * is the parallel per-cell emoji-string viewport this op maintains
 * alongside the plain ascii one - see this file's own header comment
 * on cell_emoji/viewport_emoji for the full reasoning. Panel box text
 * is plain UI chrome, never emoji-substituted, so every character this
 * function writes into `grid` is mirrored into `egrid` unchanged (as
 * its own single-character string) rather than left stale with
 * whatever map glyph happened to be under the box before - the same
 * reason the ascii path pads short rows with spaces first. */
static void draw_panel_box(char grid[][VIEWPORT_W + 1], char egrid[][VIEWPORT_W][EMOJI_BUF], int rows, const char *title,
                            char panel_rows[][BOX_TEXT_W + 1], int panel_row_count,
                            const char *cancel_row) {
    int box_row = 1, box_col = 2;
    int box_h = panel_row_count + 3; /* border + rows + Cancel + border */
    if (box_row + box_h > rows) box_h = rows - box_row;
    char top[BOX_TEXT_W + 12];
    snprintf(top, sizeof(top), "+--- %s ------------------+", title);
    char boxed[BOX_TEXT_W + 12];
    for (int r = 0; r < box_h && box_row + r < rows; r++) {
        if (r == 0) snprintf(boxed, sizeof(boxed), "%s", top);
        else if (r <= panel_row_count) snprintf(boxed, sizeof(boxed), "%s", panel_rows[r - 1]);
        else if (r == panel_row_count + 1) snprintf(boxed, sizeof(boxed), "%s", cancel_row);
        else snprintf(boxed, sizeof(boxed), "+-----------------------------+");

        int gr = box_row + r;
        int rowlen = (int)strlen(grid[gr]);
        while (rowlen < VIEWPORT_W) {
            grid[gr][rowlen] = ' ';
            if (egrid) snprintf(egrid[gr][rowlen], EMOJI_BUF, " ");
            rowlen++;
        }
        grid[gr][VIEWPORT_W] = '\0';
        int blen = (int)strlen(boxed);
        for (int i = 0; i < blen && box_col + i < VIEWPORT_W; i++) {
            grid[gr][box_col + i] = boxed[i];
            if (egrid) snprintf(egrid[gr][box_col + i], EMOJI_BUF, "%c", boxed[i]);
        }
    }
}

/* CHTPM DYNAMIC SUBMENU BRIDGE - real 1.TPMOS doctrine (see
 * DYNAMIC_SUBMENUS_FUNCTIONAL_STD.md, cited against
 * projects/groq-ollama/manager/groq-ollama_manager.c): the PARSER owns
 * nav/focus/highlighting entirely; the MANAGER only ever projects raw
 * button markup, always, never gated behind a mode flag, and never
 * hand-draws its own "[>]"/"[ ]" markers. Builds craft_panel_items/
 * inventory_panel_items as real <button onClick="KEY:n"> rows for
 * game.chtpm's own ACTIVATE-wrapped "craft"/"examine" submenus - a
 * SEPARATE representation from draw_panel_box() above, which stays
 * exactly as-is for direct terminal "run" mode (no chtpm layer there
 * to provide real focusable elements at all). Written to
 * pieces/apps/player_app/manager/gui_state.txt - the real file
 * chtpm_parser_pal.c's own load_vars() already loads unconditionally
 * for this project's layout shape (confirmed via direct read: the
 * modern_layout branch with an empty project_id falls through to
 * exactly this path).
 *
 * NAMED SIMPLIFICATION: both submenus reuse the same KEY:'1'..KEY:'9'
 * single-ASCII-digit space. That's only safe because chtpm's real
 * ACTIVATE semantics make the two submenus mutually exclusive (never
 * both navigable at once) - it also caps each list at 9 real entries.
 * mutaclsym's prisc+x has no string-command relay capability yet
 * (read_history's own pal contract is integer-only, confirmed via
 * direct read of main_loop_chtpm.pal) - real string commands (the
 * doctrine's own preferred, uncapped mechanism) are a genuine future
 * upgrade, not silently built here. */
static void write_panel_gui_state(void) {
    char buf_craft[4096] = "";
    char buf_inv[4096] = "";

    char recipes_path[PATH_BUF];
    snprintf(recipes_path, sizeof(recipes_path), "%s/pieces/registry/recipes/recipes.txt", project_root);
    FILE *rf = fopen(recipes_path, "r");
    if (rf) {
        char rline[MAX_LINE];
        int idx = 0;
        while (idx < 9 && fgets(rline, sizeof(rline), rf)) {
            if (rline[0] == '#' || rline[0] == '\n') continue;
            rline[strcspn(rline, "\n")] = '\0';
            char *p1 = strchr(rline, '|');
            if (!p1) continue;
            *p1 = '\0';
            char *rname = p1 + 1;
            char *p2 = strchr(rname, '|');
            if (!p2) continue;
            *p2 = '\0';
            char *presult = p2 + 1;
            char *p3 = strchr(presult, '|');
            if (!p3) continue;
            *p3 = '\0';
            char *reqs_str = p3 + 1;

            int satisfied = 1;
            char reqs_copy[MAX_LINE];
            snprintf(reqs_copy, sizeof(reqs_copy), "%s", reqs_str);
            char *tok = strtok(reqs_copy, ",");
            while (tok) {
                char *colon = strchr(tok, ':');
                if (colon) {
                    *colon = '\0';
                    if (count_in_inventory(tok) < atoi(colon + 1)) satisfied = 0;
                }
                tok = strtok(NULL, ",");
            }

            idx++;
            char row[256];
            snprintf(row, sizeof(row), "<button label=\"%s%s\" onClick=\"KEY:%d\" /><br/>",
                     rname, satisfied ? " (ready)" : " (missing)", '0' + idx);
            if (strlen(buf_craft) + strlen(row) < sizeof(buf_craft) - 1) strcat(buf_craft, row);
        }
        fclose(rf);
    }
    if (buf_craft[0] == '\0') snprintf(buf_craft, sizeof(buf_craft), "<text label=\"(no recipes)\" /><br/>");

    char inventory_dir_panel[PATH_BUF];
    snprintf(inventory_dir_panel, sizeof(inventory_dir_panel), "%s/pieces/hero_01/inventory", project_root);
    DIR *d = opendir(inventory_dir_panel);
    if (d) {
        struct dirent *entry;
        int idx = 0;
        while (idx < 9 && (entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 384];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir_panel, entry->d_name);
            char item_id[64];
            read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

            char iname[64], category[16], power_str[16];
            item_registry_field(item_id, 1, iname, sizeof(iname), item_id);
            item_registry_field(item_id, 2, category, sizeof(category), "?");
            item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
            int power = atoi(power_str);

            char detail[24] = "";
            if (strcmp(category, "weapon") == 0) snprintf(detail, sizeof(detail), " dmg+%d", power);
            else if (strcmp(category, "food") == 0) snprintf(detail, sizeof(detail), " hunger-%d", power);
            else if (strcmp(category, "drink") == 0) snprintf(detail, sizeof(detail), " thirst-%d", power);
            else if (strcmp(category, "armor") == 0) snprintf(detail, sizeof(detail), " def+%d", power);

            idx++;
            char row[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(row, sizeof(row), "<button label=\"%s (%s)%s\" onClick=\"KEY:%d\" /><br/>",
                     iname, category, detail, '0' + idx);
#pragma GCC diagnostic pop
            if (strlen(buf_inv) + strlen(row) < sizeof(buf_inv) - 1) strcat(buf_inv, row);
        }
        closedir(d);
    }
    if (buf_inv[0] == '\0') snprintf(buf_inv, sizeof(buf_inv), "<text label=\"(nothing carried)\" /><br/>");

    char gui_state_dir[PATH_BUF];
    snprintf(gui_state_dir, sizeof(gui_state_dir), "%s/pieces/apps/player_app/manager", project_root);
    mkdir(gui_state_dir, 0755);
    char gui_state_path[PATH_BUF];
    snprintf(gui_state_path, sizeof(gui_state_path), "%s/pieces/apps/player_app/manager/gui_state.txt", project_root);
    /* Atomic write: .tmp then rename — same pattern as view.txt.
     * Prevents reader from seeing partial/empty file (Pitfall 18 class). */
    char gui_state_tmp[PATH_BUF + 4];
    snprintf(gui_state_tmp, sizeof(gui_state_tmp), "%s.tmp", gui_state_path);
    FILE *gf = fopen(gui_state_tmp, "w");
    if (gf) {
        fprintf(gf, "craft_panel_items=%s\n", buf_craft);
        fprintf(gf, "inventory_panel_items=%s\n", buf_inv);
        fclose(gf);
        rename(gui_state_tmp, gui_state_path);
    }
}

static int read_config_int(const char *key, int def) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/config.txt", project_root);
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

/* Ledger replay: reconstruct hero position from config.txt start + ledger events.
 * This is the event-sourcing read path — current state = replay all history. */
static void replay_ledger_hero(int *out_x, int *out_y) {
    *out_x = read_config_int("hero_start_x", 0);
    *out_y = read_config_int("hero_start_y", 0);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        if (line_num == 1) continue; /* skip header */
        char actor[64];
        int nx, ny;
        char action_type[32];
        if (sscanf(line, "%*[^|]|%*d|%63[^|]|%*d|x:%d,y:%d|%31s", actor, &nx, &ny, action_type) >= 3) {
            if (strcmp(actor, "hero") == 0 && strcmp(action_type, "move") == 0) {
                *out_x = nx;
                *out_y = ny;
            }
        }
    }
    fclose(f);
}

/* Generic ledger replay for any actor: start from start_x/y, replay all
 * matching move events, return final position. Used for both hero and
 * monsters. */
static void replay_ledger_actor(const char *actor_name, int start_x, int start_y, int *out_x, int *out_y) {
    *out_x = start_x;
    *out_y = start_y;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        if (line_num == 1) continue;
        char actor[64];
        int nx, ny;
        char action_type[32];
        if (sscanf(line, "%*[^|]|%*d|%63[^|]|%*d|x:%d,y:%d|%31s", actor, &nx, &ny, action_type) >= 3) {
            if (strcmp(actor, actor_name) == 0 && strcmp(action_type, "move") == 0) {
                *out_x = nx;
                *out_y = ny;
            }
        }
    }
    fclose(f);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF], out_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/hero_01/state.txt", project_root);
    /* ONE WRITER RULE (PITFALL #17/#36): compose_frame writes view.txt,
     * NOT current_frame.txt. Parser reads view.txt and composes the final
     * current_frame.txt via ${game_map} substitution. Writing directly to
     * current_frame.txt creates a dual-writer race with chtpm_parser_pal,
     * causing visible flicker. Matches lpns+map+4's game_compose_frame.c
     * architecture exactly. */
    snprintf(out_path, sizeof(out_path), "%s/pieces/apps/player_app/view.txt", project_root);

    int px, py;
    replay_ledger_hero(&px, &py);
    int hp = read_kv_int(hero_path, "hp", 100);
    int hunger = read_kv_int(hero_path, "hunger", 0);
    int thirst = read_kv_int(hero_path, "thirst", 0);
    int stamina = read_kv_int(hero_path, "stamina", 100);
    int action_cursor = read_kv_int(hero_path, "action_cursor", -1);
    int last_key = read_kv_int(hero_path, "last_key", 0);
    int interact_mode = read_kv_int(hero_path, "interact_mode", 0);
    char possessed_id[64] = "none";
    read_kv_str(hero_path, "possessed_id", possessed_id, sizeof(possessed_id), "none");
    int emoji_mode = read_kv_int(hero_path, "emoji_mode", 1);
    /* Real, direct instruction (2026-07-17): chtpm mode's own GL
     * rendering (shared-ops/chtpm_rgb_render.c) font-rasterizes THIS
     * file's own text output verbatim - it has zero game-state
     * awareness by design (a genuinely shared, project-agnostic
     * daemon). To let it composite mutaclsym's own real 3D view
     * (ops/compose_rgb_frame.c's render_3d_view()) into the right
     * screen rectangle without hardcoding pixel offsets, this file
     * emits a sentinel marker line (a single 0x01/SOH control byte -
     * not a printable string) immediately before the map's own
     * viewport rows, ONLY when render_mode==1 - see
     * shared-ops/chtpm_rgb_render.c's own MAP3D_MARKER handling for
     * the read side. A real terminal (system/renderer.c, which also
     * reads this exact file) doesn't visibly render a bare control
     * byte, so the ASCII view is unaffected in practice even though
     * it's the same underlying file - render_mode otherwise never
     * changes what this file actually DRAWS, matching real wraith's
     * own convention that '0' only changes what the GL view shows. */
    int render_mode = read_kv_int(hero_path, "render_mode", 0);
    int xlector_x = read_kv_int(hero_path, "xlector_pos_x", px);
    int xlector_y = read_kv_int(hero_path, "xlector_pos_y", py);
    char map_id[64];
    read_kv_str(hero_path, "map_id", map_id, sizeof(map_id), "map_start");

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], turn_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(turn_path, sizeof(turn_path), "%s/state.txt", map_dir);

    int turn = read_kv_int(turn_path, "turn", 0);
    int map_w = read_kv_int(turn_path, "width", VIEWPORT_W);
    int map_h = read_kv_int(turn_path, "height", VIEWPORT_H);
    if (map_w > MAX_MAP_W) map_w = MAX_MAP_W;
    if (map_h > MAX_MAP_H) map_h = MAX_MAP_H;
    /* Multi-Z buildings (see dox/03-multi-z-design.txt): each FLOOR is
     * its own ordinary map directory, linked to the one above/below via
     * the exact same transitions.txt '<'/'>' mechanism every other
     * inter-map link already uses - zero new movement/rendering
     * machinery needed. z is purely an informational HUD field (which
     * floor is this), defaulting to 0 (absent) for every ground-level
     * outdoor map so existing maps' header stays unchanged. */
    int z = read_kv_int(turn_path, "z", 0);

    /* Build the WHOLE map in memory (absolute coordinates, can be
     * bigger than the viewport) - terrain -> furniture -> ground items
     * -> hero, each layer drawn on top of the last. The camera/
     * viewport slicing below is purely a rendering-time concern
     * layered on top of this - move_player.c/tick_monsters.c/
     * transitions.txt/every piece's pos_x/pos_y all already operate in
     * these same absolute map coordinates, unchanged. */
    char grid[MAX_MAP_H][MAX_MAP_W + 1];
    /* Parallel per-cell emoji-string map, identity-resolved (not
     * re-derived from grid's flat char afterward - see
     * terrain_or_furniture_unicode()'s own header comment for why that
     * would be genuinely ambiguous for this project's real glyph
     * collisions). static, matching ops/compose_rgb_frame.c's own
     * framebuf convention for large buffers - avoids a ~780KB stack
     * array. Only ever populated/read when emoji_mode=1; left all-zero
     * (empty strings, guaranteed by static's zero-init) otherwise, and
     * never consulted in that case - ascii-mode output is untouched,
     * byte-for-byte the same code path as before this feature existed. */
    static char cell_emoji[MAX_MAP_H][MAX_MAP_W][EMOJI_BUF];
    int rows = 0;
    FILE *mf = fopen(map_path, "r");
    FILE *ff = fopen(furniture_path, "r"); /* optional - NULL is fine, just means no furniture layer */
    if (mf) {
        char terrain_line[MAX_MAP_W + 4], furniture_line[MAX_MAP_W + 4];
        while (rows < map_h && fgets(terrain_line, sizeof(terrain_line), mf)) {
            terrain_line[strcspn(terrain_line, "\n")] = '\0';
            furniture_line[0] = '\0';
            if (ff) {
                if (!fgets(furniture_line, sizeof(furniture_line), ff)) furniture_line[0] = '\0';
                furniture_line[strcspn(furniture_line, "\n")] = '\0';
            }
            int len = (int)strlen(terrain_line);
            if (len > map_w) len = map_w;
            for (int col = 0; col < len; col++) {
                char fg = (col < (int)strlen(furniture_line)) ? furniture_line[col] : ' ';
                grid[rows][col] = (fg != ' ') ? fg : terrain_line[col];
                if (emoji_mode) terrain_or_furniture_unicode(grid[rows][col], cell_emoji[rows][col], EMOJI_BUF);
            }
            grid[rows][len] = '\0';
            rows++;
        }
        fclose(mf);
    }
    if (ff) fclose(ff);

    /* Ground items physically live under THIS map's own items/ dir now
     * (russian-doll nesting, not a location field) - drawn on top of
     * terrain/furniture. */
    char items_dir[PATH_BUF + 32];
    snprintf(items_dir, sizeof(items_dir), "%s/items", map_dir);
    DIR *d = opendir(items_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", items_dir, entry->d_name);
            int ix = read_kv_int(state_path, "pos_x", -1);
            int iy = read_kv_int(state_path, "pos_y", -1);
            if (ix < 0 || iy < 0 || ix >= map_w || iy >= rows) continue;
            char item_id[64];
            read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
            char glyph[4];
            item_registry_field(item_id, 3, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[iy][ix] = glyph[0];
            /* Resolved by item_id directly, not by re-deriving from
             * glyph[0] afterward - see terrain_or_furniture_unicode()'s
             * header comment on the real '=', '~', '%' glyph collisions
             * this project's own registries have between items and
             * terrain. field_index 6 = unicode (items.txt: id|name|
             * category|glyph|weight|power|unicode). */
            if (emoji_mode && glyph[0]) {
                char uni[EMOJI_BUF];
                char def[2] = { glyph[0], '\0' };
                item_registry_field(item_id, 6, uni, sizeof(uni), def);
                snprintf(cell_emoji[iy][ix], EMOJI_BUF, "%s", uni);
            }
        }
        closedir(d);
    }

    /* Monsters, drawn on top of items, below the hero. */
    char monsters_dir[PATH_BUF + 32];
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    d = opendir(monsters_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", monsters_dir, entry->d_name);
            int start_mx = read_kv_int(state_path, "pos_x", -1);
            int start_my = read_kv_int(state_path, "pos_y", -1);
            int mx, my;
            replay_ledger_actor(entry->d_name, start_mx, start_my, &mx, &my);
            if (mx < 0 || my < 0 || mx >= map_w || my >= rows) continue;
            char monster_type[64];
            read_kv_str(state_path, "monster_type", monster_type, sizeof(monster_type), "?");
            char glyph[4];
            monster_registry_field(monster_type, 2, glyph, sizeof(glyph), "?");
            if (glyph[0]) grid[my][mx] = glyph[0];
            /* Resolved by monster_type directly - same identity-not-
             * character reasoning as the item loop above. field_index
             * 5 = unicode (monster_types.txt: id|name|glyph|hp|damage|
             * unicode). */
            if (emoji_mode && glyph[0]) {
                char uni[EMOJI_BUF];
                char def[2] = { glyph[0], '\0' };
                monster_registry_field(monster_type, 5, uni, sizeof(uni), def);
                snprintf(cell_emoji[my][mx], EMOJI_BUF, "%s", uni);
            }
        }
        closedir(d);
    }

    /* Imported pets (see 0.a-z-pets-plan/a-z-fix-report.txt), drawn on
     * top of monsters, below the hero. Unlike monsters/items - native
     * mutaclsym content keyed by a registry - an imported pet already
     * carries its own real species_emoji as live data (ops/pet_import.c
     * wrote it straight from the origin game's trade envelope), so no
     * registry lookup here: read it directly off the pet's own
     * state.txt. Pets land as flat siblings of hero/items/monsters
     * under map_dir (see pet_import.c's own dst_dir) rather than their
     * own subdirectory, so distinguish them by exclusion - anything
     * under map_dir that ISN'T one of the fixed reserved names and has
     * its own state.txt with a real pos_x/pos_y is a pet. */
    d = opendir(map_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "hero") == 0 ||
                strcmp(entry->d_name, "items") == 0 ||
                strcmp(entry->d_name, "monsters") == 0) continue;
            char pet_state_path[PATH_BUF + 320];
            snprintf(pet_state_path, sizeof(pet_state_path), "%s/%s/state.txt", map_dir, entry->d_name);
            int pxp = read_kv_int(pet_state_path, "pos_x", -1);
            int pyp = read_kv_int(pet_state_path, "pos_y", -1);
            if (pxp < 0 || pyp < 0 || pxp >= map_w || pyp >= rows) continue;
            char pet_emoji[EMOJI_BUF];
            read_kv_str(pet_state_path, "species_emoji", pet_emoji, sizeof(pet_emoji), "🐾");
            grid[pyp][pxp] = 'p';
            if (emoji_mode) snprintf(cell_emoji[pyp][pxp], EMOJI_BUF, "%s", pet_emoji);
        }
        closedir(d);
    }

    /* Hero, drawn last, on top of everything. */
    if (py >= 0 && py < rows) {
        int rowlen = (int)strlen(grid[py]);
        while (rowlen <= px && rowlen < map_w) {
            grid[py][rowlen] = ' ';
            if (emoji_mode) snprintf(cell_emoji[py][rowlen], EMOJI_BUF, " ");
            rowlen++;
            grid[py][rowlen] = '\0';
        }
        if (px >= 0 && px < map_w) {
            grid[py][px] = '@';
            /* Hero/xlector are hardcoded chars, not registry-driven -
             * same reason they get fixed fallback colors in
             * ops/compose_rgb_frame.c's own glyph_fallback_rgb()
             * rather than a registry lookup. 🎯 for the cursor
             * deliberately reuses real fuzz-op's own 'X'->🎯 mapping
             * exactly (render_map.c:47-63, see GRAND-ARCHITECTURE.md
             * §0a) - a direct, correct port where one already exists. */
            if (emoji_mode) snprintf(cell_emoji[py][px], EMOJI_BUF, "🧑");
        }
    }

    /* Xlector cursor - real 1.TPMOS's active-target pattern, adapted
     * (see ops/choice.c's own header comment and dox/04-chtpm-parser-
     * research-and-interact-mode.txt for the full writeup). Drawn last
     * of all, on top of even the hero, since it starts exactly on the
     * hero's own tile every time interact_mode is entered - it must
     * stay visibly distinguishable. 'X' confirmed unused by every
     * terrain/furniture/item/monster registry in this project. */
    if (interact_mode == 1 && strcmp(possessed_id, "none") == 0 && xlector_y >= 0 && xlector_y < rows) {
        int rowlen = (int)strlen(grid[xlector_y]);
        while (rowlen <= xlector_x && rowlen < map_w) {
            grid[xlector_y][rowlen] = ' ';
            if (emoji_mode) snprintf(cell_emoji[xlector_y][rowlen], EMOJI_BUF, " ");
            rowlen++;
            grid[xlector_y][rowlen] = '\0';
        }
        if (xlector_x >= 0 && xlector_x < map_w) {
            grid[xlector_y][xlector_x] = 'X';
            if (emoji_mode) snprintf(cell_emoji[xlector_y][xlector_x], EMOJI_BUF, "🎯");
        }
    }

    /* Camera: keep the LIVE controlled entity roughly centered, clamped
     * so the viewport never scrolls past the map's edges - dox/01-cdda-
     * architecture.md §5a's exact formula. REAL BUG FIX (2026-07-21,
     * same class as the sibling $.4x family's compose_frame.c/
     * compose_rgb_frame.c camera bug - see camera-fix.txt): this used to
     * always center on the hero's own px/py, even while interact_mode
     * had the xlector cursor as the thing actually being controlled -
     * move_player.c's own xlector movement (interact_mode branch) is
     * genuinely FREE-ROAMING, clamped only to the map's real bounds, not
     * to the viewport or to any distance from the hero, so on any map
     * bigger than the 40x16 viewport (e.g. pieces/world_01/map_gen_01,
     * 80x40) panning the cursor far enough would move it clean off
     * screen while the camera stayed anchored on the hero's now-stale-
     * for-this-purpose position. Anchor on whichever position is
     * actually live-controlled this frame - xlector_x/xlector_y (already
     * read fresh above from hero_path's own xlector_pos_x/xlector_pos_y
     * fields, not a cached/dead copy) during interact_mode, hero's own
     * px/py otherwise - matching the fix's own general principle: read
     * the live position fresh every frame from whichever entity is
     * actually controlled, adapted to mutaclsym's real field names
     * instead of the $.4x family's invented ones. If the map is smaller
     * than the viewport in either dimension, the upper clamp bound would
     * go negative, so floor it at 0 - camera sits at the map's origin
     * and the viewport just shows blank padding past the map's real
     * edge, same as any other out-of-map read already defaults to on
     * this project. */
    int anchor_x = (interact_mode == 1) ? xlector_x : px;
    int anchor_y = (interact_mode == 1) ? xlector_y : py;
    int cam_x = anchor_x - VIEWPORT_W / 2;
    int cam_x_max = map_w - VIEWPORT_W;
    if (cam_x_max < 0) cam_x_max = 0;
    if (cam_x < 0) cam_x = 0;
    if (cam_x > cam_x_max) cam_x = cam_x_max;
    int cam_y = anchor_y - VIEWPORT_H / 2;
    int cam_y_max = map_h - VIEWPORT_H;
    if (cam_y_max < 0) cam_y_max = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_y > cam_y_max) cam_y = cam_y_max;

    /* Slice the viewport-sized window out of the full map grid - purely
     * a rendering-time crop, nothing upstream of this point (collision,
     * chase AI, item/monster positions) knows or cares about the
     * camera. Everything from here on (panel overlay, final output)
     * operates on this screen-space buffer instead of the map-space one
     * above. */
    char viewport[VIEWPORT_H][VIEWPORT_W + 1];
    /* Parallel emoji-string viewport, sliced from cell_emoji the exact
     * same way viewport itself is sliced from grid - see cell_emoji's
     * own declaration comment above. Only populated when emoji_mode=1. */
    char viewport_emoji[VIEWPORT_H][VIEWPORT_W][EMOJI_BUF];
    int viewport_rows = rows - cam_y;
    if (viewport_rows > VIEWPORT_H) viewport_rows = VIEWPORT_H;
    if (viewport_rows < 0) viewport_rows = 0;
    for (int r = 0; r < viewport_rows; r++) {
        int src_row = cam_y + r;
        int src_len = (int)strlen(grid[src_row]);
        int col;
        for (col = 0; col < VIEWPORT_W; col++) {
            int src_col = cam_x + col;
            viewport[r][col] = (src_col < src_len) ? grid[src_row][src_col] : ' ';
            if (emoji_mode) {
                if (src_col < src_len) snprintf(viewport_emoji[r][col], EMOJI_BUF, "%s", cell_emoji[src_row][src_col]);
                else snprintf(viewport_emoji[r][col], EMOJI_BUF, " ");
            }
        }
        viewport[r][VIEWPORT_W] = '\0';
    }

    /* Overlay panel (e.g. the crafting recipe picker), drawn OVER the
     * already-rendered map - real CDDA/1.TPMOS convention (researched
     * via gl_desktop.c's project_mirror window type: render the map,
     * then draw the menu list directly over it in the same pass, no
     * screen swap, no clear, no z-order/alpha math needed for a single
     * always-on-top panel - later writes simply win). Numbering here is
     * independent of the outer action-bar's piece.pdl-index-based
     * scheme (that one starts at 2 and skips move/end_turn); this is a
     * plain 1-based list matching ops/choice.c's own panel_cursor
     * convention exactly, with a trailing Cancel row at recipe_count+1
     * (same "Back always gets the next free slot" convention already
     * used in egg-pals). REAL, LIVE BUG FOUND DURING TESTING: this
     * overlay is "run"-mode-only now (chtpm mode has its own genuine
     * ACTIVATE-based submenu - see game.chtpm's own "craft"/"examine"
     * buttons and write_panel_gui_state() above) - active_panel is
     * never SET anymore while playing through chtpm (nothing relays a
     * KEY:n for entering these submenus there), but a STALE leftover
     * value from an earlier "run"-mode session (or old testing) can
     * still be sitting in hero/state.txt, and this block used to fire
     * unconditionally on it - confirmed live: a leftover
     * "active_panel=inventory" made a ghost inventory box draw ON TOP
     * OF the map, over the real, chtpm-native submenu, in a chtpm-mode
     * session that never opened any panel at all. Gated on module_path
     * (player_app/state.txt - "run" mode boots main_loop.pal, chtpm
     * mode boots main_loop_chtpm.pal) so this overlay can only ever
     * fire in the mode that actually still needs it. */
    char player_app_state_path[PATH_BUF];
    snprintf(player_app_state_path, sizeof(player_app_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    char module_path_check[PATH_BUF];
    read_kv_str(player_app_state_path, "module_path", module_path_check, sizeof(module_path_check), "");
    int is_chtpm_mode = (strstr(module_path_check, "main_loop_chtpm") != NULL);

    char active_panel[32];
    read_kv_str(hero_path, "active_panel", active_panel, sizeof(active_panel), "none");
    if (!is_chtpm_mode && strcmp(active_panel, "craft") == 0) {
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char recipes_path[PATH_BUF];
        snprintf(recipes_path, sizeof(recipes_path), "%s/pieces/registry/recipes/recipes.txt", project_root);
        FILE *rf = fopen(recipes_path, "r");
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        if (rf) {
            char rline[MAX_LINE];
            while (panel_row_count < MAX_RECIPES_DISPLAY && fgets(rline, sizeof(rline), rf)) {
                if (rline[0] == '#' || rline[0] == '\n') continue;
                rline[strcspn(rline, "\n")] = '\0';
                char *p1 = strchr(rline, '|');
                if (!p1) continue;
                *p1 = '\0';
                char *rname = p1 + 1;
                char *p2 = strchr(rname, '|');
                if (!p2) continue;
                *p2 = '\0';
                char *presult = p2 + 1;
                char *p3 = strchr(presult, '|');
                if (!p3) continue;
                *p3 = '\0';
                char *reqs_str = p3 + 1;

                int satisfied = 1;
                char reqs_copy[MAX_LINE];
                snprintf(reqs_copy, sizeof(reqs_copy), "%s", reqs_str);
                char *tok = strtok(reqs_copy, ",");
                while (tok) {
                    char *colon = strchr(tok, ':');
                    if (colon) {
                        *colon = '\0';
                        if (count_in_inventory(tok) < atoi(colon + 1)) satisfied = 0;
                    }
                    tok = strtok(NULL, ",");
                }

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s]%s",
                         cur, idx, rname, satisfied ? " (ready)" : " (missing)");
                panel_row_count++;
            }
            fclose(rf);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Cancel]", cancel_cur, cancel_idx);

        draw_panel_box(viewport, emoji_mode ? viewport_emoji : NULL, viewport_rows, "CRAFT", panel_rows, panel_row_count, cancel_row);
    } else if (!is_chtpm_mode && strcmp(active_panel, "inventory") == 0) {
        /* Read-only browse of hero/inventory/ - same overlay mechanism
         * as the craft panel, but Enter never execs anything (see
         * ops/choice.c's panel-mode handling), so every row already
         * shows full detail rather than hiding it behind a selection. */
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        char inventory_dir_panel[PATH_BUF];
        snprintf(inventory_dir_panel, sizeof(inventory_dir_panel), "%s/pieces/hero_01/inventory", project_root);
        char panel_rows[MAX_RECIPES_DISPLAY][BOX_TEXT_W + 1];
        int panel_row_count = 0;
        d = opendir(inventory_dir_panel);
        if (d) {
            struct dirent *entry;
            while (panel_row_count < MAX_RECIPES_DISPLAY && (entry = readdir(d)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char state_path[PATH_BUF + 384];
                snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir_panel, entry->d_name);
                char item_id[64];
                read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");

                char iname[64], category[16], power_str[16];
                item_registry_field(item_id, 1, iname, sizeof(iname), item_id);
                item_registry_field(item_id, 2, category, sizeof(category), "?");
                item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
                int power = atoi(power_str);

                char detail[24] = "";
                if (strcmp(category, "weapon") == 0) snprintf(detail, sizeof(detail), " dmg+%d", power);
                else if (strcmp(category, "food") == 0) snprintf(detail, sizeof(detail), " hunger-%d", power);
                else if (strcmp(category, "drink") == 0) snprintf(detail, sizeof(detail), " thirst-%d", power);
                else if (strcmp(category, "armor") == 0) snprintf(detail, sizeof(detail), " def+%d", power);

                int idx = panel_row_count + 1;
                const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
                /* iname is genuinely short (registry item names) despite
                 * gcc only being able to prove it no longer than its own
                 * 64-byte declaration - same class of warning already
                 * suppressed narrowly elsewhere in this project rather
                 * than widening panel_rows out of sync with
                 * draw_panel_box()'s fixed BOX_TEXT_W+1 signature. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(panel_rows[panel_row_count], sizeof(panel_rows[0]), "%s %d. [%s] (%s)%s",
                         cur, idx, iname, category, detail);
#pragma GCC diagnostic pop
                panel_row_count++;
            }
            closedir(d);
        }
        int cancel_idx = panel_row_count + 1;
        const char *cancel_cur = (cancel_idx == panel_cursor) ? "[>]" : "[ ]";
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s %d. [Close]", cancel_cur, cancel_idx);
        if (panel_row_count == 0) {
            snprintf(panel_rows[0], sizeof(panel_rows[0]), "[ ] (nothing carried)");
            panel_row_count = 1;
        }

        draw_panel_box(viewport, emoji_mode ? viewport_emoji : NULL, viewport_rows, "INVENTORY", panel_rows, panel_row_count, cancel_row);
    } else if (strcmp(active_panel, "xlector_ctx") == 0) {
        /* Xlector context menu — defaults (extensible later). Shown in
         * both run-mode overlay and (when state set) alongside map. */
        int panel_cursor = read_kv_int(hero_path, "panel_cursor", 1);
        static const char *labels[] = {
            "Event", "Copy", "Paste", "Delete", "Exit"
        };
        char panel_rows[5][BOX_TEXT_W + 1];
        int panel_row_count = 5;
        for (int i = 0; i < 5; i++) {
            int idx = i + 1;
            const char *cur = (idx == panel_cursor) ? "[>]" : "[ ]";
            snprintf(panel_rows[i], sizeof(panel_rows[0]), "%s %d. %s", cur, idx, labels[i]);
        }
        /* No separate Cancel row — 5 Exit is the cancel. draw_panel_box
         * wants a cancel_row; pass Exit-style empty trailing by reusing row 5. */
        char cancel_row[BOX_TEXT_W + 1];
        snprintf(cancel_row, sizeof(cancel_row), "%s", "");
        draw_panel_box(viewport, emoji_mode ? viewport_emoji : NULL, viewport_rows,
                       "XLECTOR", panel_rows, panel_row_count, cancel_row);
    }

    /* Inventory: whatever is physically nested under hero/inventory/ now,
     * regardless of which map the hero is currently on - drawn as a
     * compact name list. */
    char inventory_dir[PATH_BUF];
    snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/hero_01/inventory", project_root);
    char inventory_line[256] = "Inventory: ";
    int have_items = 0;
    d = opendir(inventory_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char state_path[PATH_BUF + 320];
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
            char item_id[64];
            read_kv_str(state_path, "item_id", item_id, sizeof(item_id), "?");
            char name[64];
            item_registry_field(item_id, 1, name, sizeof(name), item_id);
            if (have_items) strncat(inventory_line, ", ", sizeof(inventory_line) - strlen(inventory_line) - 1);
            strncat(inventory_line, name, sizeof(inventory_line) - strlen(inventory_line) - 1);
            have_items = 1;
        }
        closedir(d);
    }
    if (!have_items) snprintf(inventory_line, sizeof(inventory_line), "Inventory: (empty)");

    char floor_suffix[24];
    if (z != 0) snprintf(floor_suffix, sizeof(floor_suffix), "   Floor: %d", z);
    else floor_suffix[0] = '\0';

    /* REAL BUG FIX (found live via the MAP3D_MARKER verification test,
     * not guessed): this used to fopen(out_path, "w"), write, fclose,
     * then re-open out_path for READING to copy its content into
     * view.txt (the CHTPM VIEW BRIDGE below). That read-back is a real
     * race - chtpm_parser_pal.c (a separate process) also writes
     * current_frame.txt on its own render cycle, and can land a write
     * in the gap between this process's own fclose and its re-open,
     * so the "copy" picks up chtpm_parser_pal's already-chrome-wrapped
     * output instead of what was just written here. Confirmed live:
     * view.txt was found starting with the chrome border instead of
     * this function's own "MUTACLSYM map:" header line, and the
     * MAP3D_MARKER byte written below never survived the round trip.
     * Fix: build the whole frame in memory (open_memstream - every
     * existing fprintf/fputc/fputs call below is unchanged, it's still
     * a FILE*) and write the SAME buffer to both current_frame.txt and
     * view.txt directly, in-process, with no read-back and therefore
     * no race window at all. */
    char *framebuf = NULL;
    size_t framesize = 0;
    FILE *out = open_memstream(&framebuf, &framesize);
    if (!out) return 1;
    fprintf(out, "MUTACLSYM   map: %-10s turn: %d%s\n", map_id, turn, floor_suffix);
    if (render_mode == 1) { fputc(0x01, out); fputc('\n', out); }
    for (int r = 0; r < viewport_rows; r++) {
        if (emoji_mode) {
            for (int col = 0; col < VIEWPORT_W; col++) fputs(viewport_emoji[r][col], out);
            fputc('\n', out);
        } else {
            fprintf(out, "%s\n", viewport[r]);
        }
    }
    fprintf(out, "HP:%d  Hunger:%d%s  Thirst:%d%s  Stamina:%d",
            hp, hunger, hunger >= 100 ? "(STARVING)" : "",
            thirst, thirst >= 100 ? "(DEHYDRATED)" : "", stamina);
    /* Debug aid: last raw keycode choice.c was invoked with, on every
     * tick regardless of what it meant - lets a player (or whoever's
     * looking at a bug report) confirm input really is being read even
     * when nothing else visibly changes, e.g. repeatedly moving into a
     * wall used to look exactly like the game freezing. */
    if (last_key >= 32 && last_key < 127) fprintf(out, "  Last key: %d ('%c')\n", last_key, (char)last_key);
    else fprintf(out, "  Last key: %d\n", last_key);
    fprintf(out, "%s\n", inventory_line);
    char log_lines[LOG_TAIL][256];
    int log_n = read_message_log_tail(log_lines);
    for (int i = 0; i < log_n; i++) fprintf(out, "%s\n", log_lines[i]);
    char footer[600];
    build_choice_footer(footer, sizeof(footer), action_cursor, interact_mode);
    fputs(footer, out);
    fclose(out);

    /* ATOMIC WRITE to view.txt (out_path).
     * ONE WRITER RULE (PITFALL #17/#36): compose_frame writes view.txt,
     * NOT current_frame.txt. Parser reads view.txt and composes the final
     * current_frame.txt via ${game_map} substitution.
     *
     * fopen("w") truncates to 0 bytes BEFORE writing completes. If the
     * parser's compose_frame() reads view.txt during that window it sees
     * a partial/empty file = ASCII flicker. Fix: write to .tmp then
     * rename() — POSIX rename on same filesystem is atomic, reader sees
     * either complete OLD or complete NEW file.
     *
     * out_path is already pieces/apps/player_app/view.txt (set at
     * line 554). The old VIEW BRIDGE duplicate write was removed —
     * same path, same buffer, no reason to write twice. */
    {
        char tmp_path[PATH_BUF + 4];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", out_path);
        FILE *dst = fopen(tmp_path, "w");
        if (dst) {
            fwrite(framebuf, 1, framesize, dst);
            fclose(dst);
            rename(tmp_path, out_path);
        }
    }
    free(framebuf);

    write_panel_gui_state();

    /* CHTPM RENDER-TRIGGER MARKER: chtpm_parser_pal.c's own main loop
     * only recomposes when one of a fixed set of marker files grows -
     * growing pieces/apps/player_app/state_changed.txt (one of the
     * markers it already checks) makes it re-run load_vars() (which is
     * what re-reads view.txt), so the world's own ticking (monsters,
     * turns) shows up even when chtpm itself received no keypress this
     * cycle. Same addition already made to zoo_0000's own
     * compose_frame.c. */
    {
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/apps/player_app/state_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
    }

    return 0;
}
