/* tsc_compose - render the current True Swords Clash duel screen into
 * pieces/apps/player_app/view.txt (the file chtpm's own ${game_map}
 * placeholder substitutes verbatim). Modeled directly on 041.pal-chain's
 * chain_compose_frame.c / mychara_compose_frame.c (the "ONE WRITER
 * RULE" shape): writes ONLY view.txt, never
 * pieces/display/current_frame.txt directly (that's
 * chtpm_parser_pal.c's own exclusive job) - then bumps
 * pieces/display/frame_changed.txt so the parser's dirty-check picks up
 * the change on its next loop iteration.
 *
 * Reads config.txt (the authoritative duel state - HP/mana/status/
 * ratings/mode) and shows: both fighters' HP + mana bars, their status
 * effects, mode, ratings, the last line of the game master ledger as a
 * battle log line, and a "waiting for setup" screen until a match is
 * started (game_state=waiting_setup).
 *
 * Self-contained, no shared headers.
 * Usage: tsc_compose.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define BOX_W 64

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char l[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(l, sizeof(l), f)) {
        if (strncmp(l, key, key_len) == 0 && l[key_len] == '=') {
            char *v = l + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static FILE *g_view_out = NULL;
static void border(void) {
    if (g_view_out) {
        fputc('+', g_view_out);
        for (int i = 0; i < BOX_W; i++) fputc('=', g_view_out);
        fputc('+', g_view_out);
        fputc('\n', g_view_out);
    }
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

static void draw_bar(char *out, size_t out_sz, int cur, int max, int width) {
    int filled = 0;
    if (max > 0) filled = (cur * width) / max;
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    int i = 0;
    i += snprintf(out + i, out_sz - (size_t)i, "[");
    for (int c = 0; c < width; c++) out[i++] = (c < filled) ? '#' : '.';
    snprintf(out + i, out_sz - (size_t)i, "]");
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char config_path[PATH_BUF], view_path[PATH_BUF], ledger_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(ledger_path, sizeof(ledger_path), "%s/data/master_ledger.txt", project_root);

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    char mode[32] = "HvH";
    char game_state[32] = "waiting_setup";
    read_kv_str(config_path, "mode", mode, sizeof(mode));
    read_kv_str(config_path, "game_state", game_state, sizeof(game_state));

    char p1_name[64] = "Player1", p2_name[64] = "SKYNET";
    char p1_status[32] = "none", p2_status[32] = "none";
    char p1_type[16] = "human", p2_type[16] = "computer";
    read_kv_str(config_path, "player_1_name", p1_name, sizeof(p1_name));
    read_kv_str(config_path, "player_2_name", p2_name, sizeof(p2_name));
    read_kv_str(config_path, "player_1_status", p1_status, sizeof(p1_status));
    read_kv_str(config_path, "player_2_status", p2_status, sizeof(p2_status));
    read_kv_str(config_path, "player_1_type", p1_type, sizeof(p1_type));
    read_kv_str(config_path, "player_2_type", p2_type, sizeof(p2_type));

    int p1_hp = read_kv_int(config_path, "player_1_hp", 100);
    int p2_hp = read_kv_int(config_path, "player_2_hp", 100);
    int p1_mana = read_kv_int(config_path, "player_1_mana", 0);
    int p2_mana = read_kv_int(config_path, "player_2_mana", 0);
    int p1_rating = read_kv_int(config_path, "player_1_rating", 1000);
    int p2_rating = read_kv_int(config_path, "player_2_rating", 1000);

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "   T R U E   S W O R D S   C L A S H");
    line(rowbuf);
    border();
    blank();

    char modestr[32];
    if (strcmp(mode, "HvH") == 0) snprintf(modestr, sizeof(modestr), "HUMAN vs HUMAN");
    else if (strcmp(mode, "HvC") == 0) snprintf(modestr, sizeof(modestr), "HUMAN vs COMPUTER");
    else if (strcmp(mode, "CvC") == 0) snprintf(modestr, sizeof(modestr), "COMPUTER vs COMPUTER");
    else snprintf(modestr, sizeof(modestr), "%s", mode);

    if (strcmp(game_state, "waiting_setup") == 0) {
        snprintf(rowbuf, sizeof(rowbuf), "  Mode: %s  (no match started yet)", modestr);
        line(rowbuf);
        blank();
        line("  Waiting for the SETUP WIDGIT...");
        line("  Use the MATCH SETUP window to pick HvH / HvC / CvC");
        line("  and an opponent rating, then press START MATCH.");
    } else {
        snprintf(rowbuf, sizeof(rowbuf), "  Mode: %s   State: %s", modestr, game_state);
        line(rowbuf);
        blank();

        char bar[80];
        snprintf(rowbuf, sizeof(rowbuf), "  %s  (%s)  rating %d", p1_name, p1_type, p1_rating);
        line(rowbuf);
        draw_bar(bar, sizeof(bar), p1_hp, 100, 30);
        snprintf(rowbuf, sizeof(rowbuf), "    HP   %s  %3d / 100", bar, p1_hp);
        line(rowbuf);
        draw_bar(bar, sizeof(bar), p1_mana, 20, 30);
        snprintf(rowbuf, sizeof(rowbuf), "    Mana %s  %3d / 20", bar, p1_mana);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "    Status: %s", p1_status);
        line(rowbuf);
        blank();

        snprintf(rowbuf, sizeof(rowbuf), "  %s  (%s)  rating %d", p2_name, p2_type, p2_rating);
        line(rowbuf);
        draw_bar(bar, sizeof(bar), p2_hp, 100, 30);
        snprintf(rowbuf, sizeof(rowbuf), "    HP   %s  %3d / 100", bar, p2_hp);
        line(rowbuf);
        draw_bar(bar, sizeof(bar), p2_mana, 20, 30);
        snprintf(rowbuf, sizeof(rowbuf), "    Mana %s  %3d / 20", bar, p2_mana);
        line(rowbuf);
        snprintf(rowbuf, sizeof(rowbuf), "    Status: %s", p2_status);
        line(rowbuf);
    }

    blank();
    FILE *lf = fopen(ledger_path, "r");
    if (lf) {
        char last_line[MAX_LINE * 2] = "";
        char l[MAX_LINE * 2];
        while (fgets(l, sizeof(l), lf)) {
            l[strcspn(l, "\r\n")] = '\0';
            snprintf(last_line, sizeof(last_line), "%s", l);
        }
        fclose(lf);
        if (last_line[0]) {
            snprintf(rowbuf, sizeof(rowbuf), "  log: %s", last_line);
            line(rowbuf);
        }
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);
    return 0;
}
