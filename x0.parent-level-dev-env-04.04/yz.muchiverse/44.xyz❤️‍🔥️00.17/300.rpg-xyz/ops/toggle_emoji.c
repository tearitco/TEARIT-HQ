/* toggle_emoji - one verb, one binary, no shared headers.
 * Standalone equivalent of choice.c's own inline 'e'/'E' handler
 * (flips hero/state.txt's emoji_mode). Exists as a real, numbered
 * hero/piece.pdl METHOD row so the chtpm-level ${piece_methods} nav
 * (arrows select a number, Enter dispatches - see xyzos-standards.txt
 * §12) can reach it exactly like fuzz-op's own numbered hotkeys,
 * instead of only being reachable via the raw 'e' keypress choice.c
 * still ALSO handles directly (both paths kept - matches real
 * fuzz-op_manager.c, which keeps its own hardcoded emoji-toggle key
 * live at the same time as its dynamic method menu, not one or the
 * other). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char hero_path[PATH_BUF];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);

    FILE *f = fopen(hero_path, "r");
    if (!f) return 1;

    char lines[32][MAX_LINE];
    int nlines = 0;
    int emoji_mode = 1;
    while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) {
        char *eq = strchr(lines[nlines], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[nlines], "emoji_mode") == 0) emoji_mode = atoi(eq + 1);
            *eq = '=';
        }
        nlines++;
    }
    fclose(f);

    emoji_mode = !emoji_mode;

    f = fopen(hero_path, "w");
    if (!f) return 1;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        char *eq = strchr(lines[i], '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(lines[i], "emoji_mode") == 0) {
                fprintf(f, "emoji_mode=%d\n", emoji_mode);
                found = 1;
                *eq = '=';
                continue;
            }
            *eq = '=';
        }
        fputs(lines[i], f);
    }
    if (!found) fprintf(f, "emoji_mode=%d\n", emoji_mode);
    fclose(f);

    return 0;
}
