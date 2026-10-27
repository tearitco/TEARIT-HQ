/* life_op - the life layer's slow work as an OP (owner 2026-10-27: shared C should be ops/events; hot paths do no work, an op does it on a slow cadence; life "may use weights and should be
 * able to evolve": needs, rates and effects are DATA, not constants in C). Slice 1 of GAMES-AS-PACKAGES-AND-STORE-DESIGN.md section 18: the hunger/starvation clock of petslike_life.c.
 *
 * Usage:  life_op tick <state_dir> <app_dir>
 *   reads   <app_dir>/life.pdl      LIFE | enabled | 1   LIFE | hunger_ms | 60000   LIFE | starve_hp | 1      (same rows the game reads)
 *           <state_dir>/life_weights.txt   key=int lines, per-mille multipliers: hunger_rate_pm (1000 = normal; bounded 100..4000). THE hook for learned/evolved weights: a validated edit
 *                                          (bank, concept-bank edit, a person) changes this file; nothing in C changes. Missing file or key = 1000.
 *   keeps   <state_dir>/life_clock.txt     `<id>=<last_step_unix_ms>` lines (op-owned; the game's life_<id>.txt format is untouched)
 *   for every <state_dir>/life_<id>.txt that is not dead: when now - last >= hunger_ms * 1000 / hunger_rate_pm, ONE step: hunger+1 (to 100), at 100 hp -= starve_hp (the same rule the C had).
 *   It never kills: hp <= 0 is left for the game, which already runs the death page/effect when it reloads the file (events/death, parallel trigger).
 *   Every change appends one row to <state_dir>/life_ops.txt:  LIFE|<unix>|<id>|hunger|<old>|<new>   or   LIFE|<unix>|<id>|starve|<hp old>|<hp new>   (the ledger an evolving need learns from).
 * Prints  changed=<n>.  Exit 0 ok | 2 usage.  One-shot, no polling.  Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/life_op.+x life_op.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#define PL 4096
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return (long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static long kv_get(const char *path, const char *key, long dflt) { FILE *f = fopen(path, "r"); if (!f) return dflt; char ln[256]; size_t kl = strlen(key); long v = dflt;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, key, kl) && ln[kl] == '=') { v = atol(ln + kl + 1); break; } fclose(f); return v; }
int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "tick")) { fprintf(stderr, "usage: life_op tick <state_dir> <app_dir>\n"); return 2; }
    const char *ST = argv[2], *APP = argv[3]; char p[PL + 64], ln[400];
    long on = 1, hunger_ms = 60000, starve = 1;
    snprintf(p, sizeof p, "%s/life.pdl", APP); FILE *f = fopen(p, "r");
    if (f) { while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; char *fld[4] = {0}; int n = 0; char *s = ln, *t; while (n < 4 && (t = strsep(&s, "|"))) fld[n++] = trim(t);
            if (n < 3 || strcmp(fld[0], "LIFE")) continue; if (!strcmp(fld[1], "enabled")) on = atol(fld[2]); else if (!strcmp(fld[1], "hunger_ms")) hunger_ms = atol(fld[2]); else if (!strcmp(fld[1], "starve_hp")) starve = atol(fld[2]); }
        fclose(f); }
    if (!on || hunger_ms <= 0) { printf("changed=0\n"); return 0; }
    snprintf(p, sizeof p, "%s/life_weights.txt", ST); long rate = kv_get(p, "hunger_rate_pm", 1000); if (rate < 100) rate = 100; if (rate > 4000) rate = 4000;
    long interval = hunger_ms * 1000 / rate, now = now_ms();
    /* the op-owned clock */
    char clk[64][48]; long clv[64]; int ncl = 0; snprintf(p, sizeof p, "%s/life_clock.txt", ST); f = fopen(p, "r");
    if (f) { while (ncl < 64 && fgets(ln, sizeof ln, f)) { char *e = strchr(ln, '='); if (!e) continue; *e = 0; snprintf(clk[ncl], sizeof clk[0], "%s", ln); clv[ncl++] = atol(e + 1); } fclose(f); }
    DIR *d = opendir(ST); if (!d) { printf("changed=0\n"); return 0; } struct dirent *de; int changed = 0, clk_dirty = 0;
    while ((de = readdir(d))) {
        size_t L = strlen(de->d_name); if (L < 10 || strncmp(de->d_name, "life_", 5) || strcmp(de->d_name + L - 4, ".txt")) continue;
        char id[40]; snprintf(id, sizeof id, "%.*s", (int)(L - 9), de->d_name + 5); if (!strcmp(id, "weights") || !strcmp(id, "clock") || !strcmp(id, "ops") || !strcmp(id, "actor")) continue;
        int ix = -1; for (int i = 0; i < ncl; i++) if (!strcmp(clk[i], id)) { ix = i; break; }
        if (ix < 0 && ncl < 64) { ix = ncl++; snprintf(clk[ix], sizeof clk[0], "%s", id); clv[ix] = now; clk_dirty = 1; }
        if (ix < 0 || now - clv[ix] < interval) continue;
        char fp[PL + 64]; snprintf(fp, sizeof fp, "%s/%s", ST, de->d_name); f = fopen(fp, "r"); if (!f) continue;
        char lines[24][400]; int nl = 0, hp = 40, hunger = 0, dead = 0; while (nl < 24 && fgets(lines[nl], sizeof lines[0], f)) { char *l = lines[nl]; if (!strncmp(l, "hp=", 3)) hp = atoi(l + 3); else if (!strncmp(l, "hunger=", 7)) hunger = atoi(l + 7); else if (!strncmp(l, "dead=", 5)) dead = atoi(l + 5); nl++; }
        fclose(f); clv[ix] = now; clk_dirty = 1; if (dead) continue;
        int oh = hunger, ohp = hp; if (hunger < 100) hunger++; else hp -= (int)starve;
        char tp[PL + 80]; snprintf(tp, sizeof tp, "%s.tmp.%d", fp, (int)getpid()); FILE *o = fopen(tp, "w"); if (!o) continue;
        for (int i = 0; i < nl; i++) { if (!strncmp(lines[i], "hp=", 3)) fprintf(o, "hp=%d\n", hp); else if (!strncmp(lines[i], "hunger=", 7)) fprintf(o, "hunger=%d\n", hunger); else fputs(lines[i], o); }
        fclose(o); rename(tp, fp); changed++;
        snprintf(p, sizeof p, "%s/life_ops.txt", ST); f = fopen(p, "a"); if (f) { if (hunger != oh) fprintf(f, "LIFE|%ld|%s|hunger|%d|%d\n", (long)time(NULL), id, oh, hunger); else fprintf(f, "LIFE|%ld|%s|starve|%d|%d\n", (long)time(NULL), id, ohp, hp); fclose(f); }
    }
    closedir(d);
    if (clk_dirty) { char tp[PL + 80]; snprintf(p, sizeof p, "%s/life_clock.txt", ST); snprintf(tp, sizeof tp, "%s.tmp.%d", p, (int)getpid()); FILE *o = fopen(tp, "w"); if (o) { for (int i = 0; i < ncl; i++) fprintf(o, "%s=%ld\n", clk[i], clv[i]); fclose(o); rename(tp, p); } }
    printf("changed=%d\n", changed); return 0;
}
