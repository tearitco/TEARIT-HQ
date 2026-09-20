#define _POSIX_C_SOURCE 200809L
/* nb_media_to_sprite.c — one-job: turn a local image (or video poster)
 * into the house sprite.csv directory the shared renderer already blits
 * via item sprite=. Does not touch khtpm_core_render.c.
 *
 * usage: nb_media_to_sprite.+x <in_file> <out_dir>
 *
 * Writes <out_dir>/sprite.csv
 *   # resolution=64
 *   r,g,b,a
 * ... 64*64 rows, same contract hq_sprite() reads.
 */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "../js/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define SPR_RES 64

static int looks_video(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    char e[16];
    size_t n = strlen(dot);
    if (n >= sizeof(e)) n = sizeof(e) - 1;
    for (size_t i = 0; i < n; i++) e[i] = (char)tolower((unsigned char)dot[i]);
    e[n] = 0;
    return strcmp(e, ".mp4") == 0 || strcmp(e, ".webm") == 0 || strcmp(e, ".mkv") == 0
        || strcmp(e, ".mov") == 0 || strcmp(e, ".ogv") == 0 || strcmp(e, ".avi") == 0;
}

static int write_sprite_csv(const char *dir, const unsigned char *rgba, int w, int h) {
    char csv[4096];
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    FILE *f = fopen(csv, "w");
    if (!f) return 0;
    fprintf(f, "# resolution=%d\n", SPR_RES);
    for (int y = 0; y < SPR_RES; y++) {
        int sy = (h <= 0) ? 0 : (y * h) / SPR_RES;
        if (sy >= h) sy = h - 1;
        for (int x = 0; x < SPR_RES; x++) {
            int sx = (w <= 0) ? 0 : (x * w) / SPR_RES;
            if (sx >= w) sx = w - 1;
            const unsigned char *p = rgba + ((sy * w) + sx) * 4;
            fprintf(f, "%d,%d,%d,%d\n", p[0], p[1], p[2], p[3]);
        }
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <in_file> <out_dir>\n", argv[0]);
        return 1;
    }
    const char *in = argv[1];
    const char *dir = argv[2];
    mkdir(dir, 0755);

    char work[4096];
    snprintf(work, sizeof(work), "%s", in);
    if (looks_video(in)) {
        snprintf(work, sizeof(work), "%s/poster.png", dir);
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -y -hide_banner -loglevel error -i '%s' -vframes 1 -vf scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2 '%s'",
            in, SPR_RES, SPR_RES, SPR_RES, SPR_RES, work);
        if (system(cmd) != 0) {
            fprintf(stderr, "nb_media_to_sprite: ffmpeg poster failed\n");
            return 1;
        }
    }

    int w = 0, h = 0, n = 0;
    unsigned char *img = stbi_load(work, &w, &h, &n, 4);
    if (!img || w <= 0 || h <= 0) {
        fprintf(stderr, "nb_media_to_sprite: cannot decode %s\n", work);
        if (img) stbi_image_free(img);
        return 1;
    }
    int ok = write_sprite_csv(dir, img, w, h);
    stbi_image_free(img);
    if (!ok) {
        fprintf(stderr, "nb_media_to_sprite: cannot write sprite.csv\n");
        return 1;
    }
    return 0;
}
