/* deskopoly_ctl <command> <games_dir> [ignored...]  - what a game-bar button runs. It only APPENDS the line <command> to <games_dir>/action.txt; deskopoly_driver reads it. No game logic here. Exit 0 ok, 2 usage.
 * EXTRA ARGUMENTS ARE IGNORED ON PURPOSE: the khtpm renderer appends 2 arguments (the click position) to every action string, so a window button runs `ctl cmd dir X Y`. A first version of the repeat feature took a positional
 * count and refused argc > 4: every popup button in the real editor silently stopped working (found live 2026-11-08, the harness could not see it because it never passes the two extra arguments).
 * REPEAT (harness only): the environment variable DESKOPOLY_CTL_REPEAT=<1..200> appends the line that many times in ONE process (the trade screen's cash clamp is only reachable after 93 presses of "+10"); a window button never sets it. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: deskopoly_ctl <command> <games_dir>\n"); return 2; }
    for (const char *c = argv[1]; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '+' || *c == '-')) { fprintf(stderr, "deskopoly_ctl: a command is lower-case letters, digits, _ + -\n"); return 2; }   /* digits and + - since M9: tr_g19, tr_rc+ (still no path, space or shell character) */
    if (!argv[1][0] || strlen(argv[1]) > 40) { fprintf(stderr, "deskopoly_ctl: a command is 1 to 40 characters\n"); return 2; }
    int count = 1; const char *rp = getenv("DESKOPOLY_CTL_REPEAT"); if (rp && rp[0]) { char *e; long n = strtol(rp, &e, 10); if (*e || n < 1 || n > 200) { fprintf(stderr, "deskopoly_ctl: DESKOPOLY_CTL_REPEAT is 1 to 200\n"); return 2; } count = (int)n; }
    char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/action.txt", argv[2]);
    int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd < 0) { perror("deskopoly_ctl"); return 1; }
    char ln[80]; int l = snprintf(ln, sizeof ln, "%s\n", argv[1]); int ok = 1;
    for (int i = 0; i < count && ok; i++) ok = write(fd, ln, (size_t)l) == l;   /* one write() per line: O_APPEND keeps each line whole even with a second writer */
    close(fd); return ok ? 0 : 1;
}
