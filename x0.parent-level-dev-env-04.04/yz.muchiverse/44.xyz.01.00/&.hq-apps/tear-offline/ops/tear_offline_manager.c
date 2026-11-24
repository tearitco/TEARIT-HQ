#define _GNU_SOURCE
/* tear_offline_manager - <module> of tear-offline.xhtpm (X11-HQ window under the "network" cell: the in-house twin of the tearit web chain page, owner 2026-11-24).
 * argv: <house_root> <package_dir>      or      --open <house_root>   (the taskbar launcher: starts the shared renderer on tear-offline.xhtpm, once).
 * ALL chain rules live in ops/+x/tear_chain_op.+x (C port of docs/chain.js, graded against it by harness case tear_offline). This file only:
 *   - keeps the local copy of the chain at <pkg>/data/web_chain.txt (a plain file the user can copy, import, export),
 *   - turns window clicks (<pkg>/tear_action.txt, written by tear_item_op) into op calls, and
 *   - publishes <pkg>/tear_ui.txt (the vars the layout binds).  Idle cost: sleeps in 50 ms polls, no timers, no network except the explicit FETCH click.
 * Arena (2026-11-24): AOPEN:1v1|2v2|ffa AJOIN AFILL AFIGHT ALEAVE ANEXT, each one row of the chain through tear_chain_op. Commands: TAB:x MINE FAUCET SEND:to amount POST:[channel ]text IMPORT:path EXPORT FETCH WALLET:name REFRESH.  Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/tear_offline_manager.+x tear_offline_manager.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#define PL 1024
static char house[PL], pkg[PL], chainf[PL], wallet[48] = "me", cur_tab[16] = "chain", result[400] = "ready", chain_line[300] = "", bal_s[32] = "0";
static char msgs[40][300]; static int nmsg; static char blks[14][200]; static int nblk;
/* the ranch: every pet of the chain (parsed from `tear_chain_op pets`), my pets = owner is this wallet label; sel = the selected pet */
typedef struct { char name[32], owner[32], species[16], stats[40], rest[200]; int age, life, fat, x, y, frozen, wins, cups, best; long coins; } MyPet;
static MyPet pets_[32]; static int npets_; static char sel[32] = ""; static char gridl[8][16]; static char pl1[200] = "", pl2[200] = "", pl3[200] = "";

/* the arena: every arena of the chain (parsed from tear_chain_op arenas): id|mode|owner|open|pot|members|winners|scores|pet records */
typedef struct { char id[32], mode[8], owner[32], members[200], winners[100], scores[100]; int open; long pot; } MyArena;
static MyArena ar_[16]; static int nar_; static char asel[32] = ""; static char agrid[8][32]; static char al1[200] = "", al2[260] = "", al3[260] = "", al4[260] = "";
static void clean_id(char *s) { char *o = s; for (; *s; s++) if (isalnum((unsigned char)*s) || *s == '_' || *s == '-') *o++ = *s; *o = 0; }
static void clean_line(char *s) { for (; *s; s++) if (*s == '|' || *s == ';' || *s == '\n' || *s == '\r' || *s == '=') *s = ' '; }
/* run an op: argv[0]=binary; stdout (first 8 KB) into out; returns exit code */
static int run(char *const av[], char *out, size_t os) {
    int p[2]; if (pipe(p)) return 99; pid_t c = fork();
    if (c == 0) { dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t r; while (n + 1 < os && (r = read(p[0], out + n, os - n - 1)) > 0) n += (size_t)r; out[n] = 0; close(p[0]);
    int st = 0; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 98; }
static void op(char *out, size_t os, int *code, const char *a1, const char *a2, const char *a3, const char *a4, const char *a5) {
    char bin[PL]; snprintf(bin, sizeof bin, "%s/ops/+x/tear_chain_op.+x", pkg);
    char *av[8]; int k = 0; av[k++] = bin; if (a1) av[k++] = (char *)a1; if (a2) av[k++] = (char *)a2; if (a3) av[k++] = (char *)a3; if (a4) av[k++] = (char *)a4; if (a5) av[k++] = (char *)a5; av[k] = NULL;
    *code = run(av, out, os); }
static void first_line(char *s) { char *e = strchr(s, '\n'); if (e) *e = 0; }
static void read_pets(void) {
    char big[16384]; int c; npets_ = 0; op(big, sizeof big, &c, "pets", chainf, NULL, NULL, NULL); if (c != 0) { return; }
    for (char *p = big; *p && npets_ < 32;) { char *e = strchr(p, '\n'); if (e) { *e = 0; } if (*p) { char *fl[17]; int k = 0; fl[k++] = p; for (char *q = p; *q && k < 17; q++) { if (*q == '|') { *q = 0; fl[k++] = q + 1; } }
            if (k >= 17) { MyPet *m = &pets_[npets_++]; memset(m, 0, sizeof *m); snprintf(m->name, 32, "%s", fl[0]); snprintf(m->owner, 32, "%s", fl[1]); snprintf(m->species, 16, "%s", fl[2]); sscanf(fl[3], "%d/%d", &m->age, &m->life); m->fat = atoi(fl[4]); sscanf(fl[5], "%d,%d", &m->x, &m->y); snprintf(m->stats, 40, "%s", fl[6]); sscanf(fl[7], "%d/%d", &m->wins, &m->cups); m->best = atoi(fl[8]); m->frozen = atoi(fl[11]); m->coins = atol(fl[15]); } }
        if (!e) { break; } p = e + 1; }
    int mine = -1; for (int i = 0; i < npets_; i++) { if (!strcmp(pets_[i].owner, wallet) && !pets_[i].frozen && (mine < 0 || !strcmp(pets_[i].name, sel))) { mine = i; } }
    if (mine >= 0) { snprintf(sel, sizeof sel, "%s", pets_[mine].name); } else { sel[0] = 0; }
    for (int y = 0; y < 8; y++) { for (int x = 0; x < 12; x++) { gridl[y][x] = (x == 1 && y == 1) ? 'G' : (x == 10 && y == 1) ? 'B' : '.'; } gridl[y][12] = 0; }
    for (int i = 0; i < npets_; i++) { MyPet *m = &pets_[i]; if (m->frozen || m->x < 0 || m->x > 11 || m->y < 0 || m->y > 7) { continue; } gridl[m->y][m->x] = !strcmp(m->name, sel) ? '@' : (char)(m->name[0] >= 'a' && m->name[0] <= 'z' ? m->name[0] - 32 : m->name[0]); }
    pl1[0] = pl2[0] = pl3[0] = 0; for (int i = 0; i < npets_; i++) { MyPet *m = &pets_[i]; if (strcmp(m->name, sel)) { continue; } const char *rk[] = { "E", "D", "C", "B", "A", "S" };
        snprintf(pl1, sizeof pl1, "%s the %s  \xc2\xb7  age %d/%d  \xc2\xb7  tired %d  \xc2\xb7  %ld coins", m->name, m->species, m->age, m->life, m->fat, m->coins); snprintf(pl2, sizeof pl2, "LIF POW INT SKI SPD DEF = %s", m->stats);
        snprintf(pl3, sizeof pl3, "ranked wins %d of %d  \xc2\xb7  best rank beaten %s  \xc2\xb7  cell %d,%d", m->wins, m->cups, m->best >= 0 ? rk[m->best] : "none", m->x, m->y); }
}
static void read_arenas(void) {
    char big[8192]; int c; nar_ = 0; op(big, sizeof big, &c, "arenas", chainf, NULL, NULL, NULL); if (c != 0) { return; }
    for (char *p = big; *p && nar_ < 16;) { char *e = strchr(p, 10); if (e) { *e = 0; } if (*p) { char *fl[9]; int k = 0; fl[k++] = p; for (char *q = p; *q && k < 9; q++) { if (*q == 124) { *q = 0; fl[k++] = q + 1; } }
            if (k >= 8) { MyArena *a = &ar_[nar_++]; memset(a, 0, sizeof *a); snprintf(a->id, 32, "%s", fl[0]); snprintf(a->mode, 8, "%s", fl[1]); snprintf(a->owner, 32, "%s", fl[2]); a->open = atoi(fl[3]); a->pot = atol(fl[4]); snprintf(a->members, 200, "%s", fl[5]); snprintf(a->winners, 100, "%s", fl[6]); snprintf(a->scores, 100, "%s", fl[7]); } }
        if (!e) { break; } p = e + 1; }
    int found = -1, lastopen = -1; for (int i = 0; i < nar_; i++) { if (!strcmp(ar_[i].id, asel)) { found = i; } if (ar_[i].open) { lastopen = i; } }
    if (found < 0) { found = lastopen >= 0 ? lastopen : nar_ - 1; } if (found >= 0) { snprintf(asel, sizeof asel, "%s", ar_[found].id); } else { asel[0] = 0; }
    for (int y = 0; y < 8; y++) { for (int x = 0; x < 24; x++) { agrid[y][x] = 46; } agrid[y][24] = 0; }
    al1[0] = al2[0] = al3[0] = al4[0] = 0; if (found < 0) { return; }
    static const int SP1[2][2] = { { 3, 11 }, { 20, 11 } }, SP2[4][2] = { { 3, 6 }, { 20, 6 }, { 3, 16 }, { 20, 16 } }, SPF[8][2] = { { 3, 3 }, { 12, 2 }, { 20, 3 }, { 21, 11 }, { 20, 19 }, { 12, 20 }, { 3, 19 }, { 2, 11 } };
    MyArena *a = &ar_[found]; char mem[200]; snprintf(mem, sizeof mem, "%s", a->members); int j = 0; for (char *t = strtok(mem, ","); t && j < 8; t = strtok(NULL, ","), j++) { int cx = !strcmp(a->mode, "1v1") ? SP1[j % 2][0] : !strcmp(a->mode, "2v2") ? SP2[j % 4][0] : SPF[j][0], cy = !strcmp(a->mode, "1v1") ? SP1[j % 2][1] : !strcmp(a->mode, "2v2") ? SP2[j % 4][1] : SPF[j][1]; int row = cy * 8 / 22; if (row > 7) { row = 7; } agrid[row][cx] = (!strncmp(t, "ai-", 3)) ? 42 : (char)(t[0] >= 97 && t[0] <= 122 ? t[0] - 32 : t[0]); }
    snprintf(al1, sizeof al1, "arena %s  \xc2\xb7  %s  \xc2\xb7  pot %ld  \xc2\xb7  %s  \xc2\xb7  owner %s", a->id, a->mode, a->pot, a->open ? "open" : "finished", a->owner);
    snprintf(al2, sizeof al2, "fighters: %s", a->members[0] ? a->members : "(none yet)"); if (a->winners[0]) { snprintf(al3, sizeof al3, "winners: %s  \xc2\xb7  scores %s", a->winners, a->scores); } else { snprintf(al3, sizeof al3, "no result yet: join, fill with default fighters, then FIGHT"); }
    snprintf(al4, sizeof al4, "%d arena(s) on this chain  \xc2\xb7  * default fighter, letters pets", nar_); }
static void refresh(void) {
    char o[8192]; int c; op(o, sizeof o, &c, "verify", chainf, NULL, NULL, NULL); first_line(o);
    if (!o[0]) { snprintf(o, sizeof o, "OK height=0"); }
    if (c == 0) { long h = 0; char tip[80] = "", wk[32] = ""; sscanf(o, "OK height=%ld tip=%79s work=%31s", &h, tip, wk); tip[12] = 0; snprintf(chain_line, sizeof chain_line, "verified: %ld blocks, tip %s, work %s", h, h ? tip : "(none)", wk); } else { snprintf(chain_line, sizeof chain_line, "DOES NOT VERIFY: %s", o); }
    op(o, sizeof o, &c, "balance", chainf, wallet, NULL, NULL); first_line(o); snprintf(bal_s, sizeof bal_s, "%s", c == 0 ? o : "0");
    char big[16384]; nmsg = 0; op(big, sizeof big, &c, "messages", chainf, "*", NULL, NULL);
    if (c == 0) { char *p = big; char *last[40]; int nl = 0; while (*p) { char *e = strchr(p, '\n'); if (e) *e = 0; if (*p) { if (nl == 40) { memmove(last, last + 1, sizeof(char *) * 39); nl--; } last[nl++] = p; } if (!e) { break; } p = e + 1; }
        for (int i = nl - 1; i >= 0; i--) { char *t = strchr(last[i], '\t'); if (t) *t = 0; snprintf(msgs[nmsg++], 300, "%s: %s", last[i], t ? t + 1 : ""); } }
    read_pets(); read_arenas();
    nblk = 0; op(big, sizeof big, &c, "tail", chainf, "12", NULL, NULL);
    if (c == 0) { char *p = big; char *l[14]; int nl = 0; while (*p) { char *e = strchr(p, '\n'); if (e) *e = 0; if (*p && nl < 14) l[nl++] = p; if (!e) { break; } p = e + 1; } for (int i = nl - 1; i >= 0; i--) snprintf(blks[nblk++], 200, "%s", l[i]); } }
static void write_ui(void) {
    char tmp[PL + 16], dst[PL + 16]; snprintf(dst, sizeof dst, "%s/tear_ui.txt", pkg); snprintf(tmp, sizeof tmp, "%s/tear_ui.txt.tmp", pkg); FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "wallet_id=%s\nbalance=%s\nstatus=%s   ·   wallet %s   ·   %s mc   ·   offline copy\nresult=%s\nchain_line=%s\nchain_path=%s\n", wallet, bal_s, chain_line, wallet, bal_s, result, chain_line, chainf);
    const char *tabs[] = { "chain", "wallet", "send", "messages", "explorer", "sync", "ranch", "arena" };
    for (int i = 0; i < 8; i++) { int on = !strcmp(cur_tab, tabs[i]); fprintf(f, "tab_%s=%s\ncls_%s=%s\n", tabs[i], on ? "1" : "", tabs[i], on ? "tab-active" : ""); }
    int sm = !strcmp(cur_tab, "messages"), se = !strcmp(cur_tab, "explorer"); fprintf(f, "n_msgs=%d\n", sm ? nmsg : 0); if (sm) for (int i = 0; i < nmsg; i++) fprintf(f, "m_%d_text=%s\n", i, msgs[i]);
    fprintf(f, "msgs_empty=%s\n", (sm && !nmsg) ? "no messages yet: post one above" : ""); fprintf(f, "n_blks=%d\n", se ? nblk : 0); if (se) for (int i = 0; i < nblk; i++) fprintf(f, "b_%d_text=%s\n", i, blks[i]);
    fprintf(f, "n_grid=%d\n", !strcmp(cur_tab, "ranch") ? 8 : 0); if (!strcmp(cur_tab, "ranch")) { for (int i = 0; i < 8; i++) { fprintf(f, "g_%d_text=%s\n", i, gridl[i]); } }
    fprintf(f, "n_agrid=%d\n", !strcmp(cur_tab, "arena") ? 8 : 0); if (!strcmp(cur_tab, "arena")) { for (int i = 0; i < 8; i++) { fprintf(f, "ag_%d_text=%s\n", i, agrid[i]); } }
    fprintf(f, "arena_l1=%s\narena_l2=%s\narena_l3=%s\narena_l4=%s\n", asel[0] ? al1 : "no arena yet: open one below", al2, al3, al4);
    fprintf(f, "pet_line1=%s\npet_line2=%s\npet_line3=%s\n", sel[0] ? pl1 : "no pet yet: type a name and words below to hatch one", pl2, pl3); fprintf(f, "sel_pet=%s\n", sel);
    fprintf(f, "blks_empty=%s\n", (se && !nblk) ? "the chain is empty: mine a block on the Wallet tab" : ""); fclose(f); rename(tmp, dst); }
static void set_result(const char *s) { snprintf(result, sizeof result, "%s", s); clean_line(result); }
static void do_cmd(char *cmd) {
    char o[8192]; int c; time_t now = time(NULL); char ts[24]; snprintf(ts, sizeof ts, "%ld", (long)now);
    if (!strncmp(cmd, "TAB:", 4)) { snprintf(cur_tab, sizeof cur_tab, "%s", cmd + 4); clean_id(cur_tab); }
    else if (!strcmp(cmd, "MINE")) { op(o, sizeof o, &c, "mine", chainf, wallet, ts, NULL); first_line(o); set_result(o); }
    else if (!strcmp(cmd, "FAUCET")) { char row[200]; snprintf(row, sizeof row, "FAUCET|%s|1000|%s", wallet, ts); op(o, sizeof o, &c, "mine", chainf, wallet, ts, row); first_line(o); set_result(c == 0 ? "faucet claimed (1000)" : "faucet refused (cooldown or cap)"); }
    else if (!strncmp(cmd, "SEND:", 5)) { char to[64] = "", amt[24] = ""; if (sscanf(cmd + 5, "%63s %23s", to, amt) == 2) { clean_id(to); long a = atol(amt); if (to[0] && a > 0) { char row[300]; snprintf(row, sizeof row, "TX|%s|%s|%ld|%s|%s-%s", wallet, to, a, ts, wallet, ts); op(o, sizeof o, &c, "mine", chainf, wallet, ts, row); set_result(c == 0 ? "sent" : "refused: not enough balance"); } else set_result("need: <to-wallet> <amount>"); } else set_result("need: <to-wallet> <amount>"); }
    else if (!strncmp(cmd, "POST:", 5)) { char chn[24] = "general", *t = cmd + 5; char w1[24]; int n = 0; if (sscanf(t, "%23s %n", w1, &n) >= 1 && (!strcmp(w1, "general") || !strcmp(w1, "forum") || !strcmp(w1, "irc"))) { snprintf(chn, sizeof chn, "%s", w1); t += n; }
        char text[300]; snprintf(text, sizeof text, "%.280s", t); clean_line(text); if (text[0]) { char row[400]; snprintf(row, sizeof row, "MSG|%s|%s|%s|%s", wallet, chn, text, ts); op(o, sizeof o, &c, "mine", chainf, wallet, ts, row); set_result(c == 0 ? "posted" : "refused"); } }
    else if (!strncmp(cmd, "IMPORT:", 7)) { char *p = cmd + 7; while (*p == ' ') p++; char out[PL + 8]; snprintf(out, sizeof out, "%s.merged", chainf); op(o, sizeof o, &c, "merge", chainf, p, out, NULL); first_line(o);
        if (c == 0) { rename(out, chainf); set_result(o); } else set_result(o[0] ? o : "could not read that file"); }
    else if (!strcmp(cmd, "EXPORT")) { char dir[PL + 8], dst[PL + 40]; snprintf(dir, sizeof dir, "%s/export", pkg); mkdir(dir, 0755); snprintf(dst, sizeof dst, "%s/tearit-web-chain-%ld.txt", dir, (long)now); FILE *a = fopen(chainf, "rb"), *b = fopen(dst, "wb"); if (a && b) { char buf[4096]; size_t r; while ((r = fread(buf, 1, sizeof buf, a)) > 0) fwrite(buf, 1, r, b); snprintf(o, sizeof o, "exported to %s", dst); set_result(o); } else set_result("export failed"); if (a) fclose(a); if (b) fclose(b); }
    else if (!strcmp(cmd, "FETCH")) { char got[PL + 8], out[PL + 8]; snprintf(got, sizeof got, "%s/data/published.tmp", pkg); snprintf(out, sizeof out, "%s.merged", chainf); char *av[] = { "/usr/bin/curl", "-fsS", "-m", "15", "-o", got, "https://tearitco.github.io/tearit-web/chain/latest.txt", NULL }; c = run(av, o, sizeof o);
        if (c != 0) set_result("no connection: you are offline, your copy is unchanged"); else { op(o, sizeof o, &c, "merge", chainf, got, out, NULL); first_line(o); if (c == 0) { rename(out, chainf); set_result(o); } else set_result(o); } unlink(got); }
    else if (!strcmp(cmd, "PFAUCET")) { if (!sel[0]) { set_result("hatch a pet first"); } else { char row[200]; snprintf(row, sizeof row, "FAUCET|%s|1000|%s", sel, ts); op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); set_result(c == 0 ? "the pet claimed 1000 from the faucet" : "faucet refused (once an hour)"); } }
    else if (!strncmp(cmd, "HATCH:", 6)) { char nmv[32] = "", words[64] = ""; if (sscanf(cmd + 6, "%31s %63[^\n]", nmv, words) == 2) { clean_id(nmv); clean_line(words); char row[300]; snprintf(row, sizeof row, "PET|%s|%.40s|%s|%s", nmv, words, wallet, ts); op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); first_line(o); if (c == 0) { snprintf(sel, sizeof sel, "%s", nmv); set_result("hatched"); } else { set_result(o[0] ? o : "could not hatch (name taken, or six pets are already out)"); } } else { set_result("need: <pet-name> <words>"); } }
    else if (!strncmp(cmd, "FEED:", 5) || !strncmp(cmd, "TRAIN:", 6)) { int feed = cmd[0] == 'F'; const char *what = cmd + (feed ? 5 : 6); if (!sel[0]) { set_result("hatch a pet first"); } else { char row[300]; snprintf(row, sizeof row, "%s|%s|%.20s|%s|%s", feed ? "FEED" : "TRAIN", sel, what, wallet, ts); op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); first_line(o); char msgb[300]; snprintf(msgb, sizeof msgb, "%s", c == 0 ? (feed ? "fed" : "trained") : "refused (not enough coins, too tired, or retired)"); set_result(msgb); } }
    else if (!strcmp(cmd, "REST") || !strcmp(cmd, "RANK") || !strcmp(cmd, "NEXTPET")) { if (!sel[0]) { set_result("hatch a pet first"); } else if (!strcmp(cmd, "NEXTPET")) { int cur = -1, nx = -1; for (int i = 0; i < npets_; i++) { if (!strcmp(pets_[i].name, sel)) { cur = i; } } for (int k = 1; k <= npets_; k++) { int i = (cur + k) % npets_; if (!strcmp(pets_[i].owner, wallet) && !pets_[i].frozen) { nx = i; break; } } if (nx >= 0) { snprintf(sel, sizeof sel, "%s", pets_[nx].name); } set_result("next pet"); }
        else { char row[300]; if (!strcmp(cmd, "REST")) { snprintf(row, sizeof row, "REST|%s|%s|%s", sel, wallet, ts); } else { int nb = 0; for (int i = 0; i < npets_; i++) { if (!strcmp(pets_[i].name, sel)) { nb = pets_[i].best + 1; } } const char *rk[] = { "E", "D", "C", "B", "A", "S" }; if (nb > 5) { nb = 5; } snprintf(row, sizeof row, "COMPETE|%s|%s|%s|%s", sel, rk[nb], wallet, ts); }
            long w0 = 0; for (int i = 0; i < npets_; i++) { if (!strcmp(pets_[i].name, sel)) { w0 = pets_[i].wins; } } op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); if (c != 0) { first_line(o); set_result("refused (too tired, not enough coins, or retired)"); } else if (!strcmp(cmd, "REST")) { set_result("rested"); } else { refresh(); long w1 = 0; for (int i = 0; i < npets_; i++) { if (!strcmp(pets_[i].name, sel)) { w1 = pets_[i].wins; } } set_result(w1 > w0 ? "won the ranked fight" : "lost the ranked fight: train and try again"); } } }
    else if (!strncmp(cmd, "AOPEN:", 6)) { const char *md = cmd + 6; if (strcmp(md, "1v1") && strcmp(md, "2v2") && strcmp(md, "ffa")) { set_result("unknown mode"); } else { char id[32]; int n = 1; for (;;) { snprintf(id, sizeof id, "arena%d", n); int used = 0; for (int i = 0; i < nar_; i++) { if (!strcmp(ar_[i].id, id)) { used = 1; } } if (!used) { break; } n++; } char row[300]; snprintf(row, sizeof row, "ARENA|%s|%s|%s|%s", id, md, wallet, ts); op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); first_line(o); if (c == 0) { snprintf(asel, sizeof asel, "%s", id); set_result("arena opened"); } else { set_result(o[0] ? o : "could not open an arena"); } } }
    else if (!strcmp(cmd, "AJOIN") || !strcmp(cmd, "AFILL") || !strcmp(cmd, "AFIGHT") || !strcmp(cmd, "ALEAVE")) { if (!asel[0]) { set_result("open an arena first"); } else if (!strcmp(cmd, "AJOIN") && !sel[0]) { set_result("hatch a pet first (Ranch tab)"); } else { char row[300]; if (!strcmp(cmd, "AJOIN")) { snprintf(row, sizeof row, "JOIN|%s|%s|%s|%s", asel, sel, wallet, ts); } else if (!strcmp(cmd, "ALEAVE")) { snprintf(row, sizeof row, "LEAVE|%s|%s|%s|%s", asel, sel, wallet, ts); } else if (!strcmp(cmd, "AFILL")) { snprintf(row, sizeof row, "FILL|%s|%s|%s", asel, wallet, ts); } else { snprintf(row, sizeof row, "FIGHT|%s|%s|%s", asel, wallet, ts); }
            op(o, sizeof o, &c, "mine", chainf, "_ranch", ts, row); first_line(o); if (c == 0) { set_result(!strcmp(cmd, "AJOIN") ? "joined (entry fee 20 paid)" : !strcmp(cmd, "ALEAVE") ? "left, the entry fee came back" : !strcmp(cmd, "AFILL") ? "default fighters stepped in" : "the fight is over: see the winners above"); } else { set_result(o[0] ? o : "refused: see the rules"); } } }
    else if (!strcmp(cmd, "ANEXT")) { int cur = -1; for (int i = 0; i < nar_; i++) { if (!strcmp(ar_[i].id, asel)) { cur = i; } } if (nar_ > 0) { snprintf(asel, sizeof asel, "%s", ar_[(cur + 1) % nar_].id); } set_result("next arena"); }
    else if (!strncmp(cmd, "WALLET:", 7)) { char w[48]; snprintf(w, sizeof w, "%.40s", cmd + 7); clean_id(w); if (w[0]) { snprintf(wallet, sizeof wallet, "%s", w); set_result("wallet name set (a label: signatures come later)"); } }
    refresh(); write_ui(); }
static void poll(int *last) {
    char p[PL + 24]; snprintf(p, sizeof p, "%s/tear_action.txt", pkg); FILE *f = fopen(p, "r"); if (!f) return; char buf[2048]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    int seq = 0; char cmd[1024] = ""; for (char *l = buf; *l;) { char *e = strchr(l, '\n'); if (e) *e = 0; if (!strncmp(l, "seq=", 4)) seq = atoi(l + 4); else if (!strncmp(l, "cmd=", 4)) snprintf(cmd, sizeof cmd, "%s", l + 4); if (!e) { break; } l = e + 1; }
    if (seq > *last && cmd[0]) { *last = seq; do_cmd(cmd); } }
static void bye(int s) { (void)s; _exit(0); }
/* --open: the taskbar launcher. One window: if the renderer already runs on this layout, do nothing. Registered through the house funnel kh_spawn.sh. */
/* the process IS the renderer: argv[0] ends with khtpm_core_render.+x and an argument is the tear-offline layout (a shell whose command TEXT mentions both must not count) */
static int pid_is_window(const char *pid) { char p[64], b[4096]; snprintf(p, sizeof p, "/proc/%s/cmdline", pid); int fd = open(p, O_RDONLY); if (fd < 0) { return 0; } ssize_t n = read(fd, b, sizeof b - 1); close(fd); if (n <= 0) { return 0; } b[n] = 0; size_t l = strlen(b); const char *t = "khtpm_core_render.+x"; size_t tl = strlen(t); if (l < tl || strcmp(b + l - tl, t)) { return 0; } for (ssize_t i = (ssize_t)l + 1; i < n; i += (ssize_t)strlen(b + i) + 1) { if (strstr(b + i, "tear-offline.xhtpm")) { return 1; } } return 0; }
static int open_window(const char *h) {
    char x[PL], bin[PL], spawn[PL], logd[PL]; char self[PL]; ssize_t r = readlink("/proc/self/exe", self, sizeof self - 1); if (r <= 0) return 1; self[r] = 0; char *sl = strrchr(self, '/'); if (sl) *sl = 0;  /* .../ops/+x */
    snprintf(x, sizeof x, "%s/../../tear-offline.xhtpm", self); char rx[4096]; if (!realpath(x, rx)) { fprintf(stderr, "tear-offline: layout missing\n"); return 1; }
    snprintf(bin, sizeof bin, "%s/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x", h); if (access(bin, X_OK)) { fprintf(stderr, "tear-offline: renderer not built\n"); return 1; }
    DIR *d = opendir("/proc"); struct dirent *e; while (d && (e = readdir(d))) if (isdigit((unsigned char)e->d_name[0]) && pid_is_window(e->d_name)) { closedir(d); printf("tear-offline already open\n"); return 0; } if (d) closedir(d);
    snprintf(spawn, sizeof spawn, "%s/&.widgits/_shared-lib/kh_spawn.sh", h); snprintf(logd, sizeof logd, "%s/../../audit", self); mkdir(logd, 0755);
    pid_t c = fork(); if (c == 0) { setenv("KH_ROLE", "tool", 1); setenv("KH_APP", "tear-offline", 1); setenv("KH_LIFETIME", "keep", 1); char lg[PL + 32]; snprintf(lg, sizeof lg, "%s/tear-offline.log", logd); int fd = open(lg, O_WRONLY | O_CREAT | O_APPEND, 0644); if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); } execl("/bin/sh", "sh", spawn, h, "0", "tear-offline", "--", bin, h, rx, (char *)NULL); _exit(127); }
    int st = 0; waitpid(c, &st, 0); printf("tear-offline launched\n"); return WIFEXITED(st) ? WEXITSTATUS(st) : 1; }
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--open")) return open_window(argv[2]);
    if (argc < 3) { fprintf(stderr, "usage: %s <house_root> <package_dir>   |   --open <house_root>\n", argv[0]); return 2; }
    snprintf(house, sizeof house, "%s", argv[1]); snprintf(pkg, sizeof pkg, "%s", argv[2]); prctl(PR_SET_PDEATHSIG, SIGTERM); signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
    char dd[PL + 8]; snprintf(dd, sizeof dd, "%s/data", pkg); mkdir(dd, 0755); snprintf(chainf, sizeof chainf, "%s/data/web_chain.txt", pkg); { FILE *t = fopen(chainf, "a"); if (t) fclose(t); }
    const char *u = getenv("USER"); if (u && *u) { snprintf(wallet, sizeof wallet, "%.40s", u); clean_id(wallet); if (!wallet[0]) snprintf(wallet, sizeof wallet, "me"); }
    char ap[PL + 24]; snprintf(ap, sizeof ap, "%s/tear_action.txt", pkg); { FILE *f = fopen(ap, "w"); if (f) { fprintf(f, "seq=0\ncmd=\n"); fclose(f); } } { char sp[PL + 16]; snprintf(sp, sizeof sp, "%s/tear.seq", pkg); FILE *f = fopen(sp, "w"); if (f) { fprintf(f, "0\n"); fclose(f); } }
    refresh(); write_ui(); int last = 0; for (;;) { usleep(50000); poll(&last); } }
