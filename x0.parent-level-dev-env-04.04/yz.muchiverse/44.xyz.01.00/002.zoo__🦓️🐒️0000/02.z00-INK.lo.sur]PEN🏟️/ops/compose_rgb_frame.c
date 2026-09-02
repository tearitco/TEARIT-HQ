/* compose_rgb_frame - one verb, one binary, no shared headers.
 *
 * The GL/RGB mirror's content half for the ZOO LEVEL - direct port of
 * mutaclsym's own ops/compose_rgb_frame.c shape (flat per-tile
 * top-face color, zero GL calls, writes a raw RGBA32 buffer +
 * checksummed receipt so correctness is verifiable without looking at
 * a window - see that file's own header comment for the full
 * GOVERNING CONSTRAINT citation).
 *
 * Mirrors ops/compose_frame.c's own content exactly - same map,
 * same pets (as flat colored blocks, not sprites - this is the flat
 * "mutaclsym style" mirror, not a per-pet shaped desktop window; that
 * is a different, separate thing being built independently in
 * z0.egg-pals+13, communicating through a shared data file - see dox/
 * pet-import-export-standard.md's own note on that split), same
 * xlector cursor - "one state, multiple renderers", not two different
 * views of two different things.
 *
 * Message log / pet vitals roster (the ASCII compose_frame.c's own
 * extra text lines) are NOT mirrored here, same "one thing at a time"
 * scoping mutaclsym's own compose_rgb_frame.c already uses for its own
 * message log. Header + map tiles (terrain+pets+xlector) + the same
 * active-target-driven footer as the ASCII version is the whole HUD
 * here.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAP_ID "map_zoo"
#define VIEWPORT_W 20
#define VIEWPORT_H 12
#define TILE_PX 16
#define GLYPH_W 8
#define GLYPH_H 16
#define HEADER_ROWS 1
#define FOOTER_ROWS 1
#define FRAME_W (VIEWPORT_W * TILE_PX)
#define FRAME_H (HEADER_ROWS * GLYPH_H + VIEWPORT_H * TILE_PX + FOOTER_ROWS * GLYPH_H)
#define MAX_TEXT_COLS (FRAME_W / GLYPH_W)
#define ASSET_ID_BUF 16

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

/* Flat terrain colors - only two glyphs exist in map_zoo's own map.txt
 * today ('#' wall, '.' floor), so a plain switch is clearer here than
 * porting mutaclsym's own registry-file-driven lookup for a two-entry
 * table; grow this into a real registry file the moment a third
 * terrain glyph is ever added, matching mutaclsym's own precedent for
 * WHY a registry beats a hardcoded table once it's not trivially
 * small. */
static void glyph_to_rgb(char glyph, int *r, int *g, int *b) {
    if (glyph == '#') { *r = 90; *g = 90; *b = 100; return; }
    if (glyph == '.') { *r = 180; *g = 170; *b = 150; return; }
    *r = 0; *g = 0; *b = 0;
}

/* Same two-entry-table reasoning as glyph_to_rgb() above - a plain
 * switch on the species string is clearer than a registry file for
 * just two species; grow into a real color field on
 * pieces/registry/pets/pet_types.txt (which already has a glyph
 * column) the moment this stops being trivially small. */
static void species_to_rgb(const char *species, int *r, int *g, int *b) {
    if (strcmp(species, "dog") == 0) { *r = 200; *g = 160; *b = 100; return; }
    if (strcmp(species, "cat") == 0) { *r = 220; *g = 130; *b = 40; return; }
    *r = 255; *g = 0; *b = 255; /* unmapped species - obvious magenta, matching mutaclsym's own "unmapped reads as magenta" convention */
}

/* Terrain glyph -> asset directory name under pieces/registry/
 * emoji_assets/<id>/voxels_16.csv - the real, working pipeline ported
 * from mutaclsym's own GL emoji work (see that project's
 * compose_rgb_frame.c for the fuller citation of the actual
 * FreeType-based emoji_gen_atlas.c/emoji_xtract.c tools this reuses,
 * unchanged, not reinvented). Pet species need no such mapping -
 * "dog"/"cat" already ARE their own asset directory names. */
static const char *terrain_asset_id(char glyph) {
    if (glyph == '#') return "wall";
    if (glyph == '.') return "floor";
    return NULL;
}

/* Reads pieces/registry/emoji_assets/<asset_id>/voxels_16.csv (the
 * "# resolution=16\n...\nr,g,b,a\n<256 rows>" shape emoji_xtract.c
 * writes) into a flat TILE_PXxTILE_PX RGBA array - direct port of
 * mutaclsym's own load_emoji_voxels(), including its single-entry
 * cache (adjacent tiles are very often the same asset in this small
 * zoo pen too). */
static int load_emoji_voxels(const char *asset_id, unsigned char voxels[TILE_PX][TILE_PX][4]) {
    static char cached_id[ASSET_ID_BUF] = "";
    static unsigned char cached_voxels[TILE_PX][TILE_PX][4];
    static int cache_valid = 0;

    if (cache_valid && strcmp(cached_id, asset_id) == 0) {
        memcpy(voxels, cached_voxels, sizeof(cached_voxels));
        return 1;
    }

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/emoji_assets/%s/voxels_16.csv", project_root, asset_id);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[128];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "r,g,b,a", 7) == 0) continue;
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) continue;
        if (idx >= TILE_PX * TILE_PX) break;
        int vy = idx / TILE_PX, vx = idx % TILE_PX;
        voxels[vy][vx][0] = (unsigned char)r;
        voxels[vy][vx][1] = (unsigned char)g;
        voxels[vy][vx][2] = (unsigned char)b;
        voxels[vy][vx][3] = (unsigned char)a;
        idx++;
    }
    fclose(f);
    if (idx != TILE_PX * TILE_PX) return 0;

    snprintf(cached_id, sizeof(cached_id), "%s", asset_id);
    memcpy(cached_voxels, voxels, sizeof(cached_voxels));
    cache_valid = 1;
    return 1;
}

/* Alpha-composites real rasterized emoji pixels on top of the flat
 * base color already in the framebuffer (so transparent emoji pixels
 * show the themed background through, not black) - direct port of
 * mutaclsym's own blit_emoji_tile(). */
static void blit_emoji_tile(unsigned char fb[FRAME_H][FRAME_W][4], int tile_x0, int tile_y0,
                             unsigned char voxels[TILE_PX][TILE_PX][4]) {
    for (int y = 0; y < TILE_PX; y++) {
        int fy = tile_y0 + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < TILE_PX; x++) {
            int fx = tile_x0 + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            unsigned char a = voxels[y][x][3];
            if (a == 0) continue;
            if (a == 255) {
                fb[fy][fx][0] = voxels[y][x][0];
                fb[fy][fx][1] = voxels[y][x][1];
                fb[fy][fx][2] = voxels[y][x][2];
            } else {
                fb[fy][fx][0] = (unsigned char)((voxels[y][x][0] * a + fb[fy][fx][0] * (255 - a)) / 255);
                fb[fy][fx][1] = (unsigned char)((voxels[y][x][1] * a + fb[fy][fx][1] * (255 - a)) / 255);
                fb[fy][fx][2] = (unsigned char)((voxels[y][x][2] * a + fb[fy][fx][2] * (255 - a)) / 255);
            }
            fb[fy][fx][3] = 255;
        }
    }
}

static unsigned char glyphs[127][GLYPH_H][GLYPH_W];

static void load_glyphs(void) {
    memset(glyphs, 0, sizeof(glyphs));
    for (int c = 32; c < 127; c++) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/pieces/registry/fonts/ascii/%d/glyph.txt", project_root, c);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[64];
        int y = 0;
        while (y < GLYPH_H && fgets(line, sizeof(line), f)) {
            for (int x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[c][y][x] = (line[x] == '#') ? 1 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

static void blit_char(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, unsigned char c,
                       unsigned char r, unsigned char g, unsigned char b) {
    if (c < 32 || c > 126) return;
    for (int y = 0; y < GLYPH_H; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < GLYPH_W; x++) {
            int fx = px + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            if (!glyphs[c][y][x]) continue;
            fb[fy][fx][0] = r;
            fb[fy][fx][1] = g;
            fb[fy][fx][2] = b;
            fb[fy][fx][3] = 255;
        }
    }
}

static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, const char *text,
                       unsigned char r, unsigned char g, unsigned char b) {
    int col = 0;
    for (const char *p = text; *p && col < MAX_TEXT_COLS; p++, col++) {
        blit_char(fb, px + col * GLYPH_W, py, (unsigned char)*p, r, g, b);
    }
}

/* Same active-target-driven footer as ops/compose_frame.c's own
 * build_footer() - see that file's header comment / dox/
 * xlector-standard.md for why this is the ENTIRE mechanism behind
 * "xlector shows a different menu than a selected entity." Duplicated
 * here, not shared, per this project's "one op = one binary"
 * convention (same reason mutaclsym keeps two copies of its own
 * build_choice_footer()/build_action_footer()). */
static void build_footer(char *out, size_t out_sz, const char *active_target, int action_cursor) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[wasd] Move [enter] Act [9] Release [e] Emoji");
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
                const char *cur = (idx == action_cursor) ? ">" : " ";
                int n = snprintf(buf + len, sizeof(buf) - len, " %s%d.%s", cur, idx, label);
                if (n > 0 && (size_t)n < sizeof(buf) - len) len += (size_t)n;
            }
            idx++;
        }
        pclose(pf);
    }
    snprintf(out, out_sz, "%s", buf);
}

static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void write_receipt(const char *path, size_t byte_count, uint64_t checksum) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "op=compose_rgb_frame\n");
    fprintf(f, "frame_w=%d\n", FRAME_W);
    fprintf(f, "frame_h=%d\n", FRAME_H);
    fprintf(f, "bytes_per_pixel=4\n");
    fprintf(f, "byte_count=%zu\n", byte_count);
    fprintf(f, "expected_byte_count=%d\n", FRAME_W * FRAME_H * 4);
    fprintf(f, "checksum_fnv1a64=%016llx\n", (unsigned long long)checksum);
    fprintf(f, "written_at=%ld\n", (long)now);
    fclose(f);
}

int main(void) {
    resolve_root();
    load_glyphs();

    char xlector_path[PATH_BUF];
    snprintf(xlector_path, sizeof(xlector_path), "%s/pieces/world_01/%s/xlector/state.txt", project_root, MAP_ID);
    int xlector_x = read_kv_int(xlector_path, "pos_x", 0);
    int xlector_y = read_kv_int(xlector_path, "pos_y", 0);
    char active_target[64];
    read_kv_str(xlector_path, "active_target_id", active_target, sizeof(active_target), "xlector");
    int emoji_mode = read_kv_int(xlector_path, "emoji_mode", 0);
    int action_cursor;
    {
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/pieces/world_01/%s/%s/state.txt", project_root, MAP_ID, active_target);
        action_cursor = read_kv_int(active_path, "action_cursor", -1);
    }

    char map_path[PATH_BUF];
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/%s/map.txt", project_root, MAP_ID);
    char grid[VIEWPORT_H][VIEWPORT_W + 1];
    int rows = 0;
    FILE *mf = fopen(map_path, "r");
    if (mf) {
        /* Real bug, live-verified via a screenshot showing every other
         * map row rendered solid black: a raw line is exactly
         * VIEWPORT_W (20) characters + '\n'. fgets(dest, VIEWPORT_W+1,
         * ...) reads AT MOST (VIEWPORT_W+1)-1 = 20 bytes, which is
         * exactly the 20 '#'/'.' characters with NO room left to also
         * consume the trailing '\n' - fgets stops right there, leaving
         * the '\n' unconsumed in the stream. The VERY NEXT fgets call
         * then reads just that lone leftover '\n' as its own "line"
         * (empty string), so every real row is immediately followed by
         * a phantom empty row - exactly the alternating real-content/
         * black-row pattern seen on screen. Fix: read into an
         * oversized scratch buffer (real headroom past the actual line
         * width, not exactly matching it) so fgets always has room to
         * also consume the newline in the same call - the same
         * generous-buffer convention ops/compose_frame.c's own
         * MAX_MAP_W-sized read already uses, which is why the ASCII
         * view never had this bug. */
        char rawline[VIEWPORT_W + 8];
        while (rows < VIEWPORT_H && fgets(rawline, sizeof(rawline), mf)) {
            rawline[strcspn(rawline, "\n")] = '\0';
            /* rawline is genuinely never longer than VIEWPORT_W in this
             * project's own map.txt, but gcc can't prove that from
             * rawline's own (deliberately oversized) declared size
             * alone - same class of warning narrowly suppressed
             * elsewhere in this project family rather than removing
             * the headroom that fixed the real bug above. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(grid[rows], sizeof(grid[0]), "%s", rawline);
#pragma GCC diagnostic pop
            rows++;
        }
        fclose(mf);
    }

    /* Pets, same layer order as ops/compose_frame.c's own ASCII
     * rendering (terrain -> pets -> xlector on top) - "one state,
     * multiple renderers" means this GL view shows the exact same
     * pets at the exact same tiles, not a deliberately emptied level. */
    char pet_species[VIEWPORT_H][VIEWPORT_W][16];
    memset(pet_species, 0, sizeof(pet_species));
    /* Per-cell emoji ASSET id, identity-resolved at the same points
     * pet_species[]/grid[] are populated - species names ALREADY are
     * their own asset directory names ("dog"/"cat"), no extra mapping
     * needed, unlike terrain (terrain_asset_id() above). */
    char cell_asset[VIEWPORT_H][VIEWPORT_W][ASSET_ID_BUF];
    memset(cell_asset, 0, sizeof(cell_asset));
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
            if (px < 0 || px >= VIEWPORT_W || py < 0 || py >= VIEWPORT_H) continue;
            read_kv_str(state_path, "species", pet_species[py][px], sizeof(pet_species[0][0]), "?");
            if (emoji_mode) snprintf(cell_asset[py][px], ASSET_ID_BUF, "%s", pet_species[py][px]);
        }
        closedir(d);
    }

    static unsigned char framebuf[FRAME_H][FRAME_W][4];
    for (int fy = 0; fy < FRAME_H; fy++)
        for (int fx = 0; fx < FRAME_W; fx++)
            framebuf[fy][fx][3] = 255;

    int tile_y0 = HEADER_ROWS * GLYPH_H;
    for (int r = 0; r < VIEWPORT_H; r++) {
        int len = (r < rows) ? (int)strlen(grid[r]) : 0;
        for (int c = 0; c < VIEWPORT_W; c++) {
            char glyph = (c < len) ? grid[r][c] : ' ';
            int is_xlector = (c == xlector_x && r == xlector_y);
            int rr, gg, bb;
            if (is_xlector) { rr = 0; gg = 255; bb = 255; }
            else if (pet_species[r][c][0]) species_to_rgb(pet_species[r][c], &rr, &gg, &bb);
            else glyph_to_rgb(glyph, &rr, &gg, &bb);
            int tile_x0 = c * TILE_PX;
            int tile_y0_px = tile_y0 + r * TILE_PX;
            for (int py = 0; py < TILE_PX; py++) {
                int fy = tile_y0_px + py;
                for (int px = 0; px < TILE_PX; px++) {
                    int fx = tile_x0 + px;
                    framebuf[fy][fx][0] = (unsigned char)rr;
                    framebuf[fy][fx][1] = (unsigned char)gg;
                    framebuf[fy][fx][2] = (unsigned char)bb;
                    framebuf[fy][fx][3] = 255;
                }
            }
            /* Real rasterized emoji pixels on top of the flat base
             * color - see blit_emoji_tile()'s own header comment. */
            if (emoji_mode) {
                const char *asset_id = is_xlector ? "xlector" : (cell_asset[r][c][0] ? cell_asset[r][c] : terrain_asset_id(glyph));
                if (asset_id) {
                    unsigned char voxels[TILE_PX][TILE_PX][4];
                    if (load_emoji_voxels(asset_id, voxels)) {
                        blit_emoji_tile(framebuf, tile_x0, tile_y0_px, voxels);
                    }
                }
            }
        }
    }

    char header_text[96];
    snprintf(header_text, sizeof(header_text), "ZOO_0000  active: %s", active_target);
    blit_text(framebuf, 0, 0, header_text, 255, 255, 255);

    char footer_text[600];
    build_footer(footer_text, sizeof(footer_text), active_target, action_cursor);
    blit_text(framebuf, 0, tile_y0 + VIEWPORT_H * TILE_PX, footer_text, 255, 255, 255);

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    FILE *out = fopen(out_path, "wb");
    if (!out) return 1;
    size_t written = fwrite(framebuf, 1, byte_count, out);
    fclose(out);
    if (written != byte_count) return 1;

    uint64_t checksum = checksum_buffer((const unsigned char *)framebuf, byte_count);
    char receipt_path[PATH_BUF];
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    write_receipt(receipt_path, byte_count, checksum);

    /* Real fix ported from shared-ops/chtpm_rgb_render.c's own
     * pulse_rgb_ready() - matches real wraith_rgb_daemon.c's own
     * pulse_rgb()/rgb_frame_changed.txt two-stage pattern (a
     * SEPARATE, downstream "output is ready" signal, not the same
     * upstream frame_changed.txt system/renderer.c watches). See that
     * file's own header comment for the full why. system/gl_mirror.c
     * now watches ONLY this file. */
    {
        char rgb_pulse_path[PATH_BUF];
        snprintf(rgb_pulse_path, sizeof(rgb_pulse_path), "%s/pieces/display/rgb_frame_changed.txt", project_root);
        FILE *pf = fopen(rgb_pulse_path, "a");
        if (pf) { fputc('P', pf); fputc('\n', pf); fclose(pf); }
    }

    return 0;
}
