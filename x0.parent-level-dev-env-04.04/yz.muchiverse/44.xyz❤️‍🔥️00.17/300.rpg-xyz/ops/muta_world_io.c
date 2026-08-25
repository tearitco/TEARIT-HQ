/* muta_world_io - user-FS save slots for mutaclysm worlds.
 *
 * Usage:
 *   muta_world_io.+x seed-demo <live_or_template_world> <saves_root>
 *   muta_world_io.+x save      <live_world> <saves_root> <slot_name>
 *   muta_world_io.+x load      <live_world> <saves_root> <slot_name>
 *   muta_world_io.+x new-game  <live_world> <template_world>
 *   muta_world_io.+x list      <saves_root>
 *
 * Slot layout:
 *   <saves_root>/<slot_name>/meta.pdl
 *   <saves_root>/<slot_name>/world_01/   # full map+entity tree
 *
 * seed-demo is idempotent (skips if demo-project already has world_01).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define PATH_BUF 4352
#define MAX_LINE 2048

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int run_cmd(const char *cmd) {
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "muta_world_io: cmd failed (%d): %s\n", rc, cmd);
        return -1;
    }
    return 0;
}

static int mkdir_p(const char *path) {
    char cmd[PATH_BUF];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    return run_cmd(cmd);
}

static int cp_r(const char *src, const char *dst) {
    char cmd[PATH_BUF * 2];
    /* dst parent must exist; remove dst if present then copy */
    snprintf(cmd, sizeof(cmd),
             "rm -rf '%s' && mkdir -p '$(dirname '%s')' 2>/dev/null; "
             "mkdir -p '%s' && cp -a '%s'/. '%s'/",
             dst, dst, dst, src, dst);
    /* simpler portable: */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s' && cp -a '%s'/. '%s'/",
             dst, dst, src, dst);
    return run_cmd(cmd);
}

static void write_meta(const char *slot_dir, const char *name, const char *note) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", slot_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | mutaclysm_save\n");
    fprintf(f, "STATE        | name                 | %s\n", name);
    fprintf(f, "STATE        | kind                 | mutaclysm_save\n");
    fprintf(f, "STATE        | note                 | %s\n", note ? note : "");
    fprintf(f, "STATE        | saved_at             | %ld\n", (long)time(NULL));
    fclose(f);
}

static int seed_demo(const char *src_world, const char *saves_root) {
    if (!is_dir(src_world)) {
        fprintf(stderr, "seed-demo: source world missing: %s\n", src_world);
        return 1;
    }
    if (mkdir_p(saves_root) != 0) return 1;
    char slot[PATH_BUF], world[PATH_BUF];
    snprintf(slot, sizeof(slot), "%s/demo-project", saves_root);
    snprintf(world, sizeof(world), "%s/world_01", slot);
    if (is_dir(world)) {
        printf("seed-demo: demo-project already exists, skip\n");
        return 0;
    }
    if (mkdir_p(slot) != 0) return 1;
    if (cp_r(src_world, world) != 0) return 1;
    write_meta(slot, "demo-project", "seeded from install/template");
    printf("seed-demo: ok -> %s\n", world);
    return 0;
}

static int save_slot(const char *live, const char *saves_root, const char *name) {
    if (!is_dir(live)) {
        fprintf(stderr, "save: live world missing: %s\n", live);
        return 1;
    }
    if (!name || !name[0] || strchr(name, '/') || strchr(name, '.')) {
        /* allow simple names; reject path separators and leading dots */
        if (!name || !name[0] || strchr(name, '/')) {
            fprintf(stderr, "save: bad slot name\n");
            return 1;
        }
    }
    if (mkdir_p(saves_root) != 0) return 1;
    char slot[PATH_BUF], world[PATH_BUF];
    snprintf(slot, sizeof(slot), "%s/%s", saves_root, name);
    snprintf(world, sizeof(world), "%s/world_01", slot);
    if (mkdir_p(slot) != 0) return 1;
    if (cp_r(live, world) != 0) return 1;
    write_meta(slot, name, "user save");
    printf("save: ok -> %s\n", world);
    return 0;
}

static int load_slot(const char *live, const char *saves_root, const char *name) {
    char world[PATH_BUF];
    snprintf(world, sizeof(world), "%s/%s/world_01", saves_root, name);
    if (!is_dir(world)) {
        fprintf(stderr, "load: slot world missing: %s\n", world);
        return 1;
    }
    /* replace live */
    char parent[PATH_BUF];
    snprintf(parent, sizeof(parent), "%s", live);
    /* live is .../world_01 — parent pieces/ */
    if (cp_r(world, live) != 0) return 1;
    printf("load: ok from %s -> %s\n", world, live);
    return 0;
}

static int new_game(const char *live, const char *template_world) {
    if (!is_dir(template_world)) {
        fprintf(stderr, "new-game: template missing: %s\n", template_world);
        return 1;
    }
    if (cp_r(template_world, live) != 0) return 1;
    printf("new-game: live reset from template -> %s\n", live);
    return 0;
}

static int list_saves(const char *saves_root) {
    DIR *d = opendir(saves_root);
    if (!d) {
        printf("(no saves root)\n");
        return 0;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char p[PATH_BUF];
        snprintf(p, sizeof(p), "%s/%s/world_01", saves_root, e->d_name);
        if (is_dir(p)) {
            printf("%s\n", e->d_name);
            n++;
        }
    }
    closedir(d);
    printf("list: %d slots\n", n);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Usage: muta_world_io.+x seed-demo|save|load|new-game|list ...\n");
        return 1;
    }
    const char *op = argv[1];
    if (strcmp(op, "seed-demo") == 0) {
        if (argc < 4) return 1;
        return seed_demo(argv[2], argv[3]);
    }
    if (strcmp(op, "save") == 0) {
        if (argc < 5) return 1;
        return save_slot(argv[2], argv[3], argv[4]);
    }
    if (strcmp(op, "load") == 0) {
        if (argc < 5) return 1;
        return load_slot(argv[2], argv[3], argv[4]);
    }
    if (strcmp(op, "new-game") == 0) {
        if (argc < 4) return 1;
        return new_game(argv[2], argv[3]);
    }
    if (strcmp(op, "list") == 0) {
        if (argc < 3) return 1;
        return list_saves(argv[2]);
    }
    fprintf(stderr, "unknown op %s\n", op);
    return 1;
}
