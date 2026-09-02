/* userpal_menu_input - piece.pdl METHOD dispatch for login UI.
 * Regenerates account SWITCH rows from users/ so the login screen is
 * also an account switcher (pick a known account without retyping ID).
 *
 * Usage: userpal_menu_input.+x <keycode>
 *   key 0 = idle: refresh account METHOD list only
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 64

typedef struct {
    char label[160];
    char command[512];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
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

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[32][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

/* Resolve users/ dir (session symlink or install). */
static void users_dir(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/users", project_root);
}

/* Rebuild login piece.pdl: fixed actions + one SWITCH row per account. */
static void regenerate_login_pdl(void) {
    char pdl[PATH_BUF], udir[PATH_BUF], cur[128];
    snprintf(pdl, sizeof(pdl), "%s/projects/user-pal/pieces/login/piece.pdl", project_root);
    users_dir(udir, sizeof(udir));
    {
        char login_path[PATH_BUF];
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
        read_kv_str_local(login_path, "current_user_id", cur, sizeof(cur));
    }

    FILE *f = fopen(pdl, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | login\n");
    fprintf(f, "META         | version            | 1.0\n\n");
    fprintf(f, "METHOD       | Create Account          | SIGNUP\n");
    fprintf(f, "METHOD       | Log In                  | LOGIN\n");
    fprintf(f, "METHOD       | Log Out                 | LOGOUT\n");

    DIR *d = opendir(udir);
    if (d) {
        struct dirent *ent;
        char names[MAX_MENU_ITEMS][128];
        int n = 0;
        while (n < MAX_MENU_ITEMS - 8 && (ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char prof[PATH_BUF];
            snprintf(prof, sizeof(prof), "%s/%s/profile.txt", udir, ent->d_name);
            struct stat st;
            if (stat(prof, &st) != 0) continue;
            snprintf(names[n], sizeof(names[n]), "%s", ent->d_name);
            n++;
        }
        closedir(d);
        /* sort simple bubble for stable order */
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(names[i], names[j]) > 0) {
                    char t[128];
                    snprintf(t, sizeof(t), "%s", names[i]);
                    snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                    snprintf(names[j], sizeof(names[j]), "%s", t);
                }
        for (int i = 0; i < n; i++) {
            char prof[PATH_BUF], dname[128] = "", mark[8] = "";
            snprintf(prof, sizeof(prof), "%s/%s/profile.txt", udir, names[i]);
            read_kv_str_local(prof, "display_name", dname, sizeof(dname));
            if (!dname[0]) snprintf(dname, sizeof(dname), "%s", names[i]);
            if (cur[0] && strcmp(cur, names[i]) == 0)
                snprintf(mark, sizeof(mark), " *");
            /* label shows id + display; command switches by user_id */
            fprintf(f, "METHOD       | Switch: %s (%s)%s | SWITCH:%s\n",
                    names[i], dname, mark, names[i]);
        }
    }
    fclose(f);
}

static int load_menu_items(MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/user-pal/pieces/login/piece.pdl", project_root);
    FILE *f = fopen(pdl_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max_items && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *label = trim(p1 + 1);
        char *command = trim(p2 + 1);
        snprintf(items[n].label, sizeof(items[n].label), "%s", label);
        snprintf(items[n].command, sizeof(items[n].command), "%s", command);
        n++;
    }
    fclose(f);
    return n;
}

static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/user-pal/manager/gui_state.txt", project_root);
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
        }
    }
    fclose(f);
}

static void run_capture(const char *cmd, char *message, size_t message_sz) {
    char full[PATH_BUF * 2];
    snprintf(full, sizeof(full), "cd '%s' && %s 2>&1", project_root, cmd);
    FILE *p = popen(full, "r");
    if (!p) { snprintf(message, message_sz, "Action failed to start."); return; }
    if (!fgets(message, (int)message_sz, p)) snprintf(message, message_sz, "Ran: %s", cmd);
    else message[strcspn(message, "\n")] = '\0';
    pclose(p);
}

static void bump_screen(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/userpal_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

/* Content fingerprint of piece.pdl — used so idle ticks do NOT recompose
 * when the catalog is unchanged (mutaclsym-style: only paint on new info). */
static void pdl_fingerprint(const char *path, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(out, out_sz, "missing");
        return;
    }
    unsigned long h = 5381;
    int c;
    while ((c = fgetc(f)) != EOF)
        h = ((h << 5) + h) + (unsigned char)c;
    fclose(f);
    snprintf(out, out_sz, "%lu", h);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/userpal_menu_state.txt", project_root);

    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path),
             "%s/projects/user-pal/pieces/login/piece.pdl", project_root);

    /* Always refresh account list so Switch:* rows match users/. */
    regenerate_login_pdl();

    int key = atoi(argv[1]);
    if (key == 0) {
        /* Idle: recompute catalog quietly; ONLY pulse screen when the
         * METHOD table actually changed. Unconditional bump_screen() here
         * caused multi-frame-per-second spam (every pal loop sleep tick). */
        char fp[64], prev[64];
        pdl_fingerprint(pdl_path, fp, sizeof(fp));
        read_kv_str_local(state_path, "login_pdl_fp", prev, sizeof(prev));
        if (strcmp(prev, fp) != 0) {
            write_kv(state_path, "login_pdl_fp", fp);
            bump_screen();
        }
        return 0;
    }

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "SIGNUP") == 0) {
            char user_id[128], display_name[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            read_gui_state_str("display_name_input", display_name, sizeof(display_name));
            if (!user_id[0] || !display_name[0]) {
                snprintf(message, sizeof(message), "Enter a user ID and display name, then click Create Account.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_create_account.+x '%s' '%s'", user_id, display_name);
                run_capture(cmdbuf, message, sizeof(message));
                if (strstr(message, "created")) {
                    char loginbuf[PATH_BUF];
                    snprintf(loginbuf, sizeof(loginbuf), "./ops/+x/userpal_login.+x '%s'", user_id);
                    char discard[MAX_LINE];
                    run_capture(loginbuf, discard, sizeof(discard));
                    snprintf(message, sizeof(message), "Account created - logged in as %s.", user_id);
                    regenerate_login_pdl(); /* new Switch row */
                }
            }
        } else if (strcmp(cmd, "LOGIN") == 0) {
            char user_id[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Enter your user ID, then click Log In — or pick Switch: below.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_login.+x '%s'", user_id);
                run_capture(cmdbuf, message, sizeof(message));
                regenerate_login_pdl();
            }
        } else if (strcmp(cmd, "LOGOUT") == 0) {
            char cmdbuf[PATH_BUF];
            snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_logout.+x");
            run_capture(cmdbuf, message, sizeof(message));
            regenerate_login_pdl();
        } else if (strncmp(cmd, "SWITCH:", 7) == 0) {
            const char *uid = cmd + 7;
            if (!uid[0]) {
                snprintf(message, sizeof(message), "Empty account id.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_login.+x '%s'", uid);
                char login_msg[MAX_LINE];
                run_capture(cmdbuf, login_msg, sizeof(login_msg));
                snprintf(message, sizeof(message), "Switched account -> %s (%s)", uid, login_msg);
                regenerate_login_pdl();
            }
        } else {
            snprintf(message, sizeof(message), "Unknown command.");
        }
    }

    write_kv(state_path, "last_message", message);
    bump_screen();
    return 0;
}
