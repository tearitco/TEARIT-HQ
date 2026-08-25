/* hydrate_avatars - copy xyzfs inventory clones into local map_lobby so
 * manage/desktop windows find state.txt. Source of truth = user xyzfs
 * (session.pdl / current_login after ensure_user_identity).
 *
 * Usage: hydrate_avatars.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

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

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_BUF];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    return (mkdir(tmp, mode) != 0 && errno != EEXIST) ? -1 : 0;
}

static void resolve_xyzfs(char *xyz, size_t xyz_sz) {
    xyz[0] = '\0';
    read_session_state("xyzfs_path", xyz, xyz_sz);
    if (!xyz[0]) {
        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        read_kv(cpath, "current_xyzfs", xyz, xyz_sz);
    }
}

static int inv_has(const char *inv_path, const char *id) {
    FILE *f = fopen(inv_path, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, id) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

int main(void) {
    resolve_root();
    resolve_login_root();
    resolve_house_root();

    char xyz[512];
    resolve_xyzfs(xyz, sizeof(xyz));
    if (!xyz[0]) {
        printf("hydrate: no xyzfs path (run ensure_user_identity first)\n");
        return 0;
    }

    char inv[PATH_BUF], local_inv[PATH_BUF];
    snprintf(inv, sizeof(inv), "%s/%s/home/avatars/inventory.txt", house_root, xyz);
    snprintf(local_inv, sizeof(local_inv),
             "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
    {
        char d[PATH_BUF];
        snprintf(d, sizeof(d), "%s/pieces/world_01/map_lobby/user_01", project_root);
        mkdir_p(d, 0755);
    }

    FILE *f = fopen(inv, "r");
    if (!f) {
        printf("hydrate: no inventory at %s\n", inv);
        return 0;
    }

    int n = 0, copied = 0;
    char id[128];
    while (fgets(id, sizeof(id), f)) {
        id[strcspn(id, "\r\n")] = '\0';
        if (!id[0]) continue;
        n++;
        char remote[PATH_BUF], local[PATH_BUF];
        snprintf(remote, sizeof(remote), "%s/%s/home/avatars/%s", house_root, xyz, id);
        snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s", project_root, id);
        struct stat st;
        if (stat(remote, &st) != 0) continue;
        /* Prefer xyzfs as truth: always refresh local from remote. */
        char cmd[PATH_BUF * 3];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && cp -a '%s/.' '%s/' 2>/dev/null",
                 local, remote, local);
        if (system(cmd) == 0) copied++;
        if (!inv_has(local_inv, id)) {
            FILE *lf = fopen(local_inv, "a");
            if (lf) { fprintf(lf, "%s\n", id); fclose(lf); }
        }
    }
    fclose(f);

    /* Push local lobby clones missing from xyzfs (mods were local-only). */
    char uuuid[128] = "";
    read_session_state("user_uuid", uuuid, sizeof(uuuid));
    if (!uuuid[0]) {
        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        read_kv(cpath, "current_user_uuid", uuuid, sizeof(uuuid));
    }
    int pushed = 0;
    char lobby[PATH_BUF];
    snprintf(lobby, sizeof(lobby), "%s/pieces/world_01/map_lobby", project_root);
    DIR *d = opendir(lobby);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.' || strcmp(ent->d_name, "user_01") == 0) continue;
            if (strlen(ent->d_name) < 32) continue; /* uuid-ish */
            char local_st[PATH_BUF], owner[128] = "";
            snprintf(local_st, sizeof(local_st), "%s/%s/state.txt", lobby, ent->d_name);
            struct stat st;
            if (stat(local_st, &st) != 0) continue;
            read_kv(local_st, "owner_user_uuid", owner, sizeof(owner));
            /* guest: push all local; logged_in: only matching owner */
            if (uuuid[0] && owner[0] && strcmp(owner, uuuid) != 0) continue;
            char remote[PATH_BUF];
            snprintf(remote, sizeof(remote), "%s/%s/home/avatars/%s/state.txt",
                     house_root, xyz, ent->d_name);
            if (stat(remote, &st) == 0) continue; /* already on xyzfs */
            char rdir[PATH_BUF];
            snprintf(rdir, sizeof(rdir), "%s/%s/home/avatars/%s", house_root, xyz, ent->d_name);
            char ldir[PATH_BUF];
            snprintf(ldir, sizeof(ldir), "%s/%s", lobby, ent->d_name);
            char cmd[PATH_BUF * 3];
            snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && cp -a '%s/.' '%s/' 2>/dev/null",
                     rdir, ldir, rdir);
            if (system(cmd) == 0) {
                if (!inv_has(inv, ent->d_name)) {
                    FILE *af = fopen(inv, "a");
                    if (af) { fprintf(af, "%s\n", ent->d_name); fclose(af); }
                }
                if (!inv_has(local_inv, ent->d_name)) {
                    FILE *lf = fopen(local_inv, "a");
                    if (lf) { fprintf(lf, "%s\n", ent->d_name); fclose(lf); }
                }
                pushed++;
            }
        }
        closedir(d);
    }

    printf("hydrate: inv=%d pulled=%d pushed=%d (%s)\n", n, copied, pushed, xyz);
    return 0;
}
