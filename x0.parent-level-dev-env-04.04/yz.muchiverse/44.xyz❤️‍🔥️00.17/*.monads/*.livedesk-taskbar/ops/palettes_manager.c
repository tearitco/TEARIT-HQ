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
#include <sys/stat.h>
#include <unistd.h>

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

static void publish(void) {
    struct stat st;
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
    } else {
        snprintf(g_source_path, sizeof(g_source_path), "%s/#.ref/menu/palletes/emoji-pallet-00.00.txt", g_house_root);
    }
    snprintf(g_state_path, sizeof(g_state_path), "%s/palettes-%s_state.txt", g_package_dir, g_category);
    snprintf(g_sprite_root, sizeof(g_sprite_root), "%s/sprites/emoji", g_package_dir);
    find_emoji_tools();

    for (;;) {
        publish();
        usleep(1000000); /* 1s poll - reference data changes far less often than db-hq's own 400ms need */
    }
    return 0;
}
