/* muchi_compose_frame - renders whichever muchi-pals screen is current
 * into pieces/apps/player_app/view.txt (the file chtpm's own
 * ${panel_content} placeholder substitutes verbatim - renamed from the
 * real upstream's own "${game_map}" name, which misleadingly implied an
 * interactive map/movement concept this pure-menu project has none of),
 * modeled directly on
 * pal-chain's own ops/chain_compose_frame.c. ${piece_methods} (a
 * separate, chtpm-native placeholder) renders the current screen's own
 * numbered action buttons - this op never draws that menu itself, only
 * surrounding informational text (tokens, a selected pet's live stats,
 * the process list).
 *
 * ONE VISIBLE FRAME WRITER RULE (xyzos-standards.txt sec.20): writes ONLY
 * view.txt, NEVER pieces/display/current_frame.txt directly -
 * chtpm_parser_pal.c's own compose_frame() is the sole writer of that
 * file. Pings pieces/display/frame_changed.txt afterward - the real
 * marker chtpm_parser_pal.c's own main loop already watches
 * unconditionally, confirmed via chain_compose_frame.c's own header
 * comment on this exact point.
 *
 * Self-contained, no shared headers.
 * Usage: muchi_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 60

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static void get_current_piece_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", project_root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        const char *slash = strrchr(line, '/');
        const char *base = slash ? slash + 1 : line;
        char tmp[MAX_LINE];
        snprintf(tmp, sizeof(tmp), "%s", base);
        char *dot = strstr(tmp, ".chtpm");
        if (dot) *dot = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (tmp[0]) snprintf(out, out_sz, "%s", tmp);
#pragma GCC diagnostic pop
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

static void ping_chtpm_render_marker(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char user_state[PATH_BUF];
    snprintf(user_state, sizeof(user_state), "%s/pieces/world_01/map_lobby/user_01/state.txt", project_root);
    char msg_path[PATH_BUF];
    snprintf(msg_path, sizeof(msg_path), "%s/pieces/system/last_message.txt", project_root);
    char view_path[PATH_BUF];
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(msg_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char active_piece[128];
    get_current_piece_id(active_piece, sizeof(active_piece));

    char user_name[64];
    read_kv_str(user_state, "name", user_name, sizeof(user_name));
    int tokens = read_kv_int(user_state, "tokens", 0);

    char rowbuf[BOX_W + 1];

    border();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  M U C H I - P A L S   [%s]", active_piece);
#pragma GCC diagnostic pop
    line(rowbuf);
    border();
    blank();

    if (strcmp(active_piece, "user") == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(rowbuf, sizeof(rowbuf), "  Name: %s", user_name[0] ? user_name : "user_01");
#pragma GCC diagnostic pop
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "  Tokens: %d", tokens);
        line(rowbuf);

    } else if (strcmp(active_piece, "faucet") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Tokens: %d", tokens);
        line(rowbuf);

    } else if (strcmp(active_piece, "store") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Tokens: %d", tokens);
        line(rowbuf);

    } else if (strcmp(active_piece, "pets") == 0) {
        char pets_state[PATH_BUF];
        snprintf(pets_state, sizeof(pets_state), "%s/pieces/system/pets_screen_state.txt", project_root);
        char selected[64];
        read_kv_str(pets_state, "selected_pet", selected, sizeof(selected));

        if (selected[0]) {
            char pet_state[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(pet_state, sizeof(pet_state), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, selected);
#pragma GCC diagnostic pop
            char species[64], emoji[32], skills[128], card_status[16], facing[8];
            read_kv_str(pet_state, "species_name", species, sizeof(species));
            read_kv_str(pet_state, "species_emoji", emoji, sizeof(emoji));
            read_kv_str(pet_state, "skills", skills, sizeof(skills));
            read_kv_str(pet_state, "card_status", card_status, sizeof(card_status));
            read_kv_str(pet_state, "facing", facing, sizeof(facing));
            int hp = read_kv_int(pet_state, "hp", 0);
            int hp_max = read_kv_int(pet_state, "hp_max", 0);
            int mp = read_kv_int(pet_state, "mp", 0);
            int mp_max = read_kv_int(pet_state, "mp_max", 0);
            int level = read_kv_int(pet_state, "level", 1);
            int xp = read_kv_int(pet_state, "xp", 0);
            int hunger = read_kv_int(pet_state, "hunger", 0);
            int energy = read_kv_int(pet_state, "energy", 100);
            int poop_count = read_kv_int(pet_state, "poop_count", 0);
            int asleep = read_kv_int(pet_state, "asleep", 0);
            int grid_x = read_kv_int(pet_state, "grid_x", 0);
            int grid_y = read_kv_int(pet_state, "grid_y", 0);
            int pet_z = read_kv_int(pet_state, "z", 0);

            char title_buf[128];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(title_buf, sizeof(title_buf), "  %s %s", emoji[0] ? emoji : "?", selected);
#pragma GCC diagnostic pop
            line(title_buf);
            blank();
            char l[512]; /* generous headroom - line() itself re-truncates to BOX_W for display, same convention ops/compose_menu.c already established */
            snprintf(l, sizeof(l), "  %s  Level %d (XP %d/%d)", species[0] ? species : "?", level, xp, level * 20);
            line(l);
            snprintf(l, sizeof(l), "  HP:%d/%d MP:%d/%d", hp, hp_max, mp, mp_max);
            line(l);
            snprintf(l, sizeof(l), "  Hunger:%d Energy:%d Poop:%d %s", hunger, energy, poop_count, asleep ? "(asleep)" : "");
            line(l);
            snprintf(l, sizeof(l), "  Skills: %s", skills[0] ? skills : "none");
            line(l);
            snprintf(l, sizeof(l), "  Card: %s", card_status[0] ? card_status : "none");
            line(l);
            snprintf(l, sizeof(l), "  Position: (%d,%d,%d) Facing: %s", grid_x, grid_y, pet_z, facing[0] ? facing : "right");
            line(l);
        } else {
            line("  Select a pet below to view/manage it.");
        }

    } else if (strcmp(active_piece, "processes") == 0) {
        char exe_path[PATH_BUF], cmd[PATH_BUF + 64];
        snprintf(exe_path, sizeof(exe_path), "%s/ops/+x/list_processes.+x", project_root);
        snprintf(cmd, sizeof(cmd), "'%s' ignored >/dev/null 2>&1", exe_path);
        int rc = system(cmd);
        (void)rc;

        char list_path[PATH_BUF];
        snprintf(list_path, sizeof(list_path), "%s/pieces/system/process_list.txt", project_root);
        FILE *lf = fopen(list_path, "r");
        int rows = 0;
        if (lf) {
            char row[MAX_LINE];
            while (fgets(row, sizeof(row), lf)) {
                row[strcspn(row, "\r\n")] = '\0';
                if (!row[0]) continue;
                char *p1 = strchr(row, '|');
                char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
                if (!p1 || !p2) continue;
                *p1 = '\0'; *p2 = '\0';
                const char *pet_id = row;
                const char *pid = p1 + 1;
                int alive = atoi(p2 + 1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(rowbuf, sizeof(rowbuf), "  %-10s pid:%-8s %s", pet_id, pid, alive ? "alive" : "dead");
#pragma GCC diagnostic pop
                line(rowbuf);
                rows++;
            }
            fclose(lf);
        }
        if (rows == 0) line("  No tracked pet windows.");
    }

    blank();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rowbuf, sizeof(rowbuf), "  > %s", last_message);
#pragma GCC diagnostic pop
    line(rowbuf);
    border();

    fclose(g_view_out); g_view_out = NULL;
    ping_chtpm_render_marker();
    return 0;
}
