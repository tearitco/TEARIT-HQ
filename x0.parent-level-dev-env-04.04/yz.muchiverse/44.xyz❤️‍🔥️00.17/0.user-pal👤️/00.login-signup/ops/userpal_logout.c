/* userpal_logout - clears user-pal's own current_login.txt (empty
 * current_user_id / uuid / xyzfs). Truncate-style rewrite so the file
 * always exists for a plain read.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_logout.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";
static char install_root[MAX_PATH] = ".";

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
    fprintf(f, "# xyzfs_path is relative to 00.login-signup install root\n");
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

int main(void) {
    resolve_root();
    resolve_install_root();

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", install_root);
    FILE *f = fopen(login_path, "w");
    if (!f) {
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
        f = fopen(login_path, "w");
    }
    if (!f) { fprintf(stderr, "Could not write current_login.txt.\n"); return 1; }
    fprintf(f, "current_user_id=\n");
    fprintf(f, "current_user_uuid=\n");
    fprintf(f, "current_xyzfs=\n");
    fclose(f);

    write_session_pdl(install_root, "guest", "", "", "Guest", "", "", "");

    printf("Logged out.\n");
    return 0;
}
