/* avatar_menu_input - METHOD dispatch for avatar-creation screens
 * (muchi_menu_input shape: derive screen from current_layout.txt,
 * regenerate avatars piece.pdl when on list screen). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 48
#define MAX_AVATARS 64
#define MAX_OPTS 32

typedef struct { char label[160]; char command[256]; } MenuItem;

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
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
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
    char lines[64][MAX_LINE];
    int n = 0;
    if (f) { while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++; fclose(f); }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!found && strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value); found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}
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
/* session.pdl first (logged_in), then current_login — same as generate_clone. */
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
static void resolve_xyzfs(char *xyzfs, size_t xyz_sz) {
    xyzfs[0] = '\0';
    char mode[64] = "";
    read_session_state("mode", mode, sizeof(mode));
    if (strcmp(mode, "logged_in") == 0)
        read_session_state("xyzfs_path", xyzfs, xyz_sz);
    if (!xyzfs[0]) {
        char login_path[PATH_BUF];
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
        read_kv(login_path, "current_xyzfs", xyzfs, xyz_sz);
    }
}
static void inventory_path(char *out, size_t out_sz) {
    char xyzfs[512];
    resolve_xyzfs(xyzfs, sizeof(xyzfs));
    if (xyzfs[0])
        snprintf(out, out_sz, "%s/%s/home/avatars/inventory.txt", house_root, xyzfs);
    else
        snprintf(out, out_sz, "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
}
static int load_inventory(char ids[][128], int max) {
    char path[PATH_BUF];
    inventory_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    while (n < max && fgets(ids[n], 128, f)) {
        ids[n][strcspn(ids[n], "\r\n")] = '\0';
        if (ids[n][0]) n++;
    }
    fclose(f);
    return n;
}
/* Tell chtpm which piece.pdl to load for ${piece_methods}. Without this,
 * active_target_id stays "main" and every screen shows [No Methods] —
 * the live "menus dead-end with no FX" bug. Matches muchi write_chtpm_bridge. */
static void write_chtpm_bridge(const char *piece_id) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "module_path=system/prisc+x pal/main_loop_chtpm.pal\n");
    fprintf(f, "project_id=avatar-creation\n");
    fprintf(f, "active_target_id=%s\n", piece_id && piece_id[0] ? piece_id : "main");
    fclose(f);
}

/* List screen only: each row is "emoji name  uuid" → SELECT then jump to
 * avatar_manage.chtpm (real next screen, not methods jammed into list). */
static void regenerate_avatars_pdl(void) {
    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/projects/avatar-creation/pieces/avatars/piece.pdl", project_root);
    FILE *f = fopen(pdl, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | avatars\n");
    fprintf(f, "META         | version            | 1.0\n\n");
    char ids[MAX_AVATARS][128];
    int n = load_inventory(ids, MAX_AVATARS);
    for (int i = 0; i < n; i++) {
        char sp[PATH_BUF], name[64], emoji[32];
        snprintf(sp, sizeof(sp), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, ids[i]);
        read_kv(sp, "name", name, sizeof(name));
        read_kv(sp, "skin_emoji", emoji, sizeof(emoji));
        if (!name[0]) snprintf(name, sizeof(name), "Clone");
        /* Full uuid in the METHOD label so list is identity-clear. */
        fprintf(f, "METHOD       | %s %s  %s | SELECT_AVATAR:%s\n",
                emoji[0] ? emoji : "?", name, ids[i], ids[i]);
    }
    if (n == 0)
        fprintf(f, "METHOD       | (no clones - buy in Store)           | NOOP\n");
    fclose(f);
}

/* If inventory (xyzfs) has a clone that was never copied into this
 * project's lobby (or only exists under a dead session), materialize it
 * so manage screen / avatar_window can find state.txt. */
static void ensure_local_avatar(const char *uuid) {
    char local[PATH_BUF], remote[PATH_BUF], xyz[512], mode[64];
    snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    struct stat st;
    if (stat(local, &st) == 0) return;

    /* session.pdl xyzfs_path */
    char spath[PATH_BUF];
    snprintf(spath, sizeof(spath), "%s/xyzfs/session.pdl", login_root);
    FILE *sf = fopen(spath, "r");
    xyz[0] = '\0';
    if (sf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), sf)) {
            if (strncmp(line, "STATE", 5) != 0) continue;
            if (!strstr(line, "xyzfs_path")) continue;
            char *p = strrchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ' || *p == '\t') p++;
            p[strcspn(p, "\r\n")] = '\0';
            size_t n = strlen(p);
            while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
            if (p[0] == '#' || !p[0]) continue;
            snprintf(xyz, sizeof(xyz), "%s", p);
            break;
        }
        fclose(sf);
    }
    if (!xyz[0]) {
        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        read_kv(cpath, "current_xyzfs", xyz, sizeof(xyz));
    }
    if (!xyz[0]) return;
    snprintf(remote, sizeof(remote), "%s/%s/home/avatars/%s", house_root, xyz, uuid);
    if (stat(remote, &st) != 0) return;

    char local_dir[PATH_BUF];
    snprintf(local_dir, sizeof(local_dir), "%s/pieces/world_01/map_lobby/%s", project_root, uuid);
    char cmd[PATH_BUF * 3];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && cp -a '%s/.' '%s/' 2>/dev/null",
             local_dir, remote, local_dir);
    system(cmd);
    (void)mode;
}

/* Truncate interact_relay so stale KEY:n from a previous screen cannot
 * replay after href restarts the module (cursor resets to 0). That was
 * why Choose Clone auto-jumped into Manage: leftover KEY:2 selected the
 * first METHOD without a real click. */
static void clear_interact_relay(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/interact_relay.txt", project_root);
    FILE *f = fopen(path, "w");
    if (f) fclose(f);
}

/* chtpm only reacts when layout_changed.txt GROWS - append a line. */
static void request_layout(const char *layout_rel) {
    char lp[PATH_BUF], cl[PATH_BUF];
    snprintf(lp, sizeof(lp), "%s/pieces/display/layout_changed.txt", project_root);
    FILE *lf = fopen(lp, "a");
    if (lf) {
        fprintf(lf, "%s\n", layout_rel);
        fclose(lf);
    }
    /* Eager current_layout so our next get_screen sees the target immediately. */
    snprintf(cl, sizeof(cl), "%s/pieces/display/current_layout.txt", project_root);
    FILE *cf = fopen(cl, "w");
    if (cf) {
        fprintf(cf, "%s\n", layout_rel);
        fclose(cf);
    }
    /* Drop any keys that were meant for the previous layout/module. */
    clear_interact_relay();
}
static int load_menu_items(const char *screen, MenuItem *items, int max) {
    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/projects/avatar-creation/pieces/%s/piece.pdl", project_root, screen);
    FILE *f = fopen(pdl, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        snprintf(items[n].label, sizeof(items[n].label), "%s", trim(p1 + 1));
        snprintf(items[n].command, sizeof(items[n].command), "%s", trim(p2 + 1));
        n++;
    }
    fclose(f);
    return n;
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
static void read_gui(const char *key, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/avatar-creation/manager/gui_state.txt", project_root);
    read_kv(path, key, out, out_sz);
}
static void bump_screen(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/avatar_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}
/* Parser re-parses layout when this grows — needed so ${dna_opts_*}
 * option buttons reappear with updated [current] brackets. */
static void bump_state_reparse(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/apps/player_app/state_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}
static void seed_dna_opts_via_compose(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cd '%s' && ./ops/+x/avatar_compose_frame.+x >/dev/null 2>&1",
             project_root);
    system(cmd);
}
static int inv_count(void) {
    char ids[MAX_AVATARS][128];
    return load_inventory(ids, MAX_AVATARS);
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

static int load_list(const char *rel, char opts[][64], int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/%s", project_root, rel);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        snprintf(opts[n], 64, "%s", line);
        n++;
    }
    fclose(f);
    return n;
}
static int find_idx(char opts[][64], int n, const char *cur) {
    for (int i = 0; i < n; i++) if (strcmp(opts[i], cur) == 0) return i;
    return 0;
}
static int skin_count(const char *gender) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/registry/dna/skin_tones.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 6;
    char line[MAX_LINE];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        char g[16], em[32], lab[32];
        int idx = 0;
        if (sscanf(line, "%15[^|]|%d|%31[^|]|%31s", g, &idx, em, lab) >= 3) {
            if (strcmp(g, gender) == 0) n++;
        }
    }
    fclose(f);
    return n > 0 ? n : 6;
}

/* Seed pick_*_idx from committed clone DNA so scroller brackets match. */
static void sync_picker_from_avatar(const char *state_path, const char *uuid) {
    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    char gender[32], si[16], hair[32], shirt[32], pants[32], h[16], w[16], buf[16];
    char opts[MAX_OPTS][64];
    int n, i;

    read_kv(sp, "gender", gender, sizeof(gender));
    if (!gender[0]) snprintf(gender, sizeof(gender), "male");
    read_kv(sp, "skin_index", si, sizeof(si));
    snprintf(buf, sizeof(buf), "%d", atoi(si));
    write_kv(state_path, "pick_skin_idx", buf);
    write_kv(state_path, "pick_gender_idx", strcmp(gender, "female") == 0 ? "1" : "0");

    read_kv(sp, "hair_color", hair, sizeof(hair));
    n = load_list("pieces/registry/dna/hair_colors.txt", opts, MAX_OPTS);
    i = find_idx(opts, n, hair);
    snprintf(buf, sizeof(buf), "%d", i); write_kv(state_path, "pick_hair_idx", buf);

    read_kv(sp, "shirt_color", shirt, sizeof(shirt));
    n = load_list("pieces/registry/dna/shirt_colors.txt", opts, MAX_OPTS);
    i = find_idx(opts, n, shirt);
    snprintf(buf, sizeof(buf), "%d", i); write_kv(state_path, "pick_shirt_idx", buf);

    read_kv(sp, "pants_color", pants, sizeof(pants));
    n = load_list("pieces/registry/dna/pants_colors.txt", opts, MAX_OPTS);
    i = find_idx(opts, n, pants);
    snprintf(buf, sizeof(buf), "%d", i); write_kv(state_path, "pick_pants_idx", buf);

    read_kv(sp, "height", h, sizeof(h));
    n = load_list("pieces/registry/dna/heights.txt", opts, MAX_OPTS);
    i = find_idx(opts, n, h);
    snprintf(buf, sizeof(buf), "%d", i); write_kv(state_path, "pick_height_idx", buf);

    read_kv(sp, "weight", w, sizeof(w));
    n = load_list("pieces/registry/dna/weights.txt", opts, MAX_OPTS);
    i = find_idx(opts, n, w);
    snprintf(buf, sizeof(buf), "%d", i); write_kv(state_path, "pick_weight_idx", buf);

    write_kv(state_path, "picker_field", "skin");
}

static void gui_state_path(char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s/projects/avatar-creation/manager/gui_state.txt", project_root);
    char dir[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/projects/avatar-creation/manager", project_root);
    mkdir_p(dir, 0755);
}

/* Which DNA fold is open? Require at least one fold_*=open.
 * Returns 0 if none open — a/d must NOT apply DNA (avoids spam when
 * keys leak or INTERACT is stale). Prefer picker_field if that fold is open. */
static int resolve_picker_field(const char *state_path, char *field_out, size_t field_sz) {
    char gpath[PATH_BUF];
    gui_state_path(gpath, sizeof(gpath));
    static const char *ids[] = {
        "skin_pick", "gender_pick", "hair_pick", "shirt_pick",
        "pants_pick", "height_pick", "weight_pick"
    };
    static const char *fields[] = {
        "skin", "gender", "hair", "shirt", "pants", "height", "weight"
    };
    char preferred[32];
    read_kv(state_path, "picker_field", preferred, sizeof(preferred));

    int open_count = 0;
    int open_i = -1;
    int pref_open = -1;
    for (int i = 0; i < 7; i++) {
        char key[64], st[32];
        snprintf(key, sizeof(key), "fold_%s", ids[i]);
        read_kv(gpath, key, st, sizeof(st));
        if (strcmp(st, "open") == 0) {
            open_count++;
            open_i = i;
            if (preferred[0] && strcmp(preferred, fields[i]) == 0)
                pref_open = i;
        }
    }
    if (open_count <= 0) return 0;

    int use = (pref_open >= 0) ? pref_open : open_i;
    snprintf(field_out, field_sz, "%s", fields[use]);
    if (strcmp(preferred, field_out) != 0)
        write_kv(state_path, "picker_field", field_out);
    return 1;
}

static int field_option_count(const char *field, const char *uuid) {
    char opts[MAX_OPTS][64];
    if (strcmp(field, "skin") == 0) {
        char sp[PATH_BUF], gender[32];
        snprintf(sp, sizeof(sp), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
        read_kv(sp, "gender", gender, sizeof(gender));
        if (!gender[0]) snprintf(gender, sizeof(gender), "male");
        return skin_count(gender);
    }
    if (strcmp(field, "gender") == 0) return 2;
    const char *rel = NULL;
    if (strcmp(field, "hair") == 0) rel = "pieces/registry/dna/hair_colors.txt";
    else if (strcmp(field, "shirt") == 0) rel = "pieces/registry/dna/shirt_colors.txt";
    else if (strcmp(field, "pants") == 0) rel = "pieces/registry/dna/pants_colors.txt";
    else if (strcmp(field, "height") == 0) rel = "pieces/registry/dna/heights.txt";
    else if (strcmp(field, "weight") == 0) rel = "pieces/registry/dna/weights.txt";
    if (!rel) return 0;
    return load_list(rel, opts, MAX_OPTS);
}

static const char *pick_key_for_field(const char *field) {
    if (strcmp(field, "skin") == 0) return "pick_skin_idx";
    if (strcmp(field, "gender") == 0) return "pick_gender_idx";
    if (strcmp(field, "hair") == 0) return "pick_hair_idx";
    if (strcmp(field, "shirt") == 0) return "pick_shirt_idx";
    if (strcmp(field, "pants") == 0) return "pick_pants_idx";
    if (strcmp(field, "height") == 0) return "pick_height_idx";
    if (strcmp(field, "weight") == 0) return "pick_weight_idx";
    return "pick_skin_idx";
}

/* INTERACT scroller keys: a/d (or arrows 1000/1001) move strip and APPLY live
 * (glyph-palette style). Enter is NOT used for commit — cli_io rename/age
 * also injects Enter (13) into the same relay; treating Enter as DNA commit
 * would corrupt DNA when applying a name. */
static int handle_dna_picker(int key, const char *state_path, const char *selected,
                             char *message, size_t message_sz) {
    int delta = 0;
    if (key == 'a' || key == 'A' || key == 1000) delta = -1;
    else if (key == 'd' || key == 'D' || key == 1001) delta = 1;
    else return 0;

    if (!selected || !selected[0]) {
        snprintf(message, message_sz, "No avatar selected.");
        return 1;
    }

    char field[32];
    if (!resolve_picker_field(state_path, field, sizeof(field)))
        return 0; /* no DNA fold open — ignore a/d */
    const char *pk = pick_key_for_field(field);
    char idxbuf[16];
    read_kv(state_path, pk, idxbuf, sizeof(idxbuf));
    int idx = atoi(idxbuf);
    int n = field_option_count(field, selected);
    if (n <= 0) n = 1;

    idx = (idx + delta) % n;
    if (idx < 0) idx += n;
    snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
    write_kv(state_path, pk, idxbuf);

    char cbuf[PATH_BUF];
    snprintf(cbuf, sizeof(cbuf), "./ops/+x/cycle_dna.+x '%s' '%s' %d", selected, field, idx);
    run_capture(cbuf, message, message_sz);
    /* Gender change rebinds skin emoji set — re-seed all pick indices. */
    if (strcmp(field, "gender") == 0)
        sync_picker_from_avatar(state_path, selected);
    write_kv(state_path, "picker_field", field);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();
    resolve_login_root();
    resolve_house_root();
    int key = atoi(argv[1]);

    char state_path[PATH_BUF], screen[64], selected[128], message[MAX_LINE];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/avatar_menu_state.txt", project_root);
    get_screen(screen, sizeof(screen));
    read_kv(state_path, "selected_avatar", selected, sizeof(selected));
    read_kv(state_path, "last_message", message, sizeof(message));

    char last_screen[64];
    read_kv(state_path, "last_screen", last_screen, sizeof(last_screen));
    int screen_changed = (strcmp(last_screen, screen) != 0);
    if (screen_changed) {
        write_kv(state_path, "last_screen", screen);
        /* Keep selection only on manage/customize screens. */
        if (strcmp(screen, "avatar_manage") != 0 && strcmp(screen, "customize") != 0) {
            selected[0] = '\0';
            write_kv(state_path, "selected_avatar", "");
        }
        /* Critical: drop stale KEY:n so Choose Clone never auto-SELECTS. */
        clear_interact_relay();
        if (strcmp(screen, "avatars") == 0)
            regenerate_avatars_pdl();
        write_chtpm_bridge(screen);
        bump_screen();
        /* Never treat the same argv key as a method on the NEW screen —
         * it was almost certainly meant for the previous layout, or is a
         * replay after module restart. Idle (0) is fine; anything else
         * waits for a fresh press. */
        if (key != 0)
            return 0;
    }

    /* Idle sync: keep active_target_id = layout piece for ${piece_methods}. */
    if (key == 0) {
        char chtpm_state[PATH_BUF], cur_target[128];
        snprintf(chtpm_state, sizeof(chtpm_state), "%s/pieces/apps/player_app/state.txt", project_root);
        read_kv(chtpm_state, "active_target_id", cur_target, sizeof(cur_target));
        if (strcmp(screen, cur_target) != 0) {
            clear_interact_relay();
            write_chtpm_bridge(screen);
            if (strcmp(screen, "avatars") == 0)
                regenerate_avatars_pdl();
            bump_screen();
        }
        return 0;
    }

    /* DNA option buttons: KEY:300+ (skin), 310+ (gender), 320+ (hair), …
     * Each option is its own numbered nav row under a [+] fold — Enter applies.
     * Do NOT use a/d strip scroller for DNA (pitfall: navigates away / no indexes). */
    if ((strcmp(screen, "avatar_manage") == 0 || strcmp(screen, "customize") == 0) &&
        selected[0] && key >= 300 && key < 420) {
        const char *field = NULL;
        int idx = 0;
        if (key >= 300 && key < 310) { field = "skin";   idx = key - 300; }
        else if (key >= 310 && key < 320) { field = "gender"; idx = key - 310; }
        else if (key >= 320 && key < 340) { field = "hair";   idx = key - 320; }
        else if (key >= 340 && key < 360) { field = "shirt";  idx = key - 340; }
        else if (key >= 360 && key < 380) { field = "pants";  idx = key - 360; }
        else if (key >= 380 && key < 400) { field = "height"; idx = key - 380; }
        else if (key >= 400 && key < 420) { field = "weight"; idx = key - 400; }
        if (field) {
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/cycle_dna.+x '%s' '%s' %d",
                     selected, field, idx);
            run_capture(cbuf, message, sizeof(message));
            if (strcmp(field, "gender") == 0)
                sync_picker_from_avatar(state_path, selected);
            write_kv(state_path, "picker_field", field);
            write_kv(state_path, "last_message", message);
            write_chtpm_bridge(screen);
            seed_dna_opts_via_compose(); /* refresh [brackets] on option rows */
            bump_state_reparse();
            bump_screen();
            return 0;
        }
    }

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(screen, items, MAX_MENU_ITEMS);
    /* method_idx starts at 2 in load_dynamic_methods → KEY:2 = first METHOD.
     * resolved is 1-based METHOD row; items[resolved-1]. Same as muchi/chain.
     * CLI_IO: methods occupy slots but are not KEY-dispatched (user types). */
    int resolved = 0;
    if (key >= '0' && key <= '9') resolved = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved = key - 1;

    if (resolved < 1 || resolved > item_count) {
        write_chtpm_bridge(screen);
        return 0;
    }
    const char *cmd = items[resolved - 1].command;
    if (strncmp(cmd, "CLI_IO:", 7) == 0) {
        /* Focus-only field; ignore accidental KEY to this slot. */
        write_chtpm_bridge(screen);
        return 0;
    }

    if (strcmp(cmd, "CLAIM_TOKENS") == 0) {
        run_capture("./ops/+x/claim_tokens.+x", message, sizeof(message));
    } else if (strcmp(cmd, "BUY_CLONE") == 0) {
        run_capture("./ops/+x/buy_clone.+x", message, sizeof(message));
    } else if (strcmp(cmd, "CLAIM_FREE") == 0) {
        if (inv_count() > 0) {
            snprintf(message, sizeof(message), "Starter already claimed (you own clones).");
        } else {
            char minted[128];
            run_capture("./ops/+x/generate_clone.+x 'Starter'", minted, sizeof(minted));
            if (minted[0])
                snprintf(message, sizeof(message), "Free starter clone minted: %.36s", minted);
            else
                snprintf(message, sizeof(message), "Free starter failed.");
        }
    } else if (strncmp(cmd, "SELECT_AVATAR:", 14) == 0) {
        snprintf(selected, sizeof(selected), "%s", cmd + 14);
        write_kv(state_path, "selected_avatar", selected);
        ensure_local_avatar(selected);
        sync_picker_from_avatar(state_path, selected);
        snprintf(message, sizeof(message), "Managing %s", selected);
        /* Real next screen: manage for this clone only (not jammed list). */
        write_chtpm_bridge("avatar_manage");
        write_kv(state_path, "last_screen", "avatar_manage");
        write_kv(state_path, "last_message", message);
        seed_dna_opts_via_compose(); /* gui_state dna_opts_* before parse */
        request_layout("pieces/chtpm/layouts/avatar_manage.chtpm"); /* also clears relay */
        snprintf(screen, sizeof(screen), "avatar_manage");
        /* Mutaclysm-style RGB orbit preview (separate from 2D desktop pet). */
        {
            char cbuf[PATH_BUF], preview_msg[MAX_LINE];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/open_character_preview.+x '%s'", selected);
            run_capture(cbuf, preview_msg, sizeof(preview_msg));
            if (preview_msg[0])
                snprintf(message, sizeof(message), "%s | %s", message, preview_msg);
        }
    } else if (strcmp(cmd, "DESELECT") == 0) {
        /* Close RGB preview when leaving manage. */
        if (selected[0]) {
            char pp[PATH_BUF];
            snprintf(pp, sizeof(pp), "%s/pieces/world_01/map_lobby/%s/preview.pid",
                     project_root, selected);
            FILE *pf = fopen(pp, "r");
            if (pf) {
                long pid = -1;
                if (fscanf(pf, "%ld", &pid) == 1 && pid > 1) {
                    kill((pid_t)pid, SIGTERM);
                }
                fclose(pf);
                unlink(pp);
            }
        }
        selected[0] = '\0';
        write_kv(state_path, "selected_avatar", "");
        /* Clear DNA fold open-state so manage does not re-open all strips. */
        {
            char gpath[PATH_BUF];
            gui_state_path(gpath, sizeof(gpath));
            FILE *gf = fopen(gpath, "w");
            if (gf) fclose(gf); /* empty — folds default to [+] closed */
        }
        snprintf(message, sizeof(message), "Choose a clone.");
        write_chtpm_bridge("avatars");
        request_layout("pieces/chtpm/layouts/avatars.chtpm");
        regenerate_avatars_pdl();
        snprintf(screen, sizeof(screen), "avatars");
        write_kv(state_path, "last_screen", "avatars");
    } else if (strcmp(cmd, "OPEN_WINDOW") == 0) {
        if (!selected[0]) snprintf(message, sizeof(message), "Select an avatar first.");
        else {
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/open_avatar_window.+x '%s'", selected);
            run_capture(cbuf, message, sizeof(message));
        }
    } else if (strcmp(cmd, "OPEN_PREVIEW") == 0) {
        if (!selected[0]) snprintf(message, sizeof(message), "Select an avatar first.");
        else {
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/open_character_preview.+x '%s'", selected);
            run_capture(cbuf, message, sizeof(message));
        }
    } else if (strcmp(cmd, "TOGGLE_SLEEP") == 0) {
        if (!selected[0]) snprintf(message, sizeof(message), "Select an avatar first.");
        else {
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/toggle_sleep.+x '%s'", selected);
            run_capture(cbuf, message, sizeof(message));
        }
    } else if (strcmp(cmd, "APPLY_NAME_AGE") == 0) {
        if (!selected[0]) snprintf(message, sizeof(message), "No avatar selected.");
        else {
            char name[128], age[32];
            read_gui("avatar_name_input", name, sizeof(name));
            read_gui("avatar_age_input", age, sizeof(age));
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/apply_name_age.+x '%s' '%s' '%s'",
                     selected, name[0] ? name : "Clone", age[0] ? age : "18");
            run_capture(cbuf, message, sizeof(message));
        }
    } else if (strncmp(cmd, "CYCLE_", 6) == 0) {
        if (!selected[0]) snprintf(message, sizeof(message), "No avatar selected.");
        else {
            const char *field = "skin";
            if (strcmp(cmd, "CYCLE_GENDER") == 0) field = "gender";
            else if (strcmp(cmd, "CYCLE_SKIN") == 0) field = "skin";
            else if (strcmp(cmd, "CYCLE_HAIR") == 0) field = "hair";
            else if (strcmp(cmd, "CYCLE_SHIRT") == 0) field = "shirt";
            else if (strcmp(cmd, "CYCLE_PANTS") == 0) field = "pants";
            else if (strcmp(cmd, "CYCLE_HEIGHT") == 0) field = "height";
            else if (strcmp(cmd, "CYCLE_WEIGHT") == 0) field = "weight";
            char cbuf[PATH_BUF];
            snprintf(cbuf, sizeof(cbuf), "./ops/+x/cycle_dna.+x '%s' '%s'", selected, field);
            run_capture(cbuf, message, sizeof(message));
        }
    } else if (strcmp(cmd, "NOOP") == 0) {
        snprintf(message, sizeof(message), "Visit Store to mint a clone.");
    } else {
        snprintf(message, sizeof(message), "Unknown command.");
    }

    write_kv(state_path, "last_message", message);
    write_chtpm_bridge(screen);
    bump_screen();
    return 0;
}
