/* mybiotech_menu_input - piece.pdl METHOD-table-driven ACTION dispatch
 * for whichever my-biotech screen is currently showing. Modeled
 * directly on my-chara-txt's own ops/mychara_menu_input.c (itself
 * modeled on 041.pal-chain's real, proven chain_menu_input.c).
 *
 * P2 scope (MY_BIOTECH_DESIGN.md §7): ONE real command, RESEARCH -
 * a genuinely live Gemma-LAN call (not simulated), proving the full
 * loop: weighted-random element pick -> simple plain-text prompt
 * (NOT structured JSON - gemma3:270m can't reliably do that, see
 * design doc §2/§4) -> real connect_op.+x/json_parser.+x round trip
 * -> corpus append -> ledger append -> shown to player.
 *
 * Usage: mybiotech_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[256];
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

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str_local(path, key, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
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

static void write_kv_int(const char *path, const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    write_kv(path, key, buf);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(const char *root, const char *piece_id, MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-biotech/pieces/%s/piece.pdl", root, piece_id);
#pragma GCC diagnostic pop
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

static void write_chtpm_bridge(const char *piece_id) {
    char chtpm_state_path[PATH_BUF];
    snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
    FILE *cf = fopen(chtpm_state_path, "w");
    if (cf) {
        fprintf(cf, "project_id=my-biotech\n");
        fprintf(cf, "active_target_id=%s\n", piece_id);
        fclose(cf);
    }
}

static void get_current_piece_id(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
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

static void ledger_append(const char *root, int day, const char *action_type, const char *details) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/master_ledger.txt", root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    time_t now = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(f, "%s|%d|%s|%s\n", ts, day, action_type, details);
    fclose(f);
}

/* REAL FIX (was: a synchronous gemma_ask() call directly in this op,
 * blocking the WHOLE PAL module for the full call duration - flagged
 * as a known limitation in pal/main_module.pal's own header comment
 * and MY_BIOTECH_DESIGN.md §7's P2 entry, now fixed). The actual Gemma
 * call now lives in ops/mybiotech_research_worker.c, launched here as
 * a detached background process - PID-tracked via research.pid, guarded
 * against double-launch, exactly the same pattern 041.pal-chain's own
 * chain_menu_input.c uses for CHAIN_START_MINING (see that file's
 * header comment for the precedent). This op returns immediately;
 * mybiotech_compose_frame.c polls data/research_status.txt to show
 * live "researching..." progress. research.pid/research_status.txt
 * deliberately live under data/ (symlinked to the REAL, persistent
 * project root in every session - see button.sh) rather than
 * pieces/system/ (a fresh, ephemeral per-session directory) - a real
 * bug found+fixed 2026-08-02: if they lived in pieces/system/, quitting
 * the game while a research worker was still in flight would delete
 * its own status/pid files out from under it when the session dir got
 * rm -rf'd, orphaning the worker and breaking the double-launch guard
 * for the next session. */
#include <signal.h>

static int worker_already_running(const char *root) {
    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/data/research.pid", root);
    FILE *pf = fopen(pid_path, "r");
    if (!pf) return 0;
    int pid = 0;
    int ok = (fscanf(pf, "%d", &pid) == 1);
    fclose(pf);
    return ok && pid > 0 && kill(pid, 0) == 0;
}

/* Fixed small element list for P2 - Store/elements.txt weighted
 * selection (design doc §3) is P4 scope, not built yet. Real, if
 * simple, randomness: seeded from time+pid so repeated runs in the
 * same second still differ. */
static const char *ELEMENTS[] = { "sulfur", "nitrogen", "carbon", "phosphorus", "chlorine" };
#define NUM_ELEMENTS 5

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();
    srand((unsigned int)(time(NULL) ^ getpid()));

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-biotech/pieces/mybiotech_menu/state.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    int key = atoi(argv[1]);

    if (key == 0) {
        char derived[128];
        get_current_piece_id(project_root, derived, sizeof(derived));
        char chtpm_state_path[PATH_BUF];
        snprintf(chtpm_state_path, sizeof(chtpm_state_path), "%s/pieces/apps/player_app/state.txt", project_root);
        char current_target[128];
        read_kv_str_local(chtpm_state_path, "active_target_id", current_target, sizeof(current_target));
        if (strcmp(derived, current_target) == 0) return 0;

        write_chtpm_bridge(derived);

        char marker_path[PATH_BUF];
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mybiotech_screen_changed.txt", project_root);
        FILE *mf = fopen(marker_path, "a");
        if (mf) { fputc('.', mf); fclose(mf); }
        return 0;
    }

    char active_piece[128];
    get_current_piece_id(project_root, active_piece, sizeof(active_piece));

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(project_root, active_piece, items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "RESEARCH") == 0) {
            if (worker_already_running(project_root)) {
                snprintf(message, sizeof(message), "Already researching - check back in a moment.");
            } else {
                const char *element = ELEMENTS[rand() % NUM_ELEMENTS];
                char launch_cmd[PATH_BUF * 2];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(launch_cmd, sizeof(launch_cmd),
                         "cd '%s' && ./ops/+x/mybiotech_research_worker.+x '%s' '%s' >/tmp/mybiotech_worker.log 2>&1 &",
                         project_root, project_root, element);
#pragma GCC diagnostic pop
                int rc = system(launch_cmd);
                (void)rc;
                snprintf(message, sizeof(message), "Researching %s... (gemma-lan call in progress, check back shortly)", element);
            }
        } else if (strcmp(cmd, "END_TURN") == 0) {
            int day = read_kv_int(config_path, "day", 1);
            int max_days = read_kv_int(config_path, "max_days", 10);
            day += 1;
            write_kv_int(config_path, "day", day);

            ledger_append(project_root, day - 1, "day_end", "");

            if (day > max_days) {
                write_kv(config_path, "game_state", "game_over");
                snprintf(message, sizeof(message), "Day %d - GAME OVER (reached max_days).", day - 1);
            } else {
                snprintf(message, sizeof(message), "Day %d began.", day);
            }
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mybiotech_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
