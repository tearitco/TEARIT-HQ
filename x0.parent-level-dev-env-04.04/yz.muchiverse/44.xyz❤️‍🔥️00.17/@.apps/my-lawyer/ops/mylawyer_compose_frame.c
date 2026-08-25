/* mylawyer_compose_frame - renders whichever my-lawyer screen is
 * current into pieces/apps/player_app/view.txt. Modeled directly on
 * my-biotech's own ops/mybiotech_compose_frame.c (same ONE WRITER RULE:
 * writes ONLY view.txt, then bumps frame_changed.txt).
 *
 * Also REGENERATES the docket/case piece.pdl files each frame from live
 * data (data/docket.txt, data/cases/<id>/case_meta.txt) - the same
 * technique my-chara-txt's own farm screen uses for its per-plot METHOD
 * rows (see that project's ops/mychara_compose_frame.c), applied here to
 * a dynamic lawsuit list and a dynamic per-case action list instead of a
 * fixed 3-plot grid.
 *
 * Self-contained, no shared headers.
 * Usage: mylawyer_compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_LINE 2048
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
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            val = atoi(line + key_len + 1);
        }
    }
    fclose(f);
    return val;
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
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static char *get_current_piece_id_str(const char *root, char *out, size_t out_sz) {
    snprintf(out, out_sz, "main");
    char layout_path[PATH_BUF];
    snprintf(layout_path, sizeof(layout_path), "%s/pieces/display/current_layout.txt", root);
    FILE *f = fopen(layout_path, "r");
    if (!f) return out;
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
    return out;
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

static void wrapped(const char *text) {
    const char *p = text;
    size_t remaining = strlen(p);
    if (remaining == 0) return;
    while (remaining > 0) {
        size_t chunk = remaining < (size_t)BOX_W ? remaining : (size_t)BOX_W;
        char buf[BOX_W + 1];
        memcpy(buf, p, chunk);
        buf[chunk] = '\0';
        line(buf);
        p += chunk;
        remaining -= chunk;
    }
}

static void ping_chtpm_render_marker(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/frame_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

static void regenerate_docket_pdl(const char *root) {
    char docket_path[PATH_BUF];
    snprintf(docket_path, sizeof(docket_path), "%s/data/docket.txt", root);
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-lawyer/pieces/docket/piece.pdl", root);

    FILE *df = fopen(docket_path, "r");
    FILE *pdl_out = fopen(pdl_path, "w");
    if (!pdl_out) { if (df) fclose(df); return; }

    fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
    fprintf(pdl_out, "----------------------------------------\n");
    fprintf(pdl_out, "META         | piece_id           | docket\n\n");

    int any = 0;
    if (df) {
        char l[MAX_LINE];
        while (fgets(l, sizeof(l), df)) {
            l[strcspn(l, "\r\n")] = '\0';
            if (!l[0]) continue;
            int case_id = atoi(l);
            char *p1 = strchr(l, '|');
            char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
            char *p3 = p2 ? strchr(p2 + 1, '|') : NULL;
            if (!p1 || !p2 || !p3) continue;
            char title[200];
            char player_side[32];
            snprintf(title, sizeof(title), "%.*s", (int)(p2 - (p1 + 1)), p1 + 1);
            snprintf(player_side, sizeof(player_side), "%s", p3 + 1);

            char case_dir[PATH_BUF];
            snprintf(case_dir, sizeof(case_dir), "%s/data/cases/%d/case_meta.txt", root, case_id);
            if (file_exists(case_dir)) continue; /* already picked up */

            fprintf(pdl_out, "METHOD       | #%d %s (as %s)  | PICKUP_CASE:%d\n", case_id, title, player_side, case_id);
            any = 1;
        }
        fclose(df);
    }
    if (!any) {
        fprintf(pdl_out, "METHOD       | (no open lawsuits right now)      | NOOP\n");
    }
    fclose(pdl_out);
}

static void regenerate_case_pdl(const char *root, int case_id) {
    char meta_path[PATH_BUF];
    snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", root, case_id);
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-lawyer/pieces/case/piece.pdl", root);

    char player_side[32], plaintiff_status[32], defendant_status[32], case_status[32];
    read_kv_str(meta_path, "player_side", player_side, sizeof(player_side));
    read_kv_str(meta_path, "plaintiff_status", plaintiff_status, sizeof(plaintiff_status));
    read_kv_str(meta_path, "defendant_status", defendant_status, sizeof(defendant_status));
    read_kv_str(meta_path, "status", case_status, sizeof(case_status));

    const char *my_status = strcmp(player_side, "plaintiff") == 0 ? plaintiff_status : defendant_status;

    FILE *pdl_out = fopen(pdl_path, "w");
    if (!pdl_out) return;
    fprintf(pdl_out, "SECTION      | KEY                | VALUE\n");
    fprintf(pdl_out, "----------------------------------------\n");
    fprintf(pdl_out, "META         | piece_id           | case\n\n");

    if (strcmp(case_status, "judged") == 0 || strcmp(case_status, "settled") == 0) {
        fprintf(pdl_out, "METHOD       | (case closed - see summary above)  | NOOP\n");
    } else {
        char pid_path[PATH_BUF];
        snprintf(pid_path, sizeof(pid_path), "%s/data/cases/%d/%s_worker.pid", root, case_id, player_side);
        FILE *pf = fopen(pid_path, "r");
        int worker_running = 0;
        if (pf) { fclose(pf); worker_running = file_exists(pid_path); }

        if (strcmp(my_status, "ready") == 0) {
            fprintf(pdl_out, "METHOD       | Your case is complete (see %s_case.txt) | NOOP\n", player_side);
        } else if (worker_running) {
            fprintf(pdl_out, "METHOD       | Building your case... (in progress)  | NOOP\n");
        } else {
            fprintf(pdl_out, "METHOD       | Build Case (research + write argument) | BUILD_CASE\n");
        }

        if (strcmp(plaintiff_status, "ready") == 0 && strcmp(defendant_status, "ready") == 0 && strcmp(case_status, "judging") != 0) {
            fprintf(pdl_out, "METHOD       | Present to Judge                      | PRESENT_JUDGE\n");
        }
        if (strcmp(case_status, "judging") != 0) {
            fprintf(pdl_out, "METHOD       | Settle (instant, lower risk)          | SETTLE\n");
        }
    }
    fclose(pdl_out);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], view_path[PATH_BUF], config_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/projects/my-lawyer/pieces/mylawyer_menu/state.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);

    char last_message[MAX_LINE];
    read_kv_str(state_path, "last_message", last_message, sizeof(last_message));

    g_view_out = fopen(view_path, "w");
    if (!g_view_out) return 1;

    int day = read_kv_int(config_path, "day", 1);
    int max_days = read_kv_int(config_path, "max_days", 10);
    int money = read_kv_int(config_path, "money", 500);
    int active_case_id = read_kv_int(config_path, "active_case_id", 0);

    char rowbuf[MAX_LINE];
    border();
    snprintf(rowbuf, sizeof(rowbuf), "  Day %d / %d   Money: $%d", day, max_days, money);
    line(rowbuf);
    border();
    blank();

    char active_piece[128];
    get_current_piece_id_str(project_root, active_piece, sizeof(active_piece));

    if (strcmp(active_piece, "main") == 0) {
        if (active_case_id != 0) {
            char meta_path[PATH_BUF];
            snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", project_root, active_case_id);
            char title[256];
            read_kv_str(meta_path, "title", title, sizeof(title));
            snprintf(rowbuf, sizeof(rowbuf), "  Active case: #%d %s", active_case_id, title);
            line(rowbuf);
        } else {
            line("  No active case - visit Docket to pick one up.");
        }
        blank();

    } else if (strcmp(active_piece, "docket") == 0) {
        regenerate_docket_pdl(project_root);
        line("  Available lawsuits:");
        blank();

    } else if (strcmp(active_piece, "case") == 0) {
        if (active_case_id == 0) {
            line("  No active case - pick one up from the Docket.");
            blank();
            char pdl_path[PATH_BUF];
            snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-lawyer/pieces/case/piece.pdl", project_root);
            FILE *pdl_out = fopen(pdl_path, "w");
            if (pdl_out) {
                fprintf(pdl_out, "SECTION      | KEY                | VALUE\n----------------------------------------\nMETA         | piece_id           | case\n\n");
                fprintf(pdl_out, "METHOD       | (nothing to do here yet)  | NOOP\n");
                fclose(pdl_out);
            }
        } else {
            char meta_path[PATH_BUF];
            snprintf(meta_path, sizeof(meta_path), "%s/data/cases/%d/case_meta.txt", project_root, active_case_id);
            char title[256], claim[1024], player_side[32], plaintiff_status[32], defendant_status[32], case_status[32], winner[32], raw_comparison[2048];
            read_kv_str(meta_path, "title", title, sizeof(title));
            read_kv_str(meta_path, "claim_summary", claim, sizeof(claim));
            read_kv_str(meta_path, "player_side", player_side, sizeof(player_side));
            read_kv_str(meta_path, "plaintiff_status", plaintiff_status, sizeof(plaintiff_status));
            read_kv_str(meta_path, "defendant_status", defendant_status, sizeof(defendant_status));
            read_kv_str(meta_path, "status", case_status, sizeof(case_status));
            read_kv_str(meta_path, "winner", winner, sizeof(winner));
            read_kv_str(meta_path, "raw_comparison_text", raw_comparison, sizeof(raw_comparison));

            snprintf(rowbuf, sizeof(rowbuf), "  #%d %s", active_case_id, title);
            line(rowbuf);
            wrapped(claim);
            blank();
            snprintf(rowbuf, sizeof(rowbuf), "  You are the: %s", player_side);
            line(rowbuf);
            snprintf(rowbuf, sizeof(rowbuf), "  Plaintiff case: %s   Defendant case: %s", plaintiff_status, defendant_status);
            line(rowbuf);
            blank();

            /* Live worker progress - player's own side */
            char my_pid_path[PATH_BUF], my_status_path[PATH_BUF];
            snprintf(my_pid_path, sizeof(my_pid_path), "%s/data/cases/%d/%s_worker.pid", project_root, active_case_id, player_side);
            snprintf(my_status_path, sizeof(my_status_path), "%s/data/cases/%d/%s_worker_status.txt", project_root, active_case_id, player_side);
            if (file_exists(my_pid_path)) {
                char step[64];
                int round = read_kv_int(my_status_path, "round", 0);
                read_kv_str(my_status_path, "step", step, sizeof(step));
                long started_at = (long)read_kv_int(my_status_path, "updated_at", 0);
                long elapsed = started_at > 0 ? (long)time(NULL) - started_at : 0;
                snprintf(rowbuf, sizeof(rowbuf), "  \xe2\x8f\xb3 Building your case... [%s] round %d (%lds)", step[0] ? step : "?", round, elapsed);
                line(rowbuf);
                blank();
            }

            char judge_pid_path[PATH_BUF], judge_status_path[PATH_BUF];
            snprintf(judge_pid_path, sizeof(judge_pid_path), "%s/data/cases/%d/judge_worker.pid", project_root, active_case_id);
            snprintf(judge_status_path, sizeof(judge_status_path), "%s/data/cases/%d/judge_worker_status.txt", project_root, active_case_id);
            if (file_exists(judge_pid_path)) {
                char step[64];
                read_kv_str(judge_status_path, "step", step, sizeof(step));
                long started_at = (long)read_kv_int(judge_status_path, "updated_at", 0);
                long elapsed = started_at > 0 ? (long)time(NULL) - started_at : 0;
                snprintf(rowbuf, sizeof(rowbuf), "  \xe2\x8f\xb3 Judge deliberating... [%s] (%lds)", step[0] ? step : "?", elapsed);
                line(rowbuf);
                blank();
            }

            if (strcmp(case_status, "judged") == 0) {
                snprintf(rowbuf, sizeof(rowbuf), "  VERDICT: %s wins.", winner);
                line(rowbuf);
                if (raw_comparison[0]) wrapped(raw_comparison);
                blank();
            } else if (strcmp(case_status, "settled") == 0) {
                snprintf(rowbuf, sizeof(rowbuf), "  SETTLED: %s prevailed.", winner);
                line(rowbuf);
                blank();
            }

            regenerate_case_pdl(project_root, active_case_id);
        }
    }

    if (last_message[0]) {
        wrapped(last_message);
    }

    fclose(g_view_out);
    ping_chtpm_render_marker(project_root);
    return 0;
}
