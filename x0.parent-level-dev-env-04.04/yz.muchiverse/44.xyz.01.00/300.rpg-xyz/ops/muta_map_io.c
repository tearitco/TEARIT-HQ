/* muta_map_io - list maps + switch map / teleport hero+xlector
 *
 * Mutaclysm keeps the hero piece at a FIXED path:
 *   pieces/world_01/map_start/hero/state.txt
 * The field map_id=… selects which map under world_01/ is live.
 * xlector_pos_x/y and pos_x/y are the cursor and body on that map.
 *
 * Usage:
 *   muta_map_io.+x list  <world_01_dir>
 *   muta_map_io.+x switch <project_root> <map_id> [x] [y]
 *   muta_map_io.+x current <project_root>
 *
 * switch defaults x,y to 5,4 (safe indoor-ish spawn) if omitted.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define PATH_BUF 4352
#define MAX_LINE 2048
#define MAX_MAPS 64

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_map_dir(const char *world, const char *name) {
    if (!name || name[0] == '.') return 0;
    if (strcmp(name, "state.txt") == 0) return 0;
    char p[PATH_BUF], mapf[PATH_BUF], stf[PATH_BUF];
    snprintf(p, sizeof(p), "%s/%s", world, name);
    if (!is_dir(p)) return 0;
    snprintf(mapf, sizeof(mapf), "%s/map.txt", p);
    snprintf(stf, sizeof(stf), "%s/state.txt", p);
    /* maps have map.txt and/or state.txt with id= */
    if (access(mapf, F_OK) == 0) return 1;
    if (access(stf, F_OK) == 0) return 1;
    return 0;
}

static int list_maps(const char *world) {
    DIR *d = opendir(world);
    if (!d) {
        fprintf(stderr, "list: cannot open %s\n", world);
        return 1;
    }
    char names[MAX_MAPS][128];
    int n = 0;
    struct dirent *e;
    while (n < MAX_MAPS && (e = readdir(d)) != NULL) {
        if (!is_map_dir(world, e->d_name)) continue;
        snprintf(names[n], sizeof(names[n]), "%s", e->d_name);
        n++;
    }
    closedir(d);
    /* sort */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[128];
                snprintf(t, sizeof(t), "%s", names[i]);
                snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                snprintf(names[j], sizeof(names[j]), "%s", t);
            }
    for (int i = 0; i < n; i++)
        printf("%s\n", names[i]);
    printf("list: %d maps\n", n);
    return 0;
}

static void write_kv_file(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[128][MAX_LINE];
    int n = 0;
    if (f) {
        while (n < 128 && fgets(lines[n], MAX_LINE, f)) n++;
        fclose(f);
    }
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static int switch_map(const char *proj, const char *map_id, int x, int y) {
    char world[PATH_BUF], map_dir[PATH_BUF], hero[PATH_BUF];
    snprintf(world, sizeof(world), "%s/pieces/world_01", proj);
    snprintf(map_dir, sizeof(map_dir), "%s/%s", world, map_id);
    if (!is_dir(map_dir)) {
        fprintf(stderr, "switch: map not found: %s\n", map_dir);
        return 1;
    }
    /* hero is always under map_start/hero in this codebase */
    snprintf(hero, sizeof(hero), "%s/map_start/hero/state.txt", world);
    if (access(hero, F_OK) != 0) {
        fprintf(stderr, "switch: hero state missing: %s\n", hero);
        return 1;
    }

    char xs[32], ys[32];
    snprintf(xs, sizeof(xs), "%d", x);
    snprintf(ys, sizeof(ys), "%d", y);

    write_kv_file(hero, "map_id", map_id);
    write_kv_file(hero, "pos_x", xs);
    write_kv_file(hero, "pos_y", ys);
    write_kv_file(hero, "xlector_pos_x", xs);
    write_kv_file(hero, "xlector_pos_y", ys);

    /* optional session marker for widgets */
    char sess[PATH_BUF];
    snprintf(sess, sizeof(sess), "%s/pieces/system/muta_session.txt", proj);
    {
        char cmd[PATH_BUF];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/pieces/system'", proj);
        system(cmd);
    }
    FILE *sf = fopen(sess, "w");
    if (sf) {
        fprintf(sf, "current_map=%s\n", map_id);
        fprintf(sf, "hero_x=%d\n", x);
        fprintf(sf, "hero_y=%d\n", y);
        fclose(sf);
    }

    printf("switch: map_id=%s pos=%d,%d xlector=%d,%d\n", map_id, x, y, x, y);
    return 0;
}

static int current_map(const char *proj) {
    char hero[PATH_BUF], mid[64], px[32], py[32], xx[32], xy[32];
    snprintf(hero, sizeof(hero),
             "%s/pieces/world_01/map_start/hero/state.txt", proj);
    read_kv(hero, "map_id", mid, sizeof(mid));
    read_kv(hero, "pos_x", px, sizeof(px));
    read_kv(hero, "pos_y", py, sizeof(py));
    read_kv(hero, "xlector_pos_x", xx, sizeof(xx));
    read_kv(hero, "xlector_pos_y", xy, sizeof(xy));
    if (!mid[0]) snprintf(mid, sizeof(mid), "map_start");
    printf("current_map=%s\n", mid);
    printf("pos_x=%s\n", px[0] ? px : "?");
    printf("pos_y=%s\n", py[0] ? py : "?");
    printf("xlector_pos_x=%s\n", xx[0] ? xx : "?");
    printf("xlector_pos_y=%s\n", xy[0] ? xy : "?");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: muta_map_io.+x list|switch|current ...\n");
        return 1;
    }
    if (strcmp(argv[1], "list") == 0) {
        if (argc < 3) return 1;
        return list_maps(argv[2]);
    }
    if (strcmp(argv[1], "switch") == 0) {
        if (argc < 4) return 1;
        int x = 5, y = 4;
        if (argc >= 5) x = atoi(argv[4]);
        if (argc >= 6) y = atoi(argv[5]);
        return switch_map(argv[2], argv[3], x, y);
    }
    if (strcmp(argv[1], "current") == 0) {
        if (argc < 3) return 1;
        return current_map(argv[2]);
    }
    fprintf(stderr, "unknown op %s\n", argv[1]);
    return 1;
}
