/* rmmc_mode_op - the rmmc game mode switch as an OP (owner 2026-11-26: "no creative at all in this mode, toggleable thru menu but default off").
 * Creative mode = the hotbar gives the picked tool and block before every swing and place (rmmc_verb.c writes a `GIVE | actor | item | 1 | hotbar` row), so the count never goes down and nothing runs out.
 * Default OFF: a mine adds what the block drops, a place costs one from the bag, a tool must be owned. The switch is the CONFIG file <book>/play/creative.txt (creative=1); the verb layer reads it
 * to decide which rows to WRITE, so replay never needs it (the rows carry the effect). Absent file or anything but creative=1 = off. Nothing else is written.
 *   rmmc_mode_op status <book>   prints  creative=on | creative=off
 *   rmmc_mode_op toggle <book>   flips it, prints the new state
 *   rmmc_mode_op set <book> on|off
 * Exit: 0 ok | 2 usage. Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_mode_op.+x rmmc_mode_op.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

static int is_on(const char *p) { FILE *f = fopen(p, "r"); if (!f) return 0; char l[64]; int on = 0; while (fgets(l, sizeof l, f)) if (!strncmp(l, "creative=1", 10)) on = 1; fclose(f); return on; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: rmmc_mode_op status|toggle|set <book> [on|off]\n"); return 2; }
    char d[PATH_MAX], p[PATH_MAX + 32], t[PATH_MAX + 40]; snprintf(d, sizeof d, "%.4000s/play", argv[2]); snprintf(p, sizeof p, "%s/creative.txt", d); snprintf(t, sizeof t, "%s.tmp", p);
    int on = is_on(p);
    if (!strcmp(argv[1], "status")) { printf("creative=%s\n", on ? "on" : "off"); return 0; }
    int want;
    if (!strcmp(argv[1], "toggle")) want = !on; else if (!strcmp(argv[1], "set") && argc >= 4) want = !strcmp(argv[3], "on"); else { fprintf(stderr, "rmmc_mode_op: unknown verb\n"); return 2; }
    mkdir(d, 0755); FILE *f = fopen(t, "w"); if (!f) return 2; fprintf(f, "creative=%d\n", want); fclose(f); rename(t, p);
    printf("creative=%s\n", want ? "on" : "off"); return 0; }
