/* yahoo_compose_rgb_frame - convert ASCII current_frame.txt to RGBA32
 * rgb_frame.raw for gl_mirror. Modeled on chtpm_rgb_render.c's own
 * render_once()/blit_text()/blit_char() pipeline.
 *
 * Usage: yahoo_compose_rgb_frame.+x
 * Output: pieces/display/rgb_frame.raw + rgb_frame.receipt.txt
 *         grows pieces/display/rgb_frame_changed.txt when done
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define GLYPH_W 8
#define GLYPH_H 16
#define FRAME_W 640
#define FRAME_H 768
#define MAX_TEXT_ROWS 48
#define PATH_BUF 4096

static char project_root[PATH_BUF] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

static void blit_char(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py,
                      unsigned char c, unsigned char r, unsigned char g, unsigned char b) {
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

static void blit_solid_block(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, int w,
                             unsigned char r, unsigned char g, unsigned char b);

static void blit_text(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py,
                      const char *text, unsigned char r, unsigned char g, unsigned char b) {
    int x_px = px;
    const char *p = text;
    while (*p && x_px < FRAME_W) {
        unsigned char lead = (unsigned char)*p;
        if (lead >= 0x80) {
            int seqlen = 1;
            if ((lead & 0xE0) == 0xC0) seqlen = 2;
            else if ((lead & 0xF0) == 0xE0) seqlen = 3;
            else if ((lead & 0xF8) == 0xF0) seqlen = 4;
            blit_solid_block(fb, x_px, py, GLYPH_W, 128, 128, 128);
            p += seqlen;
            x_px += GLYPH_W;
            continue;
        }
        blit_char(fb, x_px, py, lead, r, g, b);
        p++;
        x_px += GLYPH_W;
    }
}

static void blit_solid_block(unsigned char fb[FRAME_H][FRAME_W][4], int px, int py, int w,
                             unsigned char r, unsigned char g, unsigned char b) {
    for (int y = 0; y < GLYPH_H; y++) {
        int fy = py + y;
        if (fy < 0 || fy >= FRAME_H) continue;
        for (int x = 0; x < w; x++) {
            int fx = px + x;
            if (fx < 0 || fx >= FRAME_W) continue;
            fb[fy][fx][0] = r;
            fb[fy][fx][1] = g;
            fb[fy][fx][2] = b;
            fb[fy][fx][3] = 255;
        }
    }
}

static uint64_t checksum_buffer(const unsigned char *buf, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void write_file_atomic(const char *path, const void *data, size_t len) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    fwrite(data, 1, len, f);
    fclose(f);
    rename(tmp, path);
}

static void write_receipt(const char *path, size_t byte_count, uint64_t checksum) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "frame_w=%d\n", FRAME_W);
    fprintf(f, "frame_h=%d\n", FRAME_H);
    fprintf(f, "byte_count=%zu\n", byte_count);
    fprintf(f, "checksum=%llu\n", (unsigned long long)checksum);
    fclose(f);
    rename(tmp, path);
}

static void pulse_rgb_ready(const char *path) {
    FILE *f = fopen(path, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static void render_once(unsigned char fb[FRAME_H][FRAME_W][4], const char *frame_path) {
    memset(fb, 0, (size_t)FRAME_H * FRAME_W * 4);
    for (int y = 0; y < FRAME_H; y++)
        for (int x = 0; x < FRAME_W; x++)
            fb[y][x][3] = 255;

    FILE *f = fopen(frame_path, "r");
    if (!f) return;

    char line[512];
    int row = 0;
    while (row < MAX_TEXT_ROWS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        blit_text(fb, 0, row * GLYPH_H, line, 255, 255, 255);
        row++;
    }
    fclose(f);
}

int main(void) {
    resolve_root();
    load_glyphs();

    char frame_path[PATH_BUF], out_path[PATH_BUF], receipt_path[PATH_BUF], rgb_pulse_path[PATH_BUF];
    snprintf(frame_path, sizeof(frame_path), "%s/pieces/display/current_frame.txt", project_root);
    snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame.raw", project_root);
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    snprintf(rgb_pulse_path, sizeof(rgb_pulse_path), "%s/pieces/display/rgb_frame_changed.txt", project_root);

    static unsigned char fb[FRAME_H][FRAME_W][4];
    render_once(fb, frame_path);
    size_t byte_count = (size_t)FRAME_W * FRAME_H * 4;
    write_file_atomic(out_path, fb, byte_count);
    write_receipt(receipt_path, byte_count, checksum_buffer((unsigned char *)fb, byte_count));
    pulse_rgb_ready(rgb_pulse_path);

    return 0;
}
