/* deskopoly_ctl <command> <games_dir>  - what a game-bar button runs. It only APPENDS one line to <games_dir>/action.txt; deskopoly_driver reads it. No game logic here. Exit 0 ok, 2 usage. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: deskopoly_ctl <command> <games_dir>\n"); return 2; }
    for (const char *c = argv[1]; *c; c++) if (!((*c >= 'a' && *c <= 'z') || *c == '_')) { fprintf(stderr, "deskopoly_ctl: a command is lower-case letters and _\n"); return 2; }
    char p[PATH_MAX + 16]; snprintf(p, sizeof p, "%s/action.txt", argv[2]);
    int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd < 0) { perror("deskopoly_ctl"); return 1; }
    char ln[80]; int l = snprintf(ln, sizeof ln, "%s\n", argv[1]); int ok = write(fd, ln, (size_t)l) == l; close(fd); return ok ? 0 : 1;
}
