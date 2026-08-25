/* avatar_compose_frame - render current screen panel into view.txt
 * and write DNA picker palette vars into project gui_state for
 * fold+scroller labels on avatar_manage / customize. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LINE 512
#define MAX_DNA_MARKUP 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60
#define MAX_OPTS 32
/* KEY codes injected by DNA option buttons (send_command KEY:n).
 * menu_input maps these to cycle_dna set. Std numbered nav rows. */
#define DNA_KEY_SKIN    300
#define DNA_KEY_GENDER  310
#define DNA_KEY_HAIR    320
#define DNA_KEY_SHIRT   340
#define DNA_KEY_PANTS   360
#define DNA_KEY_HEIGHT  380
#define DNA_KEY_WEIGHT  400

static char project_root[MAX_PATH] = ".";
static char login_root[MAX_PATH] = ".";
static char house_root[MAX_PATH] = ".";
static FILE *g_view_out = NULL;

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
    char lines[128][MAX_LINE];
    int n = 0;
    if (f) { while (n < 128 && fgets(lines[n], MAX_LINE, f)) n++; fclose(f); }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!found && strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
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
static void border(void) {
    fputc('+', g_view_out);
    for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out);
    fputc('+', g_view_out); fputc('\n', g_view_out);
}
static void line(const char *c) {
    int len = (int)strlen(c); if (len > BOX_W) len = BOX_W;
    fprintf(g_view_out, "|%.*s", len, c);
    for (int i = len; i < BOX_W; i++) fputc(' ', g_view_out);
    fputc('|', g_view_out); fputc('\n', g_view_out);
}
static void blank(void) { line(""); }

static void get_screen(char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char buf[MAX_LINE];
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        const char *slash = strrchr(buf, '/');
        const char *base = slash ? slash + 1 : buf;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
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
static void resolve_user_wallet(char *uuid, size_t uuid_sz, char *xyzfs, size_t xyz_sz) {
    uuid[0] = '\0';
    xyzfs[0] = '\0';
    char mode[64] = "";
    read_session_state("mode", mode, sizeof(mode));
    if (strcmp(mode, "logged_in") == 0) {
        read_session_state("user_uuid", uuid, uuid_sz);
        read_session_state("xyzfs_path", xyzfs, xyz_sz);
    }
    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
    if (!uuid[0]) read_kv(login_path, "current_user_uuid", uuid, uuid_sz);
    if (!xyzfs[0]) read_kv(login_path, "current_xyzfs", xyzfs, xyz_sz);
}
static int tokens_balance(void) {
    char uuid[128], xyzfs[512];
    resolve_user_wallet(uuid, sizeof(uuid), xyzfs, sizeof(xyzfs));
    char wallet[PATH_BUF], tok[32] = "0";
    if (uuid[0] && xyzfs[0])
        snprintf(wallet, sizeof(wallet), "%s/%s/home/wallet.txt", house_root, xyzfs);
    else
        snprintf(wallet, sizeof(wallet), "%s/pieces/world_01/map_lobby/user_01/state.txt", project_root);
    read_kv(wallet, "tokens", tok, sizeof(tok));
    return atoi(tok);
}

static void ping(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

/* Only append frame marker when view bytes actually changed — stops
 * manage-screen flicker/spam when compose runs but DNA/text is identical. */
static int write_view_if_changed(const char *view_path, const char *new_content, size_t n) {
    FILE *oldf = fopen(view_path, "rb");
    if (oldf) {
        if (fseek(oldf, 0, SEEK_END) == 0) {
            long sz = ftell(oldf);
            if (sz >= 0 && (size_t)sz == n) {
                rewind(oldf);
                char *old = (char *)malloc(n > 0 ? n : 1);
                if (old) {
                    size_t got = n ? fread(old, 1, n, oldf) : 0;
                    int same = (got == n && (n == 0 || memcmp(old, new_content, n) == 0));
                    free(old);
                    fclose(oldf);
                    if (same) return 0;
                    /* different content — fall through to rewrite (file already closed) */
                    goto write_new;
                }
            }
        }
        fclose(oldf);
    }
write_new:
    {
        FILE *f = fopen(view_path, "wb");
        if (!f) return -1;
        if (n > 0 && new_content) fwrite(new_content, 1, n, f);
        fclose(f);
    }
    return 1;
}

static int load_list(const char *rel, char opts[][64], int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, rel);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char linebuf[MAX_LINE];
    int n = 0;
    while (n < max && fgets(linebuf, sizeof(linebuf), f)) {
        if (linebuf[0] == '#' || linebuf[0] == '\n') continue;
        linebuf[strcspn(linebuf, "\r\n")] = '\0';
        if (!linebuf[0]) continue;
        snprintf(opts[n], 64, "%s", linebuf);
        n++;
    }
    fclose(f);
    return n;
}

static int find_idx(char opts[][64], int n, const char *cur) {
    for (int i = 0; i < n; i++) if (strcmp(opts[i], cur) == 0) return i;
    return 0;
}

/* Load skin emojis for gender into opts[], return count. */
static int load_skins(const char *gender, char opts[][64], int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/dna/skin_tones.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char linebuf[MAX_LINE];
    int n = 0;
    while (n < max && fgets(linebuf, sizeof(linebuf), f)) {
        if (linebuf[0] == '#') continue;
        linebuf[strcspn(linebuf, "\r\n")] = '\0';
        char g[16], em[32], lab[32];
        int idx = 0;
        if (sscanf(linebuf, "%15[^|]|%d|%31[^|]|%31s", g, &idx, em, lab) >= 3) {
            if (strcmp(g, gender) == 0) {
                snprintf(opts[n], 64, "%s", em);
                n++;
            }
        }
    }
    fclose(f);
    return n;
}

static void build_bracket_strip(char *out, size_t out_sz, char opts[][64], int n, int cur) {
    out[0] = '\0';
    if (n <= 0) {
        snprintf(out, out_sz, "(none)");
        return;
    }
    if (cur < 0) cur = 0;
    if (cur >= n) cur = n - 1;
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        char piece[96];
        if (i == cur)
            snprintf(piece, sizeof(piece), "[%s] ", opts[i]);
        else
            snprintf(piece, sizeof(piece), "%s ", opts[i]);
        size_t pl = strlen(piece);
        if (used + pl >= out_sz - 1) break;
        memcpy(out + used, piece, pl + 1);
        used += pl;
    }
}

static void gui_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/projects/avatar-creation/manager/gui_state.txt", project_root);
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/projects/avatar-creation/manager", project_root);
    mkdir_p(dir, 0755);
}

/* Skip rewrite when value already matches — preserve fold_* and avoid spam. */
static void write_kv_if_changed(const char *path, const char *key, const char *value) {
    char cur[MAX_DNA_MARKUP];
    cur[0] = '\0';
    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_DNA_MARKUP];
        size_t kl = strlen(key);
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
                char *v = line + kl + 1;
                v[strcspn(v, "\r\n")] = '\0';
                snprintf(cur, sizeof(cur), "%s", v);
                break;
            }
        }
        fclose(f);
    }
    if (strcmp(cur, value) == 0) return;
    /* Long-value write (DNA option markup) — same RMW as write_kv but big lines. */
    char lines[256][MAX_DNA_MARKUP];
    int n = 0;
    f = fopen(path, "r");
    if (f) {
        while (n < 256 && fgets(lines[n], MAX_DNA_MARKUP, f)) n++;
        fclose(f);
    }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!found && strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

/* One navigable button per option — KEY:base+i → menu_input SET_DNA.
 * Current choice bracketed in the label. Single-line markup for gui_state. */
static void build_opt_buttons(char *out, size_t out_sz, char opts[][64], int n, int cur, int key_base) {
    out[0] = '\0';
    if (n <= 0) {
        snprintf(out, out_sz, "<text label=\"  (no options)\" /><br/>");
        return;
    }
    size_t used = 0;
    for (int i = 0; i < n; i++) {
        char piece[192];
        if (i == cur)
            snprintf(piece, sizeof(piece),
                     "<button label=\"[%s]\" onClick=\"KEY:%d\" /><br/>",
                     opts[i], key_base + i);
        else
            snprintf(piece, sizeof(piece),
                     "<button label=\"%s\" onClick=\"KEY:%d\" /><br/>",
                     opts[i], key_base + i);
        size_t pl = strlen(piece);
        if (used + pl >= out_sz - 1) break;
        memcpy(out + used, piece, pl + 1);
        used += pl;
    }
}

/* DNA fold headers + numbered option buttons (${dna_opts_*}). */
static void write_dna_picker_vars(const char *state_path, const char *selected) {
    char gpath[PATH_BUF];
    gui_path(gpath, sizeof(gpath));
    char markup[MAX_DNA_MARKUP];
    char opts[MAX_OPTS][64];
    int n, cur;

    if (!selected || !selected[0]) {
        write_kv_if_changed(gpath, "skin_emoji", "?");
        write_kv_if_changed(gpath, "gender_view", "");
        write_kv_if_changed(gpath, "hair_color", "");
        write_kv_if_changed(gpath, "shirt_color", "");
        write_kv_if_changed(gpath, "pants_color", "");
        write_kv_if_changed(gpath, "height_cm", "");
        write_kv_if_changed(gpath, "weight_kg", "");
        write_kv_if_changed(gpath, "dna_opts_skin", "");
        write_kv_if_changed(gpath, "dna_opts_gender", "");
        write_kv_if_changed(gpath, "dna_opts_hair", "");
        write_kv_if_changed(gpath, "dna_opts_shirt", "");
        write_kv_if_changed(gpath, "dna_opts_pants", "");
        write_kv_if_changed(gpath, "dna_opts_height", "");
        write_kv_if_changed(gpath, "dna_opts_weight", "");
        return;
    }

    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, selected);

    char gender[32], si[16], hair[32], shirt[32], pants[32], h[16], w[16];
    read_kv(sp, "gender", gender, sizeof(gender));
    if (!gender[0]) snprintf(gender, sizeof(gender), "male");
    read_kv(sp, "skin_index", si, sizeof(si));
    read_kv(sp, "hair_color", hair, sizeof(hair));
    read_kv(sp, "shirt_color", shirt, sizeof(shirt));
    read_kv(sp, "pants_color", pants, sizeof(pants));
    read_kv(sp, "height", h, sizeof(h));
    read_kv(sp, "weight", w, sizeof(w));
    (void)state_path;

    /* Skin — KEY 300+idx */
    n = load_skins(gender, opts, MAX_OPTS);
    cur = atoi(si);
    if (n > 0) { if (cur < 0) cur = 0; if (cur >= n) cur = n - 1; }
    write_kv_if_changed(gpath, "skin_emoji", (n > 0) ? opts[cur] : "?");
    build_opt_buttons(markup, sizeof(markup), opts, n, cur, DNA_KEY_SKIN);
    write_kv_if_changed(gpath, "dna_opts_skin", markup);

    /* Gender — KEY 310+idx */
    snprintf(opts[0], 64, "male");
    snprintf(opts[1], 64, "female");
    cur = (strcmp(gender, "female") == 0) ? 1 : 0;
    write_kv_if_changed(gpath, "gender_view", opts[cur]);
    build_opt_buttons(markup, sizeof(markup), opts, 2, cur, DNA_KEY_GENDER);
    write_kv_if_changed(gpath, "dna_opts_gender", markup);

    struct {
        const char *rel; const char *committed; const char *label_key;
        const char *opts_key; int key_base;
    } fields[] = {
        { "pieces/registry/dna/hair_colors.txt",  hair,  "hair_color",  "dna_opts_hair",   DNA_KEY_HAIR },
        { "pieces/registry/dna/shirt_colors.txt", shirt, "shirt_color", "dna_opts_shirt",  DNA_KEY_SHIRT },
        { "pieces/registry/dna/pants_colors.txt", pants, "pants_color", "dna_opts_pants",  DNA_KEY_PANTS },
        { "pieces/registry/dna/heights.txt",      h,     "height_cm",   "dna_opts_height", DNA_KEY_HEIGHT },
        { "pieces/registry/dna/weights.txt",      w,     "weight_kg",   "dna_opts_weight", DNA_KEY_WEIGHT },
    };
    for (int f = 0; f < 5; f++) {
        n = load_list(fields[f].rel, opts, MAX_OPTS);
        cur = find_idx(opts, n, fields[f].committed);
        if (n > 0) { if (cur < 0) cur = 0; if (cur >= n) cur = n - 1; }
        write_kv_if_changed(gpath, fields[f].label_key, (n > 0) ? opts[cur] : fields[f].committed);
        build_opt_buttons(markup, sizeof(markup), opts, n, cur, fields[f].key_base);
        write_kv_if_changed(gpath, fields[f].opts_key, markup);
    }
}

int main(void) {
    resolve_root();
    resolve_login_root();
    resolve_house_root();

    char view_path[PATH_BUF], state_path[PATH_BUF], screen[64];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/avatar_menu_state.txt", project_root);
    get_screen(screen, sizeof(screen));

    char last_message[MAX_LINE], selected[128];
    read_kv(state_path, "last_message", last_message, sizeof(last_message));
    read_kv(state_path, "selected_avatar", selected, sizeof(selected));

    char login_path[PATH_BUF], user_id[128], user_uuid[128], xyzfs_show[512];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
    user_id[0] = '\0';
    user_uuid[0] = '\0';
    xyzfs_show[0] = '\0';
    {
        char mode[64] = "";
        read_session_state("mode", mode, sizeof(mode));
        if (strcmp(mode, "logged_in") == 0) {
            read_session_state("user_id", user_id, sizeof(user_id));
            read_session_state("user_uuid", user_uuid, sizeof(user_uuid));
            read_session_state("xyzfs_path", xyzfs_show, sizeof(xyzfs_show));
        }
    }
    if (!user_id[0]) read_kv(login_path, "current_user_id", user_id, sizeof(user_id));
    if (!user_uuid[0]) read_kv(login_path, "current_user_uuid", user_uuid, sizeof(user_uuid));
    (void)xyzfs_show;

    /* Keep picker vars fresh when manage/customize is up (writes only if changed). */
    if (strcmp(screen, "avatar_manage") == 0 || strcmp(screen, "customize") == 0)
        write_dna_picker_vars(state_path, selected);

    char *mem = NULL;
    size_t mem_sz = 0;
    g_view_out = open_memstream(&mem, &mem_sz);
    if (!g_view_out) return 1;

    border();
    char row[BOX_W + 1];
    if (user_id[0])
        snprintf(row, sizeof(row), "  user: %s  tokens: %d", user_id, tokens_balance());
    else
        snprintf(row, sizeof(row), "  (no login - using local user_01)  tokens: %d", tokens_balance());
    line(row);
    blank();

    if (strcmp(screen, "main") == 0) {
        line("  Clone factory: faucet -> store -> Choose Clone.");
        line("  Avatars live in your xyzfs home/avatars/<uuid>/.");
        line("  Multiple clones on the desktop, like muchi-pals pets.");
    } else if (strcmp(screen, "faucet") == 0) {
        line("  Claim free tokens to buy more clones.");
    } else if (strcmp(screen, "store") == 0) {
        line("  Free starter once, then buy clones (20 tokens).");
        line("  Each clone gets a unique avatar UUID in xyzfs.");
    } else if (strcmp(screen, "avatars") == 0) {
        line("  CHOOSE CLONE — Enter a row to manage that clone.");
        line("  (name + full uuid; methods below are the live list)");
        blank();
        char inv[PATH_BUF];
        char xyzfs[512];
        {
            char uuid_tmp[128];
            resolve_user_wallet(uuid_tmp, sizeof(uuid_tmp), xyzfs, sizeof(xyzfs));
        }
        if (xyzfs[0])
            snprintf(inv, sizeof(inv), "%s/%s/home/avatars/inventory.txt", house_root, xyzfs);
        else
            snprintf(inv, sizeof(inv), "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
        FILE *f = fopen(inv, "r");
        int n = 0;
        if (f) {
            char id[128];
            while (fgets(id, sizeof(id), f)) {
                id[strcspn(id, "\r\n")] = '\0';
                if (!id[0]) continue;
                char spa[PATH_BUF], name[64], emoji[32];
                snprintf(spa, sizeof(spa), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, id);
                read_kv(spa, "name", name, sizeof(name));
                read_kv(spa, "skin_emoji", emoji, sizeof(emoji));
                if (!name[0]) snprintf(name, sizeof(name), "Clone");
                snprintf(row, sizeof(row), "  %s %s", emoji[0] ? emoji : "?", name);
                line(row);
                snprintf(row, sizeof(row), "     %s", id);
                line(row);
                n++;
            }
            fclose(f);
        }
        if (n == 0) line("  (none yet — visit Store)");
    } else if (strcmp(screen, "avatar_manage") == 0 || strcmp(screen, "customize") == 0) {
        if (!selected[0]) {
            line("  No clone selected — Back to Choose Clone.");
        } else {
            char spa[PATH_BUF];
            snprintf(spa, sizeof(spa), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, selected);
            char name[64], age[16], gender[32], emoji[32], hair[32], shirt[32], pants[32], h[16], w[16], asleep[8];
            read_kv(spa, "name", name, sizeof(name));
            read_kv(spa, "age", age, sizeof(age));
            read_kv(spa, "gender", gender, sizeof(gender));
            read_kv(spa, "skin_emoji", emoji, sizeof(emoji));
            read_kv(spa, "hair_color", hair, sizeof(hair));
            read_kv(spa, "shirt_color", shirt, sizeof(shirt));
            read_kv(spa, "pants_color", pants, sizeof(pants));
            read_kv(spa, "height", h, sizeof(h));
            read_kv(spa, "weight", w, sizeof(w));
            read_kv(spa, "asleep", asleep, sizeof(asleep));
            char gpath[PATH_BUF], pe[32];
            gui_path(gpath, sizeof(gpath));
            read_kv(gpath, "skin_emoji", pe, sizeof(pe));
            if (pe[0]) snprintf(emoji, sizeof(emoji), "%s", pe);
            snprintf(row, sizeof(row), "  %s %s  age %s  %s",
                     emoji[0] ? emoji : "?", name[0] ? name : "?", age[0] ? age : "?",
                     strcmp(asleep, "1") == 0 ? "SLEEPING" : "awake");
            line(row);
            snprintf(row, sizeof(row), "  uuid: %s", selected);
            line(row);
            snprintf(row, sizeof(row), "  %s  hair=%s shirt=%s pants=%s",
                     gender[0] ? gender : "?", hair, shirt, pants);
            line(row);
            snprintf(row, sizeof(row), "  height=%scm weight=%skg", h, w);
            line(row);
            blank();
            line("  [+] open DNA folds; each tone/option is a numbered row.");
        }
    }

    blank();
    if (last_message[0]) {
        snprintf(row, sizeof(row), "  > %s", last_message);
        line(row);
        blank();
    }
    border();
    fclose(g_view_out);
    g_view_out = NULL;

    /* Only touch view.txt + frame marker when panel text actually changed. */
    if (write_view_if_changed(view_path, mem ? mem : "", mem_sz) == 1)
        ping();
    free(mem);
    return 0;
}
