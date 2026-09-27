#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* POSIX rename() atomically replaces an existing destination; MSVCRT's does
   not, and fails outright if the target already exists. That silently broke
   agy-text-editor's load/save on Windows -- the copy landed in <dst>.tmp and
   the real file was never replaced. Same fix as text_edit_key.c. */
static int replace_file(const char *src, const char *dst) {
#ifdef _WIN32
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(src, dst);
#endif
}

/*
 * file_copy.+x -- all-purpose, reusable file copy Op (Bible section 11's
 * REUSE RULE). Used for both "load" (copy a chosen file INTO a project's
 * working buffer) and "save" (copy a project's working buffer OUT to a
 * chosen path) by any project that needs either -- not text-editor
 * specific. Binary-safe, atomic on the destination (writes to a .tmp
 * file, then renames over the real target so a reader never sees a
 * partially-written file).
 *
 * Usage: file_copy.+x <src_path> <dst_path>
 * Exit code: 0 on success, 1 if src can't be read, 2 if dst can't be written.
 */
int main(int argc, char *argv[]) {
    const char *src_path, *dst_path;
    char tmp_path[4096];
    FILE *src, *dst;
    char buffer[8192];
    size_t bytes;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <src_path> <dst_path>\n", argv[0]);
        return 2;
    }
    src_path = argv[1];
    dst_path = argv[2];

    src = fopen(src_path, "rb");
    if (!src) {
        fprintf(stderr, "file_copy: cannot read %s\n", src_path);
        return 1;
    }

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", dst_path);
    dst = fopen(tmp_path, "wb");
    if (!dst) {
        fclose(src);
        fprintf(stderr, "file_copy: cannot write %s\n", tmp_path);
        return 2;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);
    if (replace_file(tmp_path, dst_path) != 0) {
        fprintf(stderr, "file_copy: cannot replace %s\n", dst_path);
        return 2;
    }

    return 0;
}
