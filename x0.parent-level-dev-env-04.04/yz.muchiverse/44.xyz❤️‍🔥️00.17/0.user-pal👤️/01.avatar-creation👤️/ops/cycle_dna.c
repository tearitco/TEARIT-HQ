/* cycle_dna - cycle or set one DNA field on an avatar, sync local+xyzfs.
 * Usage:
 *   cycle_dna.+x <avatar_uuid> <field>
 *   cycle_dna.+x <avatar_uuid> <field> <index_or_value>
 * field: gender|skin|hair|shirt|pants|height|weight
 *
 * With 2 args after uuid: advance one step (legacy Cycle* buttons).
 * With 3rd arg: set absolute (skin/height = index; list fields = value
 * or index). Used by fold+scroller pickers on Enter commit. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define MAX_OPTS 32

static char project_root[MAX_PATH] = ".";
static char login_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}
static void resolve_login_root(void) {
    const char *env = getenv("USERPAL_LOGIN_ROOT");
    if (env && env[0]) { snprintf(login_root, sizeof(login_root), "%s", env); return; }
    /* Emoji-free upward walk: find the nearest <dir>/00.login-signup. */
    char cand[PATH_BUF], real[MAX_PATH], dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s", project_root);
    for (int i = 0; i < 8; i++) {
        snprintf(cand, sizeof(cand), "%s/00.login-signup", dir);
        if (realpath(cand, real)) { snprintf(login_root, sizeof(login_root), "%s", real); return; }
        char *slash = strrchr(dir, '/');
        if (!slash || slash == dir) break;
        *slash = '\0';
    }
    snprintf(login_root, sizeof(login_root), "%s", project_root);
}
/* house root = parent of 0.user-pal (login_root is <house>/0.user-pal/00.login-signup). */
static void resolve_house_root(void) {
    const char *env = getenv("HOUSE_ROOT");
    if (env && env[0]) { snprintf(house_root, sizeof(house_root), "%s", env); return; }
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", login_root);
    char *s1 = strrchr(tmp, '/');
    if (s1 && s1 != tmp) *s1 = '\0';
    char *s2 = strrchr(tmp, '/');
    if (s2 && s2 != tmp) *s2 = '\0';
    snprintf(house_root, sizeof(house_root), "%s", tmp);
}
static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}
static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int n = 0;
    if (f) { while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++; fclose(f); }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!found && strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static int load_list(const char *rel, char opts[][64], int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, rel);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(opts[n], 64, "%s", line);
        n++;
    }
    fclose(f);
    return n;
}

static int find_idx(char opts[][64], int n, const char *cur) {
    for (int i = 0; i < n; i++) if (strcmp(opts[i], cur) == 0) return i;
    return 0;
}

static int all_digits(const char *s) {
    if (!s || !s[0]) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static void read_session_state(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/xyzfs/session.pdl", login_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *k = p1 + 1;
        while (*k == ' ' || *k == '\t') k++;
        if (strncmp(k, key, kl) != 0) continue;
        char *after = k + kl;
        while (*after == ' ' || *after == '\t') after++;
        if (*after != '|') continue;
        char *v = after + 1;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}
static void resolve_xyzfs(char *xyzfs, size_t xyz_sz) {
    xyzfs[0] = '\0';
    read_session_state("xyzfs_path", xyzfs, xyz_sz);
    if (!xyzfs[0]) {
        char login_path[PATH_BUF];
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
        read_kv(login_path, "current_xyzfs", xyzfs, xyz_sz);
    }
}
static void sync_state_pair(const char *uuid, const char *key, const char *val) {
    char local[PATH_BUF];
    snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    write_kv(local, key, val);

    char xyzfs[512];
    resolve_xyzfs(xyzfs, sizeof(xyzfs));
    if (xyzfs[0]) {
        char remote[PATH_BUF], rdir[PATH_BUF];
        snprintf(rdir, sizeof(rdir), "%s/%s/home/avatars/%s", house_root, xyzfs, uuid);
        snprintf(remote, sizeof(remote), "%s/state.txt", rdir);
        /* If xyzfs piece missing, seed from local so mods don't vanish. */
        struct stat st;
        if (stat(remote, &st) != 0) {
            char cmd[PATH_BUF * 3];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && cp -a '%s/pieces/world_01/map_lobby/%s/.' '%s/' 2>/dev/null",
                     rdir, project_root, uuid, rdir);
            system(cmd);
        }
        write_kv(remote, key, val);
    }
}

static void apply_skin(const char *uuid, const char *gender, int skin_index) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/dna/skin_tones.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE], emoji[32] = "👤";
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        char g[16], em[32], lab[32];
        int idx = 0;
        if (sscanf(line, "%15[^|]|%d|%31[^|]|%31s", g, &idx, em, lab) >= 3) {
            if (strcmp(g, gender) == 0 && idx == skin_index) {
                snprintf(emoji, sizeof(emoji), "%s", em);
                break;
            }
        }
    }
    fclose(f);
    char idxbuf[16];
    snprintf(idxbuf, sizeof(idxbuf), "%d", skin_index);
    sync_state_pair(uuid, "skin_index", idxbuf);
    sync_state_pair(uuid, "skin_emoji", emoji);
    sync_state_pair(uuid, "species_emoji", emoji);
}

static int skin_count_for_gender(const char *gender) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/dna/skin_tones.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 6;
    char line[MAX_LINE];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        char g[16], em[32], lab[32];
        int idx = 0;
        if (sscanf(line, "%15[^|]|%d|%31[^|]|%31s", g, &idx, em, lab) >= 3) {
            if (strcmp(g, gender) == 0) n++;
        }
    }
    fclose(f);
    return n > 0 ? n : 6;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: cycle_dna.+x <avatar_uuid> <field> [index_or_value]\n");
        return 1;
    }
    resolve_root();
    resolve_login_root();
    resolve_house_root();
    const char *uuid = argv[1];
    const char *field = argv[2];
    const char *set_arg = (argc >= 4) ? argv[3] : NULL;

    char local[PATH_BUF];
    snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    char cur[128];

    if (strcmp(field, "gender") == 0) {
        read_kv(local, "gender", cur, sizeof(cur));
        const char *next;
        if (set_arg) {
            if (strcmp(set_arg, "male") == 0 || strcmp(set_arg, "female") == 0)
                next = set_arg;
            else if (all_digits(set_arg))
                next = (atoi(set_arg) % 2 == 0) ? "male" : "female";
            else
                next = (strcmp(cur, "female") == 0) ? "male" : "female";
        } else {
            next = (strcmp(cur, "female") == 0) ? "male" : "female";
        }
        sync_state_pair(uuid, "gender", next);
        char si[16];
        read_kv(local, "skin_index", si, sizeof(si));
        apply_skin(uuid, next, atoi(si));
        printf("Gender: %s\n", next);
        return 0;
    }
    if (strcmp(field, "skin") == 0) {
        char gender[32], si[16];
        read_kv(local, "gender", gender, sizeof(gender));
        if (!gender[0]) snprintf(gender, sizeof(gender), "male");
        read_kv(local, "skin_index", si, sizeof(si));
        int nskin = skin_count_for_gender(gender);
        int idx;
        if (set_arg && all_digits(set_arg)) {
            idx = atoi(set_arg);
            if (idx < 0) idx = 0;
            if (nskin > 0) idx %= nskin;
        } else {
            idx = (atoi(si) + 1) % (nskin > 0 ? nskin : 6);
        }
        apply_skin(uuid, gender, idx);
        char emoji[32];
        snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
        read_kv(local, "skin_emoji", emoji, sizeof(emoji));
        printf("Skin: %s (index %d)\n", emoji, idx);
        return 0;
    }

    const char *list_rel = NULL;
    const char *key = NULL;
    if (strcmp(field, "hair") == 0) { list_rel = "pieces/registry/dna/hair_colors.txt"; key = "hair_color"; }
    else if (strcmp(field, "shirt") == 0) { list_rel = "pieces/registry/dna/shirt_colors.txt"; key = "shirt_color"; }
    else if (strcmp(field, "pants") == 0) { list_rel = "pieces/registry/dna/pants_colors.txt"; key = "pants_color"; }
    else if (strcmp(field, "height") == 0) { list_rel = "pieces/registry/dna/heights.txt"; key = "height"; }
    else if (strcmp(field, "weight") == 0) { list_rel = "pieces/registry/dna/weights.txt"; key = "weight"; }
    else {
        printf("Unknown field '%s'.\n", field);
        return 1;
    }

    char opts[MAX_OPTS][64];
    int n = load_list(list_rel, opts, MAX_OPTS);
    if (n <= 0) { printf("No options for %s.\n", field); return 1; }
    read_kv(local, key, cur, sizeof(cur));
    int i;
    if (set_arg) {
        if (all_digits(set_arg)) {
            i = atoi(set_arg) % n;
            if (i < 0) i = 0;
        } else {
            i = find_idx(opts, n, set_arg);
            /* if not found, find_idx returns 0 — only accept if match */
            if (strcmp(opts[i], set_arg) != 0) {
                /* try exact scan */
                int found = -1;
                for (int k = 0; k < n; k++) if (strcmp(opts[k], set_arg) == 0) { found = k; break; }
                if (found < 0) { printf("Unknown value '%s' for %s.\n", set_arg, field); return 1; }
                i = found;
            }
        }
    } else {
        i = find_idx(opts, n, cur);
        i = (i + 1) % n;
    }
    sync_state_pair(uuid, key, opts[i]);
    printf("%s: %s\n", key, opts[i]);
    return 0;
}
