/* rmmc_pick_op <item>   choose the block Asa will place (the hotbar slot click). A PREFERENCE, never the ledger: play/pick.txt holds one line, the item kind.
 * The item must be a SLOT row of <book>/hotbar.pdl (CONFIG): anything else exits 2 and writes nothing. House and book are found from this binary's own path
 * (<house>/@.apps/rmmc/ops/+x/rmmc_pick_op.+x), so a feed can put the bare path in a button action (no environment word needed).
 * Exit: 0 written (or already picked) | 2 usage / unknown item / bad characters | 3 hotbar.pdl missing | 6 not written. Output: "picked <item>".
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o ops/+x/rmmc_pick_op.+x ops/rmmc_pick_op.c */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: rmmc_pick_op <item>\n"); return 2; }
    const char *item = argv[1];
    if (!*item || strlen(item) > 40) { fprintf(stderr, "rmmc_pick_op: bad item\n"); return 2; }
    for (const char *c = item; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) { fprintf(stderr, "rmmc_pick_op: bad item\n"); return 2; }
    char self[PATH_MAX]; if (!realpath(argv[0], self)) return 2;
    char book[PATH_MAX + 8]; snprintf(book, sizeof book, "%s", self);
    for (int i = 0; i < 3; i++) { char *s = strrchr(book, '/'); if (!s) return 2; *s = 0; }          /* <book>/ops/+x/<this> */
    char hp[PATH_MAX + 32]; snprintf(hp, sizeof hp, "%s/hotbar.pdl", book);
    FILE *f = fopen(hp, "r"); if (!f) { fprintf(stderr, "rmmc_pick_op: %s missing\n", hp); return 3; }
    char line[256]; int ok = 0;
    while (fgets(line, sizeof line, f)) {
        char a[16], b[64]; if (sscanf(line, " %15[^| ] | %63[^| ]", a, b) != 2 || strcmp(a, "SLOT")) continue;
        if (!strcmp(b, item)) { ok = 1; break; }
    }
    fclose(f);
    if (!ok) { fprintf(stderr, "rmmc_pick_op: %s is not a hotbar slot\n", item); return 2; }
    char dir[PATH_MAX + 32], p[PATH_MAX + 48], t[PATH_MAX + 56]; snprintf(dir, sizeof dir, "%s/play", book); mkdir(dir, 0755);
    snprintf(p, sizeof p, "%s/pick.txt", dir); snprintf(t, sizeof t, "%s.tmp", p);
    FILE *o = fopen(t, "w"); if (!o) return 6; fprintf(o, "%s\n", item); if (fclose(o) || rename(t, p)) { remove(t); return 6; }
    printf("picked %s\n", item); return 0;
}
