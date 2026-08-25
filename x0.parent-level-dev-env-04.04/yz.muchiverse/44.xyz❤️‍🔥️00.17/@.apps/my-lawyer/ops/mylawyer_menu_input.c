/* mylawyer_menu_input - piece.pdl METHOD-table-driven ACTION dispatch
 * for whichever my-lawyer screen is currently showing (main/docket/
 * case). Modeled directly on my-biotech's own ops/mybiotech_menu_input.c
 * (itself modeled on my-chara-txt's, itself modeled on 041.pal-chain's
 * real, proven chain_menu_input.c) - same key==0 screen-sync convention,
 * same load_menu_items()-from-piece.pdl dispatch, same async-worker
 * launch pattern for anything that calls gemma-lan (MY_LAWYER_DESIGN.md
 * §3: async from day one, not retrofitted like my-biotech had to be).
 *
 * P1/P2 scope (MY_LAWYER_DESIGN.md groundwork, single active case at a
 * time - §7 open question 4/5 multi-case and abandon/lose penalties are
 * NOT built): Docket -> pick up ONE case -> Build Case (async worker,
 * player's own side) + NPC opposing side auto-built in parallel -> once
 * both sides ready, Present to Judge (async worker, describe+classify
 * pattern per §4/PITFALL 69) OR Settle at any time before judging for an
 * instant, lower-variance formula resolution (§6). Office/bias (§5) is
 * NOT built yet - explicitly scoped out of this first pass, same as this
 * design doc's own §7 flags it as "not decided in detail."
 *
 * Usage: mylawyer_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

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
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-lawyer/pieces/%s/piece.pdl", root, piece_id);
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
        fprintf(cf, "project_id=my-lawyer\n");
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

static void mkdir_p(const char *path) {
    char cmd[PATH_BUF + 32];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    int rc = system(cmd);
    (void)rc;
}

static int worker_already_running(const char *pid_path) {
    FILE *pf = fopen(pid_path, "r");
    if (!pf) return 0;
    int pid = 0;
    int ok = (fscanf(pf, "%d", &pid) == 1);
    fclose(pf);
    return ok && pid > 0 && kill(pid, 0) == 0;
}

static const char *opposite_side(const char *side) {
    return strcmp(side, "plaintiff") == 0 ? "defendant" : "plaintiff";
}

/* Read one docket.txt line by case_id: case_id|title|claim_summary|player_side */
static int read_docket_entry(const char *root, int case_id, char *title, size_t title_sz,
                              char *claim, size_t claim_sz, char *player_side, size_t side_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/data/docket.txt", root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        int this_id = atoi(line);
        if (this_id != case_id) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        char *p3 = strchr(p2 + 1, '|');
        if (!p3) continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(title, title_sz, "%.*s", (int)(p2 - (p1 + 1)), p1 + 1);
        snprintf(claim, claim_sz, "%.*s", (int)(p3 - (p2 + 1)), p2 + 1);
        snprintf(player_side, side_sz, "%s", p3 + 1);
#pragma GCC diagnostic pop
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static void launch_case_worker(const char *root, int case_id, const char *side) {
    char launch_cmd[PATH_BUF * 2];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(launch_cmd, sizeof(launch_cmd),
             "cd '%s' && ./ops/+x/mylawyer_case_worker.+x '%s' %d '%s' >/tmp/mylawyer_case_worker_%d_%s.log 2>&1 &",
             root, root, case_id, side, case_id, side);
#pragma GCC diagnostic pop
    int rc = system(launch_cmd);
    (void)rc;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();
    srand((unsigned int)(time(NULL) ^ getpid()));

    char state_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-lawyer/pieces/mylawyer_menu/state.txt", project_root);
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
        snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mylawyer_screen_changed.txt", project_root);
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

        if (strcmp(cmd, "END_TURN") == 0) {
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

        } else if (strncmp(cmd, "PICKUP_CASE:", 12) == 0) {
            int case_id = atoi(cmd + 12);
            int active_case = read_kv_int(config_path, "active_case_id", 0);
            if (active_case != 0) {
                snprintf(message, sizeof(message), "Finish or settle your current case first (only one active case at a time).");
            } else {
                char title[256], claim[1024], player_side[32];
                if (!read_docket_entry(project_root, case_id, title, sizeof(title), claim, sizeof(claim), player_side, sizeof(player_side))) {
                    snprintf(message, sizeof(message), "No such case on the docket.");
                } else {
                    char case_dir[PATH_BUF];
                    snprintf(case_dir, sizeof(case_dir), "%s/data/cases/%d", project_root, case_id);
                    mkdir_p(case_dir);

                    char meta_path[PATH_BUF];
                    snprintf(meta_path, sizeof(meta_path), "%s/case_meta.txt", case_dir);
                    FILE *mf = fopen(meta_path, "w");
                    if (mf) {
                        int day = read_kv_int(config_path, "day", 1);
                        const char *plaintiff_name = strcmp(player_side, "plaintiff") == 0 ? "Adam Chen (you)" : "Opposing Counsel";
                        const char *defendant_name = strcmp(player_side, "defendant") == 0 ? "Adam Chen (you)" : "Opposing Counsel";
                        fprintf(mf, "case_id=%d\n", case_id);
                        fprintf(mf, "title=%s\n", title);
                        fprintf(mf, "claim_summary=%s\n", claim);
                        fprintf(mf, "player_side=%s\n", player_side);
                        fprintf(mf, "plaintiff_name=%s\n", plaintiff_name);
                        fprintf(mf, "defendant_name=%s\n", defendant_name);
                        fprintf(mf, "status=building\n");
                        fprintf(mf, "plaintiff_status=not_started\n");
                        fprintf(mf, "defendant_status=not_started\n");
                        fprintf(mf, "winner=\n");
                        fprintf(mf, "raw_comparison_text=\n");
                        fprintf(mf, "raw_verdict=\n");
                        fprintf(mf, "day_picked_up=%d\n", day);
                        fclose(mf);
                    }

                    char pcase_path[PATH_BUF], dcase_path[PATH_BUF];
                    snprintf(pcase_path, sizeof(pcase_path), "%s/plaintiff_case.txt", case_dir);
                    snprintf(dcase_path, sizeof(dcase_path), "%s/defendant_case.txt", case_dir);
                    FILE *pf2 = fopen(pcase_path, "w");
                    if (pf2) { fprintf(pf2, "CASE FOR: %s\nSIDE: Plaintiff\n\n", title); fclose(pf2); }
                    FILE *df2 = fopen(dcase_path, "w");
                    if (df2) { fprintf(df2, "CASE FOR: %s\nSIDE: Defendant\n\n", title); fclose(df2); }

                    write_kv_int(config_path, "active_case_id", case_id);

                    char details[512];
                    snprintf(details, sizeof(details), "case:%d|title:%s|player_side:%s", case_id, title, player_side);
                    ledger_append(project_root, read_kv_int(config_path, "day", 1), "case_pickup", details);

                    /* NPC opposing side auto-builds in parallel from day one -
                     * MY_LAWYER_DESIGN.md §7 open question 4 (no real
                     * decision_mode-chassis opponent yet, this NPC side just
                     * runs the same worker as the player's own side would). */
                    launch_case_worker(project_root, case_id, opposite_side(player_side));
                    write_kv(meta_path, opposite_side(player_side)[0] == 'p' ? "plaintiff_status" : "defendant_status", "building");

                    snprintf(message, sizeof(message), "Picked up case #%d: %s. You are the %s. Opposing counsel has begun building their case.",
                             case_id, title, player_side);
                }
            }

        } else if (strcmp(cmd, "BUILD_CASE") == 0) {
            int case_id = read_kv_int(config_path, "active_case_id", 0);
            if (case_id == 0) {
                snprintf(message, sizeof(message), "No active case - pick one up from the Docket first.");
            } else {
                char meta_path[PATH_BUF];
                snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", project_root, case_id);
                char player_side[32], side_status[32];
                read_kv_str_local(meta_path, "player_side", player_side, sizeof(player_side));
                char status_key[32];
                snprintf(status_key, sizeof(status_key), "%s_status", player_side);
                read_kv_str_local(meta_path, status_key, side_status, sizeof(side_status));

                char pid_path[PATH_BUF];
                snprintf(pid_path, sizeof(pid_path), "%s/data/cases/%d/%s_worker.pid", project_root, case_id, player_side);

                if (worker_already_running(pid_path)) {
                    snprintf(message, sizeof(message), "Already building your case - check back in a moment.");
                } else if (strcmp(side_status, "ready") == 0) {
                    snprintf(message, sizeof(message), "Your case is already complete - see %s_case.txt, or Present to Judge.", player_side);
                } else {
                    launch_case_worker(project_root, case_id, player_side);
                    write_kv(meta_path, status_key, "building");
                    snprintf(message, sizeof(message), "Building your case as %s... (gemma-lan calls in progress, check back shortly)", player_side);
                }
            }

        } else if (strcmp(cmd, "PRESENT_JUDGE") == 0) {
            int case_id = read_kv_int(config_path, "active_case_id", 0);
            if (case_id == 0) {
                snprintf(message, sizeof(message), "No active case.");
            } else {
                char meta_path[PATH_BUF];
                snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", project_root, case_id);
                char plaintiff_status[32], defendant_status[32], case_status[32];
                read_kv_str_local(meta_path, "plaintiff_status", plaintiff_status, sizeof(plaintiff_status));
                read_kv_str_local(meta_path, "defendant_status", defendant_status, sizeof(defendant_status));
                read_kv_str_local(meta_path, "status", case_status, sizeof(case_status));

                char pid_path[PATH_BUF];
                snprintf(pid_path, sizeof(pid_path), "%s/data/cases/%d/judge_worker.pid", project_root, case_id);

                if (strcmp(case_status, "judged") == 0 || strcmp(case_status, "settled") == 0) {
                    snprintf(message, sizeof(message), "This case is already closed.");
                } else if (worker_already_running(pid_path)) {
                    snprintf(message, sizeof(message), "The judge is already deliberating - check back in a moment.");
                } else if (strcmp(plaintiff_status, "ready") != 0 || strcmp(defendant_status, "ready") != 0) {
                    snprintf(message, sizeof(message), "Both sides must finish Building their case before you can Present to Judge.");
                } else {
                    write_kv(meta_path, "status", "judging");
                    char launch_cmd[PATH_BUF * 2];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                    snprintf(launch_cmd, sizeof(launch_cmd),
                             "cd '%s' && ./ops/+x/mylawyer_judge_worker.+x '%s' %d >/tmp/mylawyer_judge_worker_%d.log 2>&1 &",
                             project_root, project_root, case_id, case_id);
#pragma GCC diagnostic pop
                    int rc = system(launch_cmd);
                    (void)rc;
                    ledger_append(project_root, read_kv_int(config_path, "day", 1), "present_to_judge", "");
                    snprintf(message, sizeof(message), "Presenting both cases to the judge... (gemma-lan call in progress, check back shortly)");
                }
            }

        } else if (strcmp(cmd, "SETTLE") == 0) {
            int case_id = read_kv_int(config_path, "active_case_id", 0);
            if (case_id == 0) {
                snprintf(message, sizeof(message), "No active case.");
            } else {
                char meta_path[PATH_BUF];
                snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", project_root, case_id);
                char case_status[32], player_side[32];
                read_kv_str_local(meta_path, "status", case_status, sizeof(case_status));
                read_kv_str_local(meta_path, "player_side", player_side, sizeof(player_side));

                if (strcmp(case_status, "judged") == 0 || strcmp(case_status, "settled") == 0) {
                    snprintf(message, sizeof(message), "This case is already closed.");
                } else {
                    /* Instant, no-research resolution (§6) - a simple coin-flip
                     * formula, deliberately lower-variance/lower-payout than
                     * going to court. Exact formula NOT decided per design
                     * doc §7 open question 2 - this is a real, working
                     * placeholder, not the final tuned version. */
                    int money = read_kv_int(config_path, "money", 500);
                    int player_wins = (rand() % 100) < 50;
                    int day = read_kv_int(config_path, "day", 1);
                    if (player_wins) {
                        money += 100;
                        write_kv(meta_path, "winner", player_side);
                        snprintf(message, sizeof(message), "Settled - you won favorable terms. +$100 (now $%d).", money);
                    } else {
                        money -= 40;
                        if (money < 0) money = 0;
                        write_kv(meta_path, "winner", strcmp(player_side, "plaintiff") == 0 ? "defendant" : "plaintiff");
                        snprintf(message, sizeof(message), "Settled - you conceded ground. -$40 (now $%d).", money);
                    }
                    write_kv_int(config_path, "money", money);
                    write_kv(meta_path, "status", "settled");
                    write_kv_int(config_path, "active_case_id", 0);
                    ledger_append(project_root, day, "case_settled", player_wins ? "player_favorable" : "player_conceded");
                }
            }
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mylawyer_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
