/* game_dispatch.c - One-shot op: read ALL keys from relay, dispatch each,
 * run NPC auto-play, compose frame, signal renderer.
 * Architecture: read all -> dispatch all -> NPC -> render -> exit.
 * Matches lpns+map+4's game_dispatch.c pattern exactly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE 4096

static int run_op(const char *path, const char *arg1) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        if (arg1) execl(path, path, arg1, NULL);
        else execl(path, path, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Step 1: Read ALL keys into memory, then truncate immediately.
     * This eliminates the race where parser writes a key between
     * our read and truncation. */
    FILE *hf = fopen("pieces/apps/player_app/interact_relay.txt", "r");
    if (!hf) return 0;

    char buf[MAX_LINE * 16];
    int buf_used = 0;
    buf[0] = '\0';
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), hf)) {
        int len = strlen(line);
        if (buf_used + len < (int)sizeof(buf)) {
            strcpy(buf + buf_used, line);
            buf_used += len;
        }
    }
    fclose(hf);

    /* Truncate immediately - any keys written after this point
     * are safe for next cycle */
    FILE *trunc = fopen("pieces/apps/player_app/interact_relay.txt", "w");
    if (trunc) fclose(trunc);

    /* Step 2: Process buffered keys - dispatch each to move_player + choice.
     * Two separate signals:
     *  any_key   = any key was processed → frame changed, recompose needed
     *  hero_moved = hero actually acted (not just xlector cursor) → advance turns */
    int any_key = 0, hero_moved = 0;
    char *p = buf;
    while (*p) {
        int keycode = -1;
        if (sscanf(p, "%d", &keycode) == 1) {
            char arg[16];
            snprintf(arg, sizeof(arg), "%d", keycode);
            int ret = run_op("./ops/+x/move_player", arg);
            if (ret == 0) hero_moved = 1;  /* hero acted */
            else if (ret == 2) any_key = 1; /* xlector moved */
            run_op("./ops/+x/choice", arg);
            run_op("./ops/+x/camera_control", arg);
            any_key = 1;
        }
        char *nl = strchr(p, '\n');
        if (nl) p = nl + 1;
        else break;
    }

    /* Step 3a: Recompose on any keypress (hero or xlector). */
    if (any_key) {
        run_op("./ops/+x/compose_frame", NULL);
        run_op("./ops/+x/compose_rgb_frame", NULL);
        {
            FILE *mf = fopen("pieces/display/frame_changed.txt", "a");
            if (mf) { fprintf(mf, "E\n"); fclose(mf); }
        }
    }

    /* Step 3b: Advance turns only when the hero actually acted.
     * Xlector cursor movement doesn't consume turns. */
    if (hero_moved) {
        run_op("./ops/+x/end_turn", NULL);
        run_op("./ops/+x/tick_monsters", NULL);
    }

    return 0;
}
