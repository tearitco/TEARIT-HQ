/* toggle_sleep - flip asleep on avatar. Usage: toggle_sleep.+x <uuid> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
            fprintf(f, "%s=%s\n", key, value); found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: toggle_sleep.+x <uuid>\n"); return 1; }
    resolve_root();
    resolve_login_root();
    resolve_house_root();
    const char *uuid = argv[1];
    char local[PATH_BUF], cur[32];
    snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    read_kv(local, "asleep", cur, sizeof(cur));
    const char *next = (strcmp(cur, "1") == 0) ? "0" : "1";
    write_kv(local, "asleep", next);
    char xyzfs[512] = "";
    {
        char spath[PATH_BUF];
        snprintf(spath, sizeof(spath), "%s/xyzfs/session.pdl", login_root);
        FILE *sf = fopen(spath, "r");
        if (sf) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), sf)) {
                if (strncmp(line, "STATE", 5) != 0 || !strstr(line, "xyzfs_path")) continue;
                char *p = strrchr(line, '|');
                if (!p) continue;
                p++;
                while (*p == ' ' || *p == '\t') p++;
                p[strcspn(p, "\r\n")] = '\0';
                size_t n = strlen(p);
                while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
                snprintf(xyzfs, sizeof(xyzfs), "%s", p);
                break;
            }
            fclose(sf);
        }
    }
    if (!xyzfs[0]) {
        char login_path[PATH_BUF];
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
        read_kv(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));
    }
    if (xyzfs[0]) {
        char remote[PATH_BUF];
        snprintf(remote, sizeof(remote), "%s/%s/home/avatars/%s/state.txt", house_root, xyzfs, uuid);
        write_kv(remote, "asleep", next);
    }
    printf("%s is now %s.\n", uuid, strcmp(next, "1") == 0 ? "asleep" : "awake");
    return 0;
}
