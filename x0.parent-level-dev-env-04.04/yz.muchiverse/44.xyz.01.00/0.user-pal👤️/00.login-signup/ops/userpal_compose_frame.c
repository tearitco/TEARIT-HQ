/* userpal_compose_frame - renders user-pal's own single login screen
 * into pieces/apps/player_app/view.txt, modeled directly on
 * pal-forum's own forum_compose_frame.c. ${piece_methods} renders the
 * numbered METHOD buttons - this op never draws that menu itself, only
 * the surrounding chrome + current_login.txt status.
 *
 * Writes ONLY view.txt, never pieces/display/current_frame.txt
 * directly (XYZOS-STANDARDS sec. 20 - ONE VISIBLE FRAME WRITER RULE).
 * Pings pieces/display/frame_changed.txt, the real render-trigger
 * marker.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60

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
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static FILE *g_view_out = NULL;
static void border(void) {
    if (g_view_out) { fputc('+', g_view_out); for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out); fputc('+', g_view_out); fputc('\n', g_view_out); }
}
static void line(const char *content) {
    int len = (int)strlen(content);
    if (len > BOX_W) len = BOX_W;
    if (g_view_out) {
        fprintf(g_view_out, "|%.*s", len, content);
        for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
        fputc('|', g_view_out);
        fputc('\n', g_view_out);
    }
}
static void blank(void) { line(""); }

static void ping_render_marker(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF], login_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/userpal_menu_state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    char current_user_id[128], current_uuid[128], current_xyzfs[256];
    read_kv_str(login_path, "current_user_id", current_user_id, sizeof(current_user_id));
    read_kv_str(login_path, "current_user_uuid", current_uuid, sizeof(current_uuid));
    read_kv_str(login_path, "current_xyzfs", current_xyzfs, sizeof(current_xyzfs));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    border();
    line("  U S E R - P A L");
    border();
    blank();
    if (current_user_id[0]) {
        char rowbuf[BOX_W + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  Logged in as: %s", current_user_id);
#pragma GCC diagnostic pop
        line(rowbuf);
        if (current_uuid[0]) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(rowbuf, sizeof(rowbuf), "  uuid: %s", current_uuid);
#pragma GCC diagnostic pop
            line(rowbuf);
        }
        if (current_xyzfs[0]) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(rowbuf, sizeof(rowbuf), "  xyzfs: %s", current_xyzfs);
#pragma GCC diagnostic pop
            line(rowbuf);
        }
    } else {
        line("  Not logged in.");
    }
    blank();
    line("  Enter a user ID (and display name, if new), then");
    line("  Create Account / Log In — or Switch: an account below.");
    blank();
    line("  Accounts on this machine:");
    {
        char udir[PATH_BUF];
        snprintf(udir, sizeof(udir), "%s/users", project_root);
        DIR *d = opendir(udir);
        int shown = 0;
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                char prof[PATH_BUF], dname[128] = "";
                snprintf(prof, sizeof(prof), "%s/%s/profile.txt", udir, ent->d_name);
                struct stat st;
                if (stat(prof, &st) != 0) continue;
                read_kv_str(prof, "display_name", dname, sizeof(dname));
                char rowbuf[BOX_W + 1];
                int is_cur = (current_user_id[0] && strcmp(current_user_id, ent->d_name) == 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(rowbuf, sizeof(rowbuf), "  %s %s%s%s",
                         is_cur ? ">" : " ",
                         ent->d_name,
                         dname[0] ? " — " : "",
                         dname);
#pragma GCC diagnostic pop
                line(rowbuf);
                shown++;
            }
            closedir(d);
        }
        if (!shown) line("  (none yet — Create Account)");
    }
    blank();

    if (last_message[0]) {
        char rowbuf[BOX_W + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  %s", last_message);
#pragma GCC diagnostic pop
        line(rowbuf);
        blank();
    }
    border();

    fclose(g_view_out);
    ping_render_marker();
    return 0;
}
