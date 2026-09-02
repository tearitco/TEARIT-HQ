/* tsots_input - dispatch TSOTS game keys (the only module every TSOTS
 * layout declares). Same shape as agy-txt's agy_edit_key: reads the
 * current layout from pieces/display/current_layout.txt so ONE op can
 * behave differently per screen.
 *
 * Usage: tsots_input.+x <keycode>
 *   key 0     = idle (auto-deal when no round is active)
 *   '1'-'9'   = pick a verse into the answer (INTERACT only)
 *   127/8     = backspace last pick
 *   10/13     = submit (playing) or next round (feedback)
 *
 * State:   pieces/system/game_state.txt
 * Round:   pieces/system/round.txt  +  solution.txt
 * Marker:  bumps pieces/display/game_screen_changed.txt on any change
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_VERSES 6
#define MAX_ANSWER 16

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
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[48][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 48 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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

static void bump_screen(void) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path),
             "%s/pieces/display/game_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void read_current_layout(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/display/current_layout.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int round_line_count(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/round.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char buf[MAX_LINE];
    while (fgets(buf, sizeof(buf), f)) n++;
    fclose(f);
    return n;
}

static void read_solution(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/solution.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t n = fread(out, 1, out_sz - 1, f);
    out[n] = '\0';
    out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static void run_deal(void) {
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && ./ops/+x/tsots_deal.+x >/dev/null 2>&1",
             project_root);
    int rc = system(cmd);
    (void)rc;
}

/* Evaluate the current answer against solution.txt. Returns 1 on
 * correct, 0 on wrong, -1 if answer incomplete/invalid. */
static int evaluate(void) {
    char solution[MAX_ANSWER];
    read_solution(solution, sizeof(solution));
    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/game_state.txt", project_root);
    char answer[MAX_ANSWER];
    read_kv_str(state_path, "answer", answer, sizeof(answer));

    int n = round_line_count();
    if (n < 1) return -1;
    if (strlen(answer) != (size_t)n) return -1;
    if (strcmp(answer, solution) != 0) return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char layout[256];
    read_current_layout(layout, sizeof(layout));

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/game_state.txt", project_root);

    char status[64] = "none";
    read_kv_str(state_path, "status", status, sizeof(status));

    /* Not the game screen: nothing to do. Menu keys never reach the
     * module anyway (no <interact> target on menu.chtpm). */
    if (strstr(layout, "game.chtpm") == NULL) return 0;

    int key = atoi(argv[1]);

    if (key == 0) {
        /* idle: auto-deal the first round on entering the game screen */
        if (strcmp(status, "playing") != 0 && strcmp(status, "feedback") != 0) {
            if (round_line_count() < 1) run_deal();
        }
        return 0;
    }

    if (key == 10 || key == 13) {
        if (strcmp(status, "playing") == 0) {
            int res = evaluate();
            char elo_raw[32] = "1000";
            char wins_raw[32] = "0";
            char losses_raw[32] = "0";
            read_kv_str(state_path, "elo", elo_raw, sizeof(elo_raw));
            read_kv_str(state_path, "wins", wins_raw, sizeof(wins_raw));
            read_kv_str(state_path, "losses", losses_raw, sizeof(losses_raw));
            int elo = atoi(elo_raw);
            if (elo < 100) elo = 100;
            int wins = atoi(wins_raw);
            int losses = atoi(losses_raw);

            if (res == 1) {
                elo += 25;
                wins++;
            } else if (res == 0) {
                elo -= 15;
                losses++;
                if (elo < 100) elo = 100;
            }

            char v[32];
            snprintf(v, sizeof(v), "%d", elo);
            write_kv(state_path, "elo", v);
            snprintf(v, sizeof(v), "%d", wins);
            write_kv(state_path, "wins", v);
            snprintf(v, sizeof(v), "%d", losses);
            write_kv(state_path, "losses", v);

            if (res == 1) {
                write_kv(state_path, "last_result", "correct");
                write_kv(state_path, "last_delta", "+25");
            } else if (res == 0) {
                write_kv(state_path, "last_result", "wrong");
                write_kv(state_path, "last_delta", "-15");
            } else {
                write_kv(state_path, "last_result", "incomplete");
                write_kv(state_path, "last_delta", "0");
            }
            write_kv(state_path, "status", "feedback");
            bump_screen();
        } else if (strcmp(status, "feedback") == 0) {
            /* Enter: deal the next round */
            run_deal();
        }
        return 0;
    }

    if (strcmp(status, "playing") == 0) {
        int n = round_line_count();
        if (key >= '1' && key <= '9') {
            int d = key - '0';
            if (d <= n) {
                char answer[MAX_ANSWER];
                read_kv_str(state_path, "answer", answer, sizeof(answer));
                if (strlen(answer) < (size_t)n && !strchr(answer, key)) {
                    char next[MAX_ANSWER];
                    size_t alen = strlen(answer);
                    if (alen + 1 < sizeof(next)) {
                        memcpy(next, answer, alen);
                        next[alen] = (char)key;
                        next[alen + 1] = '\0';
                        write_kv(state_path, "answer", next);
                        bump_screen();
                    }
                }
            }
        } else if (key == 127 || key == 8) {
            char answer[MAX_ANSWER];
            read_kv_str(state_path, "answer", answer, sizeof(answer));
            size_t len = strlen(answer);
            if (len > 0) {
                answer[len - 1] = '\0';
                write_kv(state_path, "answer", answer);
                bump_screen();
            }
        }
    }
    return 0;
}
