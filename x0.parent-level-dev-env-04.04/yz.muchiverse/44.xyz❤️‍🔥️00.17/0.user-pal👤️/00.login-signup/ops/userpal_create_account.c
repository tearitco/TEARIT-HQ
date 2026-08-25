/* userpal_create_account - creates a new user-pal identity:
 *   users/<user_id>/profile.txt  (login name + display name + uuid)
 *   xyzfs/users/<uuid>/          (per-user filesystem tree, UUID-tagged
 *                                 so many users coexist under one xyzfs/)
 *
 * No password (family v1 - same as forum/chain create ops).
 * Refuses if user_id already exists.
 *
 * install root resolution: when PRISC_PROJECT_ROOT is a session dir
 * (pieces/sessions/<id>/), users/ is a symlink to the real install.
 * xyzfs MUST be written under that real install, not inside the
 * throwaway session. resolve_install_root() follows users/ (or
 * current_login.txt) to the durable root.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_create_account.+x <user_id> <display_name> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define UUID_LEN 64

static char project_root[MAX_PATH] = ".";
static char install_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

/* Durable install root (never a pieces/sessions throwaway dir). */
static void resolve_install_root(void) {
    char probe[PATH_BUF];
    char real[MAX_PATH];

    snprintf(probe, sizeof(probe), "%s/users", project_root);
    if (realpath(probe, real)) {
        char *slash = strrchr(real, '/');
        if (slash && slash != real) {
            *slash = '\0';
            snprintf(install_root, sizeof(install_root), "%s", real);
            return;
        }
    }

    snprintf(probe, sizeof(probe), "%s/current_login.txt", project_root);
    if (realpath(probe, real)) {
        char *slash = strrchr(real, '/');
        if (slash && slash != real) {
            *slash = '\0';
            snprintf(install_root, sizeof(install_root), "%s", real);
            return;
        }
    }

    snprintf(install_root, sizeof(install_root), "%s", project_root);
}

/* house root = parent of 0.user-pal (install_root is <house>/0.user-pal/00.login-signup).
 * Per-user xyzfs homes live under <house>/xyzfs/users/<uuid>/ (clean schema §9). */
static void resolve_house_root(void) {
    const char *env = getenv("HOUSE_ROOT");
    if (env && env[0]) { snprintf(house_root, sizeof(house_root), "%s", env); return; }
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", install_root);
    char *s1 = strrchr(tmp, '/');
    if (s1 && s1 != tmp) *s1 = '\0';
    char *s2 = strrchr(tmp, '/');
    if (s2 && s2 != tmp) *s2 = '\0';
    snprintf(house_root, sizeof(house_root), "%s", tmp);
}

static int valid_user_id(const char *id) {
    if (!id[0]) return 0;
    for (const char *p = id; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) return 0;
    }
    return 1;
}

static int mint_uuid(char *out, size_t out_sz) {
    FILE *f = fopen("/proc/sys/kernel/random/uuid", "r");
    if (f) {
        if (fgets(out, (int)out_sz, f)) {
            out[strcspn(out, "\r\n")] = '\0';
            fclose(f);
            if (strlen(out) >= 32) return 0;
        } else {
            fclose(f);
        }
    }
    /* Fallback: time + pid + random (not RFC4122, but unique enough for local multi-user). */
    unsigned int r = (unsigned int)(time(NULL) ^ (getpid() << 16) ^ (unsigned)rand());
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%08x%04x",
             r,
             (unsigned)(getpid() & 0xffff),
             (unsigned)((r >> 8) & 0xffff),
             (unsigned)((r >> 16) & 0xffff),
             (unsigned)time(NULL),
             (unsigned)(getpid() & 0xffff));
    return 0;
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
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* Provision xyzfs/users/<uuid>/{home,projects} + meta.txt.
 * Returns 0 on success. rel_out gets "xyzfs/users/<uuid>" for profile. */
static int provision_xyzfs(const char *uuid, const char *user_id,
                           const char *display_name,
                           char *rel_out, size_t rel_sz) {
    char user_fs[PATH_BUF];
    snprintf(user_fs, sizeof(user_fs), "%s/xyzfs/users/%s", house_root, uuid);

    struct stat st;
    if (stat(user_fs, &st) == 0) {
        /* Collision (vanishingly rare) - refuse so caller can remint. */
        return -1;
    }

    char home[PATH_BUF], projects[PATH_BUF], exchange[PATH_BUF], net[PATH_BUF];
    snprintf(home, sizeof(home), "%s/home", user_fs);
    snprintf(projects, sizeof(projects), "%s/projects", user_fs);
    snprintf(exchange, sizeof(exchange), "%s/home/exchange", user_fs);
    snprintf(net, sizeof(net), "%s/home/net", user_fs);

    if (mkdir_p(home, 0755) != 0) return -1;
    if (mkdir_p(projects, 0755) != 0) return -1;
    if (mkdir_p(exchange, 0755) != 0) return -1;
    if (mkdir_p(net, 0755) != 0) return -1;

    /* Shared bin/ once (empty seed for later shared ops). */
    char bin[PATH_BUF];
    snprintf(bin, sizeof(bin), "%s/xyzfs/bin", install_root);
    mkdir_p(bin, 0755);

    char meta[PATH_BUF];
    snprintf(meta, sizeof(meta), "%s/meta.txt", user_fs);
    FILE *mf = fopen(meta, "w");
    if (mf) {
        fprintf(mf, "uuid=%s\n", uuid);
        fprintf(mf, "user_id=%s\n", user_id);
        fprintf(mf, "display_name=%s\n", display_name);
        fprintf(mf, "created_at=%ld\n", (long)time(NULL));
        fclose(mf);
    }

    snprintf(rel_out, rel_sz, "xyzfs/users/%s", uuid);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: userpal_create_account.+x <user_id> <display_name>\n");
        return 1;
    }
    resolve_root();
    resolve_install_root();
    resolve_house_root();
    const char *user_id = argv[1];
    const char *display_name = argv[2];

    if (!valid_user_id(user_id)) {
        fprintf(stderr, "Invalid user_id - letters, digits, _ and - only.\n");
        return 1;
    }

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", install_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) == 0) {
        fprintf(stderr, "User '%s' already exists.\n", user_id);
        return 1;
    }

    char users_root[PATH_BUF];
    snprintf(users_root, sizeof(users_root), "%s/users", install_root);
    mkdir(users_root, 0755);
    if (mkdir(user_dir, 0755) != 0) {
        fprintf(stderr, "Could not create user directory.\n");
        return 1;
    }

    char uuid[UUID_LEN];
    char xyzfs_rel[PATH_BUF];
    int provisioned = 0;
    for (int attempt = 0; attempt < 5; attempt++) {
        mint_uuid(uuid, sizeof(uuid));
        if (provision_xyzfs(uuid, user_id, display_name, xyzfs_rel, sizeof(xyzfs_rel)) == 0) {
            provisioned = 1;
            break;
        }
    }
    if (!provisioned) {
        rmdir(user_dir);
        fprintf(stderr, "Could not provision xyzfs tree for new user.\n");
        return 1;
    }

    char path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(path, sizeof(path), "%s/profile.txt", user_dir);
#pragma GCC diagnostic pop
    FILE *pf = fopen(path, "w");
    if (pf) {
        fprintf(pf, "user_id=%s\n", user_id);
        fprintf(pf, "display_name=%s\n", display_name);
        fprintf(pf, "uuid=%s\n", uuid);
        fprintf(pf, "xyzfs_path=%s\n", xyzfs_rel);
        fprintf(pf, "created_at=%ld\n", (long)time(NULL));
        fclose(pf);
    }

    /* One-line stdout for menu_input run_capture (first line = last_message). */
    printf("Account '%s' created (uuid=%s xyzfs=%s).\n", user_id, uuid, xyzfs_rel);
    return 0;
}
