/* yahoo_compose_frame - per-screen renderer for yahoo-app.
 * Modeled directly on @.apps/my-chara-txt's mychara_compose_frame.c.
 *
 * Renders:
 *   bank.chtpm -> balance + watchlist into view.txt
 *   broker_select.chtpm -> dynamic broker list into piece.pdl
 *
 * Self-contained.
 * Usage: yahoo_compose_frame.+x */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

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
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    FILE *orig = fopen(path, "r");
    if (orig) {
        char line[MAX_LINE];
        int found = 0;
        while (fgets(line, sizeof(line), orig)) {
            if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                fprintf(f, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, f);
            }
        }
        if (!found) fprintf(f, "%s=%s\n", key, val);
        fclose(orig);
    } else {
        fprintf(f, "%s=%s\n", key, val);
    }
    fclose(f);
    rename(tmp, path);
}

/* Resolve the house-logged-in player id (human user id). Falls back to the
 * most recent xyzfs user dir when no login is active, else stays empty. */
static void resolve_player_id(char *out, size_t out_sz) {
    out[0] = '\0';
    char hr_path[PATH_BUF], house_root[MAX_PATH] = "";
    snprintf(hr_path, sizeof(hr_path), "%s/pieces/system/house_root.txt", project_root);
    FILE *f = fopen(hr_path, "r");
    if (f) {
        if (fgets(house_root, sizeof(house_root), f)) house_root[strcspn(house_root, "\r\n")] = '\0';
        fclose(f);
    }
    if (!house_root[0]) return;
    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/0.user-pal👤️/00.login-signup/current_login.txt", house_root);
    read_kv_str_local(login_path, "current_user_id", out, out_sz);
    if (out[0]) return;
    char users_dir[PATH_BUF];
    snprintf(users_dir, sizeof(users_dir), "%s/xyzfs/users", house_root);
    DIR *d = opendir(users_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char home[PATH_BUF];
            snprintf(home, sizeof(home), "%s/%s/home", users_dir, e->d_name);
            if (access(home, F_OK) == 0) {
                snprintf(out, out_sz, "%s", e->d_name);
                break;
            }
        }
        closedir(d);
    }
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void render_bank(char *view, size_t view_sz) {
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    char balance[64] = "0.00";
    char last_sym[64] = "N/A";
    char last_price[64] = "0.00";
    char last_time[64] = "N/A";
    read_kv_str_local(config_path, "bank_balance", balance, sizeof(balance));
    read_kv_str_local(config_path, "last_lookup_symbol", last_sym, sizeof(last_sym));
    read_kv_str_local(config_path, "last_lookup_price", last_price, sizeof(last_price));
    read_kv_str_local(config_path, "last_lookup_time", last_time, sizeof(last_time));

    char watchlist[512] = "(empty)";
    char user_hash[64] = "";
    char player_id[64] = "";
    read_kv_str_local(config_path, "user_hash", user_hash, sizeof(user_hash));
    read_kv_str_local(config_path, "player_id", player_id, sizeof(player_id));
    if (!user_hash[0]) {
        char ident[64] = "";
        resolve_player_id(ident, sizeof(ident));
        if (ident[0]) {
            unsigned h = 2166136261u;
            for (const char *p = ident; *p; p++) {
                h ^= (unsigned char)*p;
                h *= 16777619u;
            }
            snprintf(user_hash, sizeof(user_hash), "%06X", h & 0xFFFFFFu);
            snprintf(player_id, sizeof(player_id), "%s", ident);
        } else {
            srand(time(NULL) + getpid());
            const char *chars = "0123456789ABCDEF";
            for (int i = 0; i < 6; i++) {
                user_hash[i] = chars[rand() % 16];
            }
            user_hash[6] = '\0';
            snprintf(player_id, sizeof(player_id), "%s", user_hash);
        }
        write_kv(config_path, "user_hash", user_hash);
        write_kv(config_path, "player_id", player_id);
    }
    if (user_hash[0]) {
        char acc_path[PATH_BUF];
        snprintf(acc_path, sizeof(acc_path), "%s/usr_acc.%s.txt", project_root, user_hash);
        FILE *af = fopen(acc_path, "r");
        if (af) {
            char line[MAX_LINE];
            if (fgets(line, sizeof(line), af)) {
                line[strcspn(line, "\n")] = 0;
                char *copy = strdup(line);
                if (copy) {
                    char *tok = strtok(copy, ",");
                    if (tok && strcmp(tok, "balance") == 0) {
                        tok = strtok(NULL, ",");
                        tok = strtok(NULL, ",");
                    }
                    if (tok && strcmp(tok, "watchlist") == 0) {
                        tok = strtok(NULL, ",");
                        char *wl = watchlist;
                        wl[0] = '\0';
                        int first = 1;
                        while (tok && strcmp(tok, "stocks") != 0) {
                            if (!first) {
                                size_t len = strlen(wl);
                                snprintf(wl + len, sizeof(watchlist) - len, " %s", tok);
                            } else {
                                snprintf(wl, sizeof(watchlist), "%s", tok);
                                first = 0;
                            }
                            tok = strtok(NULL, ",");
                        }
                    }
                    free(copy);
                }
            }
            fclose(af);
        }
    }

    snprintf(view, view_sz,
        "+============================================================+\n"
        "|                    Y A H O O - X Y Z                      |\n"
        "+============================================================+\n"
        "|  Bank Balance: $%-14s                              |\n"
        "|  Player: %-46s|\n"
        "+============================================================+\n"
        "|  Watchlist: %-44s|\n"
        "+============================================================+\n"
        "|  Last Lookup: %-10s @ $%-10s  %-20s|\n"
        "+============================================================+\n",
        balance, player_id, watchlist, last_sym, last_price, last_time);
}

static void render_piece_pdl_brokers(const char *layout_name) {
    char brokers_path[PATH_BUF];
    snprintf(brokers_path, sizeof(brokers_path), "%s/pieces/system/brokers.txt", project_root);
    FILE *bf = fopen(brokers_path, "r");
    if (!bf) return;

    char pdl_dir[PATH_BUF];
    snprintf(pdl_dir, sizeof(pdl_dir), "%s/pieces/%s", project_root, layout_name);
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/%s.pdl", pdl_dir, layout_name);

    mkdir(pdl_dir, 0755);
    FILE *out = fopen(pdl_path, "w");
    if (!out) { fclose(bf); return; }

    fprintf(out, "SECTION      | KEY                | VALUE\n");
    fprintf(out, "----------------------------------------\n");
    fprintf(out, "META         | piece_id           | %s\n", layout_name);

    char line[MAX_LINE];
    int idx = 1;
    while (fgets(line, sizeof(line), bf)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0' || line[0] == '#') continue;
        char id[64] = "", name[64] = "", type[64] = "";
        if (sscanf(line, "%[^|]|%[^|]|%[^|]", id, name, type) >= 2) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "SELECT_BROKER:%s", id);
            fprintf(out, "METHOD       | %s (%s)                | %s\n", name, type, cmd);
            idx++;
        }
    }
    fclose(out);
    fclose(bf);
}

int main(int argc, char *argv[]) {
    resolve_root();

    char current_layout[PATH_BUF] = "";
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    {
        FILE *f = fopen(layout_path, "r");
        if (f) {
            if (fgets(current_layout, sizeof(current_layout), f)) {
                current_layout[strcspn(current_layout, "\r\n")] = '\0';
            }
            fclose(f);
        }
    }

    const char *layout_name = strrchr(current_layout, '/');
    if (layout_name) layout_name++; else layout_name = current_layout;

    if (strcmp(layout_name, "bank.chtpm") == 0) {
        char view[2048];
        render_bank(view, sizeof(view));
        char view_path[PATH_BUF];
        snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
        write_file(view_path, view);
    } else if (strcmp(layout_name, "broker_select.chtpm") == 0) {
        render_piece_pdl_brokers("broker_select");
    }

    ping_chtpm_render_marker(project_root);

    return 0;
}
