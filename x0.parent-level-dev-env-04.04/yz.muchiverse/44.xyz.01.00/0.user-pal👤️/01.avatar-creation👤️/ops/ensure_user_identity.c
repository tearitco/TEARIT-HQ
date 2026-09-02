/* ensure_user_identity - make sure session has a durable save root.
 *
 * - logged_in: keep session user, mkdir xyzfs/users/<uuid>/home/...
 * - guest / missing / broken path: assign guest-<uuid> under
 *   xyzfs/users/guest-<uuid>/ and stamp session.pdl + current_login.txt
 *
 * Always syncs current_login.txt so claim/cycle/list agree.
 * Usage: ensure_user_identity.+x
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

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

static void mint_uuid(char *out, size_t out_sz) {
    FILE *f = fopen("/proc/sys/kernel/random/uuid", "r");
    if (f) {
        if (fgets(out, (int)out_sz, f)) {
            out[strcspn(out, "\r\n")] = '\0';
            fclose(f);
            if (strlen(out) >= 32) return;
        } else fclose(f);
    }
    unsigned int r = (unsigned int)(time(NULL) ^ (getpid() << 16));
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%08x%04x",
             r, (unsigned)(getpid() & 0xffff),
             (unsigned)((r >> 8) & 0xffff), (unsigned)((r >> 16) & 0xffff),
             (unsigned)time(NULL), (unsigned)(getpid() & 0xffff));
}

static void write_session_pdl(const char *mode, const char *uid, const char *uuuid,
                              const char *dname, const char *xyz,
                              const char *av_uuid, const char *av_path) {
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/xyzfs", login_root);
    mkdir_p(dir, 0755);
    snprintf(path, sizeof(path), "%s/session.pdl", dir);
    /* preserve active avatar if not provided */
    char old_av[128] = "", old_ap[512] = "";
    if (!av_uuid || !av_uuid[0]) {
        read_session_state("active_avatar_uuid", old_av, sizeof(old_av));
        read_session_state("active_avatar_path", old_ap, sizeof(old_ap));
        av_uuid = old_av;
        av_path = old_ap;
    }
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | xyzfs_session\n");
    fprintf(f, "META         | version            | 1.0\n");
    fprintf(f, "META         | notes              | Durable login or guest-<uuid> save root\n\n");
    fprintf(f, "STATE        | mode                 | %s\n", mode && mode[0] ? mode : "guest");
    fprintf(f, "STATE        | user_id              | %s\n", uid ? uid : "");
    fprintf(f, "STATE        | user_uuid            | %s\n", uuuid ? uuuid : "");
    fprintf(f, "STATE        | display_name         | %s\n", dname && dname[0] ? dname : "Guest");
    fprintf(f, "STATE        | xyzfs_path           | %s\n", xyz ? xyz : "");
    fprintf(f, "STATE        | logged_in_at         | %ld\n", (long)time(NULL));
    fprintf(f, "STATE        | active_avatar_uuid   | %s\n", av_uuid ? av_uuid : "");
    fprintf(f, "STATE        | active_avatar_path   | %s\n", av_path ? av_path : "");
    fclose(f);
}

static void write_current_login(const char *uid, const char *uuuid, const char *xyz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/current_login.txt", login_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "current_user_id=%s\n", uid ? uid : "");
    fprintf(f, "current_user_uuid=%s\n", uuuid ? uuuid : "");
    fprintf(f, "current_xyzfs=%s\n", xyz ? xyz : "");
    fclose(f);
}

static int path_is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void ensure_home_tree(const char *xyz_rel) {
    char home[PATH_BUF], av[PATH_BUF], wallet[PATH_BUF];
    snprintf(home, sizeof(home), "%s/%s/home", house_root, xyz_rel);
    snprintf(av, sizeof(av), "%s/avatars", home);
    mkdir_p(av, 0755);
    snprintf(wallet, sizeof(wallet), "%s/wallet.txt", home);
    struct stat st;
    if (stat(wallet, &st) != 0) {
        FILE *wf = fopen(wallet, "w");
        if (wf) { fprintf(wf, "tokens=0\n"); fclose(wf); }
    }
    char inv[PATH_BUF];
    snprintf(inv, sizeof(inv), "%s/inventory.txt", av);
    if (stat(inv, &st) != 0) {
        FILE *f = fopen(inv, "w");
        if (f) fclose(f);
    }
}

int main(void) {
    resolve_root();
    resolve_login_root();
    resolve_house_root();

    char mode[64] = "", uid[128] = "", uuuid[128] = "", dname[128] = "", xyz[512] = "";
    read_session_state("mode", mode, sizeof(mode));
    read_session_state("user_id", uid, sizeof(uid));
    read_session_state("user_uuid", uuuid, sizeof(uuuid));
    read_session_state("display_name", dname, sizeof(dname));
    read_session_state("xyzfs_path", xyz, sizeof(xyz));

    /* Fallback to current_login if session empty */
    if (!uuuid[0] || !xyz[0]) {
        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        if (!uid[0]) read_kv(cpath, "current_user_id", uid, sizeof(uid));
        if (!uuuid[0]) read_kv(cpath, "current_user_uuid", uuuid, sizeof(uuuid));
        if (!xyz[0]) read_kv(cpath, "current_xyzfs", xyz, sizeof(xyz));
    }

    int logged_in = (strcmp(mode, "logged_in") == 0 && uuuid[0] &&
                     uid[0] && strncmp(uid, "guest", 5) != 0);

    if (logged_in) {
        if (!xyz[0])
            snprintf(xyz, sizeof(xyz), "xyzfs/users/%s", uuuid);
        /* If path points at missing tree, still create it (don't drop to guest). */
        ensure_home_tree(xyz);
        if (!dname[0]) snprintf(dname, sizeof(dname), "%s", uid);
        write_session_pdl("logged_in", uid, uuuid, dname, xyz, NULL, NULL);
        write_current_login(uid, uuuid, xyz);
        printf("identity: logged_in %s -> %s\n", uid, xyz);
        return 0;
    }

    /* Guest (or broken identity): durable guest-<uuid> save root */
    if (!uuuid[0] || strncmp(uid, "guest", 5) == 0 || !xyz[0] ||
        strstr(xyz, "guest-") == NULL) {
        /* Keep existing guest uuid if already guest-* path */
        if (!(uuuid[0] && xyz[0] && strstr(xyz, "guest-"))) {
            mint_uuid(uuuid, sizeof(uuuid));
        }
        char shortid[16];
        snprintf(shortid, sizeof(shortid), "%.8s", uuuid);
        snprintf(uid, sizeof(uid), "guest-%s", shortid);
        snprintf(dname, sizeof(dname), "guest-%s", shortid);
        snprintf(xyz, sizeof(xyz), "xyzfs/users/guest-%s", uuuid);
    } else {
        /* already guest path */
        if (!uid[0]) snprintf(uid, sizeof(uid), "guest-%.8s", uuuid);
        if (!dname[0]) snprintf(dname, sizeof(dname), "%s", uid);
    }

    ensure_home_tree(xyz);
    write_session_pdl("guest", uid, uuuid, dname, xyz, NULL, NULL);
    write_current_login(uid, uuuid, xyz);
    printf("identity: guest %s -> %s\n", uid, xyz);
    return 0;
}
