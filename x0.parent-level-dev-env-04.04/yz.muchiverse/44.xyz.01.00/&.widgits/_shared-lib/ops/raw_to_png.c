/* raw_to_png - turn a raw RGBA frame (the board engine's pieces/display/rgb_frame*.raw, 4 bytes per pixel, row after row, top row first) into a PNG, so a render can be LOOKED AT and measured (frame_stats).
 * Replaces the throw-away python snippets that did this by hand (BOOK 06-testing, owner 2026-10-28: "we need to have confident testing tools").
 * Usage:  raw_to_png <in.raw> <out.png> --size <W>x<H>        or       raw_to_png <in.raw> <out.png> --receipt <receipt.txt>      (the receipt has overlay_w=<W> and overlay_h=<H>, or w=<W> and h=<H>)
 * Refused (exit 2, nothing written): a file whose length is not exactly W*H*4, W or H outside 1..16384, no size. Exit 1: cannot read the input or write the output. Exit 0: wrote the PNG; prints `wrote <out.png> <W>x<H>`.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/raw_to_png.+x raw_to_png.c -lpng */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

static int key_int(const char *txt, const char *key) { char pat[40]; snprintf(pat, sizeof pat, "%s=", key); const char *p = txt; while ((p = strstr(p, pat))) { if (p == txt || p[-1] == '\n' || p[-1] == ' ' || p[-1] == '|') return atoi(p + strlen(pat)); p++; } return -1; }
int main(int argc, char **argv) {
    if (argc < 5) goto usage;
    const char *in = argv[1], *outp = argv[2]; long w = 0, h = 0;
    if (!strcmp(argv[3], "--size")) { if (sscanf(argv[4], "%ldx%ld", &w, &h) != 2) goto usage; }
    else if (!strcmp(argv[3], "--receipt")) { FILE *r = fopen(argv[4], "r"); if (!r) { fprintf(stderr, "raw_to_png: cannot read the receipt %s\n", argv[4]); return 1; } char t[2048]; size_t n = fread(t, 1, sizeof t - 1, r); t[n] = 0; fclose(r);
        w = key_int(t, "overlay_w"); h = key_int(t, "overlay_h"); if (w < 0 || h < 0) { w = key_int(t, "w"); h = key_int(t, "h"); } }
    else goto usage;
    if (w < 1 || h < 1 || w > 16384 || h > 16384) { fprintf(stderr, "raw_to_png: no usable size (%ldx%ld)\n", w, h); return 2; }
    FILE *f = fopen(in, "rb"); if (!f) { fprintf(stderr, "raw_to_png: cannot read %s\n", in); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len != w * h * 4) { fprintf(stderr, "raw_to_png: %s is %ld bytes, %ldx%ld RGBA needs %ld\n", in, len, w, h, w * h * 4); fclose(f); return 2; }
    unsigned char *px = malloc((size_t)len); png_bytep *rows = malloc(sizeof(png_bytep) * (size_t)h); if (!px || !rows) { fclose(f); return 1; }
    if (fread(px, 1, (size_t)len, f) != (size_t)len) { fprintf(stderr, "raw_to_png: short read\n"); fclose(f); return 1; } fclose(f);
    for (long y = 0; y < h; y++) rows[y] = px + (size_t)y * (size_t)w * 4;
    FILE *o = fopen(outp, "wb"); if (!o) { fprintf(stderr, "raw_to_png: cannot write %s\n", outp); return 1; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info || setjmp(png_jmpbuf(png))) { fclose(o); remove(outp); fprintf(stderr, "raw_to_png: png error\n"); return 1; }
    png_init_io(png, o); png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info); png_write_image(png, rows); png_write_end(png, NULL); png_destroy_write_struct(&png, &info); fclose(o);
    printf("wrote %s %ldx%ld\n", outp, w, h); return 0;
usage:
    fprintf(stderr, "Usage: raw_to_png <in.raw> <out.png> --size <W>x<H> | --receipt <receipt.txt>\n"); return 2;
}
