/* pc_hq_status_manager.c — piececraft-hq live status info window manager (2026-08-30, Phase 1 v1)
 *
 * Real business logic: reads piececraft-hq's live game state and publishes
 * it in a simple key=value format for the khtpm window to display.
 *
 * State sources:
 * - pieces/system/config.txt: game_id, turn (Tick), game_state
 * - pieces/hero_01/state.txt: hp, pos_x/pos_y/pos_z, chunk_x/chunk_y, interact_mode
 * - pieces/xelector_01/state.txt: possessed_id (for interact-relevant info)
 *
 * Publishes to: piececraft-hq_state.txt (simple text, not palette grid format)
 *
 * Window positioning: managed by launcher (will query game window geometry
 * via XGetGeometry and position info window below it)
 *
 * Pattern: ported from palettes_manager.c (mtime-gated polling of real state
 * files), but output format is key=value text lines, not the emoji\tlabel\tdir
 * grid format that palette tiles use.
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PATH_BUF 4096

static char g_package_dir[PATH_BUF];
static char g_state_path[PATH_BUF];
static time_t g_source_mtime = 0;

/* Real KV reader - strips trailing \r\n to avoid strcmp() drift
 * (same bug fix pattern from §A.6-3 in !.HOUSE_STDS.md). */
static int read_kv_str(const char *file_path, const char *key, char *out, size_t outsz) {
    FILE *f = fopen(file_path, "r");
    if (!f) return 0;
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        size_t klen = (size_t)(eq - line);
        if (strncmp(line, key, klen) != 0 || strlen(key) != klen) continue;
        char *val = eq + 1;
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r')) val[--vlen] = '\0';
        snprintf(out, outsz, "%s", val);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void publish_status(const char *session_dir) {
    char tmp_path[PATH_BUF];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_state_path);
    FILE *out = fopen(tmp_path, "w");
    if (!out) return;

    /* === Read game state === */
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", session_dir);

    char turn_str[32] = "0";
    char game_state_str[64] = "unknown";
    read_kv_str(config_path, "turn", turn_str, sizeof(turn_str));
    read_kv_str(config_path, "game_state", game_state_str, sizeof(game_state_str));

    /* === Read hero state === */
    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/hero_01/state.txt", session_dir);

    char hp_str[32] = "0";
    char pos_x_str[32] = "0", pos_y_str[32] = "0", pos_z_str[32] = "0";
    char chunk_x_str[32] = "0", chunk_y_str[32] = "0";
    char interact_mode_str[32] = "0";

    read_kv_str(hero_path, "hp", hp_str, sizeof(hp_str));
    read_kv_str(hero_path, "pos_x", pos_x_str, sizeof(pos_x_str));
    read_kv_str(hero_path, "pos_y", pos_y_str, sizeof(pos_y_str));
    read_kv_str(hero_path, "pos_z", pos_z_str, sizeof(pos_z_str));
    read_kv_str(hero_path, "chunk_x", chunk_x_str, sizeof(chunk_x_str));
    read_kv_str(hero_path, "chunk_y", chunk_y_str, sizeof(chunk_y_str));
    read_kv_str(hero_path, "interact_mode", interact_mode_str, sizeof(interact_mode_str));

    /* === Read xelector state (interact info) === */
    char xelector_path[PATH_BUF];
    snprintf(xelector_path, sizeof(xelector_path), "%s/pieces/xelector_01/state.txt", session_dir);
    char possessed_id_str[64] = "none";
    read_kv_str(xelector_path, "possessed_id", possessed_id_str, sizeof(possessed_id_str));

    /* === Publish as simple key=value text format === */
    fprintf(out, "# piececraft-hq live status (2026-08-30)\n");
    fprintf(out, "tick=%s\n", turn_str);
    fprintf(out, "game_state=%s\n", game_state_str);
    fprintf(out, "hero_hp=%s\n", hp_str);
    fprintf(out, "hero_pos_x=%s\n", pos_x_str);
    fprintf(out, "hero_pos_y=%s\n", pos_y_str);
    fprintf(out, "hero_pos_z=%s\n", pos_z_str);
    fprintf(out, "chunk_x=%s\n", chunk_x_str);
    fprintf(out, "chunk_y=%s\n", chunk_y_str);
    fprintf(out, "interact_mode=%s\n", interact_mode_str);
    fprintf(out, "possessed_id=%s\n", possessed_id_str);

    fclose(out);
    rename(tmp_path, g_state_path);
}

static time_t get_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "pc_hq_status_manager: usage: <package_dir> <session_dir>\n");
        return 1;
    }
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", argv[1]);
    const char *session_dir = argv[2];
    snprintf(g_state_path, sizeof(g_state_path), "%s/piececraft-hq_state.txt", g_package_dir);

    /* Initial publish */
    publish_status(session_dir);

    /* Real mtime-gated polling loop - only republish if state files changed
     * (same discipline as palettes_manager.c) */
    char config_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", session_dir);
    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/hero_01/state.txt", session_dir);

    for (;;) {
        time_t config_mtime = get_mtime(config_path);
        time_t hero_mtime = get_mtime(hero_path);
        time_t newest = config_mtime > hero_mtime ? config_mtime : hero_mtime;

        if (newest > 0 && newest != g_source_mtime) {
            g_source_mtime = newest;
            publish_status(session_dir);
        }

        usleep(500000); /* 500ms poll interval */
    }

    return 0;
}
