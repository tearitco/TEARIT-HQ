/* userpal_login - sets user-pal's own current_login.txt to an EXISTING
 * user (refuses if the user directory doesn't exist). Writes:
 *   current_user_id, current_user_uuid, current_xyzfs, logged_in_at
 * so sibling apps can open the correct xyzfs/users/<uuid>/ tree.
 *
 * current_login.txt is SHARED + PERSISTENT (install root, never a
 * session throwaway) - same rule as before session isolation.
 *
 * Legacy profiles without uuid get a uuid + xyzfs tree minted on first
 * login (backfill), so old users/jb still work.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_login.+x <user_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512
#define UUID_LEN 64

static char project_root[MAX_PATH] = ".";
static char install_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

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

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}


/* Write xyzfs/session.pdl - who is logged in (or guest). Character
 * creation and sibling apps read this as the durable session identity. */
static void write_session_pdl(const char *install,
                              const char *mode,
                              const char *user_id,
                              const char *user_uuid,
                              const char *display_name,
                              const char *xyzfs_rel,
                              const char *active_avatar_uuid,
                              const char *active_avatar_path)
{
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/xyzfs", install);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/session.pdl", dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | xyzfs_session\n");
    fprintf(f, "META         | version            | 1.0\n");
    fprintf(f, "META         | notes              | Current logged-in user or guest for this machine\n\n");
    fprintf(f, "STATE        | mode                 | %s\n", mode && mode[0] ? mode : "guest");
    fprintf(f, "STATE        | user_id              | %s\n", user_id ? user_id : "");
    fprintf(f, "STATE        | user_uuid            | %s\n", user_uuid ? user_uuid : "");
    fprintf(f, "STATE        | display_name         | %s\n",
            (display_name && display_name[0]) ? display_name
            : ((mode && strcmp(mode, "guest") == 0) ? "Guest" : ""));
    fprintf(f, "STATE        | xyzfs_path           | %s\n", xyzfs_rel ? xyzfs_rel : "");
    fprintf(f, "STATE        | logged_in_at         | %ld\n", (long)time(NULL));
    fprintf(f, "STATE        | active_avatar_uuid   | %s\n", active_avatar_uuid ? active_avatar_uuid : "");
    fprintf(f, "STATE        | active_avatar_path   | %s\n", active_avatar_path ? active_avatar_path : "");
    fprintf(f, "\n");
    fprintf(f, "# mode=logged_in | guest\n");
    fprintf(f, "# xyzfs_path is relative to house root (parent of 0.user-pal)\n");
    fprintf(f, "# active_avatar_* set by 01.avatar-creation when a clone is minted/opened\n");
    fclose(f);
}

static void read_session_state(const char *install, const char *key, char *out, size_t out_sz)
{
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/xyzfs/session.pdl", install);
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
    unsigned int r = (unsigned int)(time(NULL) ^ (getpid() << 16) ^ (unsigned)rand());
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%08x%04x",
             r, (unsigned)(getpid() & 0xffff),
             (unsigned)((r >> 8) & 0xffff), (unsigned)((r >> 16) & 0xffff),
             (unsigned)time(NULL), (unsigned)(getpid() & 0xffff));
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

static int ensure_xyzfs(const char *uuid, const char *user_id,
                        const char *display_name, char *rel_out, size_t rel_sz) {
    char user_fs[PATH_BUF];
    snprintf(user_fs, sizeof(user_fs), "%s/xyzfs/users/%s", house_root, uuid);
    struct stat st;
    if (stat(user_fs, &st) != 0) {
        char home[PATH_BUF], projects[PATH_BUF], exchange[PATH_BUF], net[PATH_BUF], bin[PATH_BUF];
        snprintf(home, sizeof(home), "%s/home", user_fs);
        snprintf(projects, sizeof(projects), "%s/projects", user_fs);
        snprintf(exchange, sizeof(exchange), "%s/home/exchange", user_fs);
        snprintf(net, sizeof(net), "%s/home/net", user_fs);
        snprintf(bin, sizeof(bin), "%s/xyzfs/bin", install_root);
        if (mkdir_p(home, 0755) != 0) return -1;
        if (mkdir_p(projects, 0755) != 0) return -1;
        if (mkdir_p(exchange, 0755) != 0) return -1;
        if (mkdir_p(net, 0755) != 0) return -1;
        mkdir_p(bin, 0755);
        char meta[PATH_BUF];
        snprintf(meta, sizeof(meta), "%s/meta.txt", user_fs);
        FILE *mf = fopen(meta, "w");
        if (mf) {
            fprintf(mf, "uuid=%s\n", uuid);
            fprintf(mf, "user_id=%s\n", user_id);
            fprintf(mf, "display_name=%s\n", display_name);
            fprintf(mf, "created_at=%ld\n", (long)time(NULL));
            fprintf(mf, "backfilled=1\n");
            fclose(mf);
        }
    }
    snprintf(rel_out, rel_sz, "xyzfs/users/%s", uuid);
    return 0;
}

/* Rewrite profile.txt with uuid/xyzfs_path while keeping other keys. */
static void write_profile(const char *profile_path, const char *user_id,
                          const char *display_name, const char *uuid,
                          const char *xyzfs_rel, const char *created_at) {
    FILE *pf = fopen(profile_path, "w");
    if (!pf) return;
    fprintf(pf, "user_id=%s\n", user_id);
    fprintf(pf, "display_name=%s\n", display_name[0] ? display_name : user_id);
    fprintf(pf, "uuid=%s\n", uuid);
    fprintf(pf, "xyzfs_path=%s\n", xyzfs_rel);
    if (created_at[0]) fprintf(pf, "created_at=%s\n", created_at);
    else fprintf(pf, "created_at=%ld\n", (long)time(NULL));
    fclose(pf);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: userpal_login.+x <user_id>\n");
        return 1;
    }
    resolve_root();
    resolve_install_root();
    resolve_house_root();
    const char *user_id = argv[1];

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", install_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) {
        fprintf(stderr, "No such user '%s'.\n", user_id);
        return 1;
    }

    char profile_path[PATH_BUF];
    snprintf(profile_path, sizeof(profile_path), "%s/profile.txt", user_dir);

    char display_name[128], uuid[UUID_LEN], xyzfs_rel[PATH_BUF], created_at[64];
    read_kv_str(profile_path, "display_name", display_name, sizeof(display_name));
    read_kv_str(profile_path, "uuid", uuid, sizeof(uuid));
    read_kv_str(profile_path, "xyzfs_path", xyzfs_rel, sizeof(xyzfs_rel));
    read_kv_str(profile_path, "created_at", created_at, sizeof(created_at));

    if (!uuid[0]) {
        mint_uuid(uuid, sizeof(uuid));
        if (ensure_xyzfs(uuid, user_id, display_name, xyzfs_rel, sizeof(xyzfs_rel)) != 0) {
            fprintf(stderr, "Could not backfill xyzfs for '%s'.\n", user_id);
            return 1;
        }
        write_profile(profile_path, user_id, display_name, uuid, xyzfs_rel, created_at);
    } else {
        /* Ensure tree still exists (re-create skeleton if wiped). */
        if (ensure_xyzfs(uuid, user_id, display_name, xyzfs_rel, sizeof(xyzfs_rel)) != 0) {
            fprintf(stderr, "Could not open xyzfs for '%s'.\n", user_id);
            return 1;
        }
        if (!xyzfs_rel[0]) {
            snprintf(xyzfs_rel, sizeof(xyzfs_rel), "xyzfs/users/%s", uuid);
            write_profile(profile_path, user_id, display_name, uuid, xyzfs_rel, created_at);
        }
    }

    char login_path[PATH_BUF];
    /* Prefer install_root so session-dir symlink targets still get the write. */
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", install_root);
    FILE *f = fopen(login_path, "w");
    if (!f) {
        /* Fallback to project_root (symlink case). */
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
        f = fopen(login_path, "w");
    }
    if (!f) { fprintf(stderr, "Could not write current_login.txt.\n"); return 1; }
    fprintf(f, "current_user_id=%s\n", user_id);
    fprintf(f, "current_user_uuid=%s\n", uuid);
    fprintf(f, "current_xyzfs=%s\n", xyzfs_rel);
    fprintf(f, "logged_in_at=%ld\n", (long)time(NULL));
    fclose(f);

    /* Preserve active avatar if re-logging same user; else clear. */
    char prev_uuid[UUID_LEN], av_uuid[128], av_path[PATH_BUF];
    read_session_state(install_root, "user_uuid", prev_uuid, sizeof(prev_uuid));
    av_uuid[0] = av_path[0] = '\0';
    if (prev_uuid[0] && strcmp(prev_uuid, uuid) == 0) {
        read_session_state(install_root, "active_avatar_uuid", av_uuid, sizeof(av_uuid));
        read_session_state(install_root, "active_avatar_path", av_path, sizeof(av_path));
    }
    /* REAL FIX 2026-08-13, direct bug report: taskbar not showing the
     * user avatar. The preserve-check above is FRAGILE - it only
     * carries the avatar link forward if session.pdl PRIOR user_uuid
     * still matches this login. Anything that resets or recreates
     * session.pdl between avatar creation and a later login (a fresh
     * checkout, a migration, any process rewriting session.pdl
     * outside this exact flow) silently drops the avatar link
     * forever, even though the avatar directory itself is still real
     * on disk - confirmed live: a real, valid avatar (rendered and
     * visually verified) existed the whole time, session.pdl own
     * active_avatar_uuid was just empty.
     *
     * REAL BUG number 2 found while writing the first attempt at this
     * fix: install_root (resolved from THIS process own cwd or
     * PRISC_PROJECT_ROOT) is NOT reliably the same directory as the
     * real house root the rest of the system uses. In
     * khtpm_taskbar_manager.c, ktb_get_avatar_dir() always builds its
     * path from its OWN house_root directly, never from session.pdl
     * xyzfs_path field. Confirmed live: install_root resolved to this
     * app own directory here, which has its own unrelated LOCAL
     * users and xyzfs dirs with no real user home trees inside them,
     * while the REAL per-user xyzfs/users/UUID/home/avatars tree
     * lives only under the true house root - the naive install_root
     * based path silently pointed at a directory that does not exist.
     * Fixed: find the real house root the SAME way play_event.sh
     * already does, an anchor search upward for the
     * 101.mutaclsym system directory, the proven house-root marker
     * already used elsewhere in this codebase for exactly this
     * problem, instead of trusting this process own install_root.
     * FALLBACK: if the preserve-check found nothing, scan the real
     * house root avatars directory for an existing avatar (a
     * directory containing a real sprite.csv) and use the first one
     * found, rather than leaving a real avatar permanently
     * disconnected until someone notices and hand-fixes session.pdl. */
    if (!av_uuid[0]) {
        char real_house_root[PATH_BUF] = "";
        char probe[PATH_BUF];
        snprintf(probe, sizeof(probe), "%s", install_root);
        for (;;) {
            DIR *td = opendir(probe);
            int found = 0;
            if (td) {
                struct dirent *e;
                while ((e = readdir(td))) {
                    if (strncmp(e->d_name, "101.mutaclsym", 13) == 0) { found = 1; break; }
                }
                closedir(td);
            }
            if (found) { snprintf(real_house_root, sizeof(real_house_root), "%s", probe); break; }
            char *slash = strrchr(probe, '/');
            if (!slash || slash == probe) break;
            *slash = '\0';
        }
        char avatars_dir[PATH_BUF];
        snprintf(avatars_dir, sizeof(avatars_dir), "%s/%s/home/avatars",
                 real_house_root[0] ? real_house_root : install_root, xyzfs_rel);
        DIR *ad = opendir(avatars_dir);
        if (ad) {
            struct dirent *de;
            while ((de = readdir(ad))) {
                if (de->d_name[0] == '.') continue;
                char csv_probe[PATH_BUF];
                snprintf(csv_probe, sizeof(csv_probe), "%s/%s/sprite.csv", avatars_dir, de->d_name);
                struct stat st;
                if (stat(csv_probe, &st) == 0 && S_ISREG(st.st_mode)) {
                    snprintf(av_uuid, sizeof(av_uuid), "%s", de->d_name);
                    snprintf(av_path, sizeof(av_path), "%s/home/avatars/%s", xyzfs_rel, de->d_name);
                    break;
                }
            }
            closedir(ad);
        }
    }
    write_session_pdl(install_root, "logged_in", user_id, uuid,
                      display_name, xyzfs_rel, av_uuid, av_path);

    printf("Logged in as '%s'.\n", user_id);
    return 0;
}
