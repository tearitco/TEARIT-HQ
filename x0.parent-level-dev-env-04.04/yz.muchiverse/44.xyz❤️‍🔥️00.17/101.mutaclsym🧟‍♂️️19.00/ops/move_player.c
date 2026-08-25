/* move_player - one verb, one binary, no shared headers.
 * argv[1] = raw keycode (decimal). Moves the hero (always at
 * pieces/world_01/map_start/hero/ - its own map_id FIELD tracks which
 * map it's logically on, per the "legacy map_id field" resolution rule
 * in !.world_architecture+1=rusindol.txt, not its directory location) if
 * the target cell is walkable on the hero's CURRENT map, teleports it to
 * another map if the target cell is a transition tile (see
 * transitions.txt), or - if a monster piece physically sits at the
 * target cell - attacks it instead of moving (matching
 * tick_monsters.c's own move-or-attack logic in reverse). Self-contained:
 * resolves its own root, defines its own constants, reads its own copies
 * of the map/registry.
 *
 * "Interact mode" (hero/state.txt's interact_mode=1, toggled by 'i'/'I'
 * - see ops/choice.c's own header comment for the full writeup) swaps
 * what wasd/arrows control: instead of the hero, they move a real
 * xlector cursor (xlector_pos_x/xlector_pos_y, also in hero/state.txt -
 * not a separate piece/directory, a deliberate simplification since
 * mutaclsym's cursor is transient and doesn't need to persist as its
 * own inspectable entity the way real 1.TPMOS's xlector piece does).
 * Adapted from real 1.TPMOS's xlector active-target pattern
 * (`projects/fuzz-op/manager/fuzz-op_manager.c`, read in full - see
 * dox/04-chtpm-parser-research-and-interact-mode.txt for the complete
 * research writeup). Cursor movement is deliberately FREE/uncollided -
 * no terrain/furniture walkability check, no transition check - matching
 * real CDDA's own "look around"/examine-at-range convention (you can
 * look at something through a window or over a gap you couldn't walk
 * through), clamped only to the map's real width/height so it never
 * reads/writes outside the actual grid. ops/choice.c's own Enter
 * handling (not this file) is what actually EXAMINES whatever the
 * cursor is standing on. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
/* Room for MAX_PATH worth of project_root plus the longest relative
 * suffix this file appends, so gcc can prove snprintf can't truncate. */
#define PATH_BUF (MAX_PATH + 256)
/* MAX_MAP_W/MAX_MAP_H are generous compile-time buffer-size caps, NOT
 * the real per-map dimensions any more - every map's actual width/
 * height now comes from its own state.txt at runtime (read_map_dims()
 * below), matching dox/01-cdda-architecture.md §5a exactly: this is
 * the blocking prerequisite for any map bigger than the old fixed
 * 40x16, done before authoring one. */
#define MAX_MAP_W 256
#define MAX_MAP_H 256

/* Must match the sentinel values keyboard_input.c writes to history.txt
 * for arrow keys - no ncurses anywhere in this project, so these are
 * plain agreed-upon integers, not KEY_* macros from a curses header. */
#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

/* Ledger append (event sourcing - Phase 2 refactor).
 * Appends one action line to data/master_ledger.txt. */
static void append_ledger(const char *actor, int x, int y, const char *action_type) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
    int epoch = read_config_int("current_epoch", 1);
    int turn = read_config_int("current_turn", 0);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", project_root);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s|%d|%s|%d|x:%d,y:%d|%s\n", ts, epoch, actor, turn, x, y, action_type);
        fclose(f);
    }
}

/* Shared by terrain and furniture lookups - both registries use the same
 * glyph|id|name|walkable format. Local reuse within one file is fine per
 * doctrine; this is not a shared header across files. */
static int glyph_walkable(const char *registry_rel_path, char glyph) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, registry_rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return glyph == '.'; /* sane fallback if registry is missing */
    char line[MAX_LINE];
    int result = 0;
    while (fgets(line, sizeof(line), f)) {
        /* A real data row is "glyph|id|...", always exactly one glyph
         * char before the first pipe - checking line[1]=='|' (not just
         * line[0]=='#') is what lets '#' itself be a valid glyph
         * (t_wall's row is literally "#|t_wall|Wall|0|..."), while
         * still skipping real "# free text" comment lines. Found via a
         * real visual bug: the GL/RGB mirror rendered every wall tile
         * magenta ("unmapped glyph") because this exact check was
         * treating the wall's own registry row as a comment - harmless
         * here only by coincidence (the miss-fallback result=0/
         * "not walkable" happens to be the right answer for walls
         * specifically), but a real bug, not a style nit. */
        if (line[0] == '\n' || (line[0] == '#' && line[1] != '|')) continue;
        if (line[0] != glyph) continue;
        /* glyph|id|name|walkable */
        char *p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        result = atoi(p + 1);
        break;
    }
    fclose(f);
    return result;
}

/* Shared by map.txt (terrain) and furniture.txt (furniture) reads - same
 * fixed-width-row-file layout, different out-of-bounds/missing default
 * per caller ('#' for terrain = blocked, ' ' for furniture = nothing
 * there, not blocked). map_w/map_h are THIS map's own real dimensions
 * (read from its state.txt by the caller), not a fixed constant. */
static char file_glyph_at(const char *abs_path, int x, int y, char default_glyph, int map_w, int map_h) {
    if (x < 0 || y < 0 || x >= map_w || y >= map_h) return default_glyph;
    FILE *f = fopen(abs_path, "r");
    if (!f) return default_glyph;
    char line[MAX_MAP_W + 4];
    char glyph = default_glyph;
    for (int row = 0; row <= y; row++) {
        if (!fgets(line, sizeof(line), f)) { glyph = default_glyph; break; }
        if (row == y) glyph = (x < (int)strlen(line)) ? line[x] : default_glyph;
    }
    fclose(f);
    return glyph;
}

static int furniture_walkable(char glyph) {
    if (glyph == ' ') return 1; /* no furniture here - terrain alone decides */
    return glyph_walkable("pieces/registry/furniture/furniture_types.txt", glyph);
}

/* transitions.txt format: x|y|dest_map_id|dest_x|dest_y (one per line).
 * Returns 1 and fills the destination fields if (x,y) on the given map
 * has a transition; 0 otherwise. */
static int find_transition(const char *transitions_path, int x, int y,
                            char *dest_map, size_t dest_map_sz, int *dest_x, int *dest_y) {
    FILE *f = fopen(transitions_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int tx, ty, dx_, dy_;
        char map_buf[64];
        if (sscanf(line, "%d|%d|%63[^|]|%d|%d", &tx, &ty, map_buf, &dx_, &dy_) == 5) {
            if (tx == x && ty == y) {
                snprintf(dest_map, dest_map_sz, "%s", map_buf);
                *dest_x = dx_;
                *dest_y = dy_;
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

static int read_int_field(const char *path, const char *key, int def) {
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

static void read_str_field(const char *path, const char *key, char *out, size_t out_sz, const char *def) {
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

static void monster_name(const char *monster_type, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", monster_type);
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/monsters/monster_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        if ((size_t)(p1 - line) != strlen(monster_type) || strncmp(line, monster_type, p1 - line) != 0) continue;
        char *name = p1 + 1;
        char *end = strchr(name, '|');
        if (end) *end = '\0';
        snprintf(out, out_sz, "%s", name);
        break;
    }
    fclose(f);
}

/* Finds a monster piece at (x,y) under monsters_dir. Returns 1 and fills
 * state_path/hp if found, 0 otherwise. */
static int find_monster_at(const char *monsters_dir, int x, int y, char *state_path, size_t state_path_sz, int *hp) {
    DIR *d = opendir(monsters_dir);
    if (!d) return 0;
    struct dirent *entry;
    int found = 0;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char candidate[PATH_BUF + 384];
        snprintf(candidate, sizeof(candidate), "%s/%s/state.txt", monsters_dir, entry->d_name);
        int mx = read_int_field(candidate, "pos_x", -1);
        int my = read_int_field(candidate, "pos_y", -1);
        if (mx == x && my == y) {
            snprintf(state_path, state_path_sz, "%s", candidate);
            *hp = read_int_field(candidate, "hp", 1);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

/* Registry field lookup matching eat.c's own established indexing
 * convention exactly (1=name, 2=category, 5=power - fields are
 * id|name|category|glyph|weight|power) - duplicated narrowly here per
 * this project's own "no shared headers" rule, not shared. */
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

/* Bump-attack damage, REAL fix (was a flat HERO_ATTACK_DAMAGE=5
 * regardless of inventory - a direct, named user request: "different
 * damage" per whatever weapon is actually held). Scans hero's own
 * inventory for the first weapon-category item - same "first
 * satisfying item wins" convention eat.c's own consume logic already
 * uses for food/drink, not a new pattern. Falls back to a bare-hands
 * baseline (2 - lower than every real weapon's own power value, so
 * picking ANY weapon up is always a genuine upgrade) when no weapon
 * is carried. Returns the item_id consumed FOR DISPLAY ONLY - the
 * weapon itself is NOT consumed by attacking (unlike a thrown weapon,
 * see ops/choice.c's own throw_at() - melee is repeatable). */
#define HERO_UNARMED_DAMAGE 2
static int hero_weapon_damage(const char *inventory_dir, char *weapon_name_out, size_t weapon_name_out_sz) {
    DIR *d = opendir(inventory_dir);
    if (!d) { snprintf(weapon_name_out, weapon_name_out_sz, "fists"); return HERO_UNARMED_DAMAGE; }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char state_path[PATH_BUF + 384];
        snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", inventory_dir, entry->d_name);
        char item_id[64];
        read_str_field(state_path, "item_id", item_id, sizeof(item_id), "?");
        char category[16];
        item_registry_field(item_id, 2, category, sizeof(category), "?");
        if (strcmp(category, "weapon") != 0) continue;
        char power_str[16], name[64];
        item_registry_field(item_id, 5, power_str, sizeof(power_str), "0");
        item_registry_field(item_id, 1, name, sizeof(name), item_id);
        snprintf(weapon_name_out, weapon_name_out_sz, "%s", name);
        closedir(d);
        return atoi(power_str);
    }
    closedir(d);
    snprintf(weapon_name_out, weapon_name_out_sz, "fists");
    return HERO_UNARMED_DAMAGE;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    int key = atoi(argv[1]);
    int dx = 0, dy = 0;
    /* REAL, DIRECT INSTRUCTION (2026-07-17): "wasd should never work for
     * movement... arrows drive movement in 2d and 3d... wasd moves
     * camera." Arrows are now the ONLY movement keys (hero AND the
     * xlector cursor below, both driven by this same dx/dy) - wasd is
     * freed up entirely for 3D camera pan (see the CAMERA PAN block
     * further down, gated on render_mode==1). This is a real, deliberate
     * behavior change, not an addition - wasd used to also move the
     * hero; it no longer does, anywhere. */
    switch (key) {
        case ARROW_UP:    dy = -1; break;
        case ARROW_DOWN:  dy =  1; break;
        case ARROW_LEFT:  dx = -1; break;
        case ARROW_RIGHT: dx =  1; break;
        default: break; /* not an arrow - may still be a camera-pan key,
                          * handled after state is loaded (needs
                          * render_mode to know whether wasd/xz mean
                          * anything right now). */
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/hero_01/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 1;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int px = 0, py = 0;
    int chunk_x = 0, chunk_y = 0; /* voxel chunk coordinates - track which chunk hero is in */
    char map_id[64] = "map_start";
    char active_panel[32] = "none";
    int interact_mode = 0;
    char possessed_id[64] = "none"; /* per sec. 32: separate from interact_mode */
    int xlector_x = -1, xlector_y = -1; /* -1 = field absent, filled from px/py below */
    int render_mode = 0; /* 0=2D (default), 1=3D - toggled by '0' in ops/choice.c */
    int camera_mode = 1; /* 1/2/3/4 - POV preset, set by ops/choice.c */
    (void)camera_mode; /* read for passthrough; camera_control.c and choice.c own this field */
    int facing = 1002; /* last arrow direction pressed (ARROW_UP/DOWN/LEFT/RIGHT) - drives 1st-person view direction */
    int hero_z = 0; /* hero's own vertical Z level, adjusted by x/z keys */
    double cam_pan_x = 0.0, cam_pan_y = 0.0, cam_pan_z = 0.0; /* camera position offsets (preserved, not modified by this op) */
    double cam_yaw = 0.0, cam_pitch = 6.0; /* camera rotation state (preserved, not modified by this op) */
    int cam_z_level = 0; /* camera vertical offset (preserved, not modified by this op) */
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "pos_x") == 0) px = atoi(eq + 1);
            else if (strcmp(lines[nlines], "pos_y") == 0) py = atoi(eq + 1);
            else if (strcmp(lines[nlines], "map_id") == 0) {
                /* Copy into a separate buffer before stripping the
                 * newline - `eq + 1` aliases directly into
                 * lines[nlines], so stripping in place destroys that
                 * line's own trailing '\n' for the rest of this op's
                 * lifetime, corrupting the later passthrough
                 * write-back (map_id has no dedicated found/replace
                 * branch there) by merging it with whatever field
                 * follows it in the file - the exact bug class the
                 * comment below already documents for active_panel;
                 * this is the same fix applied to map_id too. */
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(map_id, sizeof(map_id), "%s", tmp);
            } else if (strcmp(lines[nlines], "active_panel") == 0) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(active_panel, sizeof(active_panel), "%s", tmp);
            } else if (strcmp(lines[nlines], "interact_mode") == 0) {
                interact_mode = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "possessed_id") == 0) {
                char tmp[64]; snprintf(tmp, sizeof(tmp), "%s", eq + 1);
                tmp[strcspn(tmp, "\n")] = '\0';
                snprintf(possessed_id, sizeof(possessed_id), "%s", tmp);
            } else if (strcmp(lines[nlines], "xlector_pos_x") == 0) {
                xlector_x = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "xlector_pos_y") == 0) {
                xlector_y = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "render_mode") == 0) {
                render_mode = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "camera_mode") == 0) {
                camera_mode = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "cam_pan_x") == 0) {
                cam_pan_x = atof(eq + 1);
            } else if (strcmp(lines[nlines], "cam_pan_y") == 0) {
                cam_pan_y = atof(eq + 1);
            } else if (strcmp(lines[nlines], "cam_pan_z") == 0) {
                cam_pan_z = atof(eq + 1);
            } else if (strcmp(lines[nlines], "facing") == 0) {
                facing = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "hero_z") == 0) {
                hero_z = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "cam_yaw") == 0) {
                cam_yaw = atof(eq + 1);
            } else if (strcmp(lines[nlines], "cam_pitch") == 0) {
                cam_pitch = atof(eq + 1);
            } else if (strcmp(lines[nlines], "cam_z_level") == 0) {
                cam_z_level = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "chunk_x") == 0) {
                chunk_x = atoi(eq + 1);
            } else if (strcmp(lines[nlines], "chunk_y") == 0) {
                chunk_y = atoi(eq + 1);
            }
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);
    if (xlector_x < 0) xlector_x = px;
    if (xlector_y < 0) xlector_y = py;

    /* An open overlay panel (e.g. the crafting list - see ops/choice.c's
     * panel-mode handling) captures input the same way real CDDA's own
     * menus do: movement is suspended while it's open, matching
     * gl_desktop.c's is_map_control mode switch (confirmed via research
     * into wraith's overlay rendering) rather than letting the hero
     * silently walk around behind an open menu.
     *
     * Only ever suspend for a value that's actually one of the two real
     * panel types - any other string (including a corrupted/malformed
     * field, e.g. two lines glued together with a missing newline from
     * some earlier partial write) fails OPEN toward movement working,
     * not closed toward the hero being permanently stuck. Hit this for
     * real once: a garbled "active_panel=nonepanel_digit_accum=0" line
     * in a live save meant strcmp(active_panel,"none") was never true
     * again, silently blocking every future move while turns kept
     * ticking in the background (end_turn/tick_monsters aren't gated on
     * this at all) - looked exactly like the game freezing.
     *
     * "Interact mode" (interact_mode=1, toggled by 'i'/'I') suspends
     * HERO movement the same way, but - unlike a panel - doesn't just
     * suspend input outright: wasd/arrows instead move a separate
     * xlector cursor (xlector_pos_x/xlector_pos_y, see below), real
     * 1.TPMOS's own "xlector active-target" pattern
     * (`projects/fuzz-op/manager/fuzz-op_manager.c`, read in full -
     * see ops/choice.c's own header comment for the complete writeup
     * and dox/04-chtpm-parser-research-and-interact-mode.txt for the
     * full research). Same fail-OPEN reasoning as active_panel above
     * for the hero-suspend half: this field defaults to 0 (unset/
     * corrupted reads as int 0 via atoi), so a missing/corrupted field
     * never accidentally freezes hero movement. */
    if (strcmp(active_panel, "craft") == 0 || strcmp(active_panel, "inventory") == 0
        || strcmp(active_panel, "xlector_ctx") == 0) return 0;

    /* HERO Z LEVEL (x/z keys): move the hero up or down one Z level
     * in 3D mode. This is the hero's OWN vertical position, independent
     * of the camera's Z level (c/v keys, handled by camera_control.c).
     * Only meaningful in 3D (render_mode==1). Arrow keys still do their
     * normal movement, so this is an additional axis, not a replacement. */
    if (render_mode == 1 && (key == 'x' || key == 'X' || key == 'z' || key == 'Z')) {
        if (key == 'x' || key == 'X') hero_z++;
        else hero_z--;

        FILE *wf = fopen(path, "w");
        if (!wf) return 1;
        int hz_found = 0;
        for (int i = 0; i < nlines; i++) {
            char *eq = strchr(lines[i], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(lines[i], "hero_z") == 0) { fprintf(wf, "hero_z=%d\n", hero_z); hz_found = 1; *eq = '='; continue; }
                *eq = '=';
            }
            fputs(lines[i], wf);
        }
        if (!hz_found) fprintf(wf, "hero_z=%d\n", hero_z);
        fclose(wf);
        return 0;
    }

    /* CAMERA CONTROLS (w/a/s/d/q/e/r/t/c/v/f) are handled by the
     * separate camera_control.c op, NOT by this file. move_player.c
     * ONLY handles hero/cursor movement via arrow keys and hero Z
     * level via x/z above. */

    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, map_id);
    char map_path[PATH_BUF + 32], furniture_path[PATH_BUF + 32], transitions_path[PATH_BUF + 32];
    char monsters_dir[PATH_BUF + 32], map_state_path[PATH_BUF + 32];
    snprintf(map_path, sizeof(map_path), "%s/map.txt", map_dir);
    snprintf(furniture_path, sizeof(furniture_path), "%s/furniture.txt", map_dir);
    snprintf(transitions_path, sizeof(transitions_path), "%s/transitions.txt", map_dir);
    snprintf(monsters_dir, sizeof(monsters_dir), "%s/monsters", map_dir);
    snprintf(map_state_path, sizeof(map_state_path), "%s/state.txt", map_dir);
    int map_w = read_int_field(map_state_path, "width", 40);
    int map_h = read_int_field(map_state_path, "height", 16);

    if (interact_mode == 0) {
        /* NAV mode: arrows are a hard no-op for game movement.
         * There is no avatar to move in this model (sec. 32.1). */
        return 0;
    }

    if (strcmp(possessed_id, "none") == 0) {
        /* ACTIVE mode, unpossessed: free xlector cursor movement.
         * Uncollided (walls don't block), clamped to map bounds only. */
        int nxl = xlector_x + dx, nyl = xlector_y + dy;
        if (nxl < 0) nxl = 0;
        if (nxl >= map_w) nxl = map_w - 1;
        if (nyl < 0) nyl = 0;
        if (nyl >= map_h) nyl = map_h - 1;
        xlector_x = nxl;
        xlector_y = nyl;

        f = fopen(path, "w");
        if (!f) return 1;
        int xx_found = 0, xy_found = 0;
        for (int i = 0; i < nlines; i++) {
            char *eq = strchr(lines[i], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(lines[i], "xlector_pos_x") == 0) { fprintf(f, "xlector_pos_x=%d\n", xlector_x); xx_found = 1; *eq = '='; continue; }
                if (strcmp(lines[i], "xlector_pos_y") == 0) { fprintf(f, "xlector_pos_y=%d\n", xlector_y); xy_found = 1; *eq = '='; continue; }
                *eq = '=';
            }
            fputs(lines[i], f);
        }
        if (!xx_found) fprintf(f, "xlector_pos_x=%d\n", xlector_x);
        if (!xy_found) fprintf(f, "xlector_pos_y=%d\n", xlector_y);
        fclose(f);
        append_ledger("hero", xlector_x, xlector_y, "xlector_move");
        return 2;
    }

    /* ACTIVE mode, possessed: move the possessed entity.
     * For now, only "hero" is possessible (future: other entities). */

    int nx = px + dx, ny = py + dy;

    /* REVERTED 2026-08-20 (direct user report: "map disappeared when i
     * moved" after re-enabling the collision/transition code below -
     * most likely the map_02 transition, which has no voxel chunk data
     * yet per todo-muta-0g+-.txt's own explicit "out of scope" flag,
     * blanking the 3D view). Restored the 2026-08-18 simplified
     * possessed-movement path: clamp nx/ny to map bounds only, mirror
     * onto xlector, write back, return - matches board-viewer's own
     * xelector model exactly. Direct instruction: rollback and don't
     * dwell on root-causing it further right now. The real collision/
     * combat/transition code is still intact below, just unreachable
     * again via this same early return. */
    if (nx < 0) nx = 0;
    if (nx >= map_w) nx = map_w - 1;
    if (ny < 0) ny = 0;
    if (ny >= map_h) ny = map_h - 1;
    xlector_x = nx;
    xlector_y = ny;
    px = nx;
    py = ny;

    f = fopen(path, "w");
    if (!f) return 1;
    int mx_found = 0, my_found = 0, xx_found2 = 0, xy_found2 = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "pos_x") == 0) { fprintf(f, "pos_x=%d\n", px); mx_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "pos_y") == 0) { fprintf(f, "pos_y=%d\n", py); my_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "xlector_pos_x") == 0) { fprintf(f, "xlector_pos_x=%d\n", xlector_x); xx_found2 = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "xlector_pos_y") == 0) { fprintf(f, "xlector_pos_y=%d\n", xlector_y); xy_found2 = 1; *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!mx_found) fprintf(f, "pos_x=%d\n", px);
    if (!my_found) fprintf(f, "pos_y=%d\n", py);
    if (!xx_found2) fprintf(f, "xlector_pos_x=%d\n", xlector_x);
    if (!xy_found2) fprintf(f, "xlector_pos_y=%d\n", xlector_y);
    fclose(f);
    append_ledger("hero", px, py, "move");
    return 0;

    /* UNREACHABLE below this point - collision/combat/transition richness,
     * kept intact for a future re-attempt once map_02's own voxel chunk
     * data (or an equivalent 3D fallback for un-converted maps) exists. */

    char msg[128] = "";
    char monster_state_path[PATH_BUF + 384];
    int monster_hp;
    if (find_monster_at(monsters_dir, nx, ny, monster_state_path, sizeof(monster_state_path), &monster_hp)) {
        /* Attack, don't move - matches tick_monsters.c's own
         * move-or-attack logic in reverse. */
        char monster_type[64], name[64];
        read_str_field(monster_state_path, "monster_type", monster_type, sizeof(monster_type), "zombie");
        monster_name(monster_type, name, sizeof(name));

        char inventory_dir[PATH_BUF];
        snprintf(inventory_dir, sizeof(inventory_dir), "%s/pieces/hero_01/inventory", project_root);
        char weapon_name[64];
        int attack_damage = hero_weapon_damage(inventory_dir, weapon_name, sizeof(weapon_name));

        monster_hp -= attack_damage;
        if (monster_hp <= 0) {
            /* Killed - delete the piece outright, same as eat.c consuming
             * a food item, not moved anywhere. */
            char *dir_end = strrchr(monster_state_path, '/');
            char monster_dir[PATH_BUF + 384];
            if (dir_end) { size_t len = dir_end - monster_state_path; snprintf(monster_dir, sizeof(monster_dir), "%.*s", (int)len, monster_state_path); }
            else snprintf(monster_dir, sizeof(monster_dir), "%s", monster_state_path);
            remove(monster_state_path);
            rmdir(monster_dir);
            snprintf(msg, sizeof(msg), "You kill the %s!", name);
        } else {
            FILE *mf = fopen(monster_state_path, "r");
            char mlines[16][MAX_LINE];
            int mnlines = 0;
            if (mf) { while (mnlines < 16 && fgets(mlines[mnlines], MAX_LINE, mf)) mnlines++; fclose(mf); }
            mf = fopen(monster_state_path, "w");
            if (mf) {
                for (int i = 0; i < mnlines; i++) {
                    if (strncmp(mlines[i], "hp", 2) == 0 && mlines[i][2] == '=') { fprintf(mf, "hp=%d\n", monster_hp); continue; }
                    fputs(mlines[i], mf);
                }
                fclose(mf);
            }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(msg, sizeof(msg), "You hit the %s with your %s for %d.", name, weapon_name, attack_damage);
#pragma GCC diagnostic pop
        }
    } else {
        char dest_map[64];
        int dest_x = 0, dest_y = 0;
        if (find_transition(transitions_path, nx, ny, dest_map, sizeof(dest_map), &dest_x, &dest_y)) {
            /* Map change - land at the destination map's entry point.
             * Does NOT also run the normal walkability check against the
             * old map's tile at (nx,ny); the transition tile is always
             * walkable-in-spirit (it's how you leave). */
            snprintf(map_id, sizeof(map_id), "%s", dest_map);
            px = dest_x;
            py = dest_y;
        } else {
            char terrain_glyph = file_glyph_at(map_path, nx, ny, '#', map_w, map_h);
            char furniture_glyph = file_glyph_at(furniture_path, nx, ny, ' ', map_w, map_h);
            if (glyph_walkable("pieces/registry/terrain/terrain_types.txt", terrain_glyph) &&
                furniture_walkable(furniture_glyph)) {
                px = nx;
                py = ny;
            } else {
                /* Blocked by a wall/obstacle - previously silent, which
                 * looked exactly like the game freezing (turns still
                 * advanced via end_turn/tick_monsters every keypress
                 * regardless, so hunger/thirst kept climbing with zero
                 * visible feedback that movement itself was rejected).
                 * Real CDDA gives the same kind of feedback on a
                 * blocked move. */
                snprintf(msg, sizeof(msg), "You can't go that way.");
            }
        }
    }

    /* REMOVED 2026-08-20 (see todo-muta-0g+-.txt Step 2): this used to
     * repoint pieces/system/board_manifest.txt to a per-16-tile
     * chunk_<x>_<y> directory on every boundary crossing, matching an
     * old separate-chunk-files layout. This session's own earlier work
     * merged chunk_0_0/chunk_1_0/chunk_2_0 into a single 48-wide
     * chunk_0_0 (the "full map" fix) specifically so the whole map
     * renders at once - board_manifest.txt now permanently points at
     * that one merged chunk and must never be rewritten again. Leaving
     * this block in would silently re-break 3D rendering (pointing at
     * chunk_1_0/chunk_2_0 directories that no longer exist) the instant
     * px crossed 16 or 32 - exactly the two rooms that fix restored.
     * chunk_x/chunk_y are still tracked below (informational only, not
     * read by muta_render_3d.c) via their own state.txt write-back. */

    /* Facing tracks the hero's own last movement direction - drives the
     * 3D renderer's 1st-person view direction (see ops/compose_rgb_
     * frame.c's own camera_mode==1 handling). Only updated on an actual
     * arrow press (dx/dy nonzero) - any other key reaching this point
     * (e.g. a blocked move into a wall, or an attack) leaves the
     * previous facing untouched rather than overwriting it with an
     * irrelevant key value. */
    if (dx != 0 || dy != 0) facing = key;

    int facing_found = 0;
    /* REAL BUG FIX 2026-08-20 (direct user report: "teleport puts player
     * back at start instead of on a new map"): hero_01/state.txt (written
     * by mua_generate_chunk.c) never had a map_id= line to begin with -
     * this loop's replace-in-place branch below only fires when a
     * matching line already exists, so a real transition's computed
     * dest_map (see find_transition() above) was silently dropped every
     * time, never reaching disk. Next read defaulted right back to
     * "map_start" (this function's own hardcoded default), so the hero
     * never actually left the start map even though pos_x/pos_y DID move
     * to the destination coordinates within that same old map - looking
     * exactly like "teleported back to start" since the destination
     * happened to be near spawn. Same append-if-missing pattern already
     * used below for facing/chunk_x/chunk_y. */
    int map_id_found = 0;
    f = fopen(path, "w");
    if (!f) return 1;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "pos_x") == 0) { fprintf(f, "pos_x=%d\n", px); *eq = '='; continue; }
            if (strcmp(lines[i], "pos_y") == 0) { fprintf(f, "pos_y=%d\n", py); *eq = '='; continue; }
            /* When possessed, xlector position mirrors entity position
             * (per sec. 32.3: "like a cursor moving a file"). */
            if (strcmp(possessed_id, "none") != 0) {
                if (strcmp(lines[i], "xlector_pos_x") == 0) { fprintf(f, "xlector_pos_x=%d\n", px); *eq = '='; continue; }
                if (strcmp(lines[i], "xlector_pos_y") == 0) { fprintf(f, "xlector_pos_y=%d\n", py); *eq = '='; continue; }
            }
            if (strcmp(lines[i], "map_id") == 0) { fprintf(f, "map_id=%s\n", map_id); map_id_found = 1; *eq = '='; continue; }
            if (strcmp(lines[i], "facing") == 0) { fprintf(f, "facing=%d\n", facing); facing_found = 1; *eq = '='; continue; }
            /* Preserve camera state fields owned by camera_control.c
             * and compose_rgb_frame.c - we don't modify them, but we
             * must not corrupt them during write-back. Also write
             * hero_z which move_player.c owns. */
            if (strcmp(lines[i], "cam_pan_x") == 0) { fprintf(f, "cam_pan_x=%.2f\n", cam_pan_x); *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pan_y") == 0) { fprintf(f, "cam_pan_y=%.2f\n", cam_pan_y); *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pan_z") == 0) { fprintf(f, "cam_pan_z=%.2f\n", cam_pan_z); *eq = '='; continue; }
            if (strcmp(lines[i], "cam_yaw") == 0) { fprintf(f, "cam_yaw=%.2f\n", cam_yaw); *eq = '='; continue; }
            if (strcmp(lines[i], "cam_pitch") == 0) { fprintf(f, "cam_pitch=%.2f\n", cam_pitch); *eq = '='; continue; }
            if (strcmp(lines[i], "cam_z_level") == 0) { fprintf(f, "cam_z_level=%d\n", cam_z_level); *eq = '='; continue; }
            if (strcmp(lines[i], "hero_z") == 0) { fprintf(f, "hero_z=%d\n", hero_z); *eq = '='; continue; }
            if (strcmp(lines[i], "chunk_x") == 0) { fprintf(f, "chunk_x=%d\n", chunk_x); *eq = '='; continue; }
            if (strcmp(lines[i], "chunk_y") == 0) { fprintf(f, "chunk_y=%d\n", chunk_y); *eq = '='; continue; }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!facing_found) fprintf(f, "facing=%d\n", facing);
    if (!map_id_found) fprintf(f, "map_id=%s\n", map_id);
    /* Ensure chunk_x and chunk_y are always present */
    int chunk_x_found = 0, chunk_y_found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], "chunk_x=", 8) == 0) chunk_x_found = 1;
        if (strncmp(lines[i], "chunk_y=", 8) == 0) chunk_y_found = 1;
    }
    if (!chunk_x_found) fprintf(f, "chunk_x=%d\n", chunk_x);
    if (!chunk_y_found) fprintf(f, "chunk_y=%d\n", chunk_y);
    fclose(f);

    if (!msg[0]) {
        append_ledger("hero", px, py, "move");
    }

    if (msg[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "%s\n", msg); fclose(lf); }
    }
    return 0;
}
