/* office_stamp.c - the example HARNZ of the pre-design book (2026-11-14): append one line to <app>/state/office_stamps.txt and keep only the last <limit> lines.
 *   office_stamp <app dir> <page> <limit | @plug:<plug>:<key>> <label | @plug:<plug>:<key>>
 * Plug references are resolved HERE with the shared plug_ref.c (the same code the engine uses: the plug must be ENABLED, the value must pass its type and bounds, else the harnz is refused with the reason and nothing is
 * written; exit 3). The file is a runtime file with a hard cap: limit is 1..200 lines whatever the plug says (about 10 KB at most), rewritten atomically. A page or label that is not a plain word is refused (exit 2).
 * Build: sh ops/build_office_stamp.sh */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../../../&.widgits/_shared-lib/plug_ref.c"

static int word(const char *s) { return plug_ref_word(s, 1); }
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: office_stamp <app dir> <page> <limit|@plug ref> <label|@plug ref>\n"); return 2; }
    const char *app = argv[1], *page = argv[2]; char plugs[4200]; snprintf(plugs, sizeof plugs, "%s/plugs", app);
    char lim_s[200], lab[200]; int rc;
    rc = plug_ref(plugs, argv[3], lim_s, sizeof lim_s); if (rc < 0) { printf("refused: %s\n", lim_s); return 3; } if (rc == 0) snprintf(lim_s, sizeof lim_s, "%s", argv[3]);
    rc = plug_ref(plugs, argv[4], lab, sizeof lab); if (rc < 0) { printf("refused: %s\n", lab); return 3; } if (rc == 0) snprintf(lab, sizeof lab, "%s", argv[4]);
    char *e; long limit = strtol(lim_s, &e, 10); if (*lim_s == 0 || *e != 0) { printf("refused: the limit is not a number\n"); return 2; } if (limit < 1) limit = 1; if (limit > 200) limit = 200;
    if (!word(page) || !word(lab)) { printf("refused: the page and the label must be plain words\n"); return 2; }
    char dir[4200], f[4300], t[4310]; snprintf(dir, sizeof dir, "%s/state", app); mkdir(dir, 0755); snprintf(f, sizeof f, "%s/office_stamps.txt", dir); snprintf(t, sizeof t, "%s.tmp", f);
    static char keep[200][160]; int n = 0; FILE *fp = fopen(f, "r"); char ln[512];
    while (fp && fgets(ln, sizeof ln, fp)) { ln[strcspn(ln, "\r\n")] = 0; if (!ln[0]) continue; if (n == 200) { memmove(keep[0], keep[1], sizeof keep[0] * 199); n = 199; } snprintf(keep[n++], sizeof keep[0], "%s", ln); }
    if (fp) fclose(fp);
    time_t now = time(NULL); struct tm tm; localtime_r(&now, &tm); char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    if (n == 200) { memmove(keep[0], keep[1], sizeof keep[0] * 199); n = 199; } snprintf(keep[n++], sizeof keep[0], "%s  %s  page %s", ts, lab, page);
    int from = n > limit ? (int)(n - limit) : 0; FILE *w = fopen(t, "w"); if (!w) { printf("refused: cannot write %s\n", t); return 1; }
    for (int i = from; i < n; i++) fprintf(w, "%s\n", keep[i]); fclose(w); if (rename(t, f)) { printf("refused: cannot replace %s\n", f); return 1; }
    printf("stamped %s (%d of at most %ld lines)\n", page, n - from, limit); return 0;
}
