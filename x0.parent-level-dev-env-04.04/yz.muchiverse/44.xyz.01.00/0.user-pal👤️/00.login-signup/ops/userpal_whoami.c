/* userpal_whoami - prints current_user_id (and uuid/xyzfs if set)
 * from current_login.txt, or "none" if logged out.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_whoami.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
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

int main(void) {
    resolve_root();

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);

    char user_id[128], uuid[128], xyzfs[512];
    read_kv_str(login_path, "current_user_id", user_id, sizeof(user_id));
    read_kv_str(login_path, "current_user_uuid", uuid, sizeof(uuid));
    read_kv_str(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));

    if (!user_id[0]) {
        printf("none\n");
        return 0;
    }
    if (uuid[0] && xyzfs[0])
        printf("%s uuid=%s xyzfs=%s\n", user_id, uuid, xyzfs);
    else if (uuid[0])
        printf("%s uuid=%s\n", user_id, uuid);
    else
        printf("%s\n", user_id);
    return 0;
}
