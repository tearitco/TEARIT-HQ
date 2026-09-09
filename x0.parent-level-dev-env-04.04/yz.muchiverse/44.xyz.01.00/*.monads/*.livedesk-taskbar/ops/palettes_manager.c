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
 * see khtpm_core_render.c's own apply_attr() "args" branch and
 * dbhq_launch_module()'s extra_arg param, both added same day for this),
 * pre-generates any missing emoji sprite.csv tiles (same emoji_gen_atlas/
 * emoji_xtract pipeline the bash version shelled out to - still shelled
 * out to here, real compiled tools, not reinvented), and publishes one
 * `emoji<TAB>label<TAB>sprite_dir_or_empty` line per tile into
 * palettes-<category>_state.txt. The renderer's own dbhq_load_palette_
 * state()/dbhq_inject_palette_tiles() (khtpm_core_render.c,
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

static void slug_key(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (; *in && j + 1 < n; in++) {
        unsigned char c = (unsigned char)*in;
        if (isalnum(c)) out[j++] = (char)c;
        else if (c == '-') out[j++] = '-';
        else if (c == ' ' || c == '&' || c == '/') {
            if (j && out[j - 1] != '_') out[j++] = '_';
        }
    }
    while (j > 0 && out[j - 1] == '_') j--;
    out[j] = 0;
}

static int emoji_is_skin(const char *hexline) {
    return strstr(hexline, "1F3FB") || strstr(hexline, "1F3FC")
        || strstr(hexline, "1F3FD") || strstr(hexline, "1F3FE")
        || strstr(hexline, "1F3FF");
}

static void publish_emojis(void) {
    FILE *in = fopen(g_source_path, "r");
    if (!in) return;

    char active_path[PATH_BUF];
    snprintf(active_path, sizeof(active_path), "%s/emojis_active.txt", g_package_dir);
    char active_grp[64] = "Smileys_Emotion", active_sub[64] = "face-smiling";
    FILE *af = fopen(active_path, "r");
    if (af) {
        char line[128];
        while (fgets(line, sizeof(line), af)) {
            char *eq = strchr(line, '='); if (!eq) continue;
            *eq = 0; char *v = eq + 1;
            size_t vn = strlen(v);
            while (vn > 0 && (v[vn-1]=='\n'||v[vn-1]=='\r')) v[--vn]=0;
            if (!strcmp(line, "dir")) snprintf(active_grp, sizeof(active_grp), "%s", v);
            else if (!strcmp(line, "tileset")) snprintf(active_sub, sizeof(active_sub), "%s", v);
        }
        fclose(af);
    }

    char gkey[16][64], glab[16][128]; int ng = 0;
    char skey[64][64], slab[64][128]; int ns = 0;
    char cur_gk[64] = "", cur_gl[128] = "", cur_sk[64] = "", cur_sl[128] = "";
    char line[512];
    while (fgets(line, sizeof(line), in)) {
        if (!strncmp(line, "# group:", 8)) {
            char *g = trim(line + 8);
            if (!strcmp(g, "Component")) { cur_gk[0] = 0; continue; }
            slug_key(g, cur_gk, sizeof(cur_gk));
            snprintf(cur_gl, sizeof(cur_gl), "%s", g);
            int found = 0;
            for (int i = 0; i < ng; i++) if (!strcmp(gkey[i], cur_gk)) found = 1;
            if (!found && ng < 16) {
                snprintf(gkey[ng], 64, "%s", cur_gk);
                snprintf(glab[ng], 128, "%s", cur_gl);
                ng++;
            }
            continue;
        }
        if (!strncmp(line, "# subgroup:", 11)) {
            char *s = trim(line + 11);
            slug_key(s, cur_sk, sizeof(cur_sk));
            snprintf(cur_sl, sizeof(cur_sl), "%s", s);
            if (cur_gk[0] && !strcmp(cur_gk, active_grp) && ns < 64) {
                int found = 0;
                for (int i = 0; i < ns; i++) if (!strcmp(skey[i], cur_sk)) found = 1;
                if (!found) {
                    snprintf(skey[ns], 64, "%s", cur_sk);
                    snprintf(slab[ns], 128, "%s", cur_sl);
                    ns++;
                }
            }
        }
    }
    int grp_ok = 0;
    for (int i = 0; i < ng; i++) if (!strcmp(gkey[i], active_grp)) grp_ok = 1;
    if (!grp_ok && ng > 0) snprintf(active_grp, sizeof(active_grp), "%s", gkey[0]);
    if (ns == 0) {
        rewind(in);
        cur_gk[0] = 0;
        while (fgets(line, sizeof(line), in)) {
            if (!strncmp(line, "# group:", 8)) {
                char *g = trim(line + 8);
                slug_key(g, cur_gk, sizeof(cur_gk));
                continue;
            }
            if (!strncmp(line, "# subgroup:", 11)) {
                char *s = trim(line + 11);
                slug_key(s, cur_sk, sizeof(cur_sk));
                snprintf(cur_sl, sizeof(cur_sl), "%s", s);
                if (cur_gk[0] && !strcmp(cur_gk, active_grp) && ns < 64) {
                    snprintf(skey[ns], 64, "%s", cur_sk);
                    snprintf(slab[ns], 128, "%s", cur_sl);
                    ns++;
                }
            }
        }
    }
    int sub_ok = 0;
    for (int i = 0; i < ns; i++) if (!strcmp(skey[i], active_sub)) sub_ok = 1;
    if (!sub_ok && ns > 0) snprintf(active_sub, sizeof(active_sub), "%s", skey[0]);

    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/emojis_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (opt) {
        fprintf(opt, "ACTIVE_DIR|%s\n", active_grp);
        fprintf(opt, "ACTIVE_TILESET|%s\n", active_sub);
        fprintf(opt, "ACTIVE_CATEGORY|%s\n", active_sub);
        for (int i = 0; i < ng; i++) fprintf(opt, "DIR|%s|%s\n", gkey[i], glab[i]);
        for (int i = 0; i < ns; i++) fprintf(opt, "TILESET|%s|%s\n", skey[i], slab[i]);
        fclose(opt);
        rename(opt_tmp, opt_path);
    }

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return; }
    rewind(in);
    cur_gk[0] = 0; cur_sk[0] = 0;
    int n = 0;
    while (n < MAX_TILES && fgets(line, sizeof(line), in)) {
        if (!strncmp(line, "# group:", 8)) {
            slug_key(trim(line + 8), cur_gk, sizeof(cur_gk));
            continue;
        }
        if (!strncmp(line, "# subgroup:", 11)) {
            slug_key(trim(line + 11), cur_sk, sizeof(cur_sk));
            continue;
        }
        if (!strstr(line, "; fully-qualified")) continue;
        if (emoji_is_skin(line)) continue;
        if (strcmp(cur_gk, active_grp) || strcmp(cur_sk, active_sub)) continue;
        char *hash = strstr(line, "# ");
        if (!hash) continue;
        hash += 2;
        while (*hash == ' ') hash++;
        char glyph[64]; size_t gi = 0;
        while (*hash && *hash != ' ' && *hash != '\t' && gi + 1 < sizeof(glyph))
            glyph[gi++] = *hash++;
        glyph[gi] = 0;
        if (!glyph[0]) continue;
        n++;
        ensure_emoji_sprite(glyph, n);
        char sprite_dir[PATH_BUF];
        snprintf(sprite_dir, sizeof(sprite_dir), "%s/%03d", g_sprite_root, n);
        char csv[PATH_BUF];
        snprintf(csv, sizeof(csv), "%s/sprite.csv", sprite_dir);
        struct stat st;
        int has_sprite = (stat(csv, &st) == 0);
        fprintf(out, "%s\t%s\t%s\n", glyph, glyph, has_sprite ? sprite_dir : "");
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
/* SOURCE PDLs live in the house zip (shared/), always shipped.
 * img_root inside each PDL may point at optional #.NNEST_ASSETS clones. */
static void asset_source_pdl_path(const char *house_root, const char *name,
                                  char *out, size_t outsz) {
    snprintf(out, outsz, "%s/shared/%s", house_root, name);
}

static int rmmv_img_root(const char *house_root, char *out, size_t outsz) {
    char pdl[PATH_BUF];
    asset_source_pdl_path(house_root, "RMMV-ASSET-SOURCE-LOCATION.pdl", pdl, sizeof(pdl));
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

/* Scale a source rectangle into a dest×dest sprite.csv (nearest). */
static void write_scaled_crop(const unsigned char *px, int w, int h,
                              int x0, int y0, int cw, int ch,
                              const char *out_path, int dest) {
    FILE *f = fopen(out_path, "w");
    if (!f) return;
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    if (dest < 1) dest = RMMV_TILE_PX;
    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", dest);
    for (int y = 0; y < dest; y++) {
        for (int x = 0; x < dest; x++) {
            int sx = x0 + x * cw / dest;
            int sy = y0 + y * ch / dest;
            if (sx >= w) sx = w - 1;
            if (sy >= h) sy = h - 1;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            const unsigned char *p = &px[((size_t)sy * (size_t)w + (size_t)sx) * 4];
            fprintf(f, "%d,%d,%d,%d\n", p[0], p[1], p[2], p[3]);
        }
    }
    fclose(f);
}

static int rmmv_emit_cell(FILE *out, int *n, const char *sprite_root, const char *label,
                          const unsigned char *px, int w, int h,
                          int x0, int y0, int cw, int ch) {
    if (*n >= MAX_TILES) return 0;
    (*n)++;
    char dir[PATH_BUF], csv[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, *n);
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    struct stat st;
    if (stat(csv, &st) != 0) {
        char mk[PATH_BUF * 2];
        snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
        system(mk);
        write_scaled_crop(px, w, h, x0, y0, cw, ch, csv, RMMV_TILE_PX);
    }
    fprintf(out, "%s\t%s\t%s\n", label, label, dir);
    return 1;
}

/* Non-tileset img/ dirs. Tilesets stay on the A1–E sheet path.
 * characters/faces/sv_actors/animations are real grids; the rest are
 * one thumbnail per file (whole image, aspect-squashed to 48²). */
static void publish_rmmv_asset_dir(const char *dirname, FILE *out) {
    char abs[PATH_BUF];
    if (!rmmv_resolve_img_dir(dirname, abs, sizeof(abs))) return;
    DIR *d = opendir(abs);
    if (!d) return;
    char names[MAX_TILES][256];
    int n_names = 0;
    struct dirent *de;
    while (n_names < MAX_TILES && (de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        if (len < 5 || len >= 255) continue;
        if (strcasecmp(name + len - 4, ".png") != 0) continue;
        snprintf(names[n_names++], sizeof(names[0]), "%s", name);
    }
    closedir(d);
    for (int i = 1; i < n_names; i++) {
        char key[256]; snprintf(key, sizeof(key), "%s", names[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], key) > 0) {
            snprintf(names[j + 1], sizeof(names[0]), "%s", names[j]);
            j--;
        }
        snprintf(names[j + 1], sizeof(names[0]), "%s", key);
    }
    int n = 0;
    for (int fi = 0; fi < n_names && n < MAX_TILES; fi++) {
        const char *name = names[fi];
        char png[PATH_BUF], stem[256];
        snprintf(png, sizeof(png), "%s/%s", abs, name);
        snprintf(stem, sizeof(stem), "%s", name);
        char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
        char sprite_root[PATH_BUF];
        snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/rmmv/dir_%s/%s", g_package_dir, dirname, stem);
        int w = 0, h = 0, ch = 0;
        unsigned char *px = stbi_load(png, &w, &h, &ch, 4);
        if (!px) continue;
        int sliced = 0;
        if (!strcmp(dirname, "characters") && w >= 48 && h >= 48) {
            if (name[0] == '$') {
                int cw = w / 3, chh = h / 4;
                if (cw > 0 && chh > 0) {
                    char lab[128];
                    snprintf(lab, sizeof(lab), "%s", stem);
                    rmmv_emit_cell(out, &n, sprite_root, lab, px, w, h, cw, 0, cw, chh);
                    sliced = 1;
                }
            } else if (w % 12 == 0 && h % 8 == 0) {
                int cw = w / 12, chh = h / 8;
                for (int cy = 0; cy < 2 && n < MAX_TILES; cy++)
                    for (int cx = 0; cx < 4 && n < MAX_TILES; cx++) {
                        char lab[128];
                        snprintf(lab, sizeof(lab), "%s %d", stem, cy * 4 + cx + 1);
                        int x0 = (cx * 3 + 1) * cw, y0 = cy * 4 * chh;
                        rmmv_emit_cell(out, &n, sprite_root, lab, px, w, h, x0, y0, cw, chh);
                    }
                sliced = 1;
            }
        } else if (!strcmp(dirname, "faces") && w % 4 == 0 && h % 2 == 0 && w >= 4 && h >= 2) {
            int cw = w / 4, chh = h / 2;
            for (int row = 0; row < 2 && n < MAX_TILES; row++)
                for (int col = 0; col < 4 && n < MAX_TILES; col++) {
                    char lab[128];
                    snprintf(lab, sizeof(lab), "%s %d", stem, row * 4 + col + 1);
                    rmmv_emit_cell(out, &n, sprite_root, lab, px, w, h, col * cw, row * chh, cw, chh);
                }
            sliced = 1;
        } else if (!strcmp(dirname, "sv_actors") && w % 9 == 0 && h % 6 == 0 && w >= 9 && h >= 6) {
            int cw = w / 9, chh = h / 6;
            for (int row = 0; row < 6 && n < MAX_TILES; row++)
                for (int col = 0; col < 9 && n < MAX_TILES; col++) {
                    char lab[128];
                    snprintf(lab, sizeof(lab), "%s r%dc%d", stem, row, col);
                    rmmv_emit_cell(out, &n, sprite_root, lab, px, w, h, col * cw, row * chh, cw, chh);
                }
            sliced = 1;
        } else if (!strcmp(dirname, "animations") && w % 5 == 0 && w >= 5) {
            int cw = w / 5, chh = cw;
            int rows = h / chh;
            if (rows < 1) rows = 1;
            for (int row = 0; row < rows && n < MAX_TILES; row++)
                for (int col = 0; col < 5 && n < MAX_TILES; col++) {
                    char lab[128];
                    snprintf(lab, sizeof(lab), "%s %d", stem, row * 5 + col + 1);
                    rmmv_emit_cell(out, &n, sprite_root, lab, px, w, h, col * cw, row * chh, cw, chh);
                }
            sliced = 1;
        }
        if (!sliced) {
            char dir[PATH_BUF], csv[PATH_BUF];
            n++;
            snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, 1);
            snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
            struct stat st;
            if (stat(csv, &st) != 0) {
                char mk[PATH_BUF * 2];
                snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
                system(mk);
                write_png_thumb_csv(png, csv);
            }
            fprintf(out, "%s\t%s\t%s\n", stem, stem, dir);
        }
        stbi_image_free(px);
    }
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

/* REAL, NEW 2026-08-29, direct instruction ("make a debug sidebar in
 * hq, and give it ops i can start or stop from list of ops we're
 * trying to debug, and have it read the debug.txt in the main
 * window") - a real, extensible list of watchable debug ops. Add a
 * new {name, binary, enabled_flag} entry here for each future one -
 * this manager publishes one toggle row per entry, checking each
 * op's own real running-or-not state via its enabled-flag file's
 * content (not by scanning /proc - the flag file IS the source of
 * truth an op like tp_debug_click_watcher.c already polls itself). */
typedef struct { const char *name; const char *bin_rel; const char *flag_name; } DebugOp;
static const DebugOp DEBUG_OPS[] = {
    { "click-watcher", "&.widgits/tile-picker/ops/+x/tp_debug_click_watcher.+x", "debug_watch_enabled.txt" },
};
#define N_DEBUG_OPS (int)(sizeof(DEBUG_OPS) / sizeof(DEBUG_OPS[0]))

static int debug_op_enabled(const char *house_root, const DebugOp *op) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/debug/%s", house_root, op->flag_name);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[8] = "";
    if (fgets(line, sizeof(line), f)) { /* nothing extra */ }
    fclose(f);
    return line[0] == '1';
}

static void publish_debug(void) {
    char debug_dir[PATH_BUF];
    snprintf(debug_dir, sizeof(debug_dir), "%s/#.desktop/debug", g_house_root);
    mkdir(debug_dir, 0777);

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;

    /* Row 1..N: one real toggle per DEBUG_OPS entry. "emoji" field
     * (first column) carries the real onclick action string - the
     * renderer's own debug-category onclick branch (khtpm_entity_menu_
     * render.c) routes "toggle:<op-index>" and "clear" specially,
     * same convention rmmv's own arm-rmmv routing already uses. */
    for (int i = 0; i < N_DEBUG_OPS; i++) {
        int on = debug_op_enabled(g_house_root, &DEBUG_OPS[i]);
        fprintf(out, "toggle:%d\t%s %s\t\n", i, on ? "\xE2\x96\xA0 Stop:" : "\xE2\x96\xB6 Start:", DEBUG_OPS[i].name);
    }
    fprintf(out, "clear\t\xF0\x9F\x97\x91 Clear debug.txt\t\n");

    /* Real debug.txt content, tail-capped so a long debugging session
     * can't blow past the renderer's own PAL_MAX_TILES (512) - shows
     * the most RECENT lines, which is what a real debug session cares
     * about. Read-only rows (onclick action "noop"). */
    char debug_txt_path[PATH_BUF];
    snprintf(debug_txt_path, sizeof(debug_txt_path), "%s/debug.txt", debug_dir);
    FILE *df = fopen(debug_txt_path, "r");
    if (df) {
        char lines[300][256];
        int n = 0;
        char buf[256];
        while (fgets(buf, sizeof(buf), df)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            if (!buf[0]) continue;
            if (n < 300) { snprintf(lines[n], sizeof(lines[n]), "%s", buf); n++; }
            else {
                /* shift, keep only the most recent 300 - real tail
                 * behavior, not a silent truncation from the START. */
                memmove(lines[0], lines[1], sizeof(lines[0]) * 299);
                snprintf(lines[299], sizeof(lines[299]), "%s", buf);
            }
        }
        fclose(df);
        for (int i = 0; i < n; i++) {
            fprintf(out, "noop\t%s\t\n", lines[i]);
        }
    } else {
        fprintf(out, "noop\t(debug.txt does not exist yet)\t\n");
    }

    fclose(out);
    rename(tmp_path, g_state_path);
}

/* ===== Piececraft Blocks (Mineclonia) — RMMV-shaped choosers =====
 * DIR  = mods pack (ITEMS / ENTITIES / …)
 * TILESET = mod folder (mcl_core, mcl_wool, …)
 * Grid = 16×16 (or scaled) PNG thumbs → sprite.csv like rmmv. */
static int pc_img_root(char *out, size_t outsz) {
    char pdl[PATH_BUF];
    asset_source_pdl_path(g_house_root, "MINECLONIA-ASSET-SOURCE-LOCATION.pdl", pdl, sizeof(pdl));
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SOURCE", 6) != 0) continue;
        char *k = strstr(line, "img_root");
        if (!k) continue;
        char *bar = strrchr(line, '|');
        if (!bar) continue;
        char *v = bar + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t n = strlen(v);
        while (n > 0 && (v[n-1] == '\n' || v[n-1] == '\r' || v[n-1] == ' ')) v[--n] = 0;
        if (n > 0) { snprintf(out, outsz, "%s", v); ok = 1; break; }
    }
    fclose(f);
    return ok && access(out, F_OK) == 0;
}

static int pc_skip_tex(const char *name) {
    if (strstr(name, "_normal")) return 1;
    if (strstr(name, "_n.png")) return 1;
    if (strstr(name, "_e.png")) return 1;
    if (strstr(name, "gui_")) return 1;
    return 0;
}

static void publish_piececraft(void) {
    char root[PATH_BUF];
    if (!pc_img_root(root, sizeof(root))) return;
    char active_path[PATH_BUF];
    snprintf(active_path, sizeof(active_path), "%s/piececraft_active.txt", g_package_dir);
    char active_pack[64] = "ITEMS", active_mod[64] = "mcl_core";
    FILE *af = fopen(active_path, "r");
    if (af) {
        char line[128];
        while (fgets(line, sizeof(line), af)) {
            char *eq = strchr(line, '='); if (!eq) continue;
            *eq = 0; char *v = eq + 1;
            size_t vn = strlen(v);
            while (vn > 0 && (v[vn-1]=='\n'||v[vn-1]=='\r')) v[--vn]=0;
            if (!strcmp(line, "dir") || !strcmp(line, "pack")) snprintf(active_pack, sizeof(active_pack), "%s", v);
            else if (!strcmp(line, "tileset") || !strcmp(line, "mod")) snprintf(active_mod, sizeof(active_mod), "%s", v);
        }
        fclose(af);
    }

    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/piececraft_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (!opt) return;
    fprintf(opt, "ACTIVE_DIR|%s\n", active_pack);
    fprintf(opt, "ACTIVE_TILESET|%s\n", active_mod);
    fprintf(opt, "ACTIVE_CATEGORY|%s\n", active_mod);

    DIR *pd = opendir(root);
    if (pd) {
        struct dirent *de;
        char packs[16][64]; int np = 0;
        while ((de = readdir(pd)) != NULL && np < 16) {
            if (de->d_name[0] == '.') continue;
            char pth[PATH_BUF];
            snprintf(pth, sizeof(pth), "%s/%s", root, de->d_name);
            struct stat st;
            if (stat(pth, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(packs[np++], 64, "%s", de->d_name);
        }
        closedir(pd);
        for (int i = 1; i < np; i++) {
            char k[64]; snprintf(k, 64, "%s", packs[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(packs[j], k) > 0) { snprintf(packs[j+1], 64, "%s", packs[j]); j--; }
            snprintf(packs[j+1], 64, "%s", k);
        }
        int pack_ok = 0;
        for (int i = 0; i < np; i++) {
            fprintf(opt, "DIR|%s|%s\n", packs[i], packs[i]);
            if (!strcmp(packs[i], active_pack)) pack_ok = 1;
        }
        if (!pack_ok && np > 0) snprintf(active_pack, sizeof(active_pack), "%s", packs[0]);
    }

    char pack_path[PATH_BUF];
    snprintf(pack_path, sizeof(pack_path), "%s/%s", root, active_pack);
    DIR *md = opendir(pack_path);
    char mods[128][64]; int nm = 0;
    if (md) {
        struct dirent *de;
        while ((de = readdir(md)) != NULL && nm < 128) {
            if (de->d_name[0] == '.' || de->d_name[0] == '_') continue;
            char tex[PATH_BUF];
            snprintf(tex, sizeof(tex), "%s/%s/textures", pack_path, de->d_name);
            struct stat st;
            if (stat(tex, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(mods[nm++], 64, "%s", de->d_name);
        }
        closedir(md);
        for (int i = 1; i < nm; i++) {
            char k[64]; snprintf(k, 64, "%s", mods[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(mods[j], k) > 0) { snprintf(mods[j+1], 64, "%s", mods[j]); j--; }
            snprintf(mods[j+1], 64, "%s", k);
        }
        int mod_ok = 0;
        for (int i = 0; i < nm; i++) {
            fprintf(opt, "TILESET|%s|%s\n", mods[i], mods[i]);
            if (!strcmp(mods[i], active_mod)) mod_ok = 1;
        }
        if (!mod_ok && nm > 0) snprintf(active_mod, sizeof(active_mod), "%s", mods[0]);
    }
    fclose(opt);
    rename(opt_tmp, opt_path);

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;
    char texdir[PATH_BUF];
    snprintf(texdir, sizeof(texdir), "%s/%s/%s/textures", root, active_pack, active_mod);
    DIR *td = opendir(texdir);
    int n = 0;
    if (td) {
        char names[MAX_TILES][256]; int nn = 0;
        struct dirent *de;
        while (nn < MAX_TILES && (de = readdir(td)) != NULL) {
            size_t len = strlen(de->d_name);
            if (len < 5 || strcasecmp(de->d_name + len - 4, ".png") != 0) continue;
            if (pc_skip_tex(de->d_name)) continue;
            snprintf(names[nn++], 256, "%s", de->d_name);
        }
        closedir(td);
        for (int i = 1; i < nn; i++) {
            char k[256]; snprintf(k, 256, "%s", names[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(names[j], k) > 0) { snprintf(names[j+1], 256, "%s", names[j]); j--; }
            snprintf(names[j+1], 256, "%s", k);
        }
        char sprite_root[PATH_BUF];
        snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/pc/%s/%s", g_package_dir, active_pack, active_mod);
        for (int i = 0; i < nn && n < MAX_TILES; i++) {
            n++;
            char dir[PATH_BUF], csv[PATH_BUF], png[PATH_BUF], stem[256];
            snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, n);
            snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
            snprintf(png, sizeof(png), "%s/%s", texdir, names[i]);
            snprintf(stem, sizeof(stem), "%s", names[i]);
            char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
            struct stat st;
            if (stat(csv, &st) != 0) {
                char mk[PATH_BUF * 2];
                snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
                system(mk);
                write_png_thumb_csv(png, csv);
            }
            fprintf(out, "%s\t%s\t%s\n", stem, stem, dir);
        }
    }
    fclose(out);
    rename(tmp_path, g_state_path);
}

static int cdda_img_root(char *out, size_t outsz) {
    char pdl[PATH_BUF];
    asset_source_pdl_path(g_house_root, "CDDA-ASSET-SOURCE-LOCATION.pdl", pdl, sizeof(pdl));
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SOURCE", 6) != 0) continue;
        if (!strstr(line, "img_root")) continue;
        char *bar = strrchr(line, '|');
        if (!bar) continue;
        char *v = bar + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t n = strlen(v);
        while (n > 0 && (v[n-1] == '\n' || v[n-1] == '\r' || v[n-1] == ' ')) v[--n] = 0;
        if (n > 0) { snprintf(out, outsz, "%s", v); ok = 1; break; }
    }
    fclose(f);
    return ok && access(out, F_OK) == 0;
}

static int cdda_skip_sheet(const char *name) {
    if (strstr(name, "filler")) return 1;
    if (strstr(name, "incomplete")) return 1;
    return 0;
}

static void collect_pngs(const char *dir, char names[][256], char paths[][PATH_BUF], int *nn, int cap) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while (*nn < cap && (de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char pth[PATH_BUF];
        snprintf(pth, sizeof(pth), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(pth, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            collect_pngs(pth, names, paths, nn, cap);
            continue;
        }
        size_t len = strlen(de->d_name);
        if (len < 5 || strcasecmp(de->d_name + len - 4, ".png") != 0) continue;
        snprintf(names[*nn], 256, "%s", de->d_name);
        char *dot = strrchr(names[*nn], '.'); if (dot) *dot = 0;
        snprintf(paths[*nn], PATH_BUF, "%s", pth);
        (*nn)++;
    }
    closedir(d);
}

static void publish_cdda(void) {
    char root[PATH_BUF];
    if (!cdda_img_root(root, sizeof(root))) return;
    char active_path[PATH_BUF];
    snprintf(active_path, sizeof(active_path), "%s/cdda_active.txt", g_package_dir);
    char active_sheet[64] = "pngs_normal_32x32", active_cat[64] = "terrain";
    FILE *af = fopen(active_path, "r");
    if (af) {
        char line[128];
        while (fgets(line, sizeof(line), af)) {
            char *eq = strchr(line, '='); if (!eq) continue;
            *eq = 0; char *v = eq + 1;
            size_t vn = strlen(v);
            while (vn > 0 && (v[vn-1]=='\n'||v[vn-1]=='\r')) v[--vn]=0;
            if (!strcmp(line, "dir")) snprintf(active_sheet, sizeof(active_sheet), "%s", v);
            else if (!strcmp(line, "tileset")) snprintf(active_cat, sizeof(active_cat), "%s", v);
        }
        fclose(af);
    }

    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/cdda_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (!opt) return;
    fprintf(opt, "ACTIVE_DIR|%s\n", active_sheet);
    fprintf(opt, "ACTIVE_TILESET|%s\n", active_cat);
    fprintf(opt, "ACTIVE_CATEGORY|%s\n", active_cat);

    DIR *pd = opendir(root);
    char sheets[32][64]; int nsh = 0;
    if (pd) {
        struct dirent *de;
        while ((de = readdir(pd)) != NULL && nsh < 32) {
            if (strncmp(de->d_name, "pngs_", 5) != 0) continue;
            if (cdda_skip_sheet(de->d_name)) continue;
            char pth[PATH_BUF];
            snprintf(pth, sizeof(pth), "%s/%s", root, de->d_name);
            struct stat st;
            if (stat(pth, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(sheets[nsh++], 64, "%s", de->d_name);
        }
        closedir(pd);
        for (int i = 1; i < nsh; i++) {
            char k[64]; snprintf(k, 64, "%s", sheets[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(sheets[j], k) > 0) { snprintf(sheets[j+1], 64, "%s", sheets[j]); j--; }
            snprintf(sheets[j+1], 64, "%s", k);
        }
        int sheet_ok = 0;
        for (int i = 0; i < nsh; i++) {
            const char *lab = sheets[i];
            if (!strncmp(lab, "pngs_", 5)) lab += 5;
            fprintf(opt, "DIR|%s|%s\n", sheets[i], lab);
            if (!strcmp(sheets[i], active_sheet)) sheet_ok = 1;
        }
        if (!sheet_ok && nsh > 0) snprintf(active_sheet, sizeof(active_sheet), "%s", sheets[0]);
    }

    char sheet_path[PATH_BUF];
    snprintf(sheet_path, sizeof(sheet_path), "%s/%s", root, active_sheet);
    DIR *cd = opendir(sheet_path);
    char cats[64][64]; int nc = 0;
    if (cd) {
        struct dirent *de;
        while ((de = readdir(cd)) != NULL && nc < 64) {
            if (de->d_name[0] == '.') continue;
            char pth[PATH_BUF];
            snprintf(pth, sizeof(pth), "%s/%s", sheet_path, de->d_name);
            struct stat st;
            if (stat(pth, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(cats[nc++], 64, "%s", de->d_name);
        }
        closedir(cd);
        for (int i = 1; i < nc; i++) {
            char k[64]; snprintf(k, 64, "%s", cats[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(cats[j], k) > 0) { snprintf(cats[j+1], 64, "%s", cats[j]); j--; }
            snprintf(cats[j+1], 64, "%s", k);
        }
        int cat_ok = 0;
        for (int i = 0; i < nc; i++) {
            fprintf(opt, "TILESET|%s|%s\n", cats[i], cats[i]);
            if (!strcmp(cats[i], active_cat)) cat_ok = 1;
        }
        if (!cat_ok && nc > 0) snprintf(active_cat, sizeof(active_cat), "%s", cats[0]);
    }
    fclose(opt);
    rename(opt_tmp, opt_path);

    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;
    char catdir[PATH_BUF];
    snprintf(catdir, sizeof(catdir), "%s/%s/%s", root, active_sheet, active_cat);
    static char names[MAX_TILES][256];
    static char paths[MAX_TILES][PATH_BUF];
    int nn = 0;
    collect_pngs(catdir, names, paths, &nn, MAX_TILES);
    for (int i = 1; i < nn; i++) {
        char kn[256], kp[PATH_BUF];
        snprintf(kn, 256, "%s", names[i]);
        snprintf(kp, PATH_BUF, "%s", paths[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], kn) > 0) {
            snprintf(names[j+1], 256, "%s", names[j]);
            snprintf(paths[j+1], PATH_BUF, "%s", paths[j]);
            j--;
        }
        snprintf(names[j+1], 256, "%s", kn);
        snprintf(paths[j+1], PATH_BUF, "%s", kp);
    }
    char sprite_root[PATH_BUF];
    snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/cdda/%s/%s", g_package_dir, active_sheet, active_cat);
    int n = 0;
    for (int i = 0; i < nn && n < MAX_TILES; i++) {
        n++;
        char dir[PATH_BUF], csv[PATH_BUF];
        snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, n);
        snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
        struct stat st;
        if (stat(csv, &st) != 0) {
            char mk[PATH_BUF * 2];
            snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
            system(mk);
            write_png_thumb_csv(paths[i], csv);
        }
        fprintf(out, "%s\t%s\t%s\n", names[i], names[i], dir);
    }
    fclose(out);
    rename(tmp_path, g_state_path);
}

static int pdl_img_root_named(const char *pdl_name, char *out, size_t outsz) {
    char pdl[PATH_BUF];
    asset_source_pdl_path(g_house_root, pdl_name, pdl, sizeof(pdl));
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SOURCE", 6) != 0) continue;
        if (!strstr(line, "img_root")) continue;
        char *bar = strrchr(line, '|');
        if (!bar) continue;
        char *v = bar + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t n = strlen(v);
        while (n > 0 && (v[n-1]=='\n'||v[n-1]=='\r'||v[n-1]==' ')) v[--n]=0;
        if (n > 0) { snprintf(out, outsz, "%s", v); ok = 1; break; }
    }
    fclose(f);
    return ok && access(out, F_OK) == 0;
}

static int guess_cell_px(int w, int h) {
    static const int t[] = { 16, 20, 32, 48, 8, 64 };
    for (int i = 0; i < 6; i++)
        if (t[i] <= w && t[i] <= h && (w % t[i]) == 0 && (h % t[i]) == 0)
            return t[i];
    return w < h ? w : h;
}

static void read_active_kv(const char *stem, char *dir, size_t dsz, char *set, size_t ssz,
                           const char *def_dir, const char *def_set) {
    snprintf(dir, dsz, "%s", def_dir);
    snprintf(set, ssz, "%s", def_set);
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/%s_active.txt", g_package_dir, stem);
    FILE *af = fopen(p, "r");
    if (!af) return;
    char line[256];
    while (fgets(line, sizeof(line), af)) {
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; char *v = eq + 1;
        size_t vn = strlen(v);
        while (vn > 0 && (v[vn-1]=='\n'||v[vn-1]=='\r')) v[--vn]=0;
        if (!strcmp(line, "dir")) snprintf(dir, dsz, "%s", v);
        else if (!strcmp(line, "tileset")) snprintf(set, ssz, "%s", v);
    }
    fclose(af);
}

static void publish_grid_from_png(const char *png, int force_tw, int force_th, int ohr_scale,
                                  const char *sprite_root, FILE *out) {
    int w, h, ch;
    unsigned char *px = stbi_load(png, &w, &h, &ch, 4);
    if (!px) return;
    unsigned char *use = px;
    int uw = w, uh = h;
    unsigned char *scaled = NULL;
    if (ohr_scale && (w != 320 || h != 200)) {
        uw = 320; uh = 200;
        scaled = (unsigned char *)malloc((size_t)uw * uh * 4);
        if (scaled) {
            for (int y = 0; y < uh; y++)
                for (int x = 0; x < uw; x++) {
                    int sx = x * w / uw, sy = y * h / uh;
                    if (sx >= w) sx = w - 1;
                    if (sy >= h) sy = h - 1;
                    memcpy(&scaled[((size_t)y * uw + x) * 4],
                           &px[((size_t)sy * w + sx) * 4], 4);
                }
            use = scaled;
        }
    }
    int tw = force_tw > 0 ? force_tw : guess_cell_px(uw, uh);
    int th = force_th > 0 ? force_th : tw;
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    int cols = uw / tw, rows = uh / th;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    int n = 0;
    for (int r = 0; r < rows && n < MAX_TILES; r++) {
        for (int c = 0; c < cols && n < MAX_TILES; c++) {
            n++;
            char dir[PATH_BUF], csv[PATH_BUF];
            snprintf(dir, sizeof(dir), "%s/%03d", sprite_root, n);
            snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
            struct stat st;
            if (stat(csv, &st) != 0) {
                char mk[PATH_BUF * 2];
                snprintf(mk, sizeof(mk), "mkdir -p '%s'", dir);
                int ign = system(mk); (void)ign;
                FILE *f = fopen(csv, "w");
                if (f) {
                    fprintf(f, "# resolution=%d\n# scale=1.0\n# transform=0,0,0\nr,g,b,a\n", RMMV_TILE_PX);
                    for (int y = 0; y < RMMV_TILE_PX; y++) {
                        for (int x = 0; x < RMMV_TILE_PX; x++) {
                            int ax = c * tw + x * tw / RMMV_TILE_PX;
                            int ay = r * th + y * th / RMMV_TILE_PX;
                            if (ax >= uw) ax = uw - 1;
                            if (ay >= uh) ay = uh - 1;
                            const unsigned char *p = &use[((size_t)ay * uw + ax) * 4];
                            fprintf(f, "%d,%d,%d,%d\n", p[0], p[1], p[2], p[3]);
                        }
                    }
                    fclose(f);
                }
            }
            char lab[64];
            snprintf(lab, sizeof(lab), "%d_%d", r, c);
            fprintf(out, "%s\t%s\t%s\n", lab, lab, dir);
        }
    }
    if (scaled) free(scaled);
    stbi_image_free(px);
}

static int png_or_bmp(const char *name) {
    size_t len = strlen(name);
    if (len < 5) return 0;
    const char *e = name + len - 4;
    return !strcasecmp(e, ".png") || !strcasecmp(e, ".bmp") || !strcasecmp(e, ".jpg")
        || (len >= 5 && !strcasecmp(name + len - 5, ".jpeg"));
}

static void publish_tiled(void) {
    char root[PATH_BUF];
    if (!pdl_img_root_named("TILED-ASSET-SOURCE-LOCATION.pdl", root, sizeof(root))) return;
    char active_dir[64], active_set[64];
    read_active_kv("tiled", active_dir, sizeof(active_dir), active_set, sizeof(active_set), "Tilesets", "Grass");
    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/tiled_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (!opt) return;
    fprintf(opt, "ACTIVE_DIR|%s\nACTIVE_TILESET|%s\nACTIVE_CATEGORY|%s\n", active_dir, active_set, active_set);
    DIR *pd = opendir(root);
    char dirs[32][64]; int nd = 0;
    if (pd) {
        struct dirent *de;
        while ((de = readdir(pd)) != NULL && nd < 32) {
            if (de->d_name[0] == '.') continue;
            char pth[PATH_BUF];
            snprintf(pth, sizeof(pth), "%s/%s", root, de->d_name);
            struct stat st;
            if (stat(pth, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(dirs[nd++], 64, "%s", de->d_name);
        }
        closedir(pd);
    }
    int dir_ok = 0;
    for (int i = 0; i < nd; i++) {
        fprintf(opt, "DIR|%s|%s\n", dirs[i], dirs[i]);
        if (!strcmp(dirs[i], active_dir)) dir_ok = 1;
    }
    if (!dir_ok && nd > 0) snprintf(active_dir, sizeof(active_dir), "%s", dirs[0]);
    char folder[PATH_BUF];
    snprintf(folder, sizeof(folder), "%s/%s", root, active_dir);
    DIR *sd = opendir(folder);
    char sets[64][64]; int ns = 0;
    if (sd) {
        struct dirent *de;
        while ((de = readdir(sd)) != NULL && ns < 64) {
            if (!png_or_bmp(de->d_name)) continue;
            char stem[64];
            snprintf(stem, sizeof(stem), "%s", de->d_name);
            char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
            snprintf(sets[ns++], 64, "%s", stem);
        }
        closedir(sd);
    }
    int set_ok = 0;
    for (int i = 0; i < ns; i++) {
        fprintf(opt, "TILESET|%s|%s\n", sets[i], sets[i]);
        if (!strcmp(sets[i], active_set)) set_ok = 1;
    }
    if (!set_ok && ns > 0) snprintf(active_set, sizeof(active_set), "%s", sets[0]);
    fclose(opt);
    rename(opt_tmp, opt_path);
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;
    char png[PATH_BUF];
    snprintf(png, sizeof(png), "%s/%s/%s.png", folder, "", active_set);
    /* try png then bmp */
    char try[PATH_BUF];
    const char *exts[] = { ".png", ".bmp", ".jpg", NULL };
    png[0] = 0;
    for (int i = 0; exts[i]; i++) {
        snprintf(try, sizeof(try), "%s/%s%s", folder, active_set, exts[i]);
        if (access(try, R_OK) == 0) { snprintf(png, sizeof(png), "%s", try); break; }
    }
    char sprite_root[PATH_BUF];
    snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/tiled/%s/%s", g_package_dir, active_dir, active_set);
    if (png[0]) publish_grid_from_png(png, 0, 0, 0, sprite_root, out);
    fclose(out);
    rename(tmp_path, g_state_path);
}

static void publish_ohr(void) {
    char root[PATH_BUF];
    if (!pdl_img_root_named("OHRRPGCE-ASSET-SOURCE-LOCATION.pdl", root, sizeof(root))) return;
    char active_dir[64], active_set[64];
    read_active_kv("ohrrpgce", active_dir, sizeof(active_dir), active_set, sizeof(active_set), "stock", "320px-TS02_town.bmp");
    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/ohrrpgce_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (!opt) return;
    fprintf(opt, "ACTIVE_DIR|stock\nACTIVE_TILESET|%s\nACTIVE_CATEGORY|%s\n", active_set, active_set);
    fprintf(opt, "DIR|stock|stock\n");
    DIR *sd = opendir(root);
    char sets[64][128]; int ns = 0;
    if (sd) {
        struct dirent *de;
        while ((de = readdir(sd)) != NULL && ns < 64) {
            if (!png_or_bmp(de->d_name)) continue;
            char stem[128];
            snprintf(stem, sizeof(stem), "%s", de->d_name);
            char *dot = strrchr(stem, '.');
            /* keep .bmp in 320px-foo.bmp.png stems: strip last ext only */
            if (dot) *dot = 0;
            snprintf(sets[ns++], 128, "%s", stem);
        }
        closedir(sd);
    }
    int set_ok = 0;
    for (int i = 0; i < ns; i++) {
        fprintf(opt, "TILESET|%s|%s\n", sets[i], sets[i]);
        if (!strcmp(sets[i], active_set)) set_ok = 1;
    }
    if (!set_ok && ns > 0) snprintf(active_set, sizeof(active_set), "%s", sets[0]);
    fclose(opt);
    rename(opt_tmp, opt_path);
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;
    char png[PATH_BUF] = "";
    char try[PATH_BUF];
    const char *exts[] = { ".png", ".bmp", NULL };
    for (int i = 0; exts[i]; i++) {
        snprintf(try, sizeof(try), "%s/%s%s", root, active_set, exts[i]);
        if (access(try, R_OK) == 0) { snprintf(png, sizeof(png), "%s", try); break; }
    }
    /* 320px-foo.bmp.png */
    if (!png[0]) {
        snprintf(try, sizeof(try), "%s/%s.png", root, active_set);
        if (access(try, R_OK) == 0) snprintf(png, sizeof(png), "%s", try);
    }
    char sprite_root[PATH_BUF];
    snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/ohr/%s", g_package_dir, active_set);
    if (png[0]) publish_grid_from_png(png, 20, 20, 1, sprite_root, out);
    fclose(out);
    rename(tmp_path, g_state_path);
}

static void mypal_maybe_import(void) {
    char uip[PATH_BUF];
    snprintf(uip, sizeof(uip), "%s/&.widgits/file-explorer/file_explorer_ui.txt", g_house_root);
    FILE *f = fopen(uip, "r");
    if (!f) return;
    char line[PATH_BUF], result[PATH_BUF] = "";
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "result=", 7)) {
            snprintf(result, sizeof(result), "%s", line + 7);
            char *nl = strpbrk(result, "\r\n"); if (nl) *nl = 0;
        }
    }
    fclose(f);
    if (!result[0] || access(result, R_OK) != 0) return;
    static char last[PATH_BUF];
    if (!strcmp(last, result)) return;
    snprintf(last, sizeof(last), "%s", result);
    const char *base = strrchr(result, '/');
    base = base ? base + 1 : result;
    char stem[128];
    snprintf(stem, sizeof(stem), "%s", base);
    char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
    char dest_dir[PATH_BUF];
    snprintf(dest_dir, sizeof(dest_dir), "%s/my-library/%s", g_package_dir, stem);
    char mk[PATH_BUF * 2];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", dest_dir);
    int ign = system(mk); (void)ign;
    char dest[PATH_BUF];
    snprintf(dest, sizeof(dest), "%s/%s", dest_dir, base);
    snprintf(mk, sizeof(mk), "cp -f '%s' '%s'", result, dest);
    ign = system(mk); (void)ign;
    char meta[PATH_BUF];
    snprintf(meta, sizeof(meta), "%s/meta.pdl", dest_dir);
    FILE *mf = fopen(meta, "w");
    if (mf) {
        fprintf(mf, "style=other\nsource_path=%s\nlabel=%s\n", result, stem);
        fclose(mf);
    }
}

static void publish_mypal(void) {
    mypal_maybe_import();
    char lib[PATH_BUF];
    snprintf(lib, sizeof(lib), "%s/my-library", g_package_dir);
    char mk[PATH_BUF * 2];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", lib);
    int ign = system(mk); (void)ign;
    char active_dir[64], active_set[64];
    read_active_kv("my-palettes", active_dir, sizeof(active_dir), active_set, sizeof(active_set), "", "");
    char opt_path[PATH_BUF], opt_tmp[PATH_BUF];
    snprintf(opt_path, sizeof(opt_path), "%s/my-palettes_options.txt", g_package_dir);
    snprintf(opt_tmp, sizeof(opt_tmp), "%s.tmp", opt_path);
    FILE *opt = fopen(opt_tmp, "w");
    if (!opt) return;
    DIR *pd = opendir(lib);
    char dirs[64][64]; int nd = 0;
    if (pd) {
        struct dirent *de;
        while ((de = readdir(pd)) != NULL && nd < 64) {
            if (de->d_name[0] == '.') continue;
            char pth[PATH_BUF];
            snprintf(pth, sizeof(pth), "%s/%s", lib, de->d_name);
            struct stat st;
            if (stat(pth, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            snprintf(dirs[nd++], 64, "%s", de->d_name);
        }
        closedir(pd);
    }
    if (!active_dir[0] && nd > 0) snprintf(active_dir, sizeof(active_dir), "%s", dirs[0]);
    fprintf(opt, "ACTIVE_DIR|%s\nACTIVE_TILESET|%s\nACTIVE_CATEGORY|%s\n", active_dir, active_set[0]?active_set:active_dir, active_dir);
    for (int i = 0; i < nd; i++) fprintf(opt, "DIR|%s|%s\n", dirs[i], dirs[i]);
    char folder[PATH_BUF];
    snprintf(folder, sizeof(folder), "%s/%s", lib, active_dir);
    DIR *sd = opendir(folder);
    char sets[32][64]; int ns = 0;
    if (sd) {
        struct dirent *de;
        while ((de = readdir(sd)) != NULL && ns < 32) {
            if (!png_or_bmp(de->d_name)) continue;
            char stem[64];
            snprintf(stem, sizeof(stem), "%s", de->d_name);
            char *dot = strrchr(stem, '.'); if (dot) *dot = 0;
            snprintf(sets[ns++], 64, "%s", stem);
        }
        closedir(sd);
    }
    int set_ok = 0;
    for (int i = 0; i < ns; i++) {
        fprintf(opt, "TILESET|%s|%s\n", sets[i], sets[i]);
        if (!strcmp(sets[i], active_set)) set_ok = 1;
    }
    if (!set_ok && ns > 0) snprintf(active_set, sizeof(active_set), "%s", sets[0]);
    fclose(opt);
    rename(opt_tmp, opt_path);
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;
    char png[PATH_BUF] = "", try[PATH_BUF];
    const char *exts[] = { ".png", ".bmp", ".jpg", NULL };
    for (int i = 0; exts[i]; i++) {
        snprintf(try, sizeof(try), "%s/%s%s", folder, active_set, exts[i]);
        if (access(try, R_OK) == 0) { snprintf(png, sizeof(png), "%s", try); break; }
    }
    char sprite_root[PATH_BUF];
    snprintf(sprite_root, sizeof(sprite_root), "%s/sprites/mypal/%s/%s", g_package_dir, active_dir, active_set);
    if (png[0]) publish_grid_from_png(png, 0, 0, 0, sprite_root, out);
    fclose(out);
    rename(tmp_path, g_state_path);
}

static void publish(void) {
    if (strcmp(g_category, "debug") == 0) { publish_debug(); return; }
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
    if (strcmp(g_category, "piececraft") == 0) {
        static char s_last_pc[256] = "";
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/piececraft_active.txt", g_package_dir);
        char active_content[256] = "";
        FILE *af_check = fopen(active_path, "r");
        if (af_check) {
            size_t n = fread(active_content, 1, sizeof(active_content) - 1, af_check);
            active_content[n] = '\0';
            fclose(af_check);
        }
        int changed = strcmp(active_content, s_last_pc) != 0;
        snprintf(s_last_pc, sizeof(s_last_pc), "%s", active_content);
        if (!changed && g_source_mtime != 0) return;
        g_source_mtime = 1;
        publish_piececraft();
        return;
    }
    if (strcmp(g_category, "cdda") == 0) {
        static char s_last_cdda[256] = "";
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/cdda_active.txt", g_package_dir);
        char active_content[256] = "";
        FILE *af_check = fopen(active_path, "r");
        if (af_check) {
            size_t n = fread(active_content, 1, sizeof(active_content) - 1, af_check);
            active_content[n] = '\0';
            fclose(af_check);
        }
        int changed = strcmp(active_content, s_last_cdda) != 0;
        snprintf(s_last_cdda, sizeof(s_last_cdda), "%s", active_content);
        if (!changed && g_source_mtime != 0) return;
        g_source_mtime = 1;
        publish_cdda();
        return;
    }
    if (strcmp(g_category, "tiled") == 0 || strcmp(g_category, "ohrrpgce") == 0
        || strcmp(g_category, "my-palettes") == 0) {
        static char s_last_atlas[256] = "";
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/%s_active.txt", g_package_dir, g_category);
        char active_content[256] = "";
        FILE *af_check = fopen(active_path, "r");
        if (af_check) {
            size_t n = fread(active_content, 1, sizeof(active_content) - 1, af_check);
            active_content[n] = '\0';
            fclose(af_check);
        }
        int changed = strcmp(active_content, s_last_atlas) != 0;
        snprintf(s_last_atlas, sizeof(s_last_atlas), "%s", active_content);
        if (!changed && g_source_mtime != 0 && strcmp(g_category, "my-palettes") != 0) return;
        g_source_mtime = 1;
        if (!strcmp(g_category, "tiled")) publish_tiled();
        else if (!strcmp(g_category, "ohrrpgce")) publish_ohr();
        else publish_mypal();
        return;
    }
    if (strcmp(g_category, "emojis") == 0) {
        static char s_last_emo[256] = "";
        char active_path[PATH_BUF];
        snprintf(active_path, sizeof(active_path), "%s/emojis_active.txt", g_package_dir);
        char active_content[256] = "";
        FILE *af_check = fopen(active_path, "r");
        if (af_check) {
            size_t n = fread(active_content, 1, sizeof(active_content) - 1, af_check);
            active_content[n] = '\0';
            fclose(af_check);
        }
        int changed = strcmp(active_content, s_last_emo) != 0;
        snprintf(s_last_emo, sizeof(s_last_emo), "%s", active_content);
        if (!changed && g_source_mtime != 0) return;
        g_source_mtime = 1;
        publish_emojis();
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
    } else if (strcmp(g_category, "rmmv") == 0 || strcmp(g_category, "piececraft") == 0 || strcmp(g_category, "cdda") == 0
            || strcmp(g_category, "tiled") == 0 || strcmp(g_category, "ohrrpgce") == 0 || strcmp(g_category, "my-palettes") == 0) {
        g_source_path[0] = '\0';
    } else if (strcmp(g_category, "emojis") == 0) {
        char pdl[PATH_BUF];
        asset_source_pdl_path(g_house_root, "UNICODE-EMOJI-SOURCE-LOCATION.pdl", pdl, sizeof(pdl));
        g_source_path[0] = '\0';
        FILE *pf = fopen(pdl, "r");
        if (pf) {
            char line[PATH_BUF];
            while (fgets(line, sizeof(line), pf)) {
                if (strncmp(line, "SOURCE", 6) != 0) continue;
                if (!strstr(line, "source_file")) continue;
                char *bar = strrchr(line, '|');
                if (!bar) continue;
                char *v = bar + 1;
                while (*v == ' ' || *v == '\t') v++;
                size_t n = strlen(v);
                while (n > 0 && (v[n-1]=='\n'||v[n-1]=='\r'||v[n-1]==' ')) v[--n]=0;
                snprintf(g_source_path, sizeof(g_source_path), "%s", v);
                break;
            }
            fclose(pf);
        }
        if (!g_source_path[0])
            snprintf(g_source_path, sizeof(g_source_path), "%s/#.ref/menu/palletes/emoji-pallet-00.00.txt", g_house_root);
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
        usleep(strcmp(g_category, "debug") == 0 ? 300000 : (strcmp(g_category, "rmmv") == 0 || strcmp(g_category, "piececraft") == 0 || strcmp(g_category, "cdda") == 0 || strcmp(g_category, "emojis") == 0 || strcmp(g_category, "tiled") == 0 || strcmp(g_category, "ohrrpgce") == 0 || strcmp(g_category, "my-palettes") == 0) ? 100000 : 1000000);
    }
    return 0;
}
