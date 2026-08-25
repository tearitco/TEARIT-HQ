/* dump_rgb_png - SHARED OP (yz.muchiverse/2.muchi-verse/shared-ops/,
 * see shared-ops-refactor-plan.txt for why this lives here instead of
 * a separate copy per project). DEBUG TOOL, not part of any project's
 * game loop, never wired into that project's own default_op.txt/pal/
 * main_loop.pal. Reads whatever that project's own compose_rgb_frame.c
 * most recently wrote (pieces/display/rgb_frame.raw + its
 * rgb_frame.receipt.txt for the real dimensions - not a hardcoded
 * duplicate of FRAME_W/FRAME_H, so this can't silently drift out of
 * sync with whichever project's compose_rgb_frame.c wrote it) and
 * writes a real PNG via stb_image_write (lib/stb_image_write.h, public
 * domain, vendored from real 1.TPMOS rather than fetched - the exact
 * same header several 1.TPMOS pieces already use, e.g.
 * pieces/system/emoji_extract/). Zero hardcoded project-specific
 * content - genuinely identical behavior regardless of which
 * project's PRISC_PROJECT_ROOT it's pointed at, confirmed by having
 * already been byte-identical between mutaclsym's and zoo_0000's own
 * former per-project copies before this file replaced both.
 *
 * Exists because an agent has no way to look at a live GLUT window
 * directly - the checksum/receipt pattern proves the BYTES are
 * correct, but a human (or an image-capable read of this PNG) is
 * still the only way to confirm the pixels look right - direct user
 * instruction, originally to mutaclsym: "u complained u couldn't view
 * the current gl... use stb image write... get it to create u a jpg/
 * png u can view whenever u need." Run manually whenever that's
 * needed, e.g.:
 *   ./ops/+x/dump_rgb_png.+x
 *   ./ops/+x/dump_rgb_png.+x pieces/display/my_debug_frame.png
 *
 * Usage: dump_rgb_png.+x [output_png_path] */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

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

int main(int argc, char **argv) {
    resolve_root();

    char receipt_path[PATH_BUF], raw_path[PATH_BUF];
    snprintf(receipt_path, sizeof(receipt_path), "%s/pieces/display/rgb_frame.receipt.txt", project_root);
    snprintf(raw_path, sizeof(raw_path), "%s/pieces/display/rgb_frame.raw", project_root);

    int w = read_kv_int(receipt_path, "frame_w", 0);
    int h = read_kv_int(receipt_path, "frame_h", 0);
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "dump_rgb_png: couldn't read frame_w/frame_h from %s "
                        "(run ops/+x/compose_rgb_frame.+x first)\n", receipt_path);
        return 1;
    }

    size_t expected_bytes = (size_t)w * h * 4;
    unsigned char *buf = malloc(expected_bytes);
    if (!buf) return 1;

    FILE *f = fopen(raw_path, "rb");
    if (!f) {
        fprintf(stderr, "dump_rgb_png: couldn't open %s\n", raw_path);
        free(buf);
        return 1;
    }
    size_t got = fread(buf, 1, expected_bytes, f);
    fclose(f);
    if (got != expected_bytes) {
        fprintf(stderr, "dump_rgb_png: short read (%zu of %zu bytes) - "
                        "frame_w/h in the receipt doesn't match rgb_frame.raw's actual size\n",
                got, expected_bytes);
        free(buf);
        return 1;
    }

    char out_path[PATH_BUF];
    if (argc > 1) {
        snprintf(out_path, sizeof(out_path), "%s", argv[1]);
    } else {
        snprintf(out_path, sizeof(out_path), "%s/pieces/display/rgb_frame_debug.png", project_root);
    }

    int ok = stbi_write_png(out_path, w, h, 4, buf, w * 4);
    free(buf);
    if (!ok) {
        fprintf(stderr, "dump_rgb_png: stbi_write_png failed writing %s\n", out_path);
        return 1;
    }

    printf("wrote %s (%dx%d)\n", out_path, w, h);
    return 0;
}
