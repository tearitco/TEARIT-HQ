#define _GNU_SOURCE
/* pet_net_say - a pet chooses what to say on the IRC network FROM ITS OWN STATE and posts it through the guard (phase 2, 2026-11-04). The first pet that acts instead of a scripted string.
 *   pet_net_say <house_root> <pet_id> <room>
 * Facts it reads (no writes except its own memory file): <app>/state/pet_<id>.txt (room= x= y= steps=), <app>/state/daylight.txt (phase=), <app>/state/lamps_on.txt (0/1).
 * Rules: <app>/net_say.pdl rows `SAY | <cond> | <text>`; cond = comma-joined `key=value`, `key>n`, `key<n` over the facts room x y steps phase lamps (no cond = always); text may use {pet} {room} {phase} {x} {y}.
 * The first row whose cond holds AND whose text is not the pet's last posted line wins (memory: <app>/state/net_say_last_<id>.txt, one line). No row wins = it stays quiet (exit 0, prints QUIET): a pet never repeats itself.
 * The post goes ONLY through the guard: <house>/&.hq-apps/irc-chat-hq/ops/+x/pet_post.+x post <room> pet-<id> <text> with PRISC_PROJECT_ROOT = the chat node, so the kill switch, rate cap, length cap, hold mode and audit apply.
 * Env overrides for the harness: PET_SAY_APP, PET_SAY_NODE, PET_SAY_POST. COST: one short process per call, a few small file reads; nothing runs between calls. Exit: 0 posted/held/quiet, 3 the guard refused, 2 usage/no rules. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/wait.h>
static char APP[PATH_MAX];
static char fk[16][24], fv[16][96]; static int nf;
static void setf(const char *k, const char *v) { for (int i = 0; i < nf; i++) if (!strcmp(fk[i], k)) { snprintf(fv[i], 96, "%s", v); return; } if (nf < 16) { snprintf(fk[nf], 24, "%s", k); snprintf(fv[nf], 96, "%s", v); nf++; } }
static const char *getf(const char *k) { for (int i = 0; i < nf; i++) if (!strcmp(fk[i], k)) return fv[i]; return NULL; }
static void kvfile(const char *p, const char *only) { FILE *f = fopen(p, "r"); if (!f) return; char ln[256]; while (fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; char *e = strchr(ln, '='); if (!e) continue; *e = 0; if (!only || !strcmp(ln, only)) setf(ln, e + 1); } fclose(f); }
static int cond_ok(char *c) {
    char tmp[256]; snprintf(tmp, sizeof tmp, "%s", c); char *sp = NULL;
    for (char *t = strtok_r(tmp, ",", &sp); t; t = strtok_r(NULL, ",", &sp)) { while (*t == ' ') t++; char *op = strpbrk(t, "=<>"); if (!op) return 0; char o = *op; *op = 0; char *k = t, *v = op + 1; for (char *e = k + strlen(k); e > k && e[-1] == ' '; ) *--e = 0; while (*v == ' ') v++; for (char *e = v + strlen(v); e > v && e[-1] == ' '; ) *--e = 0;
        const char *have = getf(k); if (!have) return 0;
        if (o == '=') { if (strcmp(have, v)) return 0; } else { long a = atol(have), b = atol(v); if (o == '<' ? !(a < b) : !(a > b)) return 0; } }
    return 1;
}
static void fill(char *out, size_t cap, const char *tpl, const char *pet) {
    size_t o = 0; for (const char *p = tpl; *p && o + 2 < cap; ) { if (*p == '{') { const char *e = strchr(p, '}'); if (e) { char nm[24]; size_t n = (size_t)(e - p - 1); if (n < sizeof nm) { memcpy(nm, p + 1, n); nm[n] = 0; const char *v = !strcmp(nm, "pet") ? pet : getf(nm); if (v) { o += (size_t)snprintf(out + o, cap - o, "%s", v); p = e + 1; continue; } } } } out[o++] = *p++; } out[o] = 0;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: pet_net_say <house_root> <pet_id> <room>\n"); return 2; }
    const char *house = argv[1], *pet = argv[2]; char roomb[96]; snprintf(roomb, sizeof roomb, "%s", argv[3]); const char *room = roomb; const char *e = getenv("PET_SAY_APP"); if (e && e[0]) snprintf(APP, sizeof APP, "%s", e); else snprintf(APP, sizeof APP, "%s/@.apps/rpg-edit-hq", house);
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/state/pet_%s.txt", APP, pet); kvfile(p, NULL); snprintf(p, sizeof p, "%s/state/daylight.txt", APP); kvfile(p, "phase");
    { snprintf(p, sizeof p, "%s/state/lamps_on.txt", APP); FILE *f = fopen(p, "r"); setf("lamps", "0"); if (f) { char b[16]; if (fgets(b, sizeof b, f)) { b[strcspn(b, "\r\n")] = 0; setf("lamps", b); } fclose(f); } }
    snprintf(p, sizeof p, "%s/state/net_say_last_%s.txt", APP, pet); char last[400] = ""; { FILE *f = fopen(p, "r"); if (f) { if (fgets(last, sizeof last, f)) last[strcspn(last, "\r\n")] = 0; fclose(f); } }
    char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/net_say.pdl", APP); FILE *rf = fopen(rp, "r"); if (!rf) { fprintf(stderr, "no rules: %s\n", rp); return 2; }
    if (!strcmp(room, "-")) { snprintf(roomb, sizeof roomb, "lobby"); char l2[300]; while (fgets(l2, sizeof l2, rf)) if (!strncmp(l2, "ROOM", 4)) { char *b = strchr(l2, '|'); if (b) { b++; while (*b == ' ') b++; b[strcspn(b, " \r\n")] = 0; if (*b) snprintf(roomb, sizeof roomb, "%s", b); } } rewind(rf); }      /* room "-" = the ROOM row of the rules file */
    char ln[600], text[400] = ""; int found = 0;
    while (!found && fgets(ln, sizeof ln, rf)) { if (strncmp(ln, "SAY", 3)) continue; ln[strcspn(ln, "\r\n")] = 0; char *a = strchr(ln, '|'); if (!a) continue; char *b = strchr(a + 1, '|'); if (!b) continue; *b = 0; char cond[256]; snprintf(cond, sizeof cond, "%s", a + 1);
        { char *s = cond; while (*s == ' ') s++; memmove(cond, s, strlen(s) + 1); } if (cond[0] && !cond_ok(cond)) continue;
        char *t = b + 1; while (*t == ' ') t++; fill(text, sizeof text, t, pet); if (!strcmp(text, last)) continue; found = 1; }
    fclose(rf); if (!found) { printf("QUIET\n"); return 0; }
    char node[PATH_MAX] = ""; e = getenv("PET_SAY_NODE"); if (e && e[0]) snprintf(node, sizeof node, "%s", e); else { DIR *d = opendir(house); struct dirent *de; if (d) { while ((de = readdir(d))) if (!strncmp(de->d_name, "044.pal-chat-irc", 16)) { snprintf(node, sizeof node, "%s/%s", house, de->d_name); break; } closedir(d); } }
    char post[PATH_MAX]; e = getenv("PET_SAY_POST"); if (e && e[0]) snprintf(post, sizeof post, "%s", e); else snprintf(post, sizeof post, "%s/&.hq-apps/irc-chat-hq/ops/+x/pet_post.+x", house);
    if (!node[0]) { fprintf(stderr, "no chat node\n"); return 2; }
    char pid[80]; snprintf(pid, sizeof pid, "pet-%s", pet); setenv("PRISC_PROJECT_ROOT", node, 1);
    int pf[2]; if (pipe(pf) != 0) return 2; pid_t k = fork(); if (k == 0) { dup2(pf[1], 1); close(pf[0]); execl(post, post, "post", room, pid, text, (char *)NULL); _exit(127); } close(pf[1]);
    char all[1024] = ""; size_t got = 0; ssize_t n; while (got < sizeof all - 1 && (n = read(pf[0], all + got, sizeof all - 1 - got)) > 0) got += (size_t)n; all[got] = 0; while (got > 0 && (all[got - 1] == '\n' || all[got - 1] == '\r')) all[--got] = 0; char *out = strrchr(all, '\n'); out = out ? out + 1 : all; int st = 0;      /* the guard's verdict is its LAST line (the chat op prints "Posted to ..." before it) */ waitpid(k, &st, 0); int rc = WIFEXITED(st) ? WEXITSTATUS(st) : 2;
    printf("%s | %s\n", out[0] ? out : "(no answer)", text);
    if (rc == 0) { snprintf(p, sizeof p, "%s/state/net_say_last_%s.txt", APP, pet); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%s\n", text); fclose(f); } }
    return rc == 0 ? 0 : 3;
}
