/* palettes_manager.c — palettes' real MANAGER binary (2026-08-25, real
 * TPMOS-compliant rebuild — au11-hq/TPMOS-COMPLIANCE-DEBT.md's own
 * standing rule: build the compliant manager+<module> pattern, same
 * shape as its own proven siblings (stats_hq_manager.c, bookmarks_
 * manager.c), not a renderer-side workaround).
 *
 * Real business logic owned here (moved out of palettes_menu.sh's own
 * compose_emojis()/compose_elements()/emit_tiles_matrix() entirely):
 * reads the real emoji pallet list or chemistry CSV for whichever
 * category this instance serves (argv[3], from <module args="..."/> -
 * see khtpm_entity_menu_render.c's own apply_attr() "args" branch and
 * dbhq_launch_module()'s extra_arg param, both added same day for this),
 * pre-generates any missing emoji sprite.csv tiles (same emoji_gen_atlas/
 * emoji_xtract pipeline the bash version shelled out to - still shelled
 * out to here, real compiled tools, not reinvented), and publishes one
 * `emoji<TAB>label<TAB>sprite_dir_or_empty` line per tile into
 * palettes-<category>_state.txt. The renderer's own dbhq_load_palette_
 * state()/dbhq_inject_palette_tiles() (khtpm_entity_menu_render.c,
 * 2026-08-25) reads that and builds the real <row>/<button> grid at
 * runtime - no bash XML generation, no awk row-chunking. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PATH_BUF 4096
#define MAX_TILES 512

static char g_house_root[PATH_BUF];
static char g_package_dir[PATH_BUF];
static char g_category[64];
static char g_source_path[PATH_BUF];
static char g_state_path[PATH_BUF];
static char g_sprite_root[PATH_BUF];
static char g_emoji_tools[PATH_BUF];
static time_t g_source_mtime = 0;

static char *trim(char *s); /* forward decl - defined just below, needed by publish_layout_flag() above it */

/* REAL FIX 2026-08-27 (direct instruction: "flag hardcoded things in
 * parser... fix that chem hardcoding also. we dont want it to suggest
 * non std behavior is ok") - the renderer used to hardcode `strcmp(
 * g_pal_category, "elements") == 0` to decide the wide-tile layout.
 * That's now a real, explicit `WIDE` column on pallets.pdl's own
 * CATEGORY rows (SECTION|KEY|LABEL|PICKER|WIDE) - this function reads
 * THIS category's own real WIDE value once at startup and publishes it
 * to a small sibling file the renderer reads generically, same real
 * "manager owns the decision, renderer just reads published state"
 * shape every other real field in this file already uses. Defaults to
 * 0 (narrow) if the category isn't found or the file is missing -
 * matches every existing category's real behavior before this fix
 * (only "elements" was ever wide). */
static void publish_layout_flag(void) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/&.widgits/palettes/pallets.pdl", g_house_root);
    int wide = 0;
    FILE *f = fopen(pdl_path, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "CATEGORY", 8) != 0) continue;
            char buf[512]; snprintf(buf, sizeof(buf), "%s", line);
            char *fields[8]; int nf = 0;
            char *tok = strtok(buf, "|");
            while (tok && nf < 8) { fields[nf++] = tok; tok = strtok(NULL, "|"); }
            if (nf < 5) continue; /* SECTION|KEY|LABEL|PICKER|WIDE */
            char *key = trim(fields[1]);
            if (strcmp(key, g_category) != 0) continue;
            wide = atoi(trim(fields[4]));
            break;
        }
        fclose(f);
    }
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/palettes-%s_layout.txt", g_package_dir, g_category);
    FILE *out = fopen(layout_path, "w");
    if (out) { fprintf(out, "wide=%d\n", wide); fclose(out); }
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r')) s[--len] = '\0';
    return s;
}

/* Minimal quote-aware CSV field splitter - the real chemistry CSV has
 * embedded commas inside quoted fields (e.g. "Carboxylic acid, pKa=4.76")
 * that a naive IFS=, split (the OLD bash version's own approach) would
 * mis-split on. Returns field count, fields point into a mutated copy
 * of line (commas/quotes replaced with '\0' in place). */
static int csv_split(char *line, char **fields, int max_fields) {
    int n = 0;
    char *p = line;
    while (*p && n < max_fields) {
        fields[n] = p;
        if (*p == '"') {
            p++;
            fields[n] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
            while (*p && *p != ',') p++;
        } else {
            while (*p && *p != ',') p++;
        }
        if (*p == ',') { *p = '\0'; p++; }
        n++;
    }
    return n;
}

/* finds the taskbar ops dir carrying emoji_gen_atlas.+x, same search
 * palettes_menu.sh's own EMOJI_TOOLS loop used. */
static void find_emoji_tools(void) {
    char probe[PATH_BUF];
    snprintf(probe, sizeof(probe), "%s/*.monads/*.livedesk-taskbar/ops/+x", g_house_root);
    /* the literal '*' in this house's own dir names isn't a shell glob
     * here (no shell involved) - it's a real, fixed directory name (see
     * !.HOUSE_STDS.md's own convention) - use it verbatim. */
    snprintf(g_emoji_tools, sizeof(g_emoji_tools), "%s/*.monads/*.livedesk-taskbar/ops/+x", g_house_root);
    (void)probe;
}

static void ensure_emoji_sprite(const char *glyph, int n) {
    char atlas_bin[PATH_BUF], xtract_bin[PATH_BUF];
    snprintf(atlas_bin, sizeof(atlas_bin), "%s/emoji_gen_atlas.+x", g_emoji_tools);
    snprintf(xtract_bin, sizeof(xtract_bin), "%s/emoji_xtract.+x", g_emoji_tools);
    struct stat st;
    if (stat(atlas_bin, &st) != 0) return;

    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%03d", g_sprite_root, n);
    char csv[PATH_BUF];
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    if (stat(csv, &st) == 0) return; /* already cached */

    char mkcmd[PATH_BUF * 2];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    system(mkcmd);

    char atlas[PATH_BUF];
    snprintf(atlas, sizeof(atlas), "%s/atlas.png", dir);
    char cmd[PATH_BUF * 4];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' >/dev/null 2>&1", atlas_bin, glyph, atlas);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "'%s' '%s' 0 64 '%s' >/dev/null 2>&1", xtract_bin, atlas, csv);
    system(cmd);
}

/* REAL, NEW 2026-08-25 (live report: "some of them are missing emojis -
 * just have blank glyphs") - the chemistry CSV's own compound labels
 * ("🧪 Acetic Acid (CH₃COOH)") are drawn as plain text via draw_text_
 * emoji() (khtpm_draw_core.c's own inline text+emoji renderer, ported
 * from open-hai), which only recognizes codepoints already present in
 * open-hai's own emoji_assets registry - a 36-entry set built for chat
 * text, not chemistry glyphs. Confirmed live: 46 of 49 compound emoji
 * (🧪🍷🧈🐟...) simply aren't in it, so they fell through to a plain
 * Xft glyph draw - tofu, since this house's own default font can't
 * render color emoji. Real fix: populate the SAME open-hai registry
 * (not a second, parallel one) with the missing chemistry codepoints,
 * using the exact same emoji_gen_atlas/emoji_xtract pipeline already
 * proven for palettes' own 64px sprite cache, just at the registry's
 * own 16px resolution - any other consumer of draw_text_emoji()
 * (open-hai chat included) gets these entries for free too, not a
 * palettes-only fix. */
static int utf8_decode_cp(const char *s, unsigned int *cp) {
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0) { *cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0) { *cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    if ((p[0] & 0xF8) == 0xF0) { *cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); return 4; }
    *cp = 0xFFFD; return 1;
}

static void ensure_registry_entry(const char *glyph) {
    unsigned int cp;
    utf8_decode_cp(glyph, &cp);
    if (cp == 0xFE0F || cp == 0x200D) return; /* variation selector / ZWJ alone - no real glyph */

    char atlas_bin[PATH_BUF], xtract_bin[PATH_BUF];
    snprintf(atlas_bin, sizeof(atlas_bin), "%s/emoji_gen_atlas.+x", g_emoji_tools);
    snprintf(xtract_bin, sizeof(xtract_bin), "%s/emoji_xtract.+x", g_emoji_tools);
    struct stat st;
    if (stat(atlas_bin, &st) != 0) return;

    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/&.widgits/open-hai/pieces/registry/emoji_assets/%x", g_house_root, cp);
    char csv[PATH_BUF];
    snprintf(csv, sizeof(csv), "%s/voxels_16.csv", dir);
    if (stat(csv, &st) == 0) return; /* already registered */

    char mkcmd[PATH_BUF * 2];
    snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
    system(mkcmd);

    char atlas[PATH_BUF];
    snprintf(atlas, sizeof(atlas), "%s/atlas.png", dir);
    char cmd[PATH_BUF * 4];
    snprintf(cmd, sizeof(cmd), "'%s' '%s' '%s' >/dev/null 2>&1", atlas_bin, glyph, atlas);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "'%s' '%s' 0 16 '%s' >/dev/null 2>&1", xtract_bin, atlas, csv);
    system(cmd);
    remove(atlas); /* registry entries don't keep the intermediate atlas.png (checked: existing entries don't have one) */
}

static void publish_emojis(void) {
    FILE *in = fopen(g_source_path, "r");
    if (!in) return;
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return; }

    char line[256];
    int n = 0;
    while (n < MAX_TILES && fgets(line, sizeof(line), in)) {
        char *g = trim(line);
        if (!g[0]) continue;
        n++;
        ensure_emoji_sprite(g, n);
        char sprite_dir[PATH_BUF];
        snprintf(sprite_dir, sizeof(sprite_dir), "%s/%03d", g_sprite_root, n);
        char csv[PATH_BUF];
        snprintf(csv, sizeof(csv), "%s/sprite.csv", sprite_dir);
        struct stat st;
        int has_sprite = (stat(csv, &st) == 0);
        fprintf(out, "%s\t%s\t%s\n", g, g, has_sprite ? sprite_dir : "");
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, g_state_path);
}

static void publish_elements(void) {
    FILE *in = fopen(g_source_path, "r");
    if (!in) return;
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return; }

    char line[1024];
    int first = 1;
    int n = 0;
    while (n < MAX_TILES && fgets(line, sizeof(line), in)) {
        if (first) { first = 0; continue; } /* header row */
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s", line);
        char *fields[16];
        int nf = csv_split(buf, fields, 16);
        if (nf < 3) continue;
        char *emoji = trim(fields[0]);
        char *name = trim(fields[1]);
        char *formula = trim(fields[2]);
        if (!emoji[0]) continue;
        ensure_registry_entry(emoji);
        char label[256];
        if (formula[0]) snprintf(label, sizeof(label), "%s %s (%s)", emoji, name, formula);
        else snprintf(label, sizeof(label), "%s %s", emoji, name);
        n++;
        fprintf(out, "%s\t%s\t\n", emoji, label); /* no sprite - matches old bash's own real behavior */
    }
    fclose(in);
    fclose(out);
    rename(tmp_path, g_state_path);
}

/* ===== RPG Maker Tiles (real "rmmv" category, 2026-08-27) =====
 * TILE-SYSTEM-DESIGN.md sec.4b - real, manager-owned, exactly matching
 * this file's own established compliant shape (compose_emojis()/
 * compose_elements() above) - the renderer gets ZERO new tile-specific
 * code, same generic palettes-<category>_state.txt +
 * dbhq_load_palette_state()/dbhq_inject_palette_tiles() consumption
 * every other category already uses. Real tile PIXELS are cropped
 * directly from the real tileset PNG (via stb_image, already proven
 * this session in tile_autotile.c's own standalone visual-verification
 * pass) into the SAME real sprite.csv format ensure_emoji_sprite()
 * already produces (`# resolution=N` / `# scale=1.0` /
 * `# transform=0,0,0` header + `r,g,b,a` rows) - no new sprite format,
 * no glyph-rendering pipeline needed since real pixels already exist. */
#define RMMV_TILE_PX 48

/* Real, live directory scan of the rmmv tileset folder (2026-08-28,
 * per direct external-review correction: "scan the real tilesets
 * folder... do not hardcode" - replaces the earlier tileset_registry.pdl
 * hand-authored approach entirely, since a real RPG Maker asset drop
 * follows a fixed, parseable naming convention on its own:
 * "<Prefix>_<Suffix>.png" where Suffix is one of A1/A2/A3/A4/A5/B/C/D/E
 * (e.g. "World_A2.png", "SF_Inside_A4.png" - the prefix is everything
 * before the LAST underscore, so multi-underscore prefixes like
 * "SF_Inside" still parse correctly). A tileset (for the bottom
 * chooser) is any distinct prefix seen; a sheet/category (for the top
 * tabs) is any distinct suffix seen FOR THE ACTIVE prefix only - never
 * fabricates a tileset or sheet that has no real file backing it. */
/* REAL FIX 2026-08-28 (RMMV-IMG-DIR-TABS-PLAN.md §10 - "move ALL img
 * including tilesets OUT to a folder above the house... reference that
 * folder from .pdl so the path can change without a C rewrite"): the
 * ONE real path pointer for every rmmv img directory (tilesets AND
 * every other category) is the `img_root` key in
 * RMMV-ASSET-SOURCE-LOCATION.pdl. Real, exact key match (not a loose
 * strstr()) so this doesn't accidentally match the PDL's own
 * `img_root_added`/`img_root_why` NOTE lines, which also contain the
 * substring "img_root". No hardcoded `&.widgits/palettes/assets/`, no
 * hardcoded `&.widgits/palettes/tilesets/rmmv`, and no `/media/.../
 * www/img` USB fallback anymore - img_root is the single source of
 * truth for where the house's own stable local copy lives. */
static int rmmv_img_root(const char *house_root, char *out, size_t outsz) {
    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/../#.#.✅️.cal-user-sum/1.^V-hq/RMMV-ASSET-SOURCE-LOCATION.pdl", house_root);
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *bar1 = strchr(line, '|');
        if (!bar1) continue;
        char *bar2 = strchr(bar1 + 1, '|');
        if (!bar2) continue;
        char key[64];
        size_t klen = (size_t)(bar2 - (bar1 + 1));
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, bar1 + 1, klen);
        key[klen] = '\0';
        char *ks = key;
        while (*ks == ' ') ks++;
        char *ke = ks + strlen(ks);
        while (ke > ks && ke[-1] == ' ') { ke--; *ke = '\0'; }
        if (strcmp(ks, "img_root") != 0) continue;
        char *val = bar2 + 1;
        while (*val == ' ') val++;
        size_t n = strlen(val);
        while (n > 0 && (val[n-1] == '\n' || val[n-1] == '\r' || val[n-1] == ' ')) val[--n] = 0;
        snprintf(out, outsz, "%s", val);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

#define RMMV_MAX_ENTRIES 256
typedef struct { char prefix[64]; char suffix[4]; } RmmvFile;
static int scan_rmmv_dir(const char *house_root, RmmvFile *out, int max_out) {
    char root[PATH_BUF];
    if (!rmmv_img_root(house_root, root, sizeof(root))) return 0;
    char dir_path[PATH_BUF];
    snprintf(dir_path, sizeof(dir_path), "%s/tilesets", root);
    DIR *d = opendir(dir_path);
    if (!d) return 0;
    int n = 0;
    struct dirent *de;
    while (n < max_out && (de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        if (len < 5 || strcmp(name + len - 4, ".png") != 0) continue;
        char base[128];
        size_t base_len = len - 4;
        if (base_len >= sizeof(base)) base_len = sizeof(base) - 1;
        memcpy(base, name, base_len);
        base[base_len] = '\0';
        char *us = strrchr(base, '_');
        if (!us || !us[1]) continue;
        char suffix[8];
        snprintf(suffix, sizeof(suffix), "%s", us + 1);
        /* Real suffix whitelist - a1/a2/a3/a4/a5/b/c/d/e, case-
         * insensitive on disk (RPG Maker ships them uppercase), never
         * treats an unrelated "_something.png" as a real sheet. */
        int valid = 0;
        const char *valid_suffixes[] = { "A1","A2","A3","A4","A5","B","C","D","E" };
        for (size_t i = 0; i < sizeof(valid_suffixes) / sizeof(valid_suffixes[0]); i++) {
            if (strcasecmp(suffix, valid_suffixes[i]) == 0) { valid = 1; snprintf(suffix, sizeof(suffix), "%s", valid_suffixes[i]); break; }
        }
        if (!valid) continue;
        *us = '\0';
        snprintf(out[n].prefix, sizeof(out[n].prefix), "%s", base);
        snprintf(out[n].suffix, sizeof(out[n].suffix), "%s", suffix);
        n++;
    }
    closedir(d);
    return n;
}

/* Real, direct pixel-crop sprite writer - reads a real tileset PNG
 * (already loaded once per publish, not per-tile), writes one tile's
 * real RGBA crop into the exact sprite.csv format dbhq_inject_
 * palette_tiles()'s own sprite loader already expects. */
static void write_rmmv_sprite_csv(const unsigned char *atlas, int atlas_w, int atlas_h,
                                   int tile_col, int tile_row, const char *out_path) {
    FILE *f = fopen(out_path, "w");
    if (!f) return;
    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", RMMV_TILE_PX);
    for (int y = 0; y < RMMV_TILE_PX; y++) {
        for (int x = 0; x < RMMV_TILE_PX; x++) {
            int ax = tile_col * RMMV_TILE_PX + x, ay = tile_row * RMMV_TILE_PX + y;
            unsigned char r = 0, g = 0, b = 0, a = 0;
            if (ax < atlas_w && ay < atlas_h) {
                const unsigned char *px = &atlas[((size_t)ay * atlas_w + ax) * 4];
                r = px[0]; g = px[1]; b = px[2]; a = px[3];
            }
            fprintf(f, "%d,%d,%d,%d\n", r, g, b, a);
        }
    }
    fclose(f);
}

/* Real RPG Maker MV/MZ sheet-letter grouping (2026-08-28) - the
 * picker's top tabs are the real A/B/C/D/E SHEET letters, not raw
 * A1..A5 suffixes: A1/A2/A3/A4/A5 all belong under the single "A"
 * sheet tab, B/C/D/E are each their own sheet. `suffix` is always one
 * of the whitelisted values scan_rmmv_dir() already validated. */
static char rmmv_tab_letter_for(const char *suffix) {
    if (suffix[0] == 'A') return 'A';
    return suffix[0];
}

/* Real "which internal category key does this suffix map to" - lower-
 * cased suffix IS the internal category key (a1/a2/a3/a4/a5/b/c/d/e),
 * matching publish_rmmv()'s own existing block_cols/rows branch. */
static void rmmv_cat_for_suffix(const char *suffix, char *out, size_t outsz) {
    size_t i = 0;
    for (; suffix[i] && i + 1 < outsz; i++) out[i] = (char)tolower((unsigned char)suffix[i]);
    out[i] = '\0';
}

/* Real, generic "what tabs/chooser should the renderer show" publisher
 * (2026-08-28 rewrite: sourced from a live directory scan of the real
 * rmmv tileset folder - see scan_rmmv_dir()'s own header comment -
 * instead of a hand-authored registry file. Emits every real distinct
 * tileset PREFIX found on disk (bottom chooser) plus every real sheet
 * SUFFIX found for the CURRENTLY ACTIVE prefix only, grouped into A-E
 * tabs (top tab bar) - never fabricates a tileset or sheet with no
 * real file backing it. Also resolves and returns the real, concrete
 * active category (e.g. "a2") for the active tab letter, since a tab
 * click only specifies a LETTER, not which of a1..a5 backs it. */

static const char *k_rmmv_img_dirs[] = {
    "tilesets","characters","faces","sv_actors","sv_enemies",
    "enemies","battlebacks1","battlebacks2","parallaxes",
    "pictures","animations","system","titles1","titles2"
};

/* REAL FIX 2026-08-28 - see rmmv_img_root()'s own header comment
 * above. `tilesets` is no longer special-cased here: `img_root/
 * tilesets` is a real, ordinary sibling of every other category
 * directory under img_root now (RMMV-IMG-DIR-TABS-PLAN.md §10's own
 * layout - `tilesets_dir` in the PDL is just `<img_root>/tilesets`
 * for documentation/reference, this function doesn't need a separate
 * lookup for it). No more `&.widgits/palettes/assets/` fallback, no
 * more `/media/.../www/img` USB fallback - img_root is the single
 * source of truth. */
static int rmmv_resolve_img_dir(const char *dirname, char *out, size_t outsz) {
    char root[PATH_BUF];
    if (!rmmv_img_root(g_house_root, root, sizeof(root))) return 0;
    snprintf(out, outsz, "%s/%s", root, dirname);
    return access(out, F_OK) == 0;
}

static void write_png_thumb_csv(const char *png_path, const char *csv_path) {
    int w, h, ch;
    unsigned char *px = stbi_load(png_path, &w, &h, &ch, 4);
    if (!px) return;
    FILE *f = fopen(csv_path, "w");
    if (!f) { stbi_image_free(px); return; }
    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", RMMV_TILE_PX);
    for (int y = 0; y < RMMV_TILE_PX; y++) {
        for (int x = 0; x < RMMV_TILE_PX; x++) {
            int sx = (w > 0) ? x * w / RMMV_TILE_PX : 0;
            int sy = (h > 0) ? y * h / RMMV_TILE_PX : 0;
            if (sx >= w) sx = w - 1;
            if (sy >= h) sy = h - 1;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            const unsigned char *p = &px[((size_t)sy * (size_t)w + (size_t)sx) * 4];
            fprintf(f, "%d,%d,%d,%d\n", p[0], p[1], p[2], p[3]);
        }
    }
    fclose(f);
    stbi_image_free(px);
}

static void publish_rmmv_asset_dir(const char *dirname, FILE *out) {
    char abs[PATH_BUF];
    if (!rmmv_resolve_img_dir(dirname, abs, sizeof(abs))) return;
    DIR *d = opendir(abs);
    if (!d) return;
    char sprite_root[PATH_BUF];
    snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/rmmv/dir_%s", g_package_dir, dirname);
    int n = 0;
    struct dirent *de;
    while (n < MAX_TILES && (de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        if (len < 5) continue;
        if (strcasecmp(name + len - 4, ".png") != 0) continue;
        n++;
        char dir[PATH_BUF], csv[PATH_BUF], png[PATH_BUF];
        snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, n);
        snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
        snprintf(png, sizeof(png), "%s/%s", abs, name);
        struct stat st;
        if (stat(csv, &st) != 0) {
            char mk[PATH_BUF * 2];
            snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
            system(mk);
            write_png_thumb_csv(png, csv);
        }
        fprintf(out, "%s\t%s\t%s\n", name, name, dir);
    }
    closedir(d);
}

static void publish_rmmv_options(const char *house_root, const char *active_key,
                                  const char *active_tab_letter, char *out_active_cat, size_t out_active_cat_sz,
                                  const char *active_dir) {
    char opt_path[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/rmmv_options.txt", g_package_dir);
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", opt_path);
    FILE *out = fopen(tmp, "w");
    if (!out) return;

    {
        const char *ad = (active_dir && active_dir[0]) ? active_dir : "tilesets";
        fprintf(out, "ACTIVE_DIR|%s\n", ad);
        for (size_t di = 0; di < sizeof(k_rmmv_img_dirs)/sizeof(k_rmmv_img_dirs[0]); di++) {
            char pth[PATH_BUF];
            if (!rmmv_resolve_img_dir(k_rmmv_img_dirs[di], pth, sizeof(pth))) continue;
            fprintf(out, "DIR|%s|%s\n", k_rmmv_img_dirs[di], k_rmmv_img_dirs[di]);
        }
    }

    RmmvFile entries[RMMV_MAX_ENTRIES];
    int n_entries = scan_rmmv_dir(house_root, entries, RMMV_MAX_ENTRIES);

    /* Real alphabetical-by-prefix ordering (2026-08-28) - readdir()
     * gives no ordering guarantee (same real reason the A-E tabs
     * needed sorting below); collect distinct prefixes first, sort,
     * then emit, so the bottom chooser always lists tilesets in a
     * stable, predictable order instead of raw filesystem order. */
    char seen_keys[32][64]; int n_seen = 0;
    for (int i = 0; i < n_entries; i++) {
        int dup = 0;
        for (int j = 0; j < n_seen; j++) if (strcmp(seen_keys[j], entries[i].prefix) == 0) dup = 1;
        if (dup) continue;
        if (n_seen < 32) snprintf(seen_keys[n_seen++], sizeof(seen_keys[0]), "%s", entries[i].prefix);
    }
    for (int i = 1; i < n_seen; i++) {
        char key[64]; snprintf(key, sizeof(key), "%s", seen_keys[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(seen_keys[j], key) > 0) { snprintf(seen_keys[j + 1], sizeof(seen_keys[0]), "%s", seen_keys[j]); j--; }
        snprintf(seen_keys[j + 1], sizeof(seen_keys[0]), "%s", key);
    }
    for (int i = 0; i < n_seen; i++) {
        char label[64];
        snprintf(label, sizeof(label), "%s", seen_keys[i]);
        for (char *p = label; *p; p++) if (*p == '_') *p = ' ';
        fprintf(out, "TILESET|%s|%s\n", seen_keys[i], label);
    }

    /* Real per-tab-letter grouping for the ACTIVE tileset only - the
     * first real suffix seen for a given letter becomes that tab's own
     * "which concrete category does clicking this letter resolve to"
     * (e.g. "A" -> "a2" if only World_A2.png exists; once World_A1.png
     * also exists, "A" still resolves to whichever A-family suffix was
     * found FIRST in the scan - real, not a fabricated preference). */
    char tab_letters[8]; int n_tabs = 0;
    char tab_default_cat[8][16];
    out_active_cat[0] = '\0';
    for (int i = 0; i < n_entries; i++) {
        if (strcmp(entries[i].prefix, active_key) != 0) continue;
        char cat[16];
        rmmv_cat_for_suffix(entries[i].suffix, cat, sizeof(cat));
        char letter = rmmv_tab_letter_for(entries[i].suffix);
        int dup = 0;
        for (int j = 0; j < n_tabs; j++) if (tab_letters[j] == letter) dup = 1;
        if (!dup && n_tabs < 8) {
            tab_letters[n_tabs] = letter;
            snprintf(tab_default_cat[n_tabs], sizeof(tab_default_cat[0]), "%s", cat);
            n_tabs++;
        }
        if (letter == active_tab_letter[0] && !out_active_cat[0])
            snprintf(out_active_cat, out_active_cat_sz, "%s", cat);
    }
    /* Same real a2>a1>a3>a4>a5 preference as the TAB|A|... line below,
     * applied here too so the ACTUALLY-RENDERED category (out_active_cat)
     * never disagrees with what the "A" tab's own label implies. */
    if (active_tab_letter[0] == 'A') {
        const char *pref0[] = { "a2", "a1", "a3", "a4", "a5" };
        for (size_t p = 0; p < sizeof(pref0) / sizeof(pref0[0]); p++) {
            int found = 0;
            for (int j = 0; j < n_entries; j++) {
                if (strcmp(entries[j].prefix, active_key) != 0) continue;
                char cat[16]; rmmv_cat_for_suffix(entries[j].suffix, cat, sizeof(cat));
                if (strcmp(cat, pref0[p]) == 0) { found = 1; break; }
            }
            if (found) { snprintf(out_active_cat, out_active_cat_sz, "%s", pref0[p]); break; }
        }
    }
    /* Real tie-break for the "A" tab's default sub-category (2026-08-28)
     * - raw directory-scan order is arbitrary (readdir gives no
     * guarantee), so "A" could resolve to a4/a5 before a2 depending on
     * filesystem order, which is a confusing first click for a real
     * user (a2 - ground/floor - is the sheet RPG Maker's own editor
     * shows first). Prefer a2, then a1, then a3/a4/a5 in that order,
     * but ONLY among suffixes actually confirmed present for this
     * tileset above - never picks one that isn't real. */
    for (int i = 0; i < n_tabs; i++) {
        if (tab_letters[i] != 'A') continue;
        const char *pref[] = { "a2", "a1", "a3", "a4", "a5" };
        for (size_t p = 0; p < sizeof(pref) / sizeof(pref[0]); p++) {
            int found = 0;
            for (int j = 0; j < n_entries; j++) {
                if (strcmp(entries[j].prefix, active_key) != 0) continue;
                char cat[16]; rmmv_cat_for_suffix(entries[j].suffix, cat, sizeof(cat));
                if (strcmp(cat, pref[p]) == 0) { found = 1; break; }
            }
            if (found) { snprintf(tab_default_cat[i], sizeof(tab_default_cat[0]), "%s", pref[p]); break; }
        }
    }
    /* Real alphabetical sort (2026-08-28) - readdir() gives no ordering
     * guarantee, so tabs were appearing in arbitrary filesystem-scan
     * order (e.g. "B, C, A") instead of the real A-E sheet order a user
     * expects. Tiny n (max 5), plain insertion sort is plenty. */
    for (int i = 1; i < n_tabs; i++) {
        char lk = tab_letters[i]; char ck[16]; snprintf(ck, sizeof(ck), "%s", tab_default_cat[i]);
        int j = i - 1;
        while (j >= 0 && tab_letters[j] > lk) {
            tab_letters[j + 1] = tab_letters[j];
            snprintf(tab_default_cat[j + 1], sizeof(tab_default_cat[0]), "%s", tab_default_cat[j]);
            j--;
        }
        tab_letters[j + 1] = lk;
        snprintf(tab_default_cat[j + 1], sizeof(tab_default_cat[0]), "%s", ck);
    }
    for (int i = 0; i < n_tabs; i++)
        fprintf(out, "TAB|%c|%s\n", tab_letters[i], tab_default_cat[i]);
    fprintf(out, "ACTIVE_TILESET|%s\n", active_key);
    fprintf(out, "ACTIVE_CATEGORY|%s\n", out_active_cat);
    fclose(out);
    rename(tmp, opt_path);
}

static void publish_rmmv(void) {
    /* Real active-tileset/tab state (2026-08-28 rewrite: "tab" replaces
     * "category" here - a tab LETTER, e.g. "A", is what a real user
     * click sets; the concrete category (a1 vs a2 vs...) is resolved
     * by publish_rmmv_options() below from whatever real suffix
     * actually backs that letter for the active tileset). Default
     * tileset chosen honestly - the first real prefix scan_rmmv_dir()
     * finds - not a hardcoded name that may not even exist on disk. */
    char active_path[PATH_BUF];
    snprintf(active_path, sizeof(active_path), "%s/rmmv_active.txt", g_package_dir);
    char active_key[64] = "", active_tab_letter[4] = "A", active_dir[32] = "tilesets";
    FILE *af = fopen(active_path, "r");
    if (af) {
        char line[128];
        while (fgets(line, sizeof(line), af)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *v = eq + 1;
            size_t vn = strlen(v);
            while (vn > 0 && (v[vn-1] == '\n' || v[vn-1] == '\r')) v[--vn] = '\0';
            if (strcmp(line, "tileset") == 0) snprintf(active_key, sizeof(active_key), "%s", v);
            else if (strcmp(line, "tab") == 0) snprintf(active_tab_letter, sizeof(active_tab_letter), "%s", v);
            else if (strcmp(line, "dir") == 0) snprintf(active_dir, sizeof(active_dir), "%s", v);
        }
        fclose(af);
    }
    if (!active_key[0]) {
        RmmvFile first_scan[RMMV_MAX_ENTRIES];
        int n_first = scan_rmmv_dir(g_house_root, first_scan, RMMV_MAX_ENTRIES);
        if (n_first > 0) snprintf(active_key, sizeof(active_key), "%s", first_scan[0].prefix);
    }

    char active_cat[16];
    publish_rmmv_options(g_house_root, active_key, active_tab_letter, active_cat, sizeof(active_cat), active_dir);
    if (active_dir[0] && strcmp(active_dir, "tilesets") != 0) {
        char tmp_path[PATH_BUF];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
        FILE *outf = fopen(tmp_path, "w");
        if (outf) {
            publish_rmmv_asset_dir(active_dir, outf);
            fclose(outf);
            rename(tmp_path, g_state_path);
        }
        return;
    }

    char rel_atlas[PATH_BUF] = "";
    if (active_key[0] && active_cat[0]) {
        char suffix_upper[8]; size_t si = 0;
        for (; active_cat[si] && si + 1 < sizeof(suffix_upper); si++) suffix_upper[si] = (char)toupper((unsigned char)active_cat[si]);
        suffix_upper[si] = '\0';
        /* REAL FIX 2026-08-28 - just the filename now, no "rmmv/"
         * prefix (that was palettes/tilesets/rmmv/'s own subfolder
         * name; img_root/tilesets/ IS the tileset dir now, no extra
         * nesting - see rmmv_img_root()'s own header comment). */
        snprintf(rel_atlas, sizeof(rel_atlas), "%s_%s.png", active_key, suffix_upper);
    }

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;

    if (rel_atlas[0]) {
        char atlas_path[PATH_BUF];
        char img_root[PATH_BUF] = "";
        rmmv_img_root(g_house_root, img_root, sizeof(img_root));
        snprintf(atlas_path, sizeof(atlas_path), "%s/tilesets/%s", img_root, rel_atlas);
        /* REAL FIX 2026-08-28 (same slow-tab-switch report) - a real
         * cache-hit on every tile still paid for a full stbi_load() PNG
         * decode of the whole atlas first (the actually-expensive part
         * for a large non-autotile sheet like World_B.png at 768x768).
         * stbi_info() reads only the real header (width/height/channels)
         * - cheap - letting us compute the exact expected tile count and
         * check whether every real cache file already exists BEFORE
         * paying for a full decode neither is needed anymore. */
        int info_w = 0, info_h = 0, info_ch = 0;
        if (stbi_info(atlas_path, &info_w, &info_h, &info_ch)) {
            int probe_cols = 1, probe_rows = 1;
            if (strcmp(active_cat, "a1") == 0 || strcmp(active_cat, "a2") == 0) { probe_cols = 2; probe_rows = 3; }
            else if (strcmp(active_cat, "a3") == 0 || strcmp(active_cat, "a4") == 0) { probe_cols = 2; probe_rows = 2; }
            int probe_kx = (info_w / RMMV_TILE_PX) / probe_cols;
            int probe_ky = (info_h / RMMV_TILE_PX) / probe_rows;
            int expected = probe_kx * probe_ky;
            char probe_root[PATH_BUF];
            snprintf(probe_root, sizeof(probe_root), "%s/sprites/rmmv/%s_%s", g_package_dir, active_key, active_cat);
            int all_cached = expected > 0;
            struct stat probe_st;
            for (int i = 1; i <= expected && all_cached; i++) {
                char probe_csv[PATH_BUF];
                snprintf(probe_csv, sizeof(probe_csv), "%s/%03d/sprite.csv", probe_root, i);
                if (stat(probe_csv, &probe_st) != 0) all_cached = 0;
            }
            if (all_cached) {
                /* Every real tile already cached - re-publish the SAME
                 * labels/paths the full path below would produce,
                 * without paying for stbi_load() at all. */
                for (int ky = 0, n = 0; ky < probe_ky; ky++) {
                    for (int kx = 0; kx < probe_kx; kx++) {
                        n++;
                        char dir[PATH_BUF], label[64];
                        snprintf(dir, sizeof(dir), "%s/%03d", probe_root, n);
                        snprintf(label, sizeof(label), "%s kind %d,%d", active_cat, kx, ky);
                        fprintf(out, "%s\t%s\t%s\n", label, label, dir);
                    }
                }
                fclose(out);
                rename(tmp_path, g_state_path);
                return;
            }
        }
        int w, h, ch;
        unsigned char *pixels = stbi_load(atlas_path, &w, &h, &ch, 4);
        if (pixels) {
            /* REAL FIX 2026-08-27/28 (direct correction, verified against
             * real RPG Maker MV asset-authoring standards, not
             * guessed): a raw 48px cell is NOT an independently
             * selectable tile for autotile categories - each real
             * "kind" occupies a fixed BLOCK of raw cells, and the
             * picker should show exactly ONE real, artist-drawn
             * representative thumbnail per kind - the block's own
             * TOP-LEFT cell - not all raw cells in the block (the rest
             * are compositing FRAGMENTS, consumed later by tile_
             * autotile.c's real quadrant math at actual placement time,
             * never shown directly to a user). Per-family block size,
             * branched on active_cat (2026-08-28 extension, per
             * external review): floor-type a1/a2 = 2x3 (matches the
             * real bx=tx*2/by=(ty-2)*3 addressing sourced in RPG-CODE-
             * INDEX-REF.md); wall-type a3/a4 = 2x2 (a different real
             * block shape, not yet visually verified since no a3/a4
             * assets are sourced today - if this is wrong once real
             * assets land, fix the branch below, don't guess further);
             * a5/b/c/d/e are NOT autotile at all - every raw 48x48 cell
             * IS its own real, independently selectable tile (1x1
             * "block"), so no compositing-fragment logic applies. */
            int block_cols = 1, block_rows = 1;
            if (strcmp(active_cat, "a1") == 0 || strcmp(active_cat, "a2") == 0) {
                block_cols = 2; block_rows = 3;
            } else if (strcmp(active_cat, "a3") == 0 || strcmp(active_cat, "a4") == 0) {
                block_cols = 2; block_rows = 2;
            }
            int kinds_x = (w / RMMV_TILE_PX) / block_cols;
            int kinds_y = (h / RMMV_TILE_PX) / block_rows;
            /* REAL FIX 2026-08-28 (live report: "switch to tile tab B is
             * very slow... we have not implemented the caching algorithm
             * used in emoji tiles") - ensure_emoji_sprite()'s own real
             * cache-check ("if (stat(csv,&st)==0) return") was never
             * ported here, so EVERY tab/tileset switch re-decoded the
             * whole atlas PNG and rewrote every single sprite.csv from
             * scratch (256 real file writes for a sheet like World_B.png,
             * every time). Real fix, in two parts:
             * 1. sprite_root is now namespaced by "<tileset>_<category>"
             *    (was a bare "sprites/rmmv" shared across EVERY tileset/
             *    category combination) - a real, necessary PREREQUISITE
             *    for caching, not just a perf nicety: without this, tile
             *    "001" for World/a2 and tile "001" for Inside/a2 would
             *    collide in the SAME directory, and a naive skip-if-
             *    cached check would then silently serve the WRONG
             *    tileset's stale image after switching.
             * 2. Each tile's sprite.csv is now skipped if it already
             *    exists, exactly matching ensure_emoji_sprite()'s own
             *    real, proven pattern - real RMMV tile pixel data never
             *    changes once sourced, so a cache-hit is always correct,
             *    not just fast. */
            char sprite_root[PATH_BUF];
            snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/rmmv/%s_%s", g_package_dir, active_key, active_cat);
            int n = 0;
            struct stat cache_st;
            for (int ky = 0; ky < kinds_y && n < MAX_TILES; ky++) {
                for (int kx = 0; kx < kinds_x && n < MAX_TILES; kx++) {
                    int tx = kx * block_cols, ty = ky * block_rows; /* top-left cell of this kind's own block - the real representative */
                    n++;
                    char dir[PATH_BUF];
                    snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, n);
                    char csv[PATH_BUF];
                    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
                    if (stat(csv, &cache_st) != 0) {
                        char mkcmd[PATH_BUF * 2];
                        snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s'", dir);
                        system(mkcmd);
                        write_rmmv_sprite_csv(pixels, w, h, tx, ty, csv);
                    } /* else already cached - real tile pixels never change once sourced */
                    char label[64];
                    snprintf(label, sizeof(label), "%s kind %d,%d", active_cat, kx, ky); /* real kind index, not raw cell coords */
                    fprintf(out, "%s\t%s\t%s\n", label, label, dir);
                }
            }
            stbi_image_free(pixels);
        }
    }
    /* rel_atlas[0]=='\0' (category not sourced for this tileset) or the
     * PNG failed to load -> publishes an empty tile list, honestly, not
     * a fabricated placeholder. */
    fclose(out);
    rename(tmp_path, g_state_path);
}

static void publish(void) {
    struct stat st;
    if (strcmp(g_category, "rmmv") == 0) {
        /* Real mtime-gate, same discipline as the other two categories
         * - rmmv has TWO real inputs that can change (the real tileset
         * folder's own contents - new/removed PNGs - and the active-
         * tileset/tab choice), so gate on whichever is newer rather
         * than skipping the gate entirely (a real tile crop + sprite.
         * csv write per kind every 1s poll tick forever would be real,
         * needless disk churn). A missing active-state file (nobody's
         * picked a tileset yet) counts as mtime 0, not a reason to
         * skip - the folder's own real mtime alone is enough to trigger
         * the first publish. 2026-08-28: watches the real tilesets/rmmv/
         * DIRECTORY itself (bumps its own mtime on file add/remove,
         * standard POSIX directory semantics) instead of the retired
         * tileset_registry.pdl - see scan_rmmv_dir()'s own header
         * comment for why the registry approach was dropped. */
        time_t reg_mtime = 0, active_mtime = 0;
        char registry_path[PATH_BUF];
        { char img_root[PATH_BUF] = "";
          rmmv_img_root(g_house_root, img_root, sizeof(img_root));
          snprintf(registry_path, sizeof(registry_path), "%s/tilesets", img_root); }
        if (stat(registry_path, &st) == 0) reg_mtime = st.st_mtime;
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/rmmv_active.txt", g_package_dir);
        if (stat(active_path, &st) == 0) active_mtime = st.st_mtime;
        time_t newest = reg_mtime > active_mtime ? reg_mtime : active_mtime;
        /* REAL FIX 2026-08-28 (live report: real tab switches getting
         * silently dropped, needing 2-3 real clicks to "catch up",
         * symptom: old tab's tiles lingering as a visible "second
         * layer" under the new tab) - st_mtime has only ONE-SECOND
         * resolution; a real user clicking through tabs faster than
         * that makes rmmv_active.txt's rewrite land on the SAME mtime
         * as the previous real click, so this gate wrongly treated a
         * genuinely new tab choice as "nothing changed" and never
         * republished. File SIZE alone isn't a safe second signal here
         * (real tab values are single letters - "tab=A" and "tab=B"
         * are the identical byte length), so this compares the actual
         * real CONTENT of the small active-state file instead - cheap
         * (well under 200 bytes), catches every real change regardless
         * of same-second mtime or coincidental length match. */
        static char s_last_active_content[256] = "";
        char active_content[256] = "";
        FILE *af_check = fopen(active_path, "r");
        if (af_check) {
            size_t n = fread(active_content, 1, sizeof(active_content) - 1, af_check);
            active_content[n] = '\0';
            fclose(af_check);
        }
        int active_content_changed = (strcmp(active_content, s_last_active_content) != 0);
        snprintf(s_last_active_content, sizeof(s_last_active_content), "%s", active_content);
        if (newest == 0) return;
        if (newest == g_source_mtime && !active_content_changed) return;
        g_source_mtime = newest;
        publish_rmmv();
        return;
    }
    if (stat(g_source_path, &st) != 0) return;
    if (st.st_mtime == g_source_mtime) return;
    g_source_mtime = st.st_mtime;
    if (strcmp(g_category, "elements") == 0) publish_elements();
    else publish_emojis();
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "palettes_manager: usage: <house_root> <package_dir> <category>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", argv[2]);
    snprintf(g_category, sizeof(g_category), "%s", argv[3]);

    if (strcmp(g_category, "elements") == 0) {
        snprintf(g_source_path, sizeof(g_source_path), "%s/#.ref/menu/palletes/chemistry_tiles_expanded🏆.csv", g_house_root);
    } else if (strcmp(g_category, "rmmv") == 0) {
        g_source_path[0] = '\0'; /* unused for rmmv - publish()'s own dedicated mtime-gate handles it */
    } else {
        snprintf(g_source_path, sizeof(g_source_path), "%s/#.ref/menu/palletes/emoji-pallet-00.00.txt", g_house_root);
    }
    snprintf(g_state_path, sizeof(g_state_path), "%s/palettes-%s_state.txt", g_package_dir, g_category);
    snprintf(g_sprite_root, sizeof(g_sprite_root), "%s/sprites/emoji", g_package_dir);
    find_emoji_tools();
    publish_layout_flag();

    for (;;) {
        publish();
        /* rmmv tab/chooser clicks must land on press 1. 1s sleep made
         * A/B/C and Dungeon/Inside need 2-3 presses (live). Other
         * palettes still 1s. */
        usleep(strcmp(g_category, "rmmv") == 0 ? 100000 : 1000000);
    }
    return 0;
}
