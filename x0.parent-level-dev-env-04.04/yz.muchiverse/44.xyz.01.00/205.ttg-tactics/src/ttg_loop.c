/* ttg_loop.c — file-mediated game brain (prisc-equivalent for TTG MVP) */
#define _DEFAULT_SOURCE
#include "ttg.h"
#include <sys/time.h>
#include <unistd.h>

static int quit_requested(const Game *g) {
    char path[MAX_PATH];
    FILE *f;
    char buf[16];
    ttg_path(g, path, sizeof(path), "pieces/system/quit_flag.txt");
    f = fopen(path, "r");
    if (!f) return 0;
    if (fgets(buf, sizeof(buf), f) && (buf[0] == '1' || buf[0] == 'q')) {
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

static void frame_all(Game *g) {
    ttg_save_all(g);
    ttg_compose_frame(g);
    ttg_compose_rgb(g);
    ttg_pulse(g);
}

int main(int argc, char **argv) {
    Game g;
    const char *root = ".";
    int headless = 0;
    int max_ticks = -1;
    int tick = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) root = argv[++i];
        else if (strcmp(argv[i], "--headless") == 0) headless = 1;
        else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) max_ticks = atoi(argv[++i]);
        else if (strcmp(argv[i], "--start-match") == 0) {
            /* flag processed after init */
        }
    }
    ttg_init_empty(&g);
    ttg_set_root(&g, root);
    {
        char p[MAX_PATH];
        ttg_path(&g, p, sizeof(p), "pieces/display");
        ttg_mkdir_p(p);
        ttg_path(&g, p, sizeof(p), "pieces/apps/player_app");
        ttg_mkdir_p(p);
        ttg_path(&g, p, sizeof(p), "pieces/system");
        ttg_mkdir_p(p);
        ttg_path(&g, p, sizeof(p), "data");
        ttg_mkdir_p(p);
        ttg_path(&g, p, sizeof(p), "pieces/system/quit_flag.txt");
        ttg_write_file(p, "");
        /* Do NOT truncate history.txt — harness/AI may pre-seed keys.
         * Interactive button.sh run clears history before exec. */
        ttg_path(&g, p, sizeof(p), "pieces/apps/player_app/history.txt");
        {
            FILE *hf = fopen(p, "a"); /* ensure exists */
            if (hf) fclose(hf);
        }
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start-match") == 0)
            ttg_init_match(&g, 300000, 50);
    }
    frame_all(&g);
    fprintf(stderr, "ttg_loop root=%s headless=%d\n", root, headless);

    while (!quit_requested(&g)) {
        int n;
        if (max_ticks >= 0 && tick >= max_ticks) break;
        n = ttg_read_keys(&g, 32);
        /* clock tick ~16ms * 60 ≈ 1s drain when in match human seat? drain every second */
        if (g.phase == PH_MATCH && !g.clock_frozen) {
            static int acc = 0;
            acc += 16;
            if (acc >= 1000) {
                acc = 0;
                if (g.clock_ms[g.active_seat] > 0)
                    g.clock_ms[g.active_seat] -= 1000;
                if (g.clock_ms[g.active_seat] <= 0) {
                    snprintf(g.winner, sizeof(g.winner), "%d", 1 - g.active_seat);
                    snprintf(g.end_reason, sizeof(g.end_reason), "flag");
                    g.phase = PH_END;
                    ttg_ledger(&g, "system", "clock_flag", g.active_seat == 0 ? "seat:0" : "seat:1");
                }
            }
        }
        /* AI seat */
        if (g.phase == PH_MATCH && g.seat_type[g.active_seat] == 1) {
            ttg_ai_turn_keys(&g);
            n++;
        }
        if (n > 0 || (tick % 30) == 0)
            frame_all(&g);
        tick++;
        if (headless && max_ticks < 0 && tick > 5) break;
        usleep(16667);
    }
    frame_all(&g);
    fprintf(stderr, "ttg_loop exit ticks=%d\n", tick);
    return 0;
}
