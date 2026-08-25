/* compose_frame - one verb, one binary, no shared headers.
 * Renders the zoo map + every pet + the xlector cursor + a footer built
 * from active_target_id's OWN piece.pdl (via pdl_reader.+x) into
 * pieces/display/current_frame.txt. No camera/viewport (map.txt is
 * small enough to show whole, unlike mutaclsym's own scrolling
 * viewport) - not a missing feature, just genuinely out of scope for
 * this sandbox's fixed single small map.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAP_ID "map_zoo"
#define MAX_MAP_W 64
#define MAX_MAP_H 32
#define LOG_TAIL 4
/* Room for the longest emoji this project actually uses (a base
 * codepoint, none of which need a VS16 variation selector here,
 * unlike some of mutaclsym's own choices) plus a NUL - see mutaclsym's
 * own EMOJI_BUF for the fuller reasoning on sizing this generously. */
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

static char species_glyph(const char *species) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/pets/pet_types.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return '?';
    char line[MAX_LINE];
    char result = '?';
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = '\0';
        char *bar = strchr(line, '|');
        if (!bar) continue;
        if ((size_t)(bar - line) == strlen(species) && strncmp(line, species, bar - line) == 0) {
            result = bar[1];
            break;
        }
    }
    fclose(f);
    return result;
}

/* ASCII<->emoji terminal display, same 'e' toggle/mechanism as
 * mutaclsym's own ops/compose_frame.c (see that project's dox/
 * 00-HANDOFF.md for the full writeup) - ported as a PATTERN, not a
 * literal file copy, since zoo_0000 has its own tiny, different
 * content set (2 terrain glyphs, 2 pet species, no item/monster
 * registries at all) rather than mutaclsym's real terrain/furniture/
 * item/monster registries. No glyph collisions exist in this
 * project's own content today ('#'/'.' terrain vs 'd'/'c' pets vs 'X'
 * xlector are all disjoint), so a plain per-character/per-species
 * table is enough - no identity-vs-character ambiguity to resolve the
 * way mutaclsym's own '=', '~', '%' collisions required. */
static void terrain_unicode(char glyph, char *out, size_t out_sz) {
    if (glyph == '#') { snprintf(out, out_sz, "🧱"); return; }
    if (glyph == '.') { snprintf(out, out_sz, "🟫"); return; }
    snprintf(out, out_sz, "%c", glyph);
}

static void species_unicode(const char *species, char *out, size_t out_sz) {
    if (strcmp(species, "dog") == 0) { snprintf(out, out_sz, "🐶"); return; }
    if (strcmp(species, "cat") == 0) { snprintf(out, out_sz, "🐱"); return; }
    snprintf(out, out_sz, "%c", species_glyph(species));
}

static int read_message_log_tail(const char *project_root_, char lines_out[][256]) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/message_log.txt", project_root_);
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

/* Footer is ALWAYS built from active_target_id's own piece.pdl - the
 * same mechanic as ops/xlector_input.c's own method dispatch, see that
 * file's header comment: there is no separate "xlector menu" vs
 * "entity menu" code path anywhere in this project, just one lookup
 * parameterized by whichever piece is currently controlled. */
static void build_footer(char *out, size_t out_sz, const char *active_target, int action_cursor) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[wasd/arrows] Move  [enter] Select/Act  [9] Release to xlector  [e] Emoji");
    size_t len = strlen(buf);

    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "'%s/ops/+x/pdl_reader.+x' %s list_methods", project_root, active_target);
    FILE *pf = popen(cmd, "r");
    int idx = 0;
    if (pf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), pf)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (idx >= 2 && line[0]) {
                char label[MAX_LINE];
                snprintf(label, sizeof(label), "%s", line);
                label[0] = (char)toupper((unsigned char)label[0]);
                const char *cur = (idx == action_cursor) ? "[>]" : "[ ]";
                int n = snprintf(buf + len, sizeof(buf) - len, "  %s %d. [%s]", cur, idx, label);
                if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;
            }
            idx++;
        }
        pclose(pf);
    }
    snprintf(out, out_sz, "%s\n", buf);
}

int main(void) {
    resolve_root();

    char xlector_path[PATH_BUF];
    snprintf(xlector_path, sizeof(xlector_path), "%s/pieces/world_01/%s/xlector/state.txt", project_root, MAP_ID);
    int xlector_x = read_kv_int(xlector_path, "pos_x", 0);
    int xlector_y = read_kv_int(xlector_path, "pos_y", 0);
    char active_target[64];
    read_kv_str(xlector_path, "active_target_id", active_target, sizeof(active_target), "xlector");
    int emoji_mode = read_kv_int(xlector_path, "emoji_mode", 0);
    int action_cursor = -1;
    {
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/pieces/world_01/%s/%s/state.txt", project_root, MAP_ID, active_target);
        action_cursor = read_kv_int(active_path, "action_cursor", -1);
    }

    char map_path[PATH_BUF];
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/%s/map.txt", project_root, MAP_ID);
    char grid[MAX_MAP_H][MAX_MAP_W + 1];
    /* Parallel per-cell emoji-string map, identity-resolved at the
     * same point each layer is drawn onto grid[] below - see
     * terrain_unicode()/species_unicode()'s own header comment. Only
     * populated/read when emoji_mode=1; ascii-mode output is
     * untouched, byte-for-byte the same path as before this feature. */
    static char cell_emoji[MAX_MAP_H][MAX_MAP_W][EMOJI_BUF];
    int rows = 0;
    FILE *mf = fopen(map_path, "r");
    if (mf) {
        while (rows < MAX_MAP_H && fgets(grid[rows], MAX_MAP_W + 1, mf)) {
            grid[rows][strcspn(grid[rows], "\n")] = '\0';
            if (emoji_mode) {
                int len = (int)strlen(grid[rows]);
                for (int c = 0; c < len; c++) terrain_unicode(grid[rows][c], cell_emoji[rows][c], EMOJI_BUF);
            }
            rows++;
        }
        fclose(mf);
    }

    /* Pets, drawn on top of terrain. */
    char map_dir[PATH_BUF];
    snprintf(map_dir, sizeof(map_dir), "%s/pieces/world_01/%s", project_root, MAP_ID);
    DIR *d = opendir(map_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
            char state_path[PATH_BUF + 256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", map_dir, entry->d_name);
#pragma GCC diagnostic pop
            char type[16];
            read_kv_str(state_path, "type", type, sizeof(type), "");
            if (strcmp(type, "pet") != 0) continue;
            int px = read_kv_int(state_path, "pos_x", -1);
            int py = read_kv_int(state_path, "pos_y", -1);
            if (px < 0 || py < 0 || py >= rows) continue;
            char species[16];
            read_kv_str(state_path, "species", species, sizeof(species), "?");
            int rowlen = (int)strlen(grid[py]);
            if (px < rowlen) {
                grid[py][px] = species_glyph(species);
                if (emoji_mode) species_unicode(species, cell_emoji[py][px], EMOJI_BUF);
            }
        }
        closedir(d);
    }

    /* Xlector cursor, always drawn last, on top of everything. */
    if (xlector_y >= 0 && xlector_y < rows) {
        int rowlen = (int)strlen(grid[xlector_y]);
        if (xlector_x >= 0 && xlector_x < rowlen) {
            grid[xlector_y][xlector_x] = 'X';
            /* 🎯 deliberately reuses real fuzz-op's own 'X'->🎯 mapping
             * (render_map.c, see GRAND-ARCHITECTURE.md §0a) - a direct,
             * correct port where one already exists. */
            if (emoji_mode) snprintf(cell_emoji[xlector_y][xlector_x], EMOJI_BUF, "🎯");
        }
    }

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/current_frame.txt", project_root);
    FILE *out = fopen(out_path, "w");
    if (!out) return 1;

    fprintf(out, "ZOO_0000   active: %s\n", active_target);
    for (int r = 0; r < rows; r++) {
        if (emoji_mode) {
            int len = (int)strlen(grid[r]);
            for (int c = 0; c < len; c++) fputs(cell_emoji[r][c], out);
            fputc('\n', out);
        } else {
            fprintf(out, "%s\n", grid[r]);
        }
    }

    /* One status line per pet, plus xlector's own line when it's the
     * active target - a plain roster, not a camera-following HUD. */
    d = opendir(map_dir);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.' || strcmp(entry->d_name, "xlector") == 0) continue;
            char state_path[PATH_BUF + 256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(state_path, sizeof(state_path), "%s/%s/state.txt", map_dir, entry->d_name);
#pragma GCC diagnostic pop
            char type[16];
            read_kv_str(state_path, "type", type, sizeof(type), "");
            if (strcmp(type, "pet") != 0) continue;
            char name[64];
            read_kv_str(state_path, "name", name, sizeof(name), entry->d_name);
            int hunger = read_kv_int(state_path, "hunger", 0);
            int happiness = read_kv_int(state_path, "happiness", 0);
            int energy = read_kv_int(state_path, "energy", 0);
            const char *ctrl = (strcmp(active_target, entry->d_name) == 0) ? " (controlled)" : "";
            fprintf(out, "%-8s Hunger:%3d  Happiness:%3d  Energy:%3d%s\n", name, hunger, happiness, energy, ctrl);
        }
        closedir(d);
    }

    char log_lines[LOG_TAIL][256];
    int log_n = read_message_log_tail(project_root, log_lines);
    for (int i = 0; i < log_n; i++) fprintf(out, "%s\n", log_lines[i]);

    char footer[600];
    build_footer(footer, sizeof(footer), active_target, action_cursor);
    fputs(footer, out);

    fclose(out);

    /* CHTPM VIEW BRIDGE (see chtpm-to-pal-layout-plan.txt §8 and
     * xyzos-standards.txt §7 for the why): a chtpm layout's own
     * `${game_map}` var is populated by load_vars()'s real, unmodified
     * GENERIC VIEW LOADING logic, which checks
     * pieces/apps/player_app/view.txt as one of its own candidate
     * paths - writing this file is the ONLY thing needed to let a real
     * .chtpm menu shell display this project's live map, no
     * chtpm_parser_pal.c patch required. Simplest correct
     * implementation: copy the exact same content just written to
     * current_frame.txt, byte for byte, rather than duplicating every
     * fprintf/fputs call site above against a second FILE* - keeps this
     * addition a single, auditable block instead of scattering it
     * through the whole rendering function. */
    {
        FILE *src = fopen(out_path, "r");
        if (src) {
            char view_path[PATH_BUF];
            snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
            FILE *dst = fopen(view_path, "w");
            if (dst) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
                fclose(dst);
            }
            fclose(src);
        }
    }

    /* CHTPM RENDER-TRIGGER MARKER: REAL BUG, FOUND+FIXED 2026-07-19 -
     * this used to ping pieces/apps/player_app/state_changed.txt,
     * misreading real 1.TPMOS's own "Game layouts: render_map.c after
     * deduped view update" comment as naming THAT file. Direct re-read
     * of chtpm_parser_pal.c's own main loop shows that banner comment
     * sits next to the pieces/display/frame_changed.txt check, not
     * state_changed.txt - state_changed.txt growth only makes chtpm
     * reload vars/reparse the layout, it does NOT by itself trigger a
     * redraw (compose_frame() is what actually re-reads view.txt and
     * writes current_frame.txt, and only frame_changed.txt growth calls
     * it). Fixed to ping the actual render-trigger marker - see
     * xyzos-standards.txt sec. 20 / pal-chain's own identical fix for the
     * full account. compose_frame() itself calls load_vars() as its own
     * first line, so no separate state_changed.txt ping is needed. */
    {
        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
    }

    return 0;
}
