/* tearit_ctl <command> <play_dir> [ignored...] - what a game-bar or stamp button of tearit-map runs (copy of tear_on_ctl). It only APPENDS the line <command> to <play_dir>/action.txt; tearit_driver reads it. No game logic here.
 * EXTRA ARGUMENTS ARE IGNORED ON PURPOSE: the khtpm renderer appends 2 arguments (the click position) to every action string (the same trap as deskopoly_ctl, found live 2026-11-08).
 * The queue is capped at 64 KiB: when it is bigger the oldest half is dropped by the DRIVER, never here.  Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tearit_ctl.+x tearit_ctl.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: tearit_ctl <command> <play_dir>\n"); return 2; }
    for (const char *c = argv[1]; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) { fprintf(stderr, "tearit_ctl: a command is lower-case letters, digits, _ and -\n"); return 2; }
    if (!argv[1][0] || strlen(argv[1]) > 40) { fprintf(stderr, "tearit_ctl: a command is 1 to 40 characters\n"); return 2; }
    char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/action.txt", argv[2]);
    int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd < 0) { perror("tearit_ctl"); return 1; }
    char ln[80]; int l = snprintf(ln, sizeof ln, "%s\n", argv[1]); int ok = write(fd, ln, (size_t)l) == l; close(fd); return ok ? 0 : 1; }
