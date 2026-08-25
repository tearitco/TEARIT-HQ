#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../libraries/stb_image.h"
#include "../../../pieces/chtpm/ops/lib/tpmos_live_frame_cache.c"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GLYPH_W 8
#define GLYPH_H 16
#define COLS 128
#define ROWS 40
#define WIDTH (COLS * GLYPH_W)
#define HEIGHT (ROWS * GLYPH_H)
#define MAX_OBJECTS 2048
#define MAX_LABEL 256

#define EMOJI_CACHE_SIZE 256

/* bitmap is emoji_glyph_size*emoji_glyph_size RGBA pixels (4 bytes each,
 * r,g,b,a in that order) -- real per-pixel color, loaded from a CSV
 * asset on disk, not rendered live. See wraimoji-j12-root-cause-and-
 * fix-plan.txt: live FT_Load_Glyph in this daemon's hot path was the
 * root cause of emoji rendering as silent blank space in GL (wrong
 * FreeType API for a fixed-size color-bitmap font). The CSV assets are
 * produced offline/on-demand by pieces/system/emoji_extract/'s two Ops
 * (ported from the proven #.emoji-studio-501.02.05t/ reference), which
 * DO use the correct FreeType calls -- but only there, never here. */
typedef struct {
    uint32_t codepoint;
    unsigned char* bitmap;
    int cached;
    time_t last_used;
} EmojiCacheEntry;

#define WRAITH_UI_STATE "projects/wraith-alpha/session/desktop_ui_state.txt"
#define SEMANTIC_META_PATH "pieces/display/current_frame.meta.pdl"
#define SEMANTIC_OBJECTS_PATH "pieces/display/current_frame.objects.pdl"
#define WRAITH_FRAME_SOURCE "projects/wraith-alpha/session/rgb/current_frame.rgba32"
#define RGB_RECEIPT_PATH "projects/wraith-alpha/session/rgb/current_frame.receipt.pdl"
#define WEBCAM_FRAME_SOURCE_REF "projects/wraith-alpha/wraith-projects/web-cam/session/current_frame.png"
#define WEBCAM_FRAME_CACHE_KEY "projects/wraith-alpha/wraith-projects/web-cam/session/current_frame"

typedef struct {
    char tag[32];
    char role[32];
    char parent_id[64];
    char container_id[64];
    char source_ref[256];
    char target_surface[80];
    char ancestor_chain[256];
    char clip_chain[256];
    int nav;
    int nav_selected;
    char nav_selector_glyph[8];
    int x;
    int y;
    int w;
    int h;
    int z;
    int focused;
    unsigned char fg[3];
    unsigned char bg[3];
    unsigned char border[3];
    char label[MAX_LABEL];
    char label_core[MAX_LABEL];
    char action[256];
} FrameObject;

static unsigned char glyphs[128][GLYPH_W * GLYPH_H];
static int g_presenter_ascii_mode = 0;

static EmojiCacheEntry emoji_cache[EMOJI_CACHE_SIZE];
static int emoji_enabled = 1;
static int emoji_glyph_size = 16;

typedef struct {
    int valid;
    char project_id[128];
    char source_layout[256];
    char focused_object_id[128];
    char focused_object_dom_id[128];
    int mouse_x;
    int mouse_y;
    int mouse_hit_offset_x;
    int mouse_hit_offset_y;
    int mouse_cursor_visual_uses_offset;
} SemanticSourceInfo;

static void color_to_hex(const unsigned char rgb[3], char out[8]) {
    if (!out) {
        return;
    }
    snprintf(out, 8, "#%02X%02X%02X", rgb[0], rgb[1], rgb[2]);
}

static int is_webcam_frame_source(const char *source_ref) {
    size_t src_len;
    size_t suffix_len;
    if (!source_ref || !source_ref[0]) return 0;
    if (strcmp(source_ref, WEBCAM_FRAME_SOURCE_REF) == 0) return 1;
    if (!strstr(source_ref, "/projects/wraith-alpha/wraith-projects/web-cam/")) return 0;
    src_len = strlen(source_ref);
    suffix_len = strlen("/session/current_frame.png");
    if (src_len < suffix_len) return 0;
    return strcmp(source_ref + (src_len - suffix_len), "/session/current_frame.png") == 0;
}

static unsigned long long checksum_buffer(const unsigned char *buffer, size_t len) {
    unsigned long long hash = 1469598103934665603ULL;
    size_t i;

    if (!buffer) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        hash ^= (unsigned long long)buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static long file_mtime_epoch(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_mtime;
}

static long file_size_bytes(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

static void clear_buffer(unsigned char *buffer, unsigned char r, unsigned char g, unsigned char b) {
    int i;
    for (i = 0; i < WIDTH * HEIGHT * 4; i += 4) {
        buffer[i] = r;
        buffer[i + 1] = g;
        buffer[i + 2] = b;
        buffer[i + 3] = 255;
    }
}

static int parse_hex_color(const char *value, unsigned char rgb[3]) {
    unsigned int r, g, b;
    if (!value || value[0] != '#') return 0;
    if (sscanf(value + 1, "%02x%02x%02x", &r, &g, &b) != 3) return 0;
    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;
    return 1;
}

static void load_emoji_config(void) {
    FILE *f = fopen("pieces/config/wraith_debug.conf", "r");
    if (!f) {
        emoji_glyph_size = 16;
        return;
    }
    char line[1024];
    int in_emoji_section = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "[emoji_rendering]")) {
            in_emoji_section = 1;
            continue;
        }
        if (line[0] == '[') { in_emoji_section = 0; continue; }
        if (!in_emoji_section) continue;
        if (strncmp(line, "enabled=", 8) == 0) {
            emoji_enabled = atoi(line + 8);
        } else if (strncmp(line, "glyph_size=", 11) == 0) {
            emoji_glyph_size = atoi(line + 11);
        }
        /* font_path= is intentionally not read here -- this daemon no
           longer opens the font itself (see EmojiCacheEntry's comment
           above); pieces/system/emoji_extract/emoji_gen_atlas.c reads
           that same key independently, only where live FreeType glyph
           rendering actually still happens. */
    }
    fclose(f);
}

static void init_emoji_renderer(void) {
    if (!emoji_enabled) return;
    memset(emoji_cache, 0, sizeof(emoji_cache));
}

static uint32_t decode_utf8_codepoint(const char *str, int *len) {
    unsigned char c = (unsigned char)str[0];
    uint32_t cp = 0;
    if (c < 0x80) {
        *len = 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        cp = ((c & 0x1F) << 6) | ((unsigned char)str[1] & 0x3F);
        *len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp = ((c & 0x0F) << 12) | (((unsigned char)str[1] & 0x3F) << 6) | ((unsigned char)str[2] & 0x3F);
        *len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        cp = ((c & 0x07) << 18) | (((unsigned char)str[1] & 0x3F) << 12) | (((unsigned char)str[2] & 0x3F) << 6) | ((unsigned char)str[3] & 0x3F);
        *len = 4;
    } else {
        *len = 1;
        return '?';
    }
    return cp;
}

static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Re-encodes a decoded codepoint back to UTF-8 (lossless round-trip)
 * so it can be passed as a single argv string to emoji_gen_atlas.+x,
 * which takes the emoji symbol itself, not a numeric codepoint. */
static int encode_utf8(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = '\0';
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        out[2] = '\0';
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        out[3] = '\0';
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    out[4] = '\0';
    return 4;
}

static void get_emoji_asset_path(uint32_t codepoint, char *out, size_t out_sz) {
    snprintf(out, out_sz, "projects/wraith-alpha/assets/emoji/%X/voxels_%d.csv",
             (unsigned int)codepoint, emoji_glyph_size);
}

/* Runs the two pieces/system/emoji_extract/ Ops in sequence to produce
 * this codepoint's CSV asset -- mirrors emoji-studio_host.c's own
 * generate_voxel_csv() fork+execl+waitpid shape exactly (the proven
 * reference this whole approach is ported from). Happens at most once
 * per distinct codepoint, ever -- load_emoji_bitmap_from_disk() only
 * calls this when the CSV doesn't already exist on disk. */
static void generate_emoji_asset(uint32_t codepoint) {
    char dir_path[512], png_path[600], csv_path[600];
    char emoji_utf8[8], res_str[16];
    pid_t pid;
    int status;

    snprintf(dir_path, sizeof(dir_path), "projects/wraith-alpha/assets/emoji/%X",
             (unsigned int)codepoint);
    mkdir("projects/wraith-alpha/assets", 0777);
    mkdir("projects/wraith-alpha/assets/emoji", 0777);
    mkdir(dir_path, 0777);

    snprintf(png_path, sizeof(png_path), "%s/atlas.png", dir_path);
    get_emoji_asset_path(codepoint, csv_path, sizeof(csv_path));
    encode_utf8(codepoint, emoji_utf8);
    snprintf(res_str, sizeof(res_str), "%d", emoji_glyph_size);

    pid = fork();
    if (pid == 0) {
        execl("pieces/system/emoji_extract/+x/emoji_gen_atlas.+x",
              "emoji_gen_atlas.+x", emoji_utf8, png_path, NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
    }

    pid = fork();
    if (pid == 0) {
        execl("pieces/system/emoji_extract/+x/emoji_xtract.+x",
              "emoji_xtract.+x", png_path, "0", res_str, csv_path, NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
    }
}

/* Loads a codepoint's pre-extracted RGBA pixel data from its CSV asset,
 * generating it first via generate_emoji_asset() if this is the first
 * time this codepoint has ever been needed. Returns
 * emoji_glyph_size*emoji_glyph_size*4 bytes (r,g,b,a per pixel), or
 * NULL if extraction/parsing failed (font genuinely lacks this glyph,
 * or the Ops aren't built yet). */
static unsigned char* load_emoji_bitmap_from_disk(uint32_t codepoint) {
    char csv_path[600];
    struct stat st;
    FILE *f;
    char line[128];
    unsigned char *out;
    int idx = 0;
    int total = emoji_glyph_size * emoji_glyph_size;

    get_emoji_asset_path(codepoint, csv_path, sizeof(csv_path));
    if (stat(csv_path, &st) != 0) {
        generate_emoji_asset(codepoint);
    }

    f = fopen(csv_path, "r");
    if (!f) return NULL;

    out = malloc((size_t)total * 4);
    if (!out) {
        fclose(f);
        return NULL;
    }
    memset(out, 0, (size_t)total * 4);

    while (fgets(line, sizeof(line), f) && idx < total) {
        int r, g, b, a;
        if (line[0] == '#') continue;
        if (strstr(line, "r,g,b,a")) continue;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            out[idx * 4 + 0] = (unsigned char)r;
            out[idx * 4 + 1] = (unsigned char)g;
            out[idx * 4 + 2] = (unsigned char)b;
            out[idx * 4 + 3] = (unsigned char)a;
            idx++;
        }
    }
    fclose(f);
    return out;
}

static unsigned char* get_emoji_bitmap(uint32_t codepoint) {
    if (!emoji_enabled) return NULL;
    for (int i = 0; i < EMOJI_CACHE_SIZE; i++) {
        if (emoji_cache[i].cached && emoji_cache[i].codepoint == codepoint) {
            emoji_cache[i].last_used = time(NULL);
            return emoji_cache[i].bitmap;
        }
    }
    unsigned char *bitmap = load_emoji_bitmap_from_disk(codepoint);
    if (!bitmap) return NULL;
    int slot = -1;
    for (int i = 0; i < EMOJI_CACHE_SIZE; i++) {
        if (!emoji_cache[i].cached) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        time_t oldest = emoji_cache[0].last_used;
        slot = 0;
        for (int i = 1; i < EMOJI_CACHE_SIZE; i++) {
            if (emoji_cache[i].last_used < oldest) {
                oldest = emoji_cache[i].last_used;
                slot = i;
            }
        }
        free(emoji_cache[slot].bitmap);
    }
    emoji_cache[slot].codepoint = codepoint;
    emoji_cache[slot].bitmap = bitmap;
    emoji_cache[slot].cached = 1;
    emoji_cache[slot].last_used = time(NULL);
    return bitmap;
}

static void cleanup_emoji(void) {
    for (int i = 0; i < EMOJI_CACHE_SIZE; i++) {
        if (emoji_cache[i].bitmap) {
            free(emoji_cache[i].bitmap);
        }
    }
}

static void load_glyphs(void) {
    int i;
    memset(glyphs, 0, sizeof(glyphs));
    for (i = 32; i < 127; i++) {
        char path[1024];
        FILE *f;
        char line[64];
        int y = 0;
        snprintf(path, sizeof(path), "projects/wraith-alpha/assets/fonts/ascii/%d/glyph.txt", i);
        f = fopen(path, "r");
        if (!f) continue;
        while (fgets(line, sizeof(line), f) && y < GLYPH_H) {
            int x;
            for (x = 0; x < GLYPH_W && line[x] != '\0' && line[x] != '\n'; x++) {
                glyphs[i][y * GLYPH_W + x] = (line[x] == '#') ? 255 : 0;
            }
            y++;
        }
        fclose(f);
    }
}

static void blit_char(unsigned char *buffer, int col, int row, unsigned char c,
                      unsigned char r, unsigned char g, unsigned char b,
                      int cell_w, int cell_h) {
    int start_x;
    int start_y;
    int y, x;
    if (c > 127) return;
    if (cell_w <= 0) cell_w = GLYPH_W;
    if (cell_h <= 0) cell_h = GLYPH_H;
    start_x = col * cell_w;
    start_y = row * cell_h;
    for (y = 0; y < GLYPH_H; y++) {
        for (x = 0; x < GLYPH_W; x++) {
            int dx = start_x + x;
            int dy = start_y + y;
            int idx;
            if (dx >= WIDTH || dy >= HEIGHT) continue;
            if (!glyphs[c][y * GLYPH_W + x]) continue;
            idx = (dy * WIDTH + dx) * 4;
            buffer[idx] = r;
            buffer[idx + 1] = g;
            buffer[idx + 2] = b;
            buffer[idx + 3] = 255;
        }
    }
}

static void blit_codepoint(unsigned char *buffer, int col, int row, uint32_t codepoint,
                           const unsigned char rgb[3], int cell_w, int cell_h) {
    if (codepoint < 128 && codepoint >= 32) {
        blit_char(buffer, col, row, (unsigned char)codepoint, rgb[0], rgb[1], rgb[2], cell_w, cell_h);
    } else if (emoji_enabled && codepoint >= 128) {
        /* emoji_bitmap is real per-pixel RGBA (4 bytes/pixel), loaded
           from a pre-extracted CSV asset -- true color, not a
           silhouette tinted with the object's own fg color (rgb[]),
           per wraimoji-j12-root-cause-and-fix-plan.txt Part 3 Step 4's
           recommendation. rgb[] is still used for the '?' fallback
           below, unrelated to emoji color. */
        unsigned char *emoji_bitmap = get_emoji_bitmap(codepoint);
        if (emoji_bitmap) {
            int start_x = col * (cell_w ? cell_w : GLYPH_W);
            int start_y = row * (cell_h ? cell_h : GLYPH_H);
            /* Deliberately NOT clamped to cell_w/cell_h here (unlike
               blit_char()'s plain-ASCII path) -- cell_w/cell_h is a
               single monospace CHARACTER cell (GLYPH_W=8, GLYPH_H=16),
               narrower than emoji_glyph_size (16x16 by default config).
               Clamping to cell_w silently cropped every emoji to its
               left half -- confirmed live, 2026-07-12 screenshot showed
               exactly that. The emoji bitmap is drawn at its own full
               native size instead; it will visually extend into the
               following character's cell horizontally, which is fine
               for the common case (a space follows each emoji) but not
               generally collision-free against dense adjacent text --
               acceptable for Phase 1, flagged for later if it matters. */
            for (int y = 0; y < emoji_glyph_size; y++) {
                for (int x = 0; x < emoji_glyph_size; x++) {
                    int dx = start_x + x;
                    int dy = start_y + y;
                    int idx, src;
                    unsigned char alpha;
                    if (dx >= WIDTH || dy >= HEIGHT) continue;
                    src = (y * emoji_glyph_size + x) * 4;
                    alpha = emoji_bitmap[src + 3];
                    if (alpha < 16) continue; /* skip near-transparent pixels */
                    idx = (dy * WIDTH + dx) * 4;
                    buffer[idx] = emoji_bitmap[src];
                    buffer[idx + 1] = emoji_bitmap[src + 1];
                    buffer[idx + 2] = emoji_bitmap[src + 2];
                    buffer[idx + 3] = 255;
                }
            }
        } else {
            blit_char(buffer, col, row, '?', rgb[0], rgb[1], rgb[2], cell_w, cell_h);
        }
    }
}

static void blit_text(unsigned char *buffer, int col, int row, const char *text,
                      const unsigned char rgb[3], int max_cols, int cell_w, int cell_h) {
    int i = 0;
    int char_count = 0;
    if (!text) return;
    while (text[i] != '\0') {
        if (max_cols >= 0 && char_count >= max_cols) break;
        int byte_len;
        uint32_t cp = decode_utf8_codepoint(&text[i], &byte_len);
        blit_codepoint(buffer, col + char_count, row, cp, rgb, cell_w, cell_h);
        i += byte_len;
        char_count++;
    }
}

static void fill_rect_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                         const unsigned char rgb[3]);
static void draw_border_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                           const unsigned char rgb[3], int thickness);

static void draw_text_asset(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    FILE *f;
    char line[1024];
    int row = 0;
    int max_cols;
    int text_col;
    int text_row;
    int x0;
    int y0;
    int x1;
    int y1;

    x0 = obj->x * cell_w;
    y0 = obj->y * cell_h;
    x1 = (obj->x + obj->w) * cell_w;
    y1 = (obj->y + obj->h) * cell_h;
    fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);

    if (!obj->source_ref[0]) {
        return;
    }

    f = fopen(obj->source_ref, "r");
    if (!f) {
        return;
    }

    text_col = obj->x + 1;
    text_row = obj->y + 1;
    max_cols = obj->w - 2;
    if (max_cols < 0) max_cols = 0;

    while (row < obj->h - 2 && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        blit_text(buffer, text_col, text_row + row, line, obj->fg, max_cols, cell_w, cell_h);
        row++;
    }
    fclose(f);
}

static void build_display_label(const FrameObject *obj, char *out, size_t out_sz) {
    const char *core;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!obj) return;
    core = obj->label_core[0] != '\0' ? obj->label_core : obj->label;
    if (obj->nav > 0) {
        const char *glyph = obj->nav_selector_glyph[0] != '\0' ? obj->nav_selector_glyph : " ";
        snprintf(out, out_sz, "[%s] %d. [%s]", glyph, obj->nav, core);
        return;
    }
    snprintf(out, out_sz, "%s", core);
}

static void fill_rect_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                         const unsigned char rgb[3]) {
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > WIDTH) x1 = WIDTH;
    if (y1 > HEIGHT) y1 = HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            int idx = (y * WIDTH + x) * 4;
            buffer[idx] = rgb[0];
            buffer[idx + 1] = rgb[1];
            buffer[idx + 2] = rgb[2];
            buffer[idx + 3] = 255;
        }
    }
}

static void draw_border_px(unsigned char *buffer, int x0, int y0, int x1, int y1,
                           const unsigned char rgb[3], int thickness) {
    fill_rect_px(buffer, x0, y0, x1, y0 + thickness, rgb);
    fill_rect_px(buffer, x0, y1 - thickness, x1, y1, rgb);
    fill_rect_px(buffer, x0, y0, x0 + thickness, y1, rgb);
    fill_rect_px(buffer, x1 - thickness, y0, x1, y1, rgb);
}

static int footer_row_y_for_role(const FrameObject *obj, int cell_h) {
    int visible_rows;
    int footer_top;
    const char *chain;

    if (!obj || !obj->role[0]) {
        return -1;
    }
    if (cell_h <= 0) {
        return -1;
    }
    visible_rows = HEIGHT / cell_h;
    footer_top = visible_rows - 4;
    if (footer_top < 0) {
        footer_top = 0;
    }
    chain = obj->ancestor_chain[0] ? obj->ancestor_chain : obj->container_id;
    if (strcmp(obj->source_ref, "semantic:taskbar_banner") == 0 ||
        (chain && strcmp(chain, "wraith_root>taskbar") == 0)) {
        return footer_top;
    }
    if (strcmp(obj->role, "footer_band") == 0 || strcmp(obj->role, "taskbar_row") == 0 ||
        (chain && strstr(chain, "taskbar_row")) || (chain && strstr(chain, "footer_band"))) {
        return footer_top;
    }
    if (strcmp(obj->role, "summary_row") == 0 || (chain && strstr(chain, "summary_row"))) {
        return footer_top + 1;
    }
    if (strcmp(obj->role, "debug_row") == 0 || (chain && strstr(chain, "debug_row"))) {
        return footer_top + 2;
    }
    return -1;
}

static int focused_outline_is_red(const FrameObject *obj) {
    if (!obj || !obj->focused) {
        return 0;
    }
    if (strcmp(obj->role, "window_title") == 0) {
        return 0;
    }
    if (strcmp(obj->tag, "text") == 0) {
        return 1;
    }
    if (strcmp(obj->role, "launcher_row") == 0) {
        return 1;
    }
    return 0;
}

static void shade_rgb(const unsigned char in[3], unsigned char out[3], int pct) {
    int i;
    for (i = 0; i < 3; i++) {
        int v = ((int)in[i] * pct) / 100;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        out[i] = (unsigned char)v;
    }
}

static int render_clip_visible(int x, int y);

static void put_px(unsigned char *buffer, int x, int y, const unsigned char rgb[3]) {
    int idx;
    if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) {
        return;
    }
    if (!render_clip_visible(x, y)) {
        return;
    }
    idx = (y * WIDTH + x) * 4;
    buffer[idx] = rgb[0];
    buffer[idx + 1] = rgb[1];
    buffer[idx + 2] = rgb[2];
    buffer[idx + 3] = 255;
}

static void draw_line_px(unsigned char *buffer, int x0, int y0, int x1, int y1, const unsigned char rgb[3], int thickness) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int half = thickness > 1 ? thickness / 2 : 0;

    while (1) {
        int ox, oy;
        for (oy = -half; oy <= half; oy++) {
            for (ox = -half; ox <= half; ox++) {
                put_px(buffer, x0 + ox, y0 + oy, rgb);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static int parse_cube_meta(const FrameObject *obj, int *rx, int *ry, int *rz, int *camera_mode, double *cam_x, double *cam_y, double *cam_z, double *pitch, double *yaw, double *roll) {
    const char *rot;
    const char *camera;
    if (rx) *rx = 15;
    if (ry) *ry = 35;
    if (rz) *rz = 0;
    if (camera_mode) *camera_mode = 4;
    if (cam_x) *cam_x = 0.0;
    if (cam_y) *cam_y = 0.0;
    if (cam_z) *cam_z = 0.0;
    if (pitch) *pitch = 15.0;
    if (yaw) *yaw = 0.0;
    if (roll) *roll = 0.0;
    if (!obj) return 0;
    rot = strstr(obj->label, "rot=");
    if (rot && rx && ry && rz) {
        sscanf(rot + 4, "%d,%d,%d", rx, ry, rz);
    }
    camera = strstr(obj->label, "camera=");
    if (camera && camera_mode && cam_x && cam_y && cam_z && pitch && yaw && roll) {
        sscanf(camera + 7, "%d,%lf,%lf,%lf,%lf,%lf,%lf", camera_mode, cam_x, cam_y, cam_z, pitch, yaw, roll);
    }
    return rot != NULL || camera != NULL;
}

static void draw_zslice_piece_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int w = x1 - x0;
    int h = y1 - y0;
    int cx = x0 + w / 2;
    int cy = y0 + h / 2;
    int size = (w < h ? w : h) / 2;
    int depth = size / 3;
    unsigned char top[3], side[3], front[3], outline[3] = {255, 209, 102};
    int rx, ry, rz, camera_mode;
    double cam_x, cam_y, cam_z, pitch, yaw, roll;
    double verts[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
    };
    int pts[8][2];
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    int i;

    if (size < 24) size = 24;
    shade_rgb(obj->fg, top, 120);
    shade_rgb(obj->fg, side, 75);
    shade_rgb(obj->fg, front, 95);

    parse_cube_meta(obj, &rx, &ry, &rz, &camera_mode, &cam_x, &cam_y, &cam_z, &pitch, &yaw, &roll);

    if (strstr(obj->label, "camera=") == NULL && strstr(obj->label, "rot=") == NULL) {
        fill_rect_px(buffer, cx - size / 2 + depth, cy - size / 2 - depth, cx + size / 2 + depth, cy + size / 2 - depth, top);
        fill_rect_px(buffer, cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, front);
        fill_rect_px(buffer, cx + size / 2, cy - size / 2, cx + size / 2 + depth, cy + size / 2 - depth, side);
        draw_border_px(buffer, cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, outline, 2);
        draw_border_px(buffer, cx - size / 2 + depth, cy - size / 2 - depth, cx + size / 2 + depth, cy + size / 2 - depth, outline, 2);
        draw_border_px(buffer, cx + size / 2, cy - size / 2, cx + size / 2 + depth, cy + size / 2 - depth, outline, 2);
        return;
    }

    for (i = 0; i < 8; i++) {
        double x = verts[i][0];
        double y = verts[i][1];
        double z = verts[i][2];
        double ax = ((double)rx + pitch) * M_PI / 180.0;
        double ay = ((double)ry + yaw) * M_PI / 180.0;
        double az = ((double)rz + roll) * M_PI / 180.0;
        double cyaw = cos(ay), syaw = sin(ay);
        double cpitch = cos(ax), spitch = sin(ax);
        double croll = cos(az), sroll = sin(az);
        double x1r = x * cyaw + z * syaw;
        double z1r = -x * syaw + z * cyaw;
        double y2r = y * cpitch - z1r * spitch;
        double z2r = y * spitch + z1r * cpitch;
        double x3r = x1r * croll - y2r * sroll;
        double y3r = x1r * sroll + y2r * croll;
        double focal = (camera_mode == 1) ? 1.8 : 2.4;
        double perspective = focal / (focal + z2r + cam_z + 3.0);
        pts[i][0] = cx + (int)((x3r - cam_x) * (size * 0.45) * perspective);
        pts[i][1] = cy + (int)((y3r - cam_y) * (size * 0.45) * perspective);
    }
    fill_rect_px(buffer, x0 + 2, y0 + 2, x1 - 2, y1 - 2, obj->bg);
    for (i = 0; i < 12; i++) {
        int a = edges[i][0];
        int b = edges[i][1];
        draw_line_px(buffer, pts[a][0], pts[a][1], pts[b][0], pts[b][1], outline, 2);
    }
    for (i = 0; i < 8; i++) {
        fill_rect_px(buffer, pts[i][0] - 2, pts[i][1] - 2, pts[i][0] + 3, pts[i][1] + 3, obj->fg);
    }
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

/* Frame-scoped camera yaw (turntable orbit) applied by project_world_point().
   Set once per frame at the top of draw_tile_zmap_preview_3d() from the
   scene camera= field, read by every projection call — kept as file
   statics rather than threaded through project_tile_top()/draw_box() and
   their ~10 call sites (single-threaded rasterizer, one frame at a time,
   so no re-entrancy concern). g_cam_yaw is radians; the pivot is the
   scene-center ground point the map spins around (mouse-drag = turntable,
   matching plugy3d's orbit-around-target free camera). See py3d-inspo.md. */
static double g_cam_yaw = 0.0;
static double g_cam_pivot_x = 0.0;
static double g_cam_pivot_z = 0.0;

/*
 * Per-frame depth buffer (Z-buffer), added 2026-07-03 to replace the old
 * "paint tiles far-to-near by original map row" ordering. That ordering
 * only happened to be correct when yaw=0 (row number directly correlated
 * with camera-relative depth then); once mouse-orbit made yaw nonzero,
 * a tile's actual on-screen depth no longer matched its row index, so
 * near geometry could get painted over by far geometry drawn later —
 * live-reported as objects "getting corrupted or going in the wrong
 * place" when orbiting. A real depth test makes paint order irrelevant
 * for opaque geometry (matches how Unreal/Godot describe hardware
 * Z-buffers vs. a painter's sort) — see py3d-inspo.md / ARCHITECTURE-
 * RGB-RENDERING.md.
 *
 * Scoped to one game_map surface's screen rect per render (allocated at
 * the top of draw_tile_zmap_preview_3d(), freed at the end) rather than
 * a full-frame buffer — the desktop chrome around it is flat 2D UI with
 * no depth concept. fill_quad_px() reads/writes this via file-scope
 * globals, same pattern as g_cam_yaw above (single-threaded rasterizer,
 * one frame at a time, no re-entrancy concern).
 */
static double *g_depth_buf = NULL;
static int g_depth_x0 = 0, g_depth_y0 = 0, g_depth_w = 0, g_depth_h = 0;

static void depth_buf_begin(int x0, int y0, int w, int h) {
    size_t n = (size_t)w * (size_t)h;
    free(g_depth_buf);
    g_depth_buf = (n > 0) ? malloc(sizeof(double) * n) : NULL;
    g_depth_x0 = x0;
    g_depth_y0 = y0;
    g_depth_w = w;
    g_depth_h = h;
    if (g_depth_buf) {
        size_t i;
        for (i = 0; i < n; i++) g_depth_buf[i] = 1e18; /* "empty" = infinitely far */
    }
}

static void depth_buf_end(void) {
    free(g_depth_buf);
    g_depth_buf = NULL;
    g_depth_x0 = g_depth_y0 = g_depth_w = g_depth_h = 0;
}

/* Reads pieces/config/wraith_debug.conf's `render_beyond_viewport=` key
 * (relative to CWD, which is the project root — wraith_rgb_daemon is
 * fork/exec'd from wraith-alpha_manager without a chdir, so it inherits
 * the same CWD; see wraith-alpha_manager.c's camera_reset_on_open_enabled()
 * for the same convention on the manager side, just with an absolute
 * g_project_root prefix that binary happens to already track). Default 1
 * (on) — matches the always-render-everywhere-even-offscreen behavior
 * the whole rasterizer had before the ray-march pivot, which is what
 * ground-grid lines and entity-marker boxes (draw_line_px()/put_px()/
 * fill_poly_px(), still unclipped by design) already do. Set to 0 to
 * strictly clip ALL drawing (grid, markers, ray-marched tiles alike) to
 * the game_map widget's own screen rect instead — e.g. for a clean
 * production capture, or once you're done using the overflow to eyeball
 * off-screen geometry while debugging. Re-read fresh on every call, not
 * cached, matching wraith_debug.conf's other toggles. */
static int render_beyond_viewport_enabled(void) {
    FILE *f = fopen("pieces/config/wraith_debug.conf", "r");
    char line[256];
    int enabled = 1;
    if (!f) return enabled;
    while (fgets(line, sizeof(line), f)) {
        char *eq;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "render_beyond_viewport") == 0) {
            enabled = atoi(eq + 1) != 0;
        }
    }
    fclose(f);
    return enabled;
}

/* Scoped render-clip rect: when active (render_beyond_viewport=0), every
 * pixel-writing primitive (put_px(), fill_poly_px()) must reject writes
 * outside this rect, so ground-grid lines/entity markers/ray-marched
 * tiles are all clipped consistently to the game_map widget's own screen
 * bounds — no exceptions, unlike the old implicit "whatever draw_line_px/
 * draw_box happens to project to" behavior. Scoped exactly like
 * g_depth_buf above: set at the top of draw_tile_zmap_preview_3d(),
 * cleared at the end, single-threaded/one-frame-at-a-time so no
 * re-entrancy concern. */
static int g_render_clip_active = 0;
static int g_clip_x0 = 0, g_clip_y0 = 0, g_clip_x1 = 0, g_clip_y1 = 0;

static void render_clip_begin(int x0, int y0, int x1, int y1) {
    g_render_clip_active = !render_beyond_viewport_enabled();
    g_clip_x0 = x0;
    g_clip_y0 = y0;
    g_clip_x1 = x1;
    g_clip_y1 = y1;
}

static void render_clip_end(void) {
    g_render_clip_active = 0;
}

static int render_clip_visible(int x, int y) {
    if (!g_render_clip_active) return 1;
    return x >= g_clip_x0 && x < g_clip_x1 && y >= g_clip_y0 && y < g_clip_y1;
}

/* The near-plane distance (in camera-relative depth units, same scale as
   z2 below) inside which geometry gets clipped rather than rendered —
   matches the old clamp threshold, now used as an actual clip plane
   instead of a value to clamp toward. See clip_poly_near()/
   draw_clipped_face() below for why this replaced whole-box rejection. */
#define NEAR_PLANE 0.5

/* One vertex in camera space: rotated by yaw, translated relative to the
   camera, rotated by pitch — everything project_world_point_ex() does
   EXCEPT the perspective divide. Clipping must happen in this space
   (before the divide), because a point behind the camera doesn't have a
   valid screen position at all; clipping after the divide is already too
   late. */
typedef struct {
    double x, y, z;
} ClipVert;

/* Shared first half of project_world_point_ex() — transforms a world
   point into camera space without perspective-dividing it. Factored out
   so draw_clipped_face() can clip in this space before any divide
   happens. */
static void world_to_camera_space(double wx, double wy, double wz,
                                   double cam_x, double cam_y, double cam_z,
                                   double pitch_deg,
                                   double *out_x, double *out_y, double *out_z) {
    double px = wx - g_cam_pivot_x;
    double pz = wz - g_cam_pivot_z;
    double cyaw = cos(g_cam_yaw), syaw = sin(g_cam_yaw);
    double wxr = g_cam_pivot_x + (px * cyaw - pz * syaw);
    double wzr = g_cam_pivot_z + (px * syaw + pz * cyaw);
    double rx = wxr - cam_x;
    double ry = wy - cam_y;
    double rz = wzr - cam_z;
    double ax = pitch_deg * M_PI / 180.0;
    double cpitch = cos(ax), spitch = sin(ax);
    *out_x = rx;
    *out_y = ry * cpitch - rz * spitch;
    *out_z = ry * spitch + rz * cpitch;
}

/*
 * Real camera-relative perspective projection for the 3D grid POC: yaw-rotate
 * the world point around the scene-center ground pivot (turntable orbit),
 * translate into camera space, rotate by pitch (downward tilt), then
 * perspective-divide by depth. This is a proper "camera positioned in world
 * space, looking at a ground plane" transform, distinct from
 * draw_zslice_piece_preview()'s single-object rotation (which rotates a
 * fixed-position cube in place rather than moving a camera through world
 * space) — a full tile grid needs actual camera translation to avoid
 * near-clip singularities and get correct near/far convergence. Verified
 * standalone against the actual piececraft-wraith tile_zmap canvas bounds
 * before integration; see x0.piececrafts/ARCHITECTURE-RGB-RENDERING.md.
 *
 * Still clamps (rather than clips) when too close — this is only used
 * directly for thin decorative lines (the ground wireframe grid, tile
 * gridline outlines) where a clamped point just makes a line briefly go
 * to a weird place, not the "giant distorted filled shape" bug. Filled
 * geometry (draw_box()'s faces) goes through the proper clip pipeline
 * below (world_to_camera_space() + clip_poly_near() + draw_clipped_face())
 * instead of this function, precisely to avoid that bug. See py3d-
 * inspo.md / ARCHITECTURE-RGB-RENDERING.md's 2026-07-03 frustum-clipping
 * note.
 */
static void project_world_point_ex(double wx, double wy, double wz,
                                    double cam_x, double cam_y, double cam_z,
                                    double pitch_deg, double focal,
                                    int screen_cx, int screen_cy, double scale,
                                    int *out_x, int *out_y, double *out_z2, int *out_near_clipped) {
    double rx, y2, z2, persp;
    int near_clipped;

    world_to_camera_space(wx, wy, wz, cam_x, cam_y, cam_z, pitch_deg, &rx, &y2, &z2);
    near_clipped = (z2 <= NEAR_PLANE);
    if (near_clipped) z2 = NEAR_PLANE;
    persp = focal / z2;
    *out_x = screen_cx + (int)(rx * scale * persp);
    *out_y = screen_cy - (int)(y2 * scale * persp);
    if (out_z2) *out_z2 = z2;
    if (out_near_clipped) *out_near_clipped = near_clipped;
}

/* Plain projection (screen coords only) — a thin wrapper over
   project_world_point_ex() for the many callers (ground grid, gridline
   outlines) that don't need depth or near-clip info. draw_box() calls
   the _ex form directly since it needs both, for the depth buffer and
   the "reject instead of distort" near-clip fix — see the 2026-07-03
   depth-buffer note on draw_box() below. */
static void project_world_point(double wx, double wy, double wz,
                                 double cam_x, double cam_y, double cam_z,
                                 double pitch_deg, double focal,
                                 int screen_cx, int screen_cy, double scale,
                                 int *out_x, int *out_y) {
    project_world_point_ex(wx, wy, wz, cam_x, cam_y, cam_z, pitch_deg, focal,
                            screen_cx, screen_cy, scale, out_x, out_y, NULL, NULL);
}


/*
 * Scanline fill of a convex polygon (3-8 vertices — clipping a quad
 * against a single plane can add at most one vertex) given projected
 * screen points (in order) and their per-vertex camera-relative depths
 * (pz[], the z2 value from world_to_camera_space()/project_world_point_ex()
 * — smaller is nearer). Depth is bilinearly interpolated the same way x
 * already was (via the edge-crossing parameter t), so every filled pixel
 * gets a real, correctly-interpolated depth, not just its parent face's
 * average. Was fill_quad_px() (fixed 4 vertices) until 2026-07-03, when
 * draw_box() switched to clipping faces against the near plane before
 * filling them (see clip_poly_near()/draw_clipped_face()) — a clipped
 * quad can have 3, 4, or 5 vertices, so this generalized to N. Behavior
 * for the everyday n=4/2-crossings case is unchanged from before.
 *
 * alpha=255 (opaque, the common case — walls, tiles, voxels): test
 * against the scoped depth buffer (see depth_buf_begin() above) and, if
 * nearer, write both color and depth. alpha<255 (translucent — glass/
 * water): test-only, alpha-blend into the existing color, and
 * deliberately do NOT write depth — a translucent surface must not
 * occlude anything else translucent behind it; draw order (far-to-near,
 * translucent pass only) does that job instead, exactly the "sorted
 * back-to-front" translucency model both Unreal and Godot fall back to
 * once alpha blending breaks straightforward Z-buffer logic.
 *
 * When no depth buffer is active (g_depth_buf NULL — e.g. 2D contexts
 * that never called depth_buf_begin()), this degrades to the original
 * depth-blind fill: every pixel just gets drawn/blended unconditionally.
 */
static void fill_poly_px(unsigned char *buffer, const int *px, const int *py,
                          const double *pz, int n, const unsigned char rgb[3], int alpha) {
    int miny, maxy, i, y;
    if (n < 3) return;
    miny = maxy = py[0];
    for (i = 1; i < n; i++) {
        if (py[i] < miny) miny = py[i];
        if (py[i] > maxy) maxy = py[i];
    }
    if (miny < 0) miny = 0;
    if (maxy > HEIGHT) maxy = HEIGHT;
    for (y = miny; y <= maxy; y++) {
        double xs[16], zs[16];
        int cross = 0;
        int pair;
        for (i = 0; i < n; i++) {
            int ax_ = px[i], ay_ = py[i];
            int bx_ = px[(i + 1) % n], by_ = py[(i + 1) % n];
            if ((ay_ <= y && by_ > y) || (by_ <= y && ay_ > y)) {
                double t = (double)(y - ay_) / (double)(by_ - ay_);
                if (cross < 16) {
                    xs[cross] = ax_ + t * (bx_ - ax_);
                    zs[cross] = pz[i] + t * (pz[(i + 1) % n] - pz[i]);
                    cross++;
                }
            }
        }
        if (cross >= 2) {
            int a2, b2;
            for (a2 = 1; a2 < cross; a2++) {
                double kx = xs[a2], kz = zs[a2];
                b2 = a2 - 1;
                while (b2 >= 0 && xs[b2] > kx) {
                    xs[b2 + 1] = xs[b2];
                    zs[b2 + 1] = zs[b2];
                    b2--;
                }
                xs[b2 + 1] = kx;
                zs[b2 + 1] = kz;
            }
            for (pair = 0; pair + 1 < cross; pair += 2) {
                int x, sx0, sx1;
                double sz0, sz1;
                sx0 = (int)xs[pair];
                sx1 = (int)xs[pair + 1] + 1;
                sz0 = zs[pair];
                sz1 = zs[pair + 1];
                if (sx0 < 0) sx0 = 0;
                if (sx1 > WIDTH) sx1 = WIDTH;
                for (x = sx0; x < sx1; x++) {
                    double t = (sx1 > sx0) ? (double)(x - sx0) / (double)(sx1 - sx0) : 0.0;
                    double z = sz0 + t * (sz1 - sz0);
                    int idx;

                    if (!render_clip_visible(x, y)) continue;

                    if (g_depth_buf && x >= g_depth_x0 && x < g_depth_x0 + g_depth_w &&
                        y >= g_depth_y0 && y < g_depth_y0 + g_depth_h) {
                        size_t di = (size_t)(y - g_depth_y0) * (size_t)g_depth_w + (size_t)(x - g_depth_x0);
                        if (z >= g_depth_buf[di]) continue; /* occluded by something nearer */
                        if (alpha >= 255) g_depth_buf[di] = z;
                    }

                    idx = (y * WIDTH + x) * 4;
                    if (alpha >= 255) {
                        buffer[idx] = rgb[0];
                        buffer[idx + 1] = rgb[1];
                        buffer[idx + 2] = rgb[2];
                        buffer[idx + 3] = 255;
                    } else {
                        int inv = 255 - alpha;
                        buffer[idx] = (unsigned char)((rgb[0] * alpha + buffer[idx] * inv) / 255);
                        buffer[idx + 1] = (unsigned char)((rgb[1] * alpha + buffer[idx + 1] * inv) / 255);
                        buffer[idx + 2] = (unsigned char)((rgb[2] * alpha + buffer[idx + 2] * inv) / 255);
                        buffer[idx + 3] = 255;
                    }
                }
            }
        }
    }
}

/*
 * Sutherland-Hodgman clip of a convex polygon against the near plane
 * (z >= NEAR_PLANE, in camera space — see world_to_camera_space()). This
 * is the actual "do what GL does" fix: instead of rejecting an entire box
 * because one corner crossed the near plane (the previous behavior — see
 * ARCHITECTURE-RGB-RENDERING.md's 2026-07-03 note), a face that's partly
 * in front of and partly behind the plane gets a NEW vertex inserted
 * exactly at the intersection, so only the genuinely-invisible portion is
 * discarded. A convex polygon clipped against one half-plane is always
 * still convex, and gains at most one vertex — a quad (4) can become a
 * pentagon (5), never more. `out` must have room for n_in+1 vertices.
 * Returns the output vertex count (0 if the whole face is behind the
 * plane — genuinely invisible, correctly drawn as nothing).
 */
static int clip_poly_near(const ClipVert *in, int n_in, ClipVert *out) {
    int n = 0;
    int i;
    for (i = 0; i < n_in; i++) {
        const ClipVert *cur = &in[i];
        const ClipVert *nxt = &in[(i + 1) % n_in];
        int cur_in = (cur->z >= NEAR_PLANE);
        int nxt_in = (nxt->z >= NEAR_PLANE);
        if (cur_in) {
            out[n++] = *cur;
        }
        if (cur_in != nxt_in) {
            double t = (NEAR_PLANE - cur->z) / (nxt->z - cur->z);
            out[n].x = cur->x + t * (nxt->x - cur->x);
            out[n].y = cur->y + t * (nxt->y - cur->y);
            out[n].z = NEAR_PLANE;
            n++;
        }
    }
    return n;
}

/*
 * Transforms one quad face (4 world-space corners, in winding order) into
 * camera space, clips it against the near plane, projects whatever
 * survives to screen space, and fills it. This is the per-face pipeline
 * draw_box() now uses for every face instead of building a fixed 4-vertex
 * screen-space quad directly — see clip_poly_near()'s doc comment for why.
 */
static void draw_clipped_face(unsigned char *buffer,
                               double p0x, double p0y, double p0z,
                               double p1x, double p1y, double p1z,
                               double p2x, double p2y, double p2z,
                               double p3x, double p3y, double p3z,
                               double cam_x, double cam_y, double cam_z, double pitch, double focal,
                               int screen_cx, int screen_cy, double scale,
                               const unsigned char rgb[3], int alpha) {
    ClipVert cs[4];
    ClipVert clipped[5];
    int screen_x[5], screen_y[5];
    double screen_z[5];
    int n, i;

    world_to_camera_space(p0x, p0y, p0z, cam_x, cam_y, cam_z, pitch, &cs[0].x, &cs[0].y, &cs[0].z);
    world_to_camera_space(p1x, p1y, p1z, cam_x, cam_y, cam_z, pitch, &cs[1].x, &cs[1].y, &cs[1].z);
    world_to_camera_space(p2x, p2y, p2z, cam_x, cam_y, cam_z, pitch, &cs[2].x, &cs[2].y, &cs[2].z);
    world_to_camera_space(p3x, p3y, p3z, cam_x, cam_y, cam_z, pitch, &cs[3].x, &cs[3].y, &cs[3].z);

    n = clip_poly_near(cs, 4, clipped);
    if (n < 3) return; /* fully behind the near plane -- genuinely nothing to draw */

    for (i = 0; i < n; i++) {
        double persp = focal / clipped[i].z; /* clipped[i].z is always >= NEAR_PLANE here */
        screen_x[i] = screen_cx + (int)(clipped[i].x * scale * persp);
        screen_y[i] = screen_cy - (int)(clipped[i].y * scale * persp);
        screen_z[i] = clipped[i].z;
    }
    fill_poly_px(buffer, screen_x, screen_y, screen_z, n, rgb, alpha);
}

/*
 * Per-tile-type dimensional metadata, read from
 * assets/tiles/registry.txt (glyph -> tile_id) and
 * assets/tiles/<tile_id>.tile.txt (rgb_top, rgb_side, extrude). Default
 * (tile_id not found, or file missing) is a full 1x1x1 cube in world
 * units — "everything is 10x10x10 unless a meta file says otherwise" per
 * ARCHITECTURE-RGB-RENDERING.md. This mirrors the resolution/scale header
 * convention already used by emoji-studio's voxel CSV assets
 * (# resolution=8 / # scale=1.0 / # transform=0,0,0), applied to tiles
 * instead of emoji voxel models.
 *
 * No more direction field (see py3d-inspo.md): every tile grows straight
 * up from the shared wy=0 ground datum, same as plugy3d-engine and the
 * original piececraft-3d reference — neither has a "which way does this
 * box grow" concept, and the old explicit extrude_dir=up|down field (with
 * "down" as the default for anything that didn't set it) was what made
 * floor tiles punch a visible pit instead of reading as ground.
 *
 * Walkable tiles (grass/floor) don't draw a per-tile box at all anymore —
 * the ground plane is one shared wireframe grid sized to the whole map
 * (drawn once in draw_tile_zmap_preview_3d(), matching plugy3d's
 * draw_grid()), not a per-cell "piece." `walkable` is reused as-is from
 * the existing .tile.txt convention rather than inventing a new
 * render-mode field for this.
 *
 * Optional `voxel_source=<path relative to project dir>` (added
 * 2026-07-03, see emoji-entity-feature-report.md): points at a
 * `voxels_N.csv` in the emoji-studio/emoji-studio-wraith convention
 * (`# resolution=N` header + flat `r,g,b,a` rows, alpha=occupancy,
 * RGB=material). When present, the tile renders as an NxN grid of small
 * colored sub-voxels filling its footprint instead of one uniform box —
 * "better receipts" per py3d-inspo.md's remedy plan item 3. Falls back to
 * the plain colored box when absent/unreadable, so this is purely
 * additive.
 */
typedef struct {
    unsigned char rgb_top[3];
    unsigned char rgb_side[3];
    double extrude;  /* height, always upward from wy=0. Irrelevant for
                         walkable tiles (they draw no box). Also used as
                         the uniform per-voxel column height when
                         voxel_source is set (matches emoji-studio POC's
                         "every visible pixel gets the same height"
                         bar-chart extrusion). */
    int walkable;    /* 1 = ground tile, no per-tile box drawn (the shared
                         map-wide wireframe grid covers it). 0 = solid
                         obstacle, drawn as a box using extrude. */
    char voxel_source[512]; /* absolute path to a voxels_N.csv, or empty. */
    int alpha;       /* 0-255, default 255 (opaque). Translucent tiles
                         (glass/water) set this < 255 via `alpha=` in
                         their .tile.txt — drawn in a separate pass after
                         all opaque geometry, depth-tested but not
                         depth-writing. See draw_box()'s 2026-07-03
                         depth-buffer note. */
} TileMeta;

static void derive_project_dir(const char *source_ref, char *out, size_t out_sz) {
    const char *marker = strstr(source_ref, "/maps/");
    size_t len;
    if (!marker) {
        snprintf(out, out_sz, ".");
        return;
    }
    len = (size_t)(marker - source_ref);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, source_ref, len);
    out[len] = '\0';
}

static void load_tile_meta(const char *project_dir, const char *registry_rel, char glyph, TileMeta *out) {
    char registry_path[512], tile_path[512], line[256], tile_id[64] = "";
    FILE *f;

    out->extrude = 1.0;
    out->walkable = 0;
    out->voxel_source[0] = '\0';
    out->alpha = 255;
    out->rgb_top[0] = 180; out->rgb_top[1] = 180; out->rgb_top[2] = 180;
    out->rgb_side[0] = 120; out->rgb_side[1] = 120; out->rgb_side[2] = 120;

    snprintf(registry_path, sizeof(registry_path), "%s/%s", project_dir, registry_rel);
    f = fopen(registry_path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            /* '#' is both the wall glyph and this file's comment marker —
               only treat a leading '#' as a comment if it's NOT the
               "#=<tile_id>" registry entry format (i.e. not immediately
               followed by '='). Caught by standalone test before this
               ever reached the daemon. */
            if ((line[0] == '#' && line[1] != '=') || !line[0]) continue;
            if (line[0] == glyph && line[1] == '=') {
                snprintf(tile_id, sizeof(tile_id), "%s", line + 2);
                break;
            }
        }
        fclose(f);
    }
    if (!tile_id[0]) return;

    snprintf(tile_path, sizeof(tile_path), "%s/assets/tiles/%s.tile.txt", project_dir, tile_id);
    f = fopen(tile_path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq;
        line[strcspn(line, "\r\n")] = '\0';
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "rgb_top") == 0) {
            int r, g, b;
            if (sscanf(eq + 1, "%d,%d,%d", &r, &g, &b) == 3) {
                out->rgb_top[0] = (unsigned char)r; out->rgb_top[1] = (unsigned char)g; out->rgb_top[2] = (unsigned char)b;
            }
        } else if (strcmp(line, "rgb_side") == 0) {
            int r, g, b;
            if (sscanf(eq + 1, "%d,%d,%d", &r, &g, &b) == 3) {
                out->rgb_side[0] = (unsigned char)r; out->rgb_side[1] = (unsigned char)g; out->rgb_side[2] = (unsigned char)b;
            }
        } else if (strcmp(line, "extrude") == 0) {
            out->extrude = atof(eq + 1);
        } else if (strcmp(line, "walkable") == 0) {
            out->walkable = atoi(eq + 1);
        } else if (strcmp(line, "voxel_source") == 0) {
            snprintf(out->voxel_source, sizeof(out->voxel_source), "%s/%s", project_dir, eq + 1);
        } else if (strcmp(line, "alpha") == 0) {
            int a = atoi(eq + 1);
            if (a < 0) a = 0;
            if (a > 255) a = 255;
            out->alpha = a;
        }
    }
    fclose(f);
}

/* Rotates a world point by -g_cam_yaw around the shared orbit pivot — the
   exact inverse of the rotation project_world_point() applies. Used by
   draw_box() to find where the camera "effectively" sits in a box's own
   unrotated local frame, so the near-face heuristic below (originally
   written assuming yaw=0) keeps choosing the actually-visible faces at
   any mouse-orbit angle. See py3d-inspo.md / the yaw-face-selection fix
   note on draw_box() below for the bug this replaces. */
static void unrotate_by_yaw(double x, double z, double *out_x, double *out_z) {
    double px = x - g_cam_pivot_x;
    double pz = z - g_cam_pivot_z;
    double cyaw = cos(-g_cam_yaw), syaw = sin(-g_cam_yaw);
    *out_x = g_cam_pivot_x + (px * cyaw - pz * syaw);
    *out_z = g_cam_pivot_z + (px * syaw + pz * cyaw);
}

/*
 * Draws whichever of a box's 6 faces are actually front-facing, tested
 * independently — not a hardcoded "always exactly these 3" guess. A
 * rectangular box can show at most 3 faces to any single viewpoint
 * outside it (fewer if viewed exactly face-on or edge-on, or if the
 * viewpoint sits inside one of the box's axis slabs), and this now
 * computes exactly which ones, correctly, for ANY camera position/yaw —
 * this is a real per-face visibility test, not a 2-cases-of-yaw=0
 * heuristic. That distinction matters: a fixed "always draw these 2
 * sides" rule breaks the instant the camera moves somewhere the
   heuristic didn't anticipate (this is a game engine's box renderer, not
 * a fixed-camera demo's).
 *
 * Test, per axis: is the camera on the outward side of that face's
 * plane? For an axis-aligned box that's just a coordinate comparison
 * once the camera position is expressed in the box's own frame
 * (unrotate_by_yaw() undoes the mouse-orbit rotation project_world_point()
 * applies to world geometry, so this works at any orbit angle, not just
 * yaw=0). Y needs no un-rotation (yaw only spins around the Y axis).
 *
 * Top face corners are always returned in top_px/top_py (via the plain
 * clamped project_world_point_ex(), same as always) for callers that draw
 * a selection/gridline outline around a tile — that's a thin decorative
 * line, not filled geometry, so it was never the source of the "giant
 * distorted shape" bug and doesn't need the full clip treatment below.
 *
 * Depth buffer + proper near-plane clipping (added 2026-07-03, second
 * pass): every FILLED face (top/bottom/4 sides) now goes through
 * draw_clipped_face(), which transforms that face's 4 world corners into
 * camera space, clips them against the near plane with clip_poly_near()
 * (Sutherland-Hodgman — inserts real vertices at the clip boundary rather
 * than discarding), and only then projects + fills whatever survives.
 * This replaced an earlier, cruder fix: "if any of the box's 8 corners is
 * near-clipped, skip the whole box" — better than the original silent-
 * distortion bug (clamping z2 toward the plane and rendering at a blown-
 * up scale), but still not what a real renderer does, and a box that's
 * mostly fine but grazes the near plane at one corner would flicker out
 * completely instead of rendering its visible portion. Proper clipping
 * is what GL actually does for this, per the live discussion — see
 * ARCHITECTURE-RGB-RENDERING.md's frustum-clipping note.
 * `alpha` (0-255): 255 = opaque (test+write depth); less = translucent
 * (test-only + blended, drawn in a separate later pass — see
 * draw_tile_zmap_preview_3d()'s two-pass split).
 */
static void draw_box(unsigned char *buffer,
                      double wx0, double wx1, double wz0, double wz1,
                      double wy_bottom, double wy_top,
                      double cam_x, double cam_y, double cam_z, double pitch, double focal,
                      int screen_cx, int screen_cy, double scale,
                      const unsigned char rgb_top[3], const unsigned char rgb_side[3],
                      int alpha,
                      int top_px[4], int top_py[4]) {
    double top_z[4];

    project_world_point_ex(wx0, wy_top, wz0, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &top_px[0], &top_py[0], &top_z[0], NULL);
    project_world_point_ex(wx1, wy_top, wz0, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &top_px[1], &top_py[1], &top_z[1], NULL);
    project_world_point_ex(wx1, wy_top, wz1, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &top_px[2], &top_py[2], &top_z[2], NULL);
    project_world_point_ex(wx0, wy_top, wz1, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &top_px[3], &top_py[3], &top_z[3], NULL);

    if (wy_top - wy_bottom > 0.02) {
        double ecam_x, ecam_z;
        int wx0_visible, wx1_visible, wz0_visible, wz1_visible, bottom_visible;

        unrotate_by_yaw(cam_x, cam_z, &ecam_x, &ecam_z);
        wx0_visible = (ecam_x < wx0);
        wx1_visible = (ecam_x > wx1);
        wz0_visible = (ecam_z < wz0);
        wz1_visible = (ecam_z > wz1);
        bottom_visible = (cam_y < wy_bottom);

        if (wz0_visible) {
            draw_clipped_face(buffer,
                wx0, wy_top, wz0,  wx1, wy_top, wz0,  wx1, wy_bottom, wz0,  wx0, wy_bottom, wz0,
                cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_side, alpha);
        }
        if (wz1_visible) {
            draw_clipped_face(buffer,
                wx0, wy_top, wz1,  wx1, wy_top, wz1,  wx1, wy_bottom, wz1,  wx0, wy_bottom, wz1,
                cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_side, alpha);
        }
        if (wx0_visible) {
            draw_clipped_face(buffer,
                wx0, wy_top, wz0,  wx0, wy_top, wz1,  wx0, wy_bottom, wz1,  wx0, wy_bottom, wz0,
                cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_side, alpha);
        }
        if (wx1_visible) {
            draw_clipped_face(buffer,
                wx1, wy_top, wz0,  wx1, wy_top, wz1,  wx1, wy_bottom, wz1,  wx1, wy_bottom, wz0,
                cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_side, alpha);
        }
        if (bottom_visible) {
            draw_clipped_face(buffer,
                wx0, wy_bottom, wz0,  wx1, wy_bottom, wz0,  wx1, wy_bottom, wz1,  wx0, wy_bottom, wz1,
                cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_side, alpha);
        }
    }

    if (cam_y > wy_top) {
        draw_clipped_face(buffer,
            wx0, wy_top, wz0,  wx1, wy_top, wz0,  wx1, wy_top, wz1,  wx0, wy_top, wz1,
            cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, rgb_top, alpha);
    }
    (void)top_z;
}

/*
 * Ray-vs-axis-aligned-box slab test. Origin/direction in the SAME
 * (original, unrotated) world space the map's (col,row) grid is defined
 * in — see raymarch_tile_grid()'s doc comment for how the screen-pixel
 * ray gets there. Returns 1 and fills *out_t (entry distance) and
 * *out_face (0=-X,1=+X,2=-Y,3=+Y,4=-Z,5=+Z, whichever slab boundary the
 * entry point landed on) on a hit; 0 if the ray misses the box or the box
 * is entirely behind the ray origin. Standard technique — this, not a
 * projected-face depth comparison, is why "nearer wins" can't be gotten
 * wrong here: the entry t *is* the distance, not a value computed once
 * and compared against another computed value later.
 */
static int ray_aabb_hit(double ox, double oy, double oz, double dx, double dy, double dz,
                         double bx0, double bx1, double by0, double by1, double bz0, double bz1,
                         double *out_t, int *out_face) {
    double tmin = -1e18, tmax = 1e18;
    int face = -1;

    if (fabs(dx) < 1e-12) {
        if (ox < bx0 || ox > bx1) return 0;
    } else {
        double t0 = (bx0 - ox) / dx, t1 = (bx1 - ox) / dx;
        int f0 = 0;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 1; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dy) < 1e-12) {
        if (oy < by0 || oy > by1) return 0;
    } else {
        double t0 = (by0 - oy) / dy, t1 = (by1 - oy) / dy;
        int f0 = 2;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 3; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (fabs(dz) < 1e-12) {
        if (oz < bz0 || oz > bz1) return 0;
    } else {
        double t0 = (bz0 - oz) / dz, t1 = (bz1 - oz) / dz;
        int f0 = 4;
        if (t0 > t1) { double t = t0; t0 = t1; t1 = t; f0 = 5; }
        if (t0 > tmin) { tmin = t0; face = f0; }
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.0) return 0; /* box is entirely behind the ray origin */
    if (tmin < 0.0) { tmin = 0.0; face = -1; } /* origin started inside the box (e.g. camera clipped into a wall) */
    *out_t = tmin;
    if (out_face) *out_face = face;
    return 1;
}

/* Small persistent cache of loaded voxel_source CSVs (emoji-entity
 * tiles), keyed by path, so raymarch_tile_grid() doesn't re-read and
 * re-parse a tile's CSV from disk for every screen pixel that hits it.
 * Same `# resolution=N` + flat r,g,b,a rows format as
 * sample_voxel_pixel()/draw_voxel_grid_2d_thumbnail() — this is a
 * second reader of that format; consolidating into one shared loader is
 * reasonable future cleanup, not done here to keep this change scoped to
 * the ray-march pivot itself. Lives for the daemon process's lifetime
 * (these are static project assets that don't change mid-session).
 */
#define MAX_VOXEL_CSV_CACHE 8
typedef struct {
    char path[512];
    int resolution;
    int count;
    unsigned char pixels[4096][4];
    int loaded;
} VoxelCsvCache;
static VoxelCsvCache g_voxel_csv_cache[MAX_VOXEL_CSV_CACHE];
static int g_voxel_csv_cache_count = 0;

static VoxelCsvCache *get_voxel_csv_cached(const char *path) {
    int i;
    FILE *f;
    char line[256];
    VoxelCsvCache *c;

    for (i = 0; i < g_voxel_csv_cache_count; i++) {
        if (strcmp(g_voxel_csv_cache[i].path, path) == 0) return &g_voxel_csv_cache[i];
    }
    if (g_voxel_csv_cache_count >= MAX_VOXEL_CSV_CACHE) return NULL;

    c = &g_voxel_csv_cache[g_voxel_csv_cache_count++];
    snprintf(c->path, sizeof(c->path), "%s", path);
    c->count = 0;
    c->resolution = 0;
    c->loaded = 0;

    f = fopen(path, "r");
    if (!f) return c; /* cached as "not loaded" so we don't retry every pixel */
    while (fgets(line, sizeof(line), f) && c->count < 4096) {
        int r, g, b, a;
        if (line[0] == '#') {
            sscanf(line, "# resolution=%d", &c->resolution);
            continue;
        }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            c->pixels[c->count][0] = (unsigned char)r;
            c->pixels[c->count][1] = (unsigned char)g;
            c->pixels[c->count][2] = (unsigned char)b;
            c->pixels[c->count][3] = (unsigned char)a;
            c->count++;
        }
    }
    fclose(f);
    if (c->resolution <= 0) {
        for (c->resolution = 1; c->resolution * c->resolution < c->count; c->resolution++) {}
    }
    c->loaded = (c->resolution > 0 && c->count > 0);
    return c;
}

/* Samples a voxel_source CSV at normalized (u,v) in [0,1). Returns 1 and
 * fills out_rgb if that pixel is occupied (alpha>10); 0 if transparent
 * or the CSV couldn't be loaded (caller falls back to the tile's plain
 * rgb_side/rgb_top — see the known-simplification note in
 * ARCHITECTURE-RGB-RENDERING.md's ray-marching section). */
static int sample_voxel_pixel(const char *path, double u, double v, unsigned char out_rgb[3]) {
    VoxelCsvCache *c = get_voxel_csv_cached(path);
    int col, row, idx;
    if (!c || !c->loaded) return 0;
    if (u < 0.0) u = 0.0;
    if (u > 0.999999) u = 0.999999;
    if (v < 0.0) v = 0.0;
    if (v > 0.999999) v = 0.999999;
    col = (int)(u * c->resolution);
    row = (int)(v * c->resolution);
    idx = row * c->resolution + col;
    if (idx < 0 || idx >= c->count) return 0;
    if (c->pixels[idx][3] <= 10) return 0;
    out_rgb[0] = c->pixels[idx][0];
    out_rgb[1] = c->pixels[idx][1];
    out_rgb[2] = c->pixels[idx][2];
    return 1;
}

/*
 * Ray-marches the solid tile grid, one call per screen pixel in the
 * game_map viewport, replacing the old per-tile draw_box() rasterization
 * for opaque world geometry. See ARCHITECTURE-RGB-RENDERING.md's
 * "Pivot: Ray Marching..." section for the full rationale (short version:
 * our world is a small regular grid, same shape of problem Minecraft
 * solves, and DDA voxel traversal is correct-by-construction for
 * occlusion in a way projected-face depth comparison isn't).
 *
 * Per pixel: build a ray (screen pixel -> camera-space direction ->
 * un-pitch -> un-yaw into the SAME original/unrotated world space the
 * map's (col,row) indices live in — this reuses unrotate_by_yaw(), which
 * already does exactly this inverse transform for a single point;
 * applying it to both the ray origin and a second point one unit along
 * the ray direction and subtracting gives the direction in that same
 * space, since rotation is linear), then walk (col,row) cells via 2D DDA
 * in near-to-far order, testing each occupied cell's full 3D box
 * (ray_aabb_hit()) using that tile's own height. The DDA's job is purely
 * to visit cells in the right order so the first 3D hit found is
 * guaranteed the globally nearest one — an early-exit, not a fallback.
 *
 * Writes color and depth (the same camera-space z2 metric draw_box()'s
 * rasterized path uses) directly into buffer/g_depth_buf, so entities
 * drawn afterward via the ordinary rasterized path (xelector/pet markers)
 * still composite correctly against this.
 */
static void raymarch_tile_grid(unsigned char *buffer,
                                int x0, int y0, int x1, int y1,
                                char rows[32][128], int row_count, int col_count,
                                const char *project_dir, const char *registry_rel,
                                double cam_x, double cam_y, double cam_z, double pitch, double focal,
                                int screen_cx, int screen_cy, double scale) {
    TileMeta meta_cache[16];
    char meta_glyph[16];
    int meta_count = 0;
    double half_w = col_count / 2.0;
    double cpitch = cos(pitch * M_PI / 180.0), spitch = sin(pitch * M_PI / 180.0);
    int sx, sy;

    for (sy = y0; sy < y1; sy++) {
        for (sx = x0; sx < x1; sx++) {
            double dx_cam = (sx - screen_cx) / (scale * focal);
            double dy_cam = (screen_cy - sy) / (scale * focal);
            double dy_yaw = dy_cam * cpitch + spitch;   /* dz_cam is always 1.0 */
            double dz_yaw = -dy_cam * spitch + cpitch;
            double ox, oy, oz, px1, pz1, dir_x, dir_y, dir_z;
            double gx, gz;
            int col, row, step_col, step_row;
            double t_delta_x, t_delta_z, t_max_x, t_max_z;
            int steps, max_steps;
            int hit = 0, hit_face = -1, hit_col = -1, hit_row = -1;
            double hit_t = 0.0;

            unrotate_by_yaw(cam_x, cam_z, &ox, &oz);
            unrotate_by_yaw(cam_x + dx_cam, cam_z + dz_yaw, &px1, &pz1);
            dir_x = px1 - ox;
            dir_z = pz1 - oz;
            dir_y = dy_yaw;
            oy = cam_y; /* y is untouched by yaw, so the camera's own height
                           carries straight over into this original-grid frame */

            /* The camera usually sits well outside the grid's own footprint
               (e.g. cam_z=-16 while rows start at z=0) — walking the DDA
               from the raw camera position would burn most of max_steps
               crossing empty space before ever reaching row 0, and could
               exhaust it before arriving for a large enough gap/grid. So
               first find where the ray enters the grid's overall XZ
               footprint (a plain slab test, y unconstrained via a huge
               by0/by1 since only x/z matter here) and start the walk
               there instead — same trick real ray marchers use to skip
               empty leading space before a bounded volume. */
            {
                double entry_t;
                int entry_face;
                if (!ray_aabb_hit(ox, 0.0, oz, dir_x, dir_y, dir_z,
                                   -half_w, half_w, -1e9, 1e9, 0.0, (double)row_count,
                                   &entry_t, &entry_face)) {
                    continue; /* ray never reaches the grid's footprint at all */
                }
                if (entry_t > 0.0) {
                    double nudge = entry_t + 1e-6;
                    ox = ox + nudge * dir_x;
                    oy = oy + nudge * dir_y;
                    oz = oz + nudge * dir_z;
                }
            }

            gx = ox + half_w;
            gz = oz;
            col = (int)floor(gx);
            row = (int)floor(gz);
            step_col = (dir_x > 0.0) ? 1 : (dir_x < 0.0 ? -1 : 0);
            step_row = (dir_z > 0.0) ? 1 : (dir_z < 0.0 ? -1 : 0);
            t_delta_x = (dir_x != 0.0) ? fabs(1.0 / dir_x) : 1e18;
            t_delta_z = (dir_z != 0.0) ? fabs(1.0 / dir_z) : 1e18;
            t_max_x = (dir_x > 0.0) ? ((col + 1) - gx) / dir_x : (dir_x < 0.0 ? (col - gx) / dir_x : 1e18);
            t_max_z = (dir_z > 0.0) ? ((row + 1) - gz) / dir_z : (dir_z < 0.0 ? (row - gz) / dir_z : 1e18);

            max_steps = (col_count + row_count) * 2 + 4;
            for (steps = 0; steps < max_steps; steps++) {
                if (col >= 0 && col < col_count && row >= 0 && row < row_count && rows[row][col]) {
                    char glyph = rows[row][col];
                    TileMeta *meta = NULL;
                    TileMeta overflow_meta;
                    int mi;
                    for (mi = 0; mi < meta_count; mi++) {
                        if (meta_glyph[mi] == glyph) { meta = &meta_cache[mi]; break; }
                    }
                    if (!meta) {
                        if (meta_count < 16) {
                            load_tile_meta(project_dir, registry_rel, glyph, &meta_cache[meta_count]);
                            meta_glyph[meta_count] = glyph;
                            meta = &meta_cache[meta_count];
                            meta_count++;
                        } else {
                            load_tile_meta(project_dir, registry_rel, glyph, &overflow_meta);
                            meta = &overflow_meta;
                        }
                    }
                    if (!meta->walkable && meta->alpha >= 255) {
                        double bx0 = col - half_w, bx1 = bx0 + 1.0;
                        double bz0 = (double)row, bz1 = bz0 + 1.0;
                        double t;
                        int face;
                        if (ray_aabb_hit(ox, oy, oz, dir_x, dir_y, dir_z,
                                          bx0, bx1, 0.0, meta->extrude, bz0, bz1, &t, &face)) {
                            hit = 1; hit_t = t; hit_face = face; hit_col = col; hit_row = row;
                            break;
                        }
                    }
                }
                if (t_max_x < t_max_z) { col += step_col; t_max_x += t_delta_x; }
                else { row += step_row; t_max_z += t_delta_z; }
                if (col < -1 || col > col_count || row < -1 || row > row_count) break;
            }

            if (hit) {
                double wx = ox + hit_t * dir_x;
                double wy = oy + hit_t * dir_y;
                double wz = oz + hit_t * dir_z;
                TileMeta *meta = NULL;
                int mi;
                unsigned char rgb[3];
                double rx, ry2, rz2;

                for (mi = 0; mi < meta_count; mi++) {
                    if (meta_glyph[mi] == rows[hit_row][hit_col]) { meta = &meta_cache[mi]; break; }
                }
                if (!meta) continue;

                if (hit_face == 3) {
                    rgb[0] = meta->rgb_top[0]; rgb[1] = meta->rgb_top[1]; rgb[2] = meta->rgb_top[2];
                } else {
                    rgb[0] = meta->rgb_side[0]; rgb[1] = meta->rgb_side[1]; rgb[2] = meta->rgb_side[2];
                    if (meta->voxel_source[0]) {
                        double bx0 = hit_col - half_w, bz0 = (double)hit_row;
                        double u = (hit_face == 4 || hit_face == 5) ? (wx - bx0) : (wz - bz0);
                        double v = 1.0 - (wy / meta->extrude);
                        unsigned char sampled[3];
                        if (sample_voxel_pixel(meta->voxel_source, u, v, sampled)) {
                            rgb[0] = sampled[0]; rgb[1] = sampled[1]; rgb[2] = sampled[2];
                        }
                    }
                }

                world_to_camera_space(wx, wy, wz, cam_x, cam_y, cam_z, pitch, &rx, &ry2, &rz2);

                if (!render_clip_visible(sx, sy)) continue;

                /* Depth write is scoped to g_depth_buf's own rect (so
                   later-drawn entity markers can depth-test against this
                   ray-marched pixel); color write is NOT similarly scoped
                   -- when rendering beyond the widget rect is allowed
                   (render_clip_visible() above already let this pixel
                   through), a hit outside the depth buffer's own bounds
                   should still be drawn, same graceful-degrade pattern
                   fill_poly_px() uses. Nesting the color write inside the
                   depth-scope check (as an earlier draft of this function
                   did) would silently suppress every off-widget pixel and
                   defeat the render_beyond_viewport=1 case entirely. */
                if (g_depth_buf && sx >= g_depth_x0 && sx < g_depth_x0 + g_depth_w &&
                    sy >= g_depth_y0 && sy < g_depth_y0 + g_depth_h) {
                    size_t di = (size_t)(sy - g_depth_y0) * (size_t)g_depth_w + (size_t)(sx - g_depth_x0);
                    g_depth_buf[di] = rz2;
                }
                {
                    int idx = (sy * WIDTH + sx) * 4;
                    buffer[idx] = rgb[0];
                    buffer[idx + 1] = rgb[1];
                    buffer[idx + 2] = rgb[2];
                    buffer[idx + 3] = 255;
                }
            }
        }
    }
}

/*
 * Perspective grid renderer, real camera projection (not a lift/shading
 * trick). As of 2026-07-03 this is a hybrid renderer — see
 * ARCHITECTURE-RGB-RENDERING.md's "Pivot: Ray Marching..." section:
 *   - Pass 1, the solid tile/voxel grid (walls/trees/stone/emoji-voxel
 *     tiles): ray marched (raymarch_tile_grid()), not rasterized. Nearest-
 *     wins is the ray's literal walk order, correct-by-construction,
 *     which is why this replaced the old per-tile draw_box() loop here.
 *   - Entity markers (xelector/pet) and Pass 2 (translucent glass/water):
 *     still rasterized via draw_box(), composited against the same
 *     g_depth_buf the ray marcher writes into.
 *   - Ground wireframe grid and the 2D top-down preview path: unchanged,
 *     never implicated in the rasterizer bugs this pivot fixed.
 * Tile height comes from per-glyph metadata (load_tile_meta()), defaulting
 * to a full cube. Xelector and pet render as separate small boxes
 * extruding upward (positive wy) from the shared wy=0 ground plane,
 * standing one layer above whatever tile they're on.
 */
static void draw_tile_zmap_preview_3d(unsigned char *buffer, const FrameObject *obj,
                                       int cell_w, int cell_h,
                                       char rows[32][128], int row_count, int col_count,
                                       int sel_x, int sel_y, int pet_x, int pet_y) {
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    unsigned char marker[3] = {255, 209, 102};
    unsigned char marker_side[3] = {189, 148, 47};
    unsigned char pet_rgb[3] = {255, 140, 190};
    unsigned char pet_side[3] = {189, 90, 130};
    char project_dir[512];
    char registry_rel[128];
    TileMeta meta_cache[16];
    char meta_glyph[16];
    int meta_count = 0;
    int screen_cx = (x0 + x1) / 2;
    int screen_cy;
    double scale = 210.0;
    double pitch, focal, cam_x, cam_y, cam_z;
    int camera_mode = 1;
    double pan_x = 0.0, pan_y = 0.0, pan_z = 0.0;
    double cam_yaw_deg = 0.0, cam_pitch_delta = 0.0;
    int row, col, i;
    const char *reg;
    const char *cam_field;

    derive_project_dir(obj->source_ref, project_dir, sizeof(project_dir));
    snprintf(registry_rel, sizeof(registry_rel), "assets/tiles/registry.txt");
    reg = strstr(obj->label, "registry=");
    if (reg) {
        sscanf(reg + 9, "%127[^;]", registry_rel);
    }

    /* "camera=" field: mode,pan_x,pan_y,pan_z,yaw,pitch_delta,_ .
       Fields 5 and 6 (formerly dead placeholders) now carry mouse-driven
       camera orbit: yaw (degrees, turntable spin about the map center) and
       a pitch delta (degrees, added to the active POV preset's tilt). Both
       are written by ops/wraith_project_input.c from MOUSE_DRAG events that
       wraith-alpha forwards only while in map-control (interact) mode.
       1/2/3 still switch fixed POV presets (WASD/ZX pan stays live within
       whichever preset is active); this renderer just draws whatever
       state.txt already says. See py3d-inspo.md. */
    cam_field = strstr(obj->label, "camera=");
    if (cam_field) {
        double d6;
        sscanf(cam_field + 7, "%d,%lf,%lf,%lf,%lf,%lf,%lf", &camera_mode, &pan_x, &pan_y, &pan_z, &cam_yaw_deg, &cam_pitch_delta, &d6);
    }

    switch (camera_mode) {
        case 2: pitch = 45.0; cam_y = 10.0; cam_z = -12.0; focal = 1.0; screen_cy = y0 + (int)((y1 - y0) * 0.02); break;
        case 3: pitch = 10.0; cam_y = 4.0; cam_z = -20.0; focal = 1.0; screen_cy = y0 + (int)((y1 - y0) * 0.22); break;
        default: pitch = 22.0; cam_y = 7.0; cam_z = -16.0; focal = 1.0; screen_cy = y0 + (int)((y1 - y0) * 0.08); break;
    }
    cam_x = pan_x;
    cam_y += pan_y;
    cam_z += pan_z;
    pitch += cam_pitch_delta;

    /* Publish yaw + orbit pivot for project_world_point() (frame-scoped
       globals). Pivot is the scene-center ground point: x=0 (tiles are
       centered on x=0 below via wx0 = col - col_count/2) and the middle
       row in z. */
    g_cam_yaw = cam_yaw_deg * M_PI / 180.0;
    g_cam_pivot_x = 0.0;
    g_cam_pivot_z = row_count / 2.0;

    fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);

    /* Scoped render-clip rect: pieces/config/wraith_debug.conf's
       render_beyond_viewport (default 1/on) controls whether ANYTHING
       drawn below -- ground grid, ray-marched tiles, entity markers --
       is allowed to land outside this widget's own screen rect. Set
       once, active for every pixel-writing call between here and
       render_clip_end() below (put_px()/fill_poly_px() consult it).
       See render_clip_begin()'s doc comment for why this exists: the
       ray marcher's own pixel loop is naturally bounded to whatever
       rect it's told to scan, while draw_line_px()/draw_box() were never
       bounded at all -- leaving both "on" gave inconsistent overflow
       (grid + markers bled past the window, ray-marched walls/emoji did
       not). This makes all three agree. */
    render_clip_begin(x0, y0, x1, y1);

    /* Ground plane: one shared wireframe grid sized to the whole map
       (col_count x row_count), drawn once — matches plugy3d's
       draw_grid(). This is a property of the map/scene, not a per-tile
       "piece"; walkable tiles below don't draw their own box or outline
       at all, they just stand on this. See py3d-inspo.md. */
    {
        unsigned char grid_rgb[3] = {90, 90, 90};
        double half_w = col_count / 2.0;
        int gline;
        for (gline = 0; gline <= col_count; gline++) {
            int ax, ay, bx, by;
            project_world_point(gline - half_w, 0.0, 0.0, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &ax, &ay);
            project_world_point(gline - half_w, 0.0, (double)row_count, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &bx, &by);
            draw_line_px(buffer, ax, ay, bx, by, grid_rgb, 1);
        }
        for (gline = 0; gline <= row_count; gline++) {
            int ax, ay, bx, by;
            project_world_point(-half_w, 0.0, (double)gline, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &ax, &ay);
            project_world_point(half_w, 0.0, (double)gline, cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale, &bx, &by);
            draw_line_px(buffer, ax, ay, bx, by, grid_rgb, 1);
        }
    }

    /* Depth buffer scoped to this game_map surface — see depth_buf_begin()'s
       doc comment. This is what makes tile/box paint order stop mattering
       for opaque geometry (fixes the "objects corrupted/in the wrong
       place while rotating" bug); the far-to-near loop order below is
       kept anyway because pass 2 (translucent) still needs a real
       back-to-front order among itself. */
    depth_buf_begin(x0, y0, x1 - x0, y1 - y0);

    /* Pass 1: the solid tile/voxel grid — ray marched (see
       ARCHITECTURE-RGB-RENDERING.md's "Pivot: Ray Marching..." section
       and raymarch_tile_grid()'s doc comment for the full rationale).
       This replaced draw_box()-per-tile rasterization here specifically
       because it's the block that kept exposing the "tree disappears in
       front of the wall" corruption — nearest-wins is the ray's literal
       walk order now, not a depth comparison that can be gotten wrong.
       Writes color + depth (g_depth_buf) directly.

       Scan rect: unlike draw_line_px()/draw_box(), the ray marcher only
       ever visits pixels within whatever rect it's handed — it has no
       equivalent of "the projected point just happened to land outside
       the widget". So when render_beyond_viewport is on, hand it the
       FULL canvas to scan instead of just this widget's rect, so a tile
       that would have rendered off-screen under the old rasterizer gets
       the same chance here (render_clip_begin() above still governs
       whether the resulting pixels actually get written — this only
       controls how much ground raymarch_tile_grid() bothers walking). */
    {
        int rm_x0 = x0, rm_y0 = y0, rm_x1 = x1, rm_y1 = y1;
        if (render_beyond_viewport_enabled()) {
            rm_x0 = 0; rm_y0 = 0; rm_x1 = WIDTH; rm_y1 = HEIGHT;
        }
        raymarch_tile_grid(buffer, rm_x0, rm_y0, rm_x1, rm_y1, rows, row_count, col_count,
                            project_dir, registry_rel,
                            cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale);
    }

    /* Entity markers (pet/xelector) are few, small, and stay rasterized —
       drawn directly at their single grid position now rather than as
       part of a row/col scan, since the solid grid itself no longer
       needs that scan. draw_box()'s fill_poly_px() still depth-tests
       these against g_depth_buf, so a marker standing behind a
       ray-marched wall is still correctly hidden. */
    if (pet_x >= 0 && pet_y >= 0 && pet_x < col_count && pet_y < row_count) {
        double wx0 = pet_x - col_count / 2.0;
        double wx1 = wx0 + 1.0;
        double wz0 = (double)pet_y;
        double wz1 = wz0 + 1.0;
        int epx[4], epy[4];
        double inset = 0.22;
        draw_box(buffer, wx0 + inset, wx1 - inset, wz0 + inset, wz1 - inset, 0.0, 0.55,
                 cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale,
                 pet_rgb, pet_side, 255, epx, epy);
    }

    if (sel_x >= 0 && sel_y >= 0 && sel_x < col_count && sel_y < row_count) {
        double wx0 = sel_x - col_count / 2.0;
        double wx1 = wx0 + 1.0;
        double wz0 = (double)sel_y;
        double wz1 = wz0 + 1.0;
        int epx[4], epy[4];
        double inset = 0.18;
        draw_box(buffer, wx0 + inset, wx1 - inset, wz0 + inset, wz1 - inset, 0.0, 0.65,
                 cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale,
                 marker, marker_side, 255, epx, epy);
        draw_line_px(buffer, epx[0], epy[0], epx[2], epy[2], marker, 1);
        draw_line_px(buffer, epx[1], epy[1], epx[3], epy[3], marker, 1);
    }

    /* Pass 2: translucent tiles (glass/water/...) — after all opaque
       geometry, back-to-front among themselves (far-to-near row order,
       same as pass 1). draw_box()/fill_quad_px() depth-test these against
       the opaque buffer above (so they correctly hide behind walls) but
       never write into it — matches Unreal/Godot's fallback to sorted
       back-to-front blending once alpha breaks straightforward Z-buffer
       logic. See emoji-entity-feature-report.md / ARCHITECTURE-RGB-
       RENDERING.md for the glass/water test tile this proves out. */
    for (row = row_count - 1; row >= 0; row--) {
        for (col = 0; col < col_count && rows[row][col]; col++) {
            double wx0 = col - col_count / 2.0;
            double wx1 = wx0 + 1.0;
            double wz0 = (double)row;
            double wz1 = wz0 + 1.0;
            char glyph = rows[row][col];
            TileMeta *meta = NULL;
            TileMeta overflow_meta;
            int top_px[4], top_py[4];

            for (i = 0; i < meta_count; i++) {
                if (meta_glyph[i] == glyph) { meta = &meta_cache[i]; break; }
            }
            if (!meta) {
                if (meta_count < 16) {
                    load_tile_meta(project_dir, registry_rel, glyph, &meta_cache[meta_count]);
                    meta_glyph[meta_count] = glyph;
                    meta = &meta_cache[meta_count];
                    meta_count++;
                } else {
                    load_tile_meta(project_dir, registry_rel, glyph, &overflow_meta);
                    meta = &overflow_meta;
                }
            }

            if (!meta->walkable && meta->alpha < 255) {
                draw_box(buffer, wx0, wx1, wz0, wz1, 0.0, meta->extrude,
                         cam_x, cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale,
                         meta->rgb_top, meta->rgb_side, meta->alpha, top_px, top_py);

                {
                    unsigned char gridline[3];
                    shade_rgb(meta->rgb_top, gridline, 55);
                    draw_line_px(buffer, top_px[0], top_py[0], top_px[1], top_py[1], gridline, 1);
                    draw_line_px(buffer, top_px[1], top_py[1], top_px[2], top_py[2], gridline, 1);
                    draw_line_px(buffer, top_px[2], top_py[2], top_px[3], top_py[3], gridline, 1);
                    draw_line_px(buffer, top_px[3], top_py[3], top_px[0], top_py[0], gridline, 1);
                }
            }
        }
    }

    depth_buf_end();
    render_clip_end();
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

/* Draws an NxN grid of small colored squares straight from a voxels_N.csv
 * within a screen-pixel rect — a flat, face-up pixel-art thumbnail of
 * the same data the 3D view's ray marcher (raymarch_tile_grid() +
 * sample_voxel_pixel()) stands up as a relief facing the camera.
 * This is the "why can't the 2D view use the same 8-bit data" fix: the
 * 2D top-down map is already an inherently face-up view (you're looking
 * straight down), so there's no orientation question here the way there
 * is in 3D — just blit the CSV as a mini icon. Alpha<=10 pixels are
 * skipped (left as the tile's plain background color), matching the
 * same occupancy threshold used everywhere else this CSV format is read.
 */
static void draw_voxel_grid_2d_thumbnail(unsigned char *buffer, const char *csv_path,
                                          int px0, int py0, int px1, int py1) {
    FILE *f = fopen(csv_path, "r");
    char line[256];
    int values[4096][4];
    int count = 0;
    int resolution = 0;
    int i;
    int cell_w, cell_h;

    if (!f) return;
    while (fgets(line, sizeof(line), f) && count < 4096) {
        int r, g, b, a;
        if (line[0] == '#') {
            sscanf(line, "# resolution=%d", &resolution);
            continue;
        }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            values[count][0] = r;
            values[count][1] = g;
            values[count][2] = b;
            values[count][3] = a;
            count++;
        }
    }
    fclose(f);
    if (resolution <= 0) {
        for (resolution = 1; resolution * resolution < count; resolution++) {}
    }
    if (resolution <= 0) return;

    cell_w = (px1 - px0) / resolution;
    cell_h = (py1 - py0) / resolution;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    for (i = 0; i < count && i < resolution * resolution; i++) {
        int col = i % resolution;
        int row = i / resolution;
        int alpha = values[i][3];
        unsigned char rgb[3];
        int qx0, qy0;

        if (alpha <= 10) continue;

        rgb[0] = (unsigned char)values[i][0];
        rgb[1] = (unsigned char)values[i][1];
        rgb[2] = (unsigned char)values[i][2];
        qx0 = px0 + col * cell_w;
        qy0 = py0 + row * cell_h;
        fill_rect_px(buffer, qx0, qy0, qx0 + cell_w, qy0 + cell_h, rgb);
    }
}

static void draw_tile_zmap_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    FILE *f = fopen(obj->source_ref, "r");
    char rows[32][128];
    char line[256];
    int row_count = 0;
    int col_count = 0;
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int tile_w, tile_h;
    int y, x;
    unsigned char grass[3] = {34, 139, 34};
    unsigned char wall[3] = {95, 112, 142};
    unsigned char tree[3] = {22, 130, 29};
    unsigned char stone[3] = {108, 131, 170};
    unsigned char gap[3] = {24, 35, 52};
    unsigned char marker[3] = {255, 209, 102};
    int sel_x = -1, sel_y = -1;
    int pet_x = -1, pet_y = -1;
    int mode_3d = 0;
    const char *sel = strstr(obj->label, "selected=");
    const char *pet = strstr(obj->label, "pet=");
    const char *mode = strstr(obj->label, "mode=");
    if (sel) {
        sscanf(sel + 9, "%d,%d", &sel_x, &sel_y);
    }
    if (pet) {
        sscanf(pet + 4, "%d,%d", &pet_x, &pet_y);
    }
    if (mode) {
        mode_3d = (strncmp(mode + 5, "3d", 2) == 0);
    }

    if (!f) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }
    while (row_count < 32 && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(rows[row_count], sizeof(rows[row_count]), "%s", line);
        if ((int)strlen(line) > col_count) col_count = (int)strlen(line);
        row_count++;
    }
    fclose(f);
    if (row_count <= 0 || col_count <= 0) return;

    if (mode_3d) {
        draw_tile_zmap_preview_3d(buffer, obj, cell_w, cell_h, rows, row_count, col_count, sel_x, sel_y, pet_x, pet_y);
        return;
    }

    tile_w = (x1 - x0) / col_count;
    tile_h = (y1 - y0) / row_count;
    if (tile_w < 2) tile_w = 2;
    if (tile_h < 2) tile_h = 2;
    fill_rect_px(buffer, x0, y0, x1, y1, gap);
    {
        /* Registry/tile-meta lookup, same convention as
           draw_tile_zmap_preview_3d() — needed so voxel_source tiles
           (emoji entities) show their real pixel data here too, instead
           of silently falling through to plain grass. See "why can't we
           use the same 8-bit to render them" in emoji-entity-feature-
           report.md. */
        char project_dir[512];
        char registry_rel[128];
        TileMeta meta_cache[16];
        char meta_glyph[16];
        int meta_count = 0;
        const char *reg = strstr(obj->label, "registry=");

        derive_project_dir(obj->source_ref, project_dir, sizeof(project_dir));
        snprintf(registry_rel, sizeof(registry_rel), "assets/tiles/registry.txt");
        if (reg) {
            sscanf(reg + 9, "%127[^;]", registry_rel);
        }

        for (y = 0; y < row_count; y++) {
            for (x = 0; rows[y][x] && x < col_count; x++) {
                unsigned char *rgb = grass;
                int px = x0 + x * tile_w;
                int py = y0 + y * tile_h;
                int lift = 0;
                int is_selected = (x == sel_x && y == sel_y);
                char glyph = rows[y][x];
                TileMeta *meta = NULL;
                TileMeta overflow_meta;
                int i;

                for (i = 0; i < meta_count; i++) {
                    if (meta_glyph[i] == glyph) { meta = &meta_cache[i]; break; }
                }
                if (!meta) {
                    if (meta_count < 16) {
                        load_tile_meta(project_dir, registry_rel, glyph, &meta_cache[meta_count]);
                        meta_glyph[meta_count] = glyph;
                        meta = &meta_cache[meta_count];
                        meta_count++;
                    } else {
                        load_tile_meta(project_dir, registry_rel, glyph, &overflow_meta);
                        meta = &overflow_meta;
                    }
                }

                if (glyph == '#') rgb = wall;
                else if (glyph == 'T') {
                    rgb = tree;
                    lift = tile_h / 2;
                } else if (glyph == 'R') {
                    rgb = stone;
                    lift = tile_h / 3;
                }
                if (glyph == '#') lift = tile_h / 3;
                if (mode_3d && lift == 0) {
                    lift = tile_h / 6;
                    if (lift < 1) lift = 1;
                }

                if (meta->voxel_source[0]) {
                    /* Emoji/voxel-sourced tile: draw the real pixel data
                       as a flat face-up thumbnail (2D is already a
                       top-down view, so no orientation question here —
                       just blit the CSV). */
                    fill_rect_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, gap);
                    draw_voxel_grid_2d_thumbnail(buffer, meta->voxel_source, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1);
                    draw_border_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, obj->border, 1);
                } else if (lift > 0) {
                    unsigned char side[3];
                    shade_rgb(rgb, side, 70);
                    fill_rect_px(buffer, px + 2, py + tile_h - lift, px + tile_w - 2, py + tile_h - 1, side);
                    fill_rect_px(buffer, px + 2, py + 1, px + tile_w - 2, py + tile_h - lift, rgb);
                    draw_border_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, obj->border, 1);
                } else {
                    fill_rect_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, rgb);
                }
                if (is_selected) {
                    int cx = px + tile_w / 2;
                    int cy = py + tile_h / 2;
                    draw_border_px(buffer, px + 1, py + 1, px + tile_w - 1, py + tile_h - 1, marker, 2);
                    draw_line_px(buffer, cx - tile_w / 3, cy, cx + tile_w / 3, cy, marker, 2);
                    draw_line_px(buffer, cx, cy - tile_h / 3, cx, cy + tile_h / 3, marker, 2);
                }
            }
        }
    }
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

static void draw_widget_surface_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int mid_x = (x0 + x1) / 2;
    int mid_y = (y0 + y1) / 2;
    unsigned char bg[3] = {8, 14, 22};
    unsigned char grid[3] = {38, 58, 76};
    unsigned char accent[3] = {126, 223, 242};
    int x, y;
    fill_rect_px(buffer, x0, y0, x1, y1, bg);
    for (x = x0 + 12; x < x1; x += 24) {
        draw_line_px(buffer, x, y0, x, y1, grid, 1);
    }
    for (y = y0 + 12; y < y1; y += 24) {
        draw_line_px(buffer, x0, y, x1, y, grid, 1);
    }
    draw_line_px(buffer, mid_x - 45, mid_y + 25, mid_x, mid_y - 35, accent, 2);
    draw_line_px(buffer, mid_x, mid_y - 35, mid_x + 45, mid_y + 25, accent, 2);
    draw_line_px(buffer, mid_x - 45, mid_y + 25, mid_x + 45, mid_y + 25, accent, 2);
    blit_text(buffer, obj->x + 2, obj->y + 1, "CHTMGL", obj->fg, obj->w - 4, cell_w, cell_h);
    blit_text(buffer, obj->x + 2, obj->y + 2, "canvas->game_map", obj->fg, obj->w - 4, cell_w, cell_h);
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

static int is_chtmgl_control_role(const FrameObject *obj) {
    if (!obj) return 0;
    return strcmp(obj->role, "chtmgl_button") == 0 ||
           strcmp(obj->role, "chtmgl_checkbox") == 0 ||
           strcmp(obj->role, "chtmgl_slider") == 0 ||
           strcmp(obj->role, "chtmgl_menu") == 0 ||
           strcmp(obj->role, "emoji_resolution_picker") == 0;
}

static void draw_chtmgl_control_shape(unsigned char *buffer, const FrameObject *obj, int x0, int y0, int x1, int y1) {
    if (strcmp(obj->role, "chtmgl_checkbox") == 0) {
        int box = (y1 - y0) - 6;
        if (box < 6) box = 6;
        fill_rect_px(buffer, x0 + 4, y0 + 3, x0 + 4 + box, y0 + 3 + box, obj->bg);
        draw_border_px(buffer, x0 + 4, y0 + 3, x0 + 4 + box, y0 + 3 + box, obj->border, 1);
        draw_line_px(buffer, x0 + 7, y0 + 3 + box / 2, x0 + 4 + box / 2, y0 + 3 + box - 4, obj->border, 2);
        draw_line_px(buffer, x0 + 4 + box / 2, y0 + 3 + box - 4, x0 + 4 + box - 3, y0 + 5, obj->border, 2);
        return;
    }
    if (strcmp(obj->role, "chtmgl_slider") == 0) {
        int cy = (y0 + y1) / 2;
        int thumb = x0 + ((x1 - x0) * 55) / 100;
        unsigned char track[3] = {80, 100, 120};
        draw_line_px(buffer, x0 + 8, cy, x1 - 8, cy, track, 3);
        fill_rect_px(buffer, thumb - 4, cy - 7, thumb + 5, cy + 8, obj->border);
        return;
    }
    if (strcmp(obj->role, "chtmgl_menu") == 0) {
        unsigned char drop[3] = {255, 209, 102};
        draw_line_px(buffer, x1 - 18, y0 + 7, x1 - 12, y0 + 13, drop, 2);
        draw_line_px(buffer, x1 - 12, y0 + 13, x1 - 6, y0 + 7, drop, 2);
        return;
    }
}

static void draw_rgba_extrusion_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    FILE *f = fopen(obj->source_ref, "r");
    char line[256];
    int values[4096][4];
    int count = 0;
    int resolution = 0;
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int cell;
    int i;
    unsigned char bg[3] = {10, 16, 24};

    if (!f) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }
    while (fgets(line, sizeof(line), f) && count < 4096) {
        int r, g, b, a;
        if (line[0] == '#') {
            if (sscanf(line, "# resolution=%d", &resolution) == 1) {}
            continue;
        }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            values[count][0] = r;
            values[count][1] = g;
            values[count][2] = b;
            values[count][3] = a;
            count++;
        }
    }
    fclose(f);
    if (resolution <= 0) {
        for (resolution = 1; resolution * resolution < count; resolution++) {}
    }
    if (resolution <= 0) return;
    cell = (x1 - x0) / resolution;
    if ((y1 - y0) / resolution < cell) cell = (y1 - y0) / resolution;
    if (cell < 2) cell = 2;
    fill_rect_px(buffer, x0, y0, x1, y1, bg);
    for (i = 0; i < count && i < resolution * resolution; i++) {
        int px_i = i % resolution;
        int py_i = i / resolution;
        int alpha = values[i][3];
        int height = alpha > 0 ? 3 + (alpha / 64) : 0;
        unsigned char rgb[3];
        int px = x0 + px_i * cell;
        int py = y0 + py_i * cell;
        if (alpha <= 0) continue;
        rgb[0] = (unsigned char)values[i][0];
        rgb[1] = (unsigned char)values[i][1];
        rgb[2] = (unsigned char)values[i][2];
        fill_rect_px(buffer, px + height, py - height, px + cell + height, py + cell - height, rgb);
        draw_border_px(buffer, px + height, py - height, px + cell + height, py + cell - height, obj->border, 1);
    }
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

static void draw_image_asset_preview(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int x0 = obj->x * cell_w;
    int y0 = obj->y * cell_h;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1 = (obj->y + obj->h) * cell_h;
    int dst_w = x1 - x0;
    int dst_h = y1 - y0;
    int src_w, src_h, src_channels;
    unsigned char *pixels = NULL;
    unsigned char *cache_pixels = NULL;
    size_t cache_bytes = 0;
    int dy, dx;

    fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
    if (!obj->source_ref[0]) {
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }

    if (is_webcam_frame_source(obj->source_ref) &&
        tpmos_live_frame_cache_read_rgba(WEBCAM_FRAME_CACHE_KEY, &cache_pixels, &cache_bytes, &src_w, &src_h, &src_channels, NULL, NULL) == 0 &&
        cache_pixels && cache_bytes > 0) {
        pixels = cache_pixels;
        cache_pixels = NULL;
    } else {
        pixels = stbi_load(obj->source_ref, &src_w, &src_h, &src_channels, 4);
    }
    if (!pixels) {
        unsigned char miss[3] = {180, 40, 180};
        fill_rect_px(buffer, x0 + 2, y0 + 2, x1 - 2, y1 - 2, miss);
        blit_text(buffer, obj->x + 1, obj->y + (obj->h / 2), "IMG_MISSING", obj->fg, obj->w - 2, cell_w, cell_h);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }

    if (dst_w <= 0 || dst_h <= 0) {
        if (cache_bytes > 0) free(pixels);
        else stbi_image_free(pixels);
        draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
        return;
    }

    for (dy = 0; dy < dst_h; dy++) {
        int sy = (dy * src_h) / dst_h;
        for (dx = 0; dx < dst_w; dx++) {
            int sx = (dx * src_w) / dst_w;
            int src_idx = (sy * src_w + sx) * 4;
            int dst_idx = ((y0 + dy) * WIDTH + (x0 + dx)) * 4;
            unsigned char a = pixels[src_idx + 3];
            if ((x0 + dx) < 0 || (x0 + dx) >= WIDTH || (y0 + dy) < 0 || (y0 + dy) >= HEIGHT) {
                continue;
            }
            if (a < 16) {
                continue;
            }
            buffer[dst_idx] = pixels[src_idx];
            buffer[dst_idx + 1] = pixels[src_idx + 1];
            buffer[dst_idx + 2] = pixels[src_idx + 2];
            buffer[dst_idx + 3] = 255;
        }
    }

    if (cache_bytes > 0) free(pixels);
    else stbi_image_free(pixels);
    draw_border_px(buffer, x0, y0, x1, y1, obj->border, 2);
}

static int count_hex_bits(unsigned int value) {
    int count = 0;
    value &= 0xffU;
    while (value) {
        count += (int)(value & 1U);
        value >>= 1U;
    }
    return count;
}

static void audit_zslice_piece(FILE *f, const FrameObject *obj, int ordinal) {
    FILE *src = fopen(obj->source_ref, "r");
    char line[512];
    int slice_count = 0;
    int row_count = 0;
    int occupied_bits = 0;
    int declared_x = -1;
    int declared_y = -1;
    int declared_z = -1;
    int rx, ry, rz, camera_mode;
    double cam_x, cam_y, cam_z, pitch, yaw, roll;
    int has_projection_meta = parse_cube_meta(obj, &rx, &ry, &rz, &camera_mode, &cam_x, &cam_y, &cam_z, &pitch, &yaw, &roll);

    if (!src) {
        fprintf(f, "PRIMITIVE | %04d | role=zslice_piece render_stage=%s source_exists=0 source_ref=%s source_mtime_epoch=-1 source_bytes=-1 parsed_slices=0 parsed_rows=0 occupied_bits=0 projection_mode=%s final_projection=%d camera_consumed=%d rotation_consumed=%d rot=%d,%d,%d camera=%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f label=%s\n",
            ordinal,
            has_projection_meta ? "projected_wireframe" : "preview",
            obj->source_ref[0] ? obj->source_ref : "none",
            has_projection_meta ? "projected_wireframe_cube" : "stacked_face_preview",
            has_projection_meta ? 1 : 0,
            has_projection_meta ? 1 : 0,
            has_projection_meta ? 1 : 0,
            rx, ry, rz, camera_mode, cam_x, cam_y, cam_z, pitch, yaw, roll,
            obj->label);
        return;
    }
    while (fgets(line, sizeof(line), src)) {
        char *eq;
        if (sscanf(line, "size=%d,%d,%d", &declared_x, &declared_y, &declared_z) == 3) {
            continue;
        }
        if (line[0] != 'z' || !isdigit((unsigned char)line[1])) {
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        slice_count++;
        eq++;
        while (*eq) {
            unsigned int byte_value;
            char *end = eq;
            while (*eq == ',' || isspace((unsigned char)*eq)) eq++;
            if (sscanf(eq, "%2x", &byte_value) == 1) {
                occupied_bits += count_hex_bits(byte_value);
                row_count++;
                end = eq + 2;
            }
            if (end == eq) break;
            eq = end;
            while (*eq && *eq != ',') eq++;
        }
    }
    fclose(src);
    fprintf(f, "PRIMITIVE | %04d | role=zslice_piece render_stage=%s source_exists=1 source_ref=%s source_mtime_epoch=%ld source_bytes=%ld declared_size=%d,%d,%d parsed_slices=%d parsed_rows=%d occupied_bits=%d projection_mode=%s final_projection=%d camera_consumed=%d rotation_consumed=%d rot=%d,%d,%d camera=%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
        ordinal,
        has_projection_meta ? "projected_wireframe" : "preview",
        obj->source_ref,
        file_mtime_epoch(obj->source_ref),
        file_size_bytes(obj->source_ref),
        declared_x,
        declared_y,
        declared_z,
        slice_count,
        row_count,
        occupied_bits,
        has_projection_meta ? "projected_wireframe_cube" : "stacked_face_preview",
        has_projection_meta ? 1 : 0,
        has_projection_meta ? 1 : 0,
        has_projection_meta ? 1 : 0,
        rx,
        ry,
        rz,
        camera_mode,
        cam_x,
        cam_y,
        cam_z,
        pitch,
        yaw,
        roll,
        obj->x,
        obj->y,
        obj->w,
        obj->h,
        obj->label);
}

static void audit_tile_zmap(FILE *f, const FrameObject *obj, int ordinal) {
    FILE *src = fopen(obj->source_ref, "r");
    char line[512];
    int rows = 0;
    int max_cols = 0;
    int solid_count = 0;
    int grass_count = 0;
    int tree_count = 0;
    int rock_count = 0;

    if (!src) {
        fprintf(f, "PRIMITIVE | %04d | role=tile_zmap render_stage=preview source_exists=0 source_ref=%s source_mtime_epoch=-1 source_bytes=-1 rows=0 cols=0 tile_count=0 projection_mode=flat_tile_preview final_projection=0 label=%s\n",
            ordinal, obj->source_ref[0] ? obj->source_ref : "none", obj->label);
        return;
    }
    while (fgets(line, sizeof(line), src)) {
        int i;
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        rows++;
        if ((int)strlen(line) > max_cols) max_cols = (int)strlen(line);
        for (i = 0; line[i]; i++) {
            if (line[i] == '#') solid_count++;
            else if (line[i] == 'T') tree_count++;
            else if (line[i] == 'R') rock_count++;
            else if (line[i] == '.') grass_count++;
        }
    }
    fclose(src);
    fprintf(f, "PRIMITIVE | %04d | role=tile_zmap render_stage=preview source_exists=1 source_ref=%s source_mtime_epoch=%ld source_bytes=%ld rows=%d cols=%d tile_count=%d solid_count=%d grass_count=%d tree_count=%d rock_count=%d projection_mode=flat_tile_preview final_projection=0 camera_consumed=0 surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
        ordinal,
        obj->source_ref,
        file_mtime_epoch(obj->source_ref),
        file_size_bytes(obj->source_ref),
        rows,
        max_cols,
        solid_count + grass_count + tree_count + rock_count,
        solid_count,
        grass_count,
        tree_count,
        rock_count,
        obj->x,
        obj->y,
        obj->w,
        obj->h,
        obj->label);
}

static void audit_rgba_extrusion(FILE *f, const FrameObject *obj, int ordinal) {
    FILE *src = fopen(obj->source_ref, "r");
    char line[512];
    int resolution = 0;
    int pixel_count = 0;
    int visible_count = 0;

    if (!src) {
        fprintf(f, "PRIMITIVE | %04d | role=rgba_extrusion render_stage=preview source_exists=0 source_ref=%s source_mtime_epoch=-1 source_bytes=-1 resolution=0 pixel_count=0 visible_pixels=0 projection_mode=flat_extrusion_preview final_projection=0 label=%s\n",
            ordinal, obj->source_ref[0] ? obj->source_ref : "none", obj->label);
        return;
    }
    while (fgets(line, sizeof(line), src)) {
        int r, g, b, a;
        if (line[0] == '#') {
            sscanf(line, "# resolution=%d", &resolution);
            continue;
        }
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixel_count++;
            if (a > 0) visible_count++;
        }
    }
    fclose(src);
    fprintf(f, "PRIMITIVE | %04d | role=rgba_extrusion render_stage=preview source_exists=1 source_ref=%s source_mtime_epoch=%ld source_bytes=%ld resolution=%d pixel_count=%d visible_pixels=%d projection_mode=flat_extrusion_preview final_projection=0 camera_consumed=0 surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
        ordinal,
        obj->source_ref,
        file_mtime_epoch(obj->source_ref),
        file_size_bytes(obj->source_ref),
        resolution,
        pixel_count,
        visible_count,
        obj->x,
        obj->y,
        obj->w,
        obj->h,
        obj->label);
}

static void audit_widget_surface_probe(FILE *f, const FrameObject *obj, int ordinal) {
    fprintf(f, "PRIMITIVE | %04d | role=widget_surface_probe render_stage=preview source_exists=%d source_ref=%s source_mtime_epoch=%ld source_bytes=%ld projection_mode=chtmgl_widget_preview final_projection=0 camera_consumed=0 surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
        ordinal,
        obj->source_ref[0] && access(obj->source_ref, F_OK) == 0 ? 1 : 0,
        obj->source_ref[0] ? obj->source_ref : "none",
        obj->source_ref[0] ? file_mtime_epoch(obj->source_ref) : -1L,
        obj->source_ref[0] ? file_size_bytes(obj->source_ref) : -1L,
        obj->x,
        obj->y,
        obj->w,
        obj->h,
        obj->label);
}

static void audit_image_asset(FILE *f, const FrameObject *obj, int ordinal) {
    int w = 0, h = 0, channels = 0;
    unsigned char *cache_pixels = NULL;
    size_t cache_bytes = 0;
    if (!obj->source_ref[0]) {
        fprintf(f, "PRIMITIVE | %04d | role=image_asset render_stage=preview source_exists=0 source_ref=none source_mtime_epoch=-1 source_bytes=-1 image_width=0 image_height=0 channels=0 projection_mode=textured_image_rect final_projection=1 label=%s\n",
            ordinal, obj->label);
        return;
    }
    if (is_webcam_frame_source(obj->source_ref) &&
        tpmos_live_frame_cache_read_rgba(WEBCAM_FRAME_CACHE_KEY, &cache_pixels, &cache_bytes, &w, &h, &channels, NULL, NULL) == 0 &&
        cache_pixels && cache_bytes > 0) {
        fprintf(f, "PRIMITIVE | %04d | role=image_asset render_stage=preview source_exists=1 source_ref=%s source_mtime_epoch=-1 source_bytes=%zu image_width=%d image_height=%d channels=%d projection_mode=live_frame_cache final_projection=1 surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
            ordinal, obj->source_ref, cache_bytes, w, h, channels, obj->x, obj->y, obj->w, obj->h, obj->label);
        free(cache_pixels);
        return;
    }
    if (!stbi_info(obj->source_ref, &w, &h, &channels)) {
        fprintf(f, "PRIMITIVE | %04d | role=image_asset render_stage=preview source_exists=0 source_ref=%s source_mtime_epoch=%ld source_bytes=%ld image_width=0 image_height=0 channels=0 projection_mode=textured_image_rect final_projection=1 label=%s\n",
            ordinal, obj->source_ref, file_mtime_epoch(obj->source_ref), file_size_bytes(obj->source_ref), obj->label);
        return;
    }
    fprintf(f, "PRIMITIVE | %04d | role=image_asset render_stage=preview source_exists=1 source_ref=%s source_mtime_epoch=%ld source_bytes=%ld image_width=%d image_height=%d channels=%d projection_mode=textured_image_rect final_projection=1 surface_cell_x=%d surface_cell_y=%d surface_cell_w=%d surface_cell_h=%d label=%s\n",
        ordinal, obj->source_ref, file_mtime_epoch(obj->source_ref), file_size_bytes(obj->source_ref),
        w, h, channels, obj->x, obj->y, obj->w, obj->h, obj->label);
}

static void write_primitive_audit_section(FILE *f, const FrameObject *objects, int object_count) {
    int i;
    int primitive_count = 0;
    fprintf(f, "SECTION | PRIMITIVES | SOURCE_AND_CONVERSION_AUDIT\n");
    for (i = 0; i < object_count; i++) {
        const FrameObject *obj = &objects[i];
        if (strcmp(obj->role, "zslice_piece") == 0) {
            primitive_count++;
            audit_zslice_piece(f, obj, i + 1);
        } else if (strcmp(obj->role, "tile_zmap") == 0) {
            primitive_count++;
            audit_tile_zmap(f, obj, i + 1);
        } else if (strcmp(obj->role, "rgba_extrusion") == 0) {
            primitive_count++;
            audit_rgba_extrusion(f, obj, i + 1);
        } else if (strcmp(obj->role, "image_asset") == 0) {
            primitive_count++;
            audit_image_asset(f, obj, i + 1);
        } else if (strcmp(obj->role, "widget_surface_probe") == 0) {
            primitive_count++;
            audit_widget_surface_probe(f, obj, i + 1);
        }
    }
    fprintf(f, "primitive_count=%d\n", primitive_count);
}

static int kvp_value(const char *line, const char *key, char *out, size_t out_sz) {
    const char *start, *end;
    size_t len;
    char needle[64];
    if (!line || !key || !out || out_sz == 0) return 0;
    snprintf(needle, sizeof(needle), "%s=", key);
    start = strstr(line, needle);
    if (!start) return 0;
    start += strlen(needle);
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end)) end++;
    len = (size_t)(end - start);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static void sync_presenter_mode(void) {
    FILE *f = fopen(WRAITH_UI_STATE, "r");
    char line[512];
    g_presenter_ascii_mode = 0;
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *value;
        if (!eq) continue;
        *eq = '\0';
        value = eq + 1;
        while (*value && isspace((unsigned char)*value)) value++;
        value[strcspn(value, "\r\n")] = '\0';
        if (strcmp(line, "desktop_presenter_mode") == 0) {
            g_presenter_ascii_mode = (strcmp(value, "gl") != 0);
            break;
        }
    }
    fclose(f);
}

static int semantic_source_is_allowed(const SemanticSourceInfo *info) {
    if (!info || !info->valid) return 0;
    if (info->project_id[0] == '\0') return 0;
    if (strcmp(info->project_id, "wraith-pm") == 0) return 0;
    if (strcmp(info->project_id, "wraith-alpha") == 0) return 1;
    if (strncmp(info->project_id, "wraith/", 7) == 0) return 1;
    return 0;
}

static void write_rgb_receipt(
    const char *mode,
    const char *semantic_status,
    const SemanticSourceInfo *info,
    const FrameObject *objects,
    int object_count,
    int cell_w,
    int cell_h,
    const unsigned char bg[3],
    unsigned long long render_checksum
) {
    int i;
    FILE *f = fopen(RGB_RECEIPT_PATH, "w");
    if (!f) return;
    time_t now = time(NULL);
    struct tm tm_utc;
    char iso_time[32];
    long frame_mtime = file_mtime_epoch(SEMANTIC_META_PATH);
    long objects_mtime = file_mtime_epoch(SEMANTIC_OBJECTS_PATH);
    long receipt_mtime = file_mtime_epoch(RGB_RECEIPT_PATH);
    iso_time[0] = '\0';
    if (gmtime_r(&now, &tm_utc) != NULL) {
        strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    }
    fprintf(f, "receipt_type=rgb_presenter_audit\n");
    fprintf(f, "generated_by=wraith_rgb_daemon\n");
    fprintf(f, "generated_at_epoch=%ld\n", (long)now);
    fprintf(f, "generated_at_iso_utc=%s\n", iso_time[0] ? iso_time : "unknown");
    fprintf(f, "receipt_generation_key=%s@%ld\n", info && info->project_id[0] ? info->project_id : "unknown", (long)now);
    fprintf(f, "mode=%s\n", mode ? mode : "unknown");
    fprintf(f, "semantic_status=%s\n", semantic_status ? semantic_status : "unknown");
    fprintf(f, "source_frame_txt=pieces/display/current_frame.txt\n");
    fprintf(f, "source_objects_pdl=%s\n", SEMANTIC_OBJECTS_PATH);
    fprintf(f, "source_meta_pdl=%s\n", SEMANTIC_META_PATH);
    fprintf(f, "source_meta_mtime_epoch=%ld\n", frame_mtime);
    fprintf(f, "source_objects_mtime_epoch=%ld\n", objects_mtime);
    fprintf(f, "previous_receipt_mtime_epoch=%ld\n", receipt_mtime);
    fprintf(f, "output_rgba32=%s\n", WRAITH_FRAME_SOURCE);
    fprintf(f, "viewport_width_px=%d\n", WIDTH);
    fprintf(f, "viewport_height_px=%d\n", HEIGHT);
    fprintf(f, "cell_width_px=%d\n", cell_w);
    fprintf(f, "cell_height_px=%d\n", cell_h);
    fprintf(f, "glyph_width_px=%d\n", GLYPH_W);
    fprintf(f, "glyph_height_px=%d\n", GLYPH_H);
    fprintf(f, "canvas_origin_x=0\n");
    fprintf(f, "canvas_origin_y=0\n");
    fprintf(f, "render_origin=top_left\n");
    fprintf(f, "render_y_axis=down\n");
    fprintf(f, "text_anchor=cell_top_left\n");
    fprintf(f, "text_clipping=on\n");
    fprintf(f, "sort_order=ascending_z\n");
    fprintf(f, "background_rgb=#%02X%02X%02X\n", bg[0], bg[1], bg[2]);
    fprintf(f, "render_checksum_fnv1a64=0x%016llX\n", render_checksum);
    fprintf(f, "object_count=%d\n", object_count);
    if (info) {
        int mouse_visual_cell_x = (info->mouse_x / cell_w) + info->mouse_hit_offset_x;
        int mouse_visual_cell_y = (info->mouse_y / cell_h) + info->mouse_hit_offset_y;
        fprintf(f, "source_project_id=%s\n", info->project_id);
        fprintf(f, "source_layout=%s\n", info->source_layout);
        fprintf(f, "focused_object_id=%s\n", info->focused_object_id);
        fprintf(f, "focused_object_dom_id=%s\n", info->focused_object_dom_id);
        fprintf(f, "mouse_x=%d\n", info->mouse_x);
        fprintf(f, "mouse_y=%d\n", info->mouse_y);
        fprintf(f, "mouse_hit_offset_x=%d\n", info->mouse_hit_offset_x);
        fprintf(f, "mouse_hit_offset_y=%d\n", info->mouse_hit_offset_y);
        fprintf(f, "mouse_cursor_visual_uses_offset=%d\n", info->mouse_cursor_visual_uses_offset);
        fprintf(f, "mouse_visual_cell_x=%d\n", mouse_visual_cell_x);
        fprintf(f, "mouse_visual_cell_y=%d\n", mouse_visual_cell_y);
    }
    fprintf(f, "SECTION | OBJECTS | DERIVED_PIXEL_BOUNDS\n");
    for (i = 0; i < object_count; i++) {
        const FrameObject *obj = &objects[i];
        char fg_hex[8];
        char bg_hex[8];
        char border_hex[8];
        int x0 = obj->x * cell_w;
        int render_y = obj->y;
        int override_y = footer_row_y_for_role(obj, cell_h);
        int y0;
        int x1 = (obj->x + obj->w) * cell_w;
        int y1;
        int clip_x0 = x0 < 0 ? 0 : x0;
        int clip_y0;
        int clip_x1 = x1 > WIDTH ? WIDTH : x1;
        int clip_y1;
        int visible;
        const char *clip_reason = "none";
        char display_label[MAX_LABEL];
        int text_col;
        int text_row;
        int max_cols;
        int rendered_chars;
        int text_px_x0 = -1;
        int text_px_y0 = -1;
        int text_px_x1 = -1;
        int text_px_y1 = -1;
        int focus_px_x0 = -1;
        int focus_px_y0 = -1;
        int focus_px_x1 = -1;
        int focus_px_y1 = -1;
        const char *focus_rect_source = "none";

        if (override_y >= 0) {
            render_y = override_y;
        }
        y0 = render_y * cell_h;
        y1 = (render_y + obj->h) * cell_h;
        clip_y0 = y0 < 0 ? 0 : y0;
        clip_y1 = y1 > HEIGHT ? HEIGHT : y1;
        visible = (clip_x0 < clip_x1 && clip_y0 < clip_y1);

        if (x1 <= 0 || y1 <= 0 || x0 >= WIDTH || y0 >= HEIGHT) {
            clip_reason = "offscreen";
        } else if (clip_x0 != x0 || clip_y0 != y0 || clip_x1 != x1 || clip_y1 != y1) {
            clip_reason = "clipped_to_viewport";
        }

        color_to_hex(obj->fg, fg_hex);
        color_to_hex(obj->bg, bg_hex);
        color_to_hex(obj->border, border_hex);
        build_display_label(obj, display_label, sizeof(display_label));
        text_col = obj->x + ((strcmp(obj->tag, "text") == 0) ? 0 : 1);
        text_row = render_y;
        max_cols = obj->w - ((strcmp(obj->tag, "text") == 0) ? 0 : 2);
        if (max_cols < 0) max_cols = 0;
        rendered_chars = (int)strlen(display_label);
        if (max_cols >= 0 && rendered_chars > max_cols) {
            rendered_chars = max_cols;
        }
        if (display_label[0] != '\0' && rendered_chars > 0) {
            text_px_x0 = text_col * cell_w;
            text_px_y0 = text_row * cell_h;
            text_px_x1 = text_px_x0 + (rendered_chars * cell_w);
            text_px_y1 = text_px_y0 + GLYPH_H;
        }
        if (focused_outline_is_red(obj)) {
            focus_px_x0 = x0;
            focus_px_y0 = y0;
            focus_px_x1 = x1;
            focus_px_y1 = y1;
            focus_rect_source = "hit_rect";
        }
        fprintf(f, "OBJECT | %04d | tag=%s role=%s draw_index=%d z=%d focused=%d nav=%d nav_selected=%d nav_selector_glyph=%s parent_id=%s container_id=%s source_ref=%s target_surface=%s ancestor_chain=%s clip_chain=%s x=%d y=%d render_y=%d w=%d h=%d px_x0=%d px_y0=%d px_x1=%d px_y1=%d clip_x0=%d clip_y0=%d clip_x1=%d clip_y1=%d visible=%d clip_reason=%s text_col=%d text_row=%d text_px_x0=%d text_px_y0=%d text_px_x1=%d text_px_y1=%d rendered_chars=%d hit_px_x0=%d hit_px_y0=%d hit_px_x1=%d hit_px_y1=%d focus_rect_source=%s focus_px_x0=%d focus_px_y0=%d focus_px_x1=%d focus_px_y1=%d fg=%s bg=%s border=%s label_core=%s label=%s action=%s\n",
            i + 1,
            obj->tag,
            obj->role,
            i,
            obj->z,
            obj->focused,
            obj->nav,
            obj->nav_selected,
            obj->nav_selector_glyph[0] ? obj->nav_selector_glyph : " ",
            obj->parent_id[0] ? obj->parent_id : "none",
            obj->container_id[0] ? obj->container_id : "none",
            obj->source_ref[0] ? obj->source_ref : "none",
            obj->target_surface[0] ? obj->target_surface : "-",
            obj->ancestor_chain[0] ? obj->ancestor_chain : "none",
            obj->clip_chain[0] ? obj->clip_chain : "none",
            obj->x,
            obj->y,
            render_y,
            obj->w,
            obj->h,
            x0,
            y0,
            x1,
            y1,
            clip_x0,
            clip_y0,
            clip_x1,
            clip_y1,
            visible,
            clip_reason,
            text_col,
            text_row,
            text_px_x0,
            text_px_y0,
            text_px_x1,
            text_px_y1,
            rendered_chars,
            x0,
            y0,
            x1,
            y1,
            focus_rect_source,
            focus_px_x0,
            focus_px_y0,
            focus_px_x1,
            focus_px_y1,
            fg_hex,
            bg_hex,
            border_hex,
            obj->label_core,
            obj->label,
            obj->action);
    }
    write_primitive_audit_section(f, objects, object_count);
    fclose(f);
}

static int parse_frame_meta(unsigned char bg[3], int *cell_w, int *cell_h, SemanticSourceInfo *info) {
    FILE *f = fopen(SEMANTIC_META_PATH, "r");
    char line[1024];
    int loaded = 0;
    bg[0] = 15; bg[1] = 23; bg[2] = 32;
    /* Must match GLYPH_W/GLYPH_H (this file's own WIDTH/HEIGHT are
       COLS*GLYPH_W/ROWS*GLYPH_H) -- this is the fallback used only if
       current_frame.meta.pdl is missing; wraith-alpha_manager.c's own
       emission of these same two fields was fixed 2026-07-13 for the
       identical reason (see that file's comment above its
       cell_width_px/cell_height_px fprintf calls). This default
       previously said 10/18, same wrong guess. */
    *cell_w = GLYPH_W;
    *cell_h = GLYPH_H;
    if (info) {
        memset(info, 0, sizeof(*info));
    }
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "FRAME | cell_width_px |")) {
            *cell_w = atoi(strrchr(line, '|') + 1);
            loaded = 1;
        } else if (strstr(line, "FRAME | cell_height_px |")) {
            *cell_h = atoi(strrchr(line, '|') + 1);
            loaded = 1;
        } else if (info && strstr(line, "FRAME | project_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->project_id, value, sizeof(info->project_id) - 1);
                info->project_id[strcspn(info->project_id, "\r\n")] = '\0';
                info->valid = 1;
            }
        } else if (info && strstr(line, "FRAME | source_layout |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->source_layout, value, sizeof(info->source_layout) - 1);
                info->source_layout[strcspn(info->source_layout, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | focused_object_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->focused_object_id, value, sizeof(info->focused_object_id) - 1);
                info->focused_object_id[strcspn(info->focused_object_id, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | focused_object_dom_id |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                strncpy(info->focused_object_dom_id, value, sizeof(info->focused_object_dom_id) - 1);
                info->focused_object_dom_id[strcspn(info->focused_object_dom_id, "\r\n")] = '\0';
            }
        } else if (info && strstr(line, "FRAME | mouse_x |")) {
            info->mouse_x = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_y |")) {
            info->mouse_y = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_hit_offset_x |")) {
            info->mouse_hit_offset_x = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_hit_offset_y |")) {
            info->mouse_hit_offset_y = atoi(strrchr(line, '|') + 1);
        } else if (info && strstr(line, "FRAME | mouse_cursor_visual_uses_offset |")) {
            char *value = strrchr(line, '|');
            if (value) {
                value = value + 1;
                while (*value && isspace((unsigned char)*value)) value++;
                info->mouse_cursor_visual_uses_offset = (strncmp(value, "true", 4) == 0 || atoi(value) != 0);
            }
        }
    }
    fclose(f);
    return loaded;
}

static int parse_frame_objects(FrameObject objects[], int max_objects) {
    FILE *f = fopen(SEMANTIC_OBJECTS_PATH, "r");
    char line[2048];
    int count = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f) && count < max_objects) {
        FrameObject *obj;
        char value[256];
        char *label_start;
        char *action_start;
        if (strncmp(line, "OBJECT |", 8) != 0) continue;
        obj = &objects[count];
        memset(obj, 0, sizeof(*obj));
        strcpy(obj->tag, "text");
        strcpy(obj->role, "text");
        obj->fg[0] = 232; obj->fg[1] = 241; obj->fg[2] = 242;
        obj->bg[0] = 15; obj->bg[1] = 23; obj->bg[2] = 32;
        obj->border[0] = 126; obj->border[1] = 223; obj->border[2] = 242;
        if (kvp_value(line, "tag", value, sizeof(value))) strncpy(obj->tag, value, sizeof(obj->tag) - 1);
        if (kvp_value(line, "role", value, sizeof(value))) strncpy(obj->role, value, sizeof(obj->role) - 1);
        if (kvp_value(line, "nav", value, sizeof(value))) obj->nav = atoi(value);
        if (kvp_value(line, "nav_selected", value, sizeof(value))) obj->nav_selected = (strcmp(value, "true") == 0);
        if (kvp_value(line, "nav_selector_glyph", value, sizeof(value))) strncpy(obj->nav_selector_glyph, value, sizeof(obj->nav_selector_glyph) - 1);
        if (kvp_value(line, "parent_id", value, sizeof(value))) strncpy(obj->parent_id, value, sizeof(obj->parent_id) - 1);
        if (kvp_value(line, "container_id", value, sizeof(value))) strncpy(obj->container_id, value, sizeof(obj->container_id) - 1);
        if (kvp_value(line, "source_ref", value, sizeof(value))) strncpy(obj->source_ref, value, sizeof(obj->source_ref) - 1);
        if (kvp_value(line, "target_surface", value, sizeof(value))) strncpy(obj->target_surface, value, sizeof(obj->target_surface) - 1);
        if (kvp_value(line, "ancestor_chain", value, sizeof(value))) strncpy(obj->ancestor_chain, value, sizeof(obj->ancestor_chain) - 1);
        if (kvp_value(line, "clip_chain", value, sizeof(value))) strncpy(obj->clip_chain, value, sizeof(obj->clip_chain) - 1);
        if (kvp_value(line, "x", value, sizeof(value))) obj->x = atoi(value);
        if (kvp_value(line, "y", value, sizeof(value))) obj->y = atoi(value);
        if (kvp_value(line, "w", value, sizeof(value))) obj->w = atoi(value);
        if (kvp_value(line, "h", value, sizeof(value))) obj->h = atoi(value);
        if (kvp_value(line, "z", value, sizeof(value))) obj->z = atoi(value);
        if (kvp_value(line, "focused", value, sizeof(value))) obj->focused = (strcmp(value, "true") == 0);
        if (kvp_value(line, "fg", value, sizeof(value))) parse_hex_color(value, obj->fg);
        if (kvp_value(line, "bg", value, sizeof(value))) parse_hex_color(value, obj->bg);
        if (kvp_value(line, "border", value, sizeof(value))) parse_hex_color(value, obj->border);
        if (kvp_value(line, "label_core", value, sizeof(value))) strncpy(obj->label_core, value, sizeof(obj->label_core) - 1);
        label_start = strstr(line, "label=");
        if (label_start) {
            label_start += 6;
            action_start = strstr(label_start, " action=");
            if (!action_start) action_start = line + strlen(line);
            {
                size_t len = (size_t)(action_start - label_start);
                if (len >= sizeof(obj->label)) len = sizeof(obj->label) - 1;
                memcpy(obj->label, label_start, len);
                obj->label[len] = '\0';
            }
        }
        action_start = strstr(line, "action=");
        if (action_start) {
            char *src_start;
            action_start += 7;
            src_start = strstr(action_start, " src=");
            if (!src_start) src_start = line + strlen(line);
            {
                size_t len = (size_t)(src_start - action_start);
                if (len >= sizeof(obj->action)) len = sizeof(obj->action) - 1;
                memcpy(obj->action, action_start, len);
                obj->action[len] = '\0';
            }
        }
        count++;
    }
    fclose(f);
    return count;
}

static void sort_objects(FrameObject objects[], int count) {
    int i, j;
    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (objects[j].z < objects[i].z) {
                FrameObject tmp = objects[i];
                objects[i] = objects[j];
                objects[j] = tmp;
            }
        }
    }
}

static void draw_object(unsigned char *buffer, const FrameObject *obj, int cell_w, int cell_h) {
    int render_y = obj->y;
    int y_override = footer_row_y_for_role(obj, cell_h);
    int x0 = obj->x * cell_w;
    int y0;
    int x1 = (obj->x + obj->w) * cell_w;
    int y1;
    unsigned char border_rgb[3];
    char display_label[MAX_LABEL];
    memcpy(border_rgb, obj->border, sizeof(border_rgb));
    if (focused_outline_is_red(obj)) {
        border_rgb[0] = 255; border_rgb[1] = 64; border_rgb[2] = 64;
    }

    if (y_override >= 0) {
        render_y = y_override;
    }
    y0 = render_y * cell_h;
    y1 = (render_y + obj->h) * cell_h;

    if (strcmp(obj->tag, "window") == 0 || strcmp(obj->tag, "panel") == 0 || strcmp(obj->tag, "header") == 0 ||
        strcmp(obj->role, "window_toolbar_item") == 0 || is_chtmgl_control_role(obj)) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, border_rgb, 2);
    }
    if (strcmp(obj->tag, "text") == 0) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
    }
    if (strcmp(obj->tag, "surface") == 0) {
        fill_rect_px(buffer, x0, y0, x1, y1, obj->bg);
        draw_border_px(buffer, x0, y0, x1, y1, border_rgb, 2);
    }
    if (strcmp(obj->role, "zslice_piece") == 0) {
        draw_zslice_piece_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "tile_zmap") == 0) {
        draw_tile_zmap_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "rgba_extrusion") == 0) {
        draw_rgba_extrusion_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "image_asset") == 0) {
        draw_image_asset_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "widget_surface_probe") == 0) {
        draw_widget_surface_preview(buffer, obj, cell_w, cell_h);
        return;
    }
    if (strcmp(obj->role, "text_asset") == 0) {
        draw_text_asset(buffer, obj, cell_w, cell_h);
        return;
    }
    if (is_chtmgl_control_role(obj)) {
        draw_chtmgl_control_shape(buffer, obj, x0, y0, x1, y1);
    }

    build_display_label(obj, display_label, sizeof(display_label));
    if (display_label[0] != '\0') {
        int text_col = obj->x + ((strcmp(obj->tag, "text") == 0) ? 0 : 1);
        int text_row = render_y;
        int max_cols = obj->w - ((strcmp(obj->tag, "text") == 0) ? 0 : 2);
        if (max_cols < 0) max_cols = 0;
        blit_text(buffer, text_col, text_row, display_label, obj->fg, max_cols, cell_w, cell_h);
    }

    if (focused_outline_is_red(obj)) {
        unsigned char focus_rgb[3] = {255, 64, 64};
        draw_border_px(buffer, x0, y0, x1, y1, focus_rgb, 1);
    }
}

static int render_semantic_frame(unsigned char *buffer) {
    FrameObject objects[MAX_OBJECTS];
    unsigned char bg[3];
    int cell_w, cell_h;
    int count, i;
    SemanticSourceInfo info;
    unsigned long long checksum;
    parse_frame_meta(bg, &cell_w, &cell_h, &info);
    if (!semantic_source_is_allowed(&info)) {
        write_rgb_receipt("gl", "rejected_non_wraith_semantic_source", &info, NULL, 0, cell_w, cell_h, bg, 0);
        return 0;
    }
    count = parse_frame_objects(objects, MAX_OBJECTS);
    if (count <= 0) {
        write_rgb_receipt("gl", "rejected_empty_semantic_scene", &info, NULL, 0, cell_w, cell_h, bg, 0);
        return 0;
    }
    clear_buffer(buffer, bg[0], bg[1], bg[2]);
    sort_objects(objects, count);
    for (i = 0; i < count; i++) {
        draw_object(buffer, &objects[i], cell_w, cell_h);
    }
    checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
    write_rgb_receipt("gl", "accepted_semantic_scene", &info, objects, count, cell_w, cell_h, bg, checksum);
    return 1;
}

static void render_ascii_frame(const char *frame_path, unsigned char *buffer) {
    FILE *f;
    char line[1024];
    int row = 0;
    clear_buffer(buffer, 0, 0, 68);
    f = fopen(frame_path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && row < ROWS) {
        int col = 0;
        int byte_idx = 0;
        unsigned char r = 200, g = 200, b = 200;
        if (strstr(line, "[>]")) {
            r = 0; g = 255; b = 255;
        }
        while (byte_idx < (int)strlen(line) && col < COLS) {
            unsigned char c = (unsigned char)line[byte_idx];
            if (c == '\n' || c == '\r') break;
            int byte_len;
            uint32_t cp = decode_utf8_codepoint(&line[byte_idx], &byte_len);
            unsigned char rgb[3] = {r, g, b};
            blit_codepoint(buffer, col, row, cp, rgb, GLYPH_W, GLYPH_H);
            byte_idx += byte_len;
            col++;
        }
        row++;
    }
    fclose(f);
}

static void pulse_rgb(void) {
    FILE *f = fopen("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt", "a");
    if (f) {
        fprintf(f, "P\n");
        fclose(f);
    }
}

int main(void) {
    struct stat st;
    off_t last_size = 0;
    const char *trigger = "pieces/display/frame_changed.txt";
    const char *frame_src = "pieces/display/current_frame.txt";
    const char *output = "projects/wraith-alpha/session/rgb/current_frame.rgba32";
    unsigned char *buffer;

    printf("[RGB-DAEMON] Starting Wraith RGB converter...\n");
    load_glyphs();
    load_emoji_config();
    init_emoji_renderer();
    buffer = malloc(WIDTH * HEIGHT * 4);
    if (!buffer) return 1;

    if (stat(trigger, &st) == 0) last_size = st.st_size;

    while (1) {
        static int rendered_initial_frame = 0;
        int dirty = !rendered_initial_frame;
        FILE *f;
        if (stat(trigger, &st) == 0 && st.st_size != last_size) {
            last_size = st.st_size;
            dirty = 1;
        }
        if (!dirty) {
            usleep(16667);
            continue;
        }

        sync_presenter_mode();
        if (g_presenter_ascii_mode) {
            render_ascii_frame(frame_src, buffer);
            {
                unsigned char ascii_bg[3] = {0, 0, 68};
                unsigned long long checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
                write_rgb_receipt("ascii", "ascii_frame_rendered", NULL, NULL, 0, 10, 18, ascii_bg, checksum);
            }
        } else if (!render_semantic_frame(buffer)) {
            render_ascii_frame(frame_src, buffer);
            {
                unsigned char fallback_bg[3] = {0, 0, 68};
                unsigned long long checksum = checksum_buffer(buffer, WIDTH * HEIGHT * 4);
                write_rgb_receipt("gl", "fallback_to_ascii_frame", NULL, NULL, 0, 10, 18, fallback_bg, checksum);
            }
        }

        f = fopen(output, "wb");
        if (f) {
            fwrite(buffer, 1, WIDTH * HEIGHT * 4, f);
            fclose(f);
            pulse_rgb();
        }
        rendered_initial_frame = 1;
        usleep(16667);
    }

    cleanup_emoji();
    free(buffer);
    return 0;
}
