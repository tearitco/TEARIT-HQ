/* claim_tokens - +10 tokens to logged-in user's xyzfs wallet
 * (or local user_01 fallback). Usage: claim_tokens.+x
 *
 * Identity: same as generate_clone — prefer xyzfs/session.pdl when
 * mode=logged_in (current_login.txt is often empty even when session is). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define CLAIM_AMOUNT 10

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
static void load_user_wallet_identity(char *user_uuid, size_t uuid_sz,
                                      char *xyzfs_rel, size_t xyz_sz) {
    user_uuid[0] = '\0';
    xyzfs_rel[0] = '\0';
    char mode[64] = "";
    read_session_state("mode", mode, sizeof(mode));
    if (strcmp(mode, "logged_in") == 0) {
        read_session_state("user_uuid", user_uuid, uuid_sz);
        read_session_state("xyzfs_path", xyzfs_rel, xyz_sz);
    }
    if (!user_uuid[0] || !xyzfs_rel[0]) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/current_login.txt", login_root);
        if (!user_uuid[0]) read_kv(path, "current_user_uuid", user_uuid, uuid_sz);
        if (!xyzfs_rel[0]) read_kv(path, "current_xyzfs", xyzfs_rel, xyz_sz);
    }
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
static int adjust_tokens(const char *wallet_path, int delta, int *out_bal) {
    char lines[32][MAX_LINE];
    int nlines = 0, tokens = 0, has_file = 0;
    FILE *f = fopen(wallet_path, "r");
    if (f) {
        has_file = 1;
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
            char *eq = strchr(lines[nlines], '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(lines[nlines], "tokens") == 0) tokens = atoi(eq + 1);
                *eq = '=';
            }
            nlines++;
        }
        fclose(f);
    }
    if (!has_file) {
        tokens = 0;
        if (delta < 0) { *out_bal = 0; return 0; }
    }
    if (tokens + delta < 0) { *out_bal = tokens; return 0; }
    tokens += delta;
    f = fopen(wallet_path, "w");
    if (!f) return 0;
    int wrote = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "tokens") == 0) {
                fprintf(f, "tokens=%d\n", tokens);
                wrote = 1; *eq = '='; continue;
            }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!wrote) fprintf(f, "tokens=%d\n", tokens);
    fclose(f);
    *out_bal = tokens;
    return 1;
}

int main(void) {
    resolve_root();
    resolve_login_root();
    resolve_house_root();
    char user_uuid[128], xyzfs_rel[512];
    load_user_wallet_identity(user_uuid, sizeof(user_uuid), xyzfs_rel, sizeof(xyzfs_rel));

    char wallet[PATH_BUF];
    if (user_uuid[0] && xyzfs_rel[0]) {
        char home[PATH_BUF];
        snprintf(home, sizeof(home), "%s/%s/home", house_root, xyzfs_rel);
        mkdir_p(home, 0755);
        snprintf(wallet, sizeof(wallet), "%s/wallet.txt", home);
    } else {
        snprintf(wallet, sizeof(wallet), "%s/pieces/world_01/map_lobby/user_01/state.txt", project_root);
    }

    int bal = 0;
    if (!adjust_tokens(wallet, CLAIM_AMOUNT, &bal)) {
        printf("Claim failed.\n");
        return 1;
    }
    if (user_uuid[0] && xyzfs_rel[0])
        printf("Claimed %d tokens! Balance: %d (user wallet)\n", CLAIM_AMOUNT, bal);
    else
        printf("Claimed %d tokens! Balance: %d (local guest)\n", CLAIM_AMOUNT, bal);
    return 0;
}
