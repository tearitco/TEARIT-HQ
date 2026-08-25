/* mychara_ai_decide - the decision_mode-branching automation brain for
 * my-chara-txt, modeled directly on 014.wsr-pal's ops/corp_decide.c (the
 * real, proven decision_mode chassis: 0=preset/1=weighted/2=rl-stub/
 * 3=llm/4=human-park-and-wait). P1 scope: preset tier only, a trivial
 * threshold rule per %.harnesses/xo-human.md §4's own suggested shape
 * ("if grain<20, farm; if have ore, sell it; else mine") - adapted to
 * what's actually built (no Store yet, so "sell ore" is dropped).
 *
 * Called from main_module.pal's own idle (no_key) branch on EVERY idle
 * tick (~30ms), same cadence mychara_menu_input's own key==0 fast-path
 * already runs at safely - this op is a self-gating cheap no-op unless
 * supervision_mode is semi/full AND not currently paused, so it adds
 * no meaningful CPU cost in the default (manual) case. When actually
 * automating, it additionally self-throttles to at most ONE real
 * action per wall-clock second (last_auto_tick field) so Full mode
 * doesn't flood master_ledger.txt or make automated play impossible to
 * observe/interrupt - this is a deliberate safety margin, not required
 * by decision_mode's own design, added because this op drives real
 * unattended gameplay rather than a single per-request decision like
 * corp_decide.c's own usage.
 *
 * supervision_mode (separate from decision_mode, orthogonal per
 * CIV_TXT_DESIGN.md §7 / this project's own §10a retrofit note):
 *   manual - this op returns immediately, does nothing (status quo)
 *   semi   - decides + executes ONE action, then sets
 *            paused_for_confirmation=1 and stops until a human sends
 *            CONTINUE_AUTO (piece.pdl row on automation.chtpm)
 *   full   - decides + executes repeatedly, throttled to ~1/sec,
 *            never sets paused_for_confirmation
 *
 * decision_mode (the actual decision tier, independent of supervision):
 *   0 (preset)   - implemented below, real logic
 *   1 (weighted) - NOT YET IMPLEMENTED, falls back to preset (matches
 *                  corp_decide.c's own honest rl-stub-falls-back
 *                  precedent for its currently-unimplemented tier)
 *   2 (rl)       - falls back to preset
 *   3 (llm)      - falls back to preset (a real LLM call is real future
 *                  work, deliberately not built here per
 *                  xo-human.md's speed doctrine - never the default
 *                  per-tick tier anyway)
 *   4 (human)    - should not occur while supervision != manual; if
 *                  seen, treated as manual (no-op), since decision_mode
 *                  4 IS corp_decide.c's own separate park-and-wait
 *                  concept and shouldn't be double-driven
 *
 * Self-contained, no shared headers.
 * Usage: mychara_ai_decide.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

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
            v[strcspn(v, "\r\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

static int read_kv_int(const char *path, const char *key, int def) {
    char buf[64];
    read_kv_str(path, key, buf, sizeof(buf));
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

static void bump_screen_changed(const char *root) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/mychara_screen_changed.txt", root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
}

int main(void) {
    resolve_root();

    char config_path[PATH_BUF], plots_path[PATH_BUF], state_path[PATH_BUF];
    snprintf(config_path, sizeof(config_path), "%s/pieces/system/config.txt", project_root);
    snprintf(plots_path, sizeof(plots_path), "%s/pieces/system/plots.txt", project_root);
    snprintf(state_path, sizeof(state_path), "%s/projects/my-chara-txt/pieces/mychara_menu/state.txt", project_root);

    /* Cheap gate #1: supervision mode. This check alone must stay fast -
     * it runs on every ~30ms idle tick regardless of mode. */
    char supervision[32] = "";
    read_kv_str(config_path, "supervision_mode", supervision, sizeof(supervision));
    if (!supervision[0] || strcmp(supervision, "manual") == 0) return 0;

    /* Cheap gate #2: semi-mode pause flag. */
    int paused = read_kv_int(config_path, "paused_for_confirmation", 0);
    if (strcmp(supervision, "semi") == 0 && paused) return 0;

    /* Cheap gate #3: game must still be playing. */
    char game_state[32] = "";
    read_kv_str(config_path, "game_state", game_state, sizeof(game_state));
    if (!game_state[0]) snprintf(game_state, sizeof(game_state), "playing");
    if (strcmp(game_state, "playing") != 0) return 0;

    /* Throttle: at most one real automated action per wall-clock second,
     * so Full mode is observable/interruptible and never floods the
     * ledger, even though this op itself is called every ~30ms. */
    time_t now = time(NULL);
    int last_tick = read_kv_int(config_path, "last_auto_tick", 0);
    if (strcmp(supervision, "full") == 0 && (now - last_tick) < 1) return 0;
    if (strcmp(supervision, "semi") == 0 && last_tick != 0 && (now - last_tick) < 1) return 0;

    int decision_mode = read_kv_int(config_path, "decision_mode", 0);
    /* Only preset (0) is implemented; every other tier falls back to it
     * for now, matching corp_decide.c's own honest rl-stub precedent. */
    (void)decision_mode;

    int day = read_kv_int(config_path, "day", 1);
    int grain = read_kv_int(config_path, "grain_in_inventory", 10);

    char message[MAX_LINE] = "";
    int acted = 0;

    /* Preset rule, in priority order: harvest ripe plots first (free
     * value sitting on the board), then plant if grain allows and a
     * plot is empty, then mine as the fallback action, then end the
     * turn if nothing else is actionable this tick. */
    for (int i = 0; i < 3 && !acted; i++) {
        char key_state[64], key_crop[64];
        snprintf(key_state, sizeof(key_state), "plot_%d_state", i);
        snprintf(key_crop, sizeof(key_crop), "plot_%d_crop", i);
        char pstate[32] = "", crop[32] = "";
        read_kv_str(plots_path, key_state, pstate, sizeof(pstate));
        read_kv_str(plots_path, key_crop, crop, sizeof(crop));
        if (!pstate[0]) snprintf(pstate, sizeof(pstate), "empty");

        if (strcmp(pstate, "ripe") == 0) {
            int harvest_amount = strcmp(crop, "wheat") == 0 ? 50 : 60;
            grain += harvest_amount;
            write_kv_int(config_path, "grain_in_inventory", grain);
            write_kv(plots_path, key_state, "empty");
            write_kv(plots_path, key_crop, "");

            char details[128];
            snprintf(details, sizeof(details), "auto:%s:%d:plot_%d", crop, harvest_amount, i);
            ledger_append(project_root, day, "harvest", details);
            snprintf(message, sizeof(message), "[AUTO] Harvested %d %s from plot %d.", harvest_amount, crop, i);
            acted = 1;
        }
    }

    if (!acted && grain >= 10) {
        for (int i = 0; i < 3 && !acted; i++) {
            char key_state[64], key_crop[64], key_harvest[64];
            snprintf(key_state, sizeof(key_state), "plot_%d_state", i);
            snprintf(key_crop, sizeof(key_crop), "plot_%d_crop", i);
            snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", i);
            char pstate[32] = "";
            read_kv_str(plots_path, key_state, pstate, sizeof(pstate));
            if (!pstate[0]) snprintf(pstate, sizeof(pstate), "empty");

            if (strcmp(pstate, "empty") == 0) {
                grain -= 10;
                write_kv_int(config_path, "grain_in_inventory", grain);
                write_kv(plots_path, key_state, "growing");
                write_kv(plots_path, key_crop, "wheat");
                char harvest_day_str[32];
                snprintf(harvest_day_str, sizeof(harvest_day_str), "%d", day + 3);
                write_kv(plots_path, key_harvest, harvest_day_str);

                char details[128];
                snprintf(details, sizeof(details), "auto:wheat:plot_%d", i);
                ledger_append(project_root, day, "plant", details);
                snprintf(message, sizeof(message), "[AUTO] Planted wheat on plot %d.", i);
                acted = 1;
            }
        }
    }

    /* Fallback: nothing to harvest, nothing plantable right now (either
     * grain < 10 or every plot already growing) - the only way to make
     * real forward progress is to end the turn, since growing->ripe
     * transitions and health decay ONLY happen inside END_TURN's own
     * tick logic (mychara_menu_input.c), never elsewhere. Without this,
     * automation would spin forever never advancing a single day - a
     * real correctness bug, not a style choice. Duplicated verbatim
     * from mychara_menu_input.c's own END_TURN handler, per house
     * no-shared-headers convention. */
    if (!acted) {
        int max_days = read_kv_int(config_path, "max_days", 10);
        int health = read_kv_int(config_path, "health", 100);

        health -= 5;
        if (health < 0) health = 0;
        int new_day = day + 1;

        write_kv_int(config_path, "health", health);
        write_kv_int(config_path, "day", new_day);

        for (int i = 0; i < 3; i++) {
            char key_state[64], key_harvest[64];
            snprintf(key_state, sizeof(key_state), "plot_%d_state", i);
            snprintf(key_harvest, sizeof(key_harvest), "plot_%d_harvest_day", i);
            char pstate[32] = "";
            read_kv_str(plots_path, key_state, pstate, sizeof(pstate));
            if (!pstate[0]) snprintf(pstate, sizeof(pstate), "empty");
            if (strcmp(pstate, "growing") == 0) {
                char harvest_str[32] = "0";
                read_kv_str(plots_path, key_harvest, harvest_str, sizeof(harvest_str));
                int harvest_day = atoi(harvest_str);
                if (new_day >= harvest_day) write_kv(plots_path, key_state, "ripe");
            }
        }

        char details[128];
        snprintf(details, sizeof(details), "auto:health:%d", health);
        ledger_append(project_root, new_day - 1, "day_end", details);

        if (new_day > max_days) {
            write_kv(config_path, "game_state", "game_over");
            snprintf(message, sizeof(message), "[AUTO] Day %d - GAME OVER (reached max_days).", new_day - 1);
        } else if (health <= 0) {
            write_kv(config_path, "game_state", "game_over");
            snprintf(message, sizeof(message), "[AUTO] Day %d - GAME OVER (health reached 0).", new_day - 1);
        } else {
            snprintf(message, sizeof(message), "[AUTO] Day %d began. Health %d.", new_day, health);
        }
        acted = 1;
    }

    write_kv_int(config_path, "last_auto_tick", (int)now);
    write_kv(state_path, "last_message", message);
    bump_screen_changed(project_root);

    if (strcmp(supervision, "semi") == 0) {
        write_kv_int(config_path, "paused_for_confirmation", 1);
    }

    return 0;
}
