/* rmmc_sprite_op - draw the hotbar sprites of sprites.pdl (DATA: palette + 16x16 letter rows) into hotbar/<name>-bgr.png (48x48 RGB, 3x nearest, R and B swapped: the editor swaps them back, as for the block sprites).
 *   rmmc_sprite_op.+x <book dir> [name]     all SPRITE rows, or only <name>; prints one line per file; exit 0 ok | 2 usage/bad file | 3 a sprite is not 16x16 | 4 unknown letter | 6 cannot write
 * One authority per fact: the picture lives in sprites.pdl; the PNG is a PROJECTION (safe to delete, rewritten byte for byte).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_sprite_op.+x rmmc_sprite_op.c -lpng */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char PR[128][3]; static int have[128];
static int emit(const char *path, char rows[16][17], int n) {
    if (n != 16) return 3; static png_byte img[48][48 * 3];
    for (int y = 0; y < 16; y++) { if (strlen(rows[y]) != 16) return 3; for (int x = 0; x < 16; x++) { unsigned char c = (unsigned char)rows[y][x]; if (c >= 128 || !have[c]) return 4;
        for (int dy = 0; dy < 3; dy++) for (int dx = 0; dx < 3; dx++) { png_byte *p = &img[y * 3 + dy][(x * 3 + dx) * 3]; p[0] = PR[c][2]; p[1] = PR[c][1]; p[2] = PR[c][0]; } } }
    FILE *f = fopen(path, "wb"); if (!f) return 6; png_structp s = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop i = png_create_info_struct(s);
    if (setjmp(png_jmpbuf(s))) { fclose(f); return 6; }
    png_init_io(s, f); png_set_IHDR(s, i, 48, 48, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT); png_write_info(s, i);
    for (int y = 0; y < 48; y++) png_write_row(s, img[y]); png_write_end(s, NULL); png_destroy_write_struct(&s, &i); fclose(f); return 0; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: rmmc_sprite_op <book dir> [name]\n"); return 2; }
    char sp[PATH_MAX + 32]; snprintf(sp, sizeof sp, "%s/sprites.pdl", argv[1]); FILE *f = fopen(sp, "r"); if (!f) { fprintf(stderr, "rmmc_sprite_op: %s missing\n", sp); return 2; }
    char line[256], name[64] = "", rows[16][17]; int n = 0, rc = 0, open = 0;
    #define FLUSH() do { if (open && (argc < 3 || !strcmp(argv[2], name))) { char out[PATH_MAX + 96]; snprintf(out, sizeof out, "%s/hotbar/%s-bgr.png", argv[1], name); int r = emit(out, rows, n); if (r) { fprintf(stderr, "rmmc_sprite_op: %s: code %d\n", name, r); rc = r; } else printf("wrote %s\n", out); } } while (0)
    while (fgets(line, sizeof line, f)) {
        char k[16], a[64], b[64], c[64], d[64]; int m = sscanf(line, " %15[^| ] | %63[^| ] | %63[^| ] | %63[^| \r\n] | %63[^| \r\n]", k, a, b, c, d);
        a[strcspn(a, "\r\n")] = 0;
        if (m >= 5 && !strcmp(k, "PAL")) { unsigned char l = (unsigned char)a[0]; if (l < 128) { PR[l][0] = (unsigned char)atoi(b); PR[l][1] = (unsigned char)atoi(c); PR[l][2] = (unsigned char)atoi(d); have[l] = 1; } }
        else if (m >= 2 && !strcmp(k, "SPRITE")) { FLUSH(); snprintf(name, sizeof name, "%s", a); open = 1; n = 0; }
        else if (m >= 2 && !strcmp(k, "ROW") && open && n < 17) { if (n < 16) snprintf(rows[n], 17, "%.16s", a); n++; } }
    FLUSH(); fclose(f); return rc; }
