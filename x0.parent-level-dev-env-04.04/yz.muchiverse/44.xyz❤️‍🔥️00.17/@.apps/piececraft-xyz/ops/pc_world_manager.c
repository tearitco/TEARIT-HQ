/* pc_world_manager - helper op for loading/saving piececraft worlds
 * (saved desks). Supports two main use cases:
 * 1) LIST: scan pieces/piececraft-desks/ and print available worlds
 * 2) LOAD:<name>: copy a saved world's files to pieces/world_01/
 *
 * Part of Piece 1 implementation: in-scene world selection screen.
 * Self-contained, no shared headers.
 * Usage: pc_world_manager.+x list|load <world_name> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "win_posix_shim.h"

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
#ifdef _WIN32
    if (access("pieces", F_OK) == 0) {
        snprintf(project_root, sizeof(project_root), ".");
        return;
    }
#endif
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void resolve_real_root(const char *proj_root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", proj_root);
    char real_root_path[PATH_BUF];
    snprintf(real_root_path, sizeof(real_root_path), "%s/pieces/system/real_project_root.txt", proj_root);
    FILE *rf = fopen(real_root_path, "r");
    if (rf) {
        char buf[PATH_BUF];
        if (fgets(buf, sizeof(buf), rf)) {
            buf[strcspn(buf, "\r\n")] = '\0';
            if (buf[0]) snprintf(out, out_sz, "%s", buf);
        }
        fclose(rf);
    }
}

/* Copy a file from src to dst. Returns 1 on success, 0 on failure. */
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }

    char buf[4096];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }
    fclose(in);
    fclose(out);
    return ok;
}

/* Recursively copy a directory tree. Returns 1 on success, 0 on failure. */
static int copy_tree(const char *src_dir, const char *dst_dir) {
    /* Create destination directory */
    if (mkdir(dst_dir, 0755) != 0 && errno != EEXIST) return 0;

    DIR *d = opendir(src_dir);
    if (!d) return 0;

    int ok = 1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char src_path[PATH_BUF], dst_path[PATH_BUF];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, ent->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, ent->d_name);

        struct stat st;
        if (stat(src_path, &st) != 0) { ok = 0; break; }

        if (S_ISDIR(st.st_mode)) {
            if (!copy_tree(src_path, dst_path)) { ok = 0; break; }
        } else {
            if (!copy_file(src_path, dst_path)) { ok = 0; break; }
        }
    }
    closedir(d);
    return ok;
}

/* Remove all files/dirs in a directory tree. Doesn't remove the root dir itself.
 * Returns 1 on success, 0 on failure. */
static int rm_tree_contents(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    int ok = 1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) { ok = 0; break; }

        if (S_ISDIR(st.st_mode)) {
            if (!rm_tree_contents(path)) { ok = 0; break; }
            if (rmdir(path) != 0) { ok = 0; break; }
        } else {
            if (unlink(path) != 0) { ok = 0; break; }
        }
    }
    closedir(d);
    return ok;
}

/* LIST: print all available worlds */
static void list_worlds(const char *proj_root) {
    char real_root[PATH_BUF];
    resolve_real_root(proj_root, real_root, sizeof(real_root));

    char desks_dir[PATH_BUF];
    snprintf(desks_dir, sizeof(desks_dir), "%s/pieces/piececraft-desks", real_root);

    DIR *d = opendir(desks_dir);
    if (!d) {
        /* No desks dir yet - that's OK, will be created on first world creation */
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char world_path[PATH_BUF];
        snprintf(world_path, sizeof(world_path), "%s/%s", desks_dir, ent->d_name);

        struct stat st;
        if (stat(world_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Read metadata if it exists */
            char state_path[PATH_BUF];
            snprintf(state_path, sizeof(state_path), "%s/state.txt", world_path);

            char world_type[32] = "unknown";
            char created[64] = "unknown";
            FILE *sf = fopen(state_path, "r");
            if (sf) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), sf)) {
                    if (strncmp(line, "world_type=", 11) == 0) {
                        char *v = line + 11;
                        v[strcspn(v, "\r\n")] = '\0';
                        snprintf(world_type, sizeof(world_type), "%s", v);
                    } else if (strncmp(line, "created=", 8) == 0) {
                        char *v = line + 8;
                        v[strcspn(v, "\r\n")] = '\0';
                        snprintf(created, sizeof(created), "%s", v);
                    }
                }
                fclose(sf);
            }

            printf("%s|%s|%s\n", ent->d_name, world_type, created);
        }
    }
    closedir(d);
}

/* LOAD: copy a saved world into pieces/world_01/ */
static void load_world(const char *proj_root, const char *world_name) {
    char real_root[PATH_BUF];
    resolve_real_root(proj_root, real_root, sizeof(real_root));

    char src_dir[PATH_BUF], dst_dir[PATH_BUF];
    snprintf(src_dir, sizeof(src_dir), "%s/pieces/piececraft-desks/%s", real_root, world_name);
    snprintf(dst_dir, sizeof(dst_dir), "%s/pieces", real_root);

    /* Remove existing world state */
    char world_01_path[PATH_BUF];
    snprintf(world_01_path, sizeof(world_01_path), "%s/world_01", dst_dir);
    char hero_01_path[PATH_BUF];
    snprintf(hero_01_path, sizeof(hero_01_path), "%s/hero_01", dst_dir);
    char xelector_01_path[PATH_BUF];
    snprintf(xelector_01_path, sizeof(xelector_01_path), "%s/xelector_01", dst_dir);

    rm_tree_contents(world_01_path);
    rm_tree_contents(hero_01_path);
    rm_tree_contents(xelector_01_path);

    /* Copy in the saved world */
    if (!copy_tree(src_dir, dst_dir)) {
        fprintf(stderr, "pc_world_manager: failed to load world %s\n", world_name);
        return;
    }

    /* Update last_world in config */
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", real_root);
    FILE *cf = fopen(config_path, "r+");
    if (cf) {
        char lines[64][MAX_LINE];
        int nlines = 0;
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, cf)) nlines++;

        int found = 0;
        fseek(cf, 0, SEEK_SET);
        for (int i = 0; i < nlines; i++) {
            if (strncmp(lines[i], "last_world=", 11) == 0) {
                fprintf(cf, "last_world=%s\n", world_name);
                found = 1;
            } else {
                fputs(lines[i], cf);
            }
        }
        if (!found) fprintf(cf, "last_world=%s\n", world_name);

        fflush(cf);
        long endpos = ftell(cf);
        if (endpos >= 0) { int _rc = ftruncate(fileno(cf), endpos); (void)_rc; }
        fclose(cf);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pc_world_manager list|load <world_name>\n");
        return 1;
    }

    resolve_root();

    const char *cmd = argv[1];
    if (strcmp(cmd, "list") == 0) {
        list_worlds(project_root);
    } else if (strcmp(cmd, "load") == 0) {
        if (argc < 3) {
            fprintf(stderr, "load requires a world name argument\n");
            return 1;
        }
        load_world(project_root, argv[2]);
    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
