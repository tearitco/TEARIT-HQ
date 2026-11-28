/* civ_driver - the HUMAN controller of civ-snes (D157; the shape of rmmc_driver.c / tearit_driver.c): tails the editor's key relay and turns each key into a VERB PAGE of the one rule op (civ_op), or, before a game exists, into a
 * SETUP action. It holds no rule and no number of its own: keys come from keys.pdl (KEYVERB rows while a game runs, SETUPKEY rows in the setup), key codes from the book's keybinds.pdl, speeds from keys.pdl RULE rows.
 *   civ_driver <house> <relay_file> [--keys F] [--keybinds F] [--feed F] [--editor-pkg DIR] [--lock F] [--owner-pid N] [--from-start] [--once]
 * relay_file = <editor>/state/interact_relay.txt, lines `<code> <ms>` (the engine appends them while a game-driven token, the rig:civ piece, is possessed). The byte cursor starts at the END unless --from-start; it lives in memory only.
 * MODES (derived from the ledger every pass, never stored): no ledger (or empty) = SETUP: the draft play/setup.txt (a PREFERENCE) is changed by `civ_op setup`, the desk shows `civ_op preview` (the map the choices would make, all civs' starts), Space = the `start` verb page = ONE
 * NEW ledger row; a ledger = PLAY: arrows move the selected unit (a PREFERENCE, play/sel.txt), B founds, 1 / 2 build, Space ends the turn, N picks the next unit, all as the event pages of civ_verb (through play_event.sh, or directly with a visible note when
 * the page is missing or the owner paused events in Blueprintz). The camera keys Y U I O P are never bound (D152). The left bar (--feed) is a PROJECTION written only when its bytes change.
 * Env (harness / scratch houses): CIV_LEDGER, CIV_DESK. The loop ends when --owner-pid is gone (zombie counts as gone) or on SIGTERM. --once reads to the end of the relay, handles it and exits (0 = every key accepted or ignored, else the first refusal's code).
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/civ_driver.+x civ_driver.c */
#define _DEFAULT_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"   /* paths are bounded by P and checked by the open calls; a truncated path simply fails to open */
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define P 4400
static volatile sig_atomic_t stop_; static void on_term(int s) { (void)s; stop_ = 1; }
static int split(char *line, char **f, int max) { int n = 0; char *s = line; while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; } for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; } return n; }
static struct { char name[24]; int code; } GK[48]; static int ngk;
static struct { char name[24], verb[24], arg[24]; } KV[48]; static int nkv; static struct { char name[24], act[24], arg[24]; } SK[24]; static int nsk;
static int poll_ms = 100, idle_poll_ms = 0, step_ms = 120;
static int load_keys(const char *kb, const char *kv) { char l[300], *x[6]; FILE *f = fopen(kb, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 3 && !strcmp(x[0], "GAMEKEY") && ngk < 48) { snprintf(GK[ngk].name, 24, "%s", x[1]); GK[ngk++].code = atoi(x[2]); } } fclose(f);
    f = fopen(kv, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6);
        if (n >= 4 && !strcmp(x[0], "KEYVERB") && nkv < 48) { snprintf(KV[nkv].name, 24, "%s", x[1]); snprintf(KV[nkv].verb, 24, "%s", x[2]); snprintf(KV[nkv].arg, 24, "%s", !strcmp(x[3], "-") ? "" : x[3]); nkv++; }
        else if (n >= 4 && !strcmp(x[0], "SETUPKEY") && nsk < 24) { snprintf(SK[nsk].name, 24, "%s", x[1]); snprintf(SK[nsk].act, 24, "%s", x[2]); snprintf(SK[nsk].arg, 24, "%s", !strcmp(x[3], "-") ? "" : x[3]); nsk++; }
        else if (n >= 3 && !strcmp(x[0], "RULE")) { if (!strcmp(x[1], "poll_ms")) poll_ms = atoi(x[2]); else if (!strcmp(x[1], "idle_poll_ms")) idle_poll_ms = atoi(x[2]); else if (!strcmp(x[1], "step_ms")) step_ms = atoi(x[2]); } }
    fclose(f); return ngk && (nkv || nsk) ? 0 : 2; }
static int owner_gone(long pid) { if (kill((pid_t)pid, 0) != 0) return 1; char p[64], b[512]; snprintf(p, sizeof p, "/proc/%ld/stat", pid); FILE *f = fopen(p, "r"); if (!f) return 1; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0; char *r = strrchr(b, ')'); return r && r[1] == ' ' && r[2] == 'Z'; }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int run_out(char *const av[], char *out, size_t osz) { int p[2]; if (pipe(p)) return 99; pid_t c = fork(); if (c < 0) return 99;
    if (c == 0) { alarm(30); dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]); int st = 0; waitpid(c, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static void put_if_changed(const char *path, const char *buf, size_t n) { static char cur[20000]; FILE *f = fopen(path, "r"); if (f) { size_t got = fread(cur, 1, sizeof cur, f); int more = fgetc(f) != EOF; fclose(f); if (!more && got == n && !memcmp(cur, buf, n)) return; }
    char t[P + 8]; snprintf(t, sizeof t, "%s.tmp", path); f = fopen(t, "w"); if (!f) return; fwrite(buf, 1, n, f); fclose(f); rename(t, path); }
/* value of key= in a multi-line op answer; out is empty when absent */
static void kv(const char *text, const char *key, char *out, size_t osz) { out[0] = 0; size_t kl = strlen(key); const char *s = text; while (s && *s) { if (!strncmp(s, key, kl) && s[kl] == '=') { snprintf(out, osz, "%.*s", (int)strcspn(s + kl + 1, "\n"), s + kl + 1); return; } const char *nl = strchr(s, '\n'); s = nl ? nl + 1 : NULL; } }
/* tell the engine to redraw (it only redraws on a change): one byte to <session>/pieces/display/bv_screen_changed.txt, the session read from the editor board's ui.txt */
static void bump_view(const char *pkg) { if (!pkg || !pkg[0]) return; char p[P + 160], ln[P + 40], ses[P] = ""; snprintf(p, sizeof p, "%.4000s/../board/state/ui.txt", pkg); FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) if (!strncmp(ln, "bv_session=", 11)) { snprintf(ses, sizeof ses, "%.4000s", ln + 11); ses[strcspn(ses, "\r\n")] = 0; }
    fclose(f); if (!ses[0]) return; snprintf(p, sizeof p, "%.4000s/pieces/display/bv_screen_changed.txt", ses); FILE *m = fopen(p, "a"); if (m) { fputc('.', m); fclose(m); } }
static char house[P], book[P], opb[P], verbb[P], led[P], desk[P], draft[P], selp[P], tut[P], play_dir[P];
static char sel[24] = "";
static int ledger_started(void) { struct stat st; return stat(led, &st) == 0 && st.st_size > 0; }
static void sel_save(void) { FILE *f = fopen(selp, "w"); if (f) { fprintf(f, "%s\n", sel); fclose(f); } }
static void sel_load(void) { sel[0] = 0; FILE *f = fopen(selp, "r"); if (f) { if (fgets(sel, sizeof sel, f)) sel[strcspn(sel, "\r\n")] = 0; fclose(f); } }
/* the selected unit must be a live unit of civ1: else the first one with moves (nextunit), else none (a PREFERENCE, never a fact) */
static void sel_fix(int force_next) { char o[200], *av1[] = { opb, led, "units", NULL }; char units[4000]; run_out(av1, units, sizeof units); int ok = 0; char pat[40]; snprintf(pat, sizeof pat, "unit %s civ1 ", sel);
    if (sel[0] && strstr(units, pat)) ok = 1;
    if (!ok || force_next) { char *av[] = { opb, led, "nextunit", "civ1", ok ? sel : (char *)"", NULL }; if (!ok) av[4] = NULL; int rc = run_out(av, o, sizeof o); o[strcspn(o, "\r\n")] = 0; if (!rc && strncmp(o, "none", 4)) snprintf(sel, sizeof sel, "%s", o); else if (!ok) sel[0] = 0; sel_save(); } }
static void project_desk(void) { char o[300]; if (ledger_started()) { char *av[] = { opb, led, "project", "civ1", desk, sel[0] ? sel : NULL, NULL }; run_out(av, o, sizeof o); } else { char *av[] = { opb, "-", "preview", draft, desk, NULL }; run_out(av, o, sizeof o); } }
/* one verb as its EVENT PAGE (play_event.sh), or directly through civ_verb with a visible note when the page is missing or events are paused in Blueprintz (as rmmc_play.inc) */
static int play_verb(const char *verb, const char *args, char *msg, size_t msz) {
    char pd[P], pl[P], rp[P], bp[P], bl[64], rb[400] = ""; int held = 0, ran = 0, code = 0; struct stat ps; const char *ov = getenv("BLUEPRINTZ_DEBUG_FILE");
    snprintf(pd, sizeof pd, "%s/events/%s", book, verb); snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", house); snprintf(rp, sizeof rp, "%s/result.txt", play_dir);
    snprintf(bp, sizeof bp, "%s", ov ? ov : ""); if (!ov) snprintf(bp, sizeof bp, "%s/#.desktop/blueprintz_debug.txt", house);
    { FILE *bf = fopen(bp, "r"); if (bf) { while (fgets(bl, sizeof bl, bf)) if (!strncmp(bl, "paused=1", 8)) held = 1; fclose(bf); } }
    setenv("GAME_ARG", args, 1); setenv("CIV_LEDGER", led, 1); setenv("CIV_DESK", desk, 1); if (sel[0]) setenv("CIV_SEL", sel, 1); else unsetenv("CIV_SEL");
    if (!held && !stat(pd, &ps) && !access(pl, R_OK)) { char *av[] = { "/bin/bash", pl, pd, house, "ui", NULL }, o[200]; mkdir(play_dir, 0755); unlink(rp); run_out(av, o, sizeof o); FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); } ran = rb[0] != 0; }
    if (!ran) { char *av[] = { verbb, (char *)verb, NULL }, o[300]; code = run_out(av, o, sizeof o); FILE *rf = fopen(rp, "r"); if (rf) { if (fgets(rb, sizeof rb, rf)) rb[strcspn(rb, "\r\n")] = 0; fclose(rf); } if (!rb[0]) snprintf(rb, sizeof rb, "%s", code ? "refused" : "ok");
        if (!strcmp(rb, "ok")) snprintf(msg, msz, "%s ok (direct%s)", verb, held ? ", events paused" : ""); else snprintf(msg, msz, "%s", rb); }
    else { if (!strcmp(rb, "ok")) { code = 0; snprintf(msg, msz, "%s ok", verb); } else { const char *x = strstr(rb, "[exit "); code = x ? atoi(x + 6) : 1; snprintf(msg, msz, "%s", rb); } }
    unsetenv("GAME_ARG"); return code; }
static char msg[500] = "waiting for keys"; static const char HINT[] = "ready"; static long g_last_step = 0;
/* the first city of civ1 (the build key's target): the city of the selection, else the first */
static int first_city(char *id, size_t n) { char o[3000], *av[] = { opb, led, "cities", NULL }; run_out(av, o, sizeof o); char *s = o; while (s && *s) { char cid[24], cv[8]; if (sscanf(s, "city %23s %7s", cid, cv) == 2 && !strcmp(cv, "civ1")) { snprintf(id, n, "%s", cid); return 1; } char *nl = strchr(s, '\n'); s = nl ? nl + 1 : NULL; } return 0; }
static int game_key(const char *name) { int kv_ = -1; for (int i = 0; i < nkv; i++) if (!strcmp(KV[i].name, name)) kv_ = i; if (kv_ < 0) return 0;
    const char *verb = KV[kv_].verb, *arg = KV[kv_].arg; char args[100] = "";
    if (!strcmp(verb, "select")) { sel_fix(1); snprintf(msg, sizeof msg, sel[0] ? "selected %s" : "no unit has moves", sel); project_desk(); return 0; }
    if (!strcmp(verb, "move") && step_ms > 0) { long tn = now_ms(); if (g_last_step && tn - g_last_step < step_ms) return 0; }
    sel_fix(0);
    if (!strcmp(verb, "move") || !strcmp(verb, "found")) { if (!sel[0]) { snprintf(msg, sizeof msg, "no unit selected: press M"); return 1; } snprintf(args, sizeof args, "%s %s", sel, arg); }
    else if (!strcmp(verb, "build")) { char cid[24]; if (!first_city(cid, sizeof cid)) { snprintf(msg, sizeof msg, "you have no city yet: found one with B"); return 1; } snprintf(args, sizeof args, "%s %s", cid, arg); }
    else if (!strcmp(verb, "endturn")) args[0] = 0; else snprintf(args, sizeof args, "%s", arg);
    int rc = play_verb(verb, args, msg, sizeof msg); if (!strcmp(verb, "move")) g_last_step = now_ms();
    sel_fix(0); if (!rc) { char *pv[] = { opb, led, "project", "civ1", desk, sel[0] ? sel : NULL, NULL }, o[200]; run_out(pv, o, sizeof o); } return rc; }
static int setup_key(const char *name, const char *pkg) { (void)pkg; int sk = -1; for (int i = 0; i < nsk; i++) if (!strcmp(SK[i].name, name)) sk = i; if (sk < 0) return 0; char o[3000];
    if (!strcmp(SK[sk].act, "start")) { int rc = play_verb("start", "", msg, sizeof msg); if (!rc) { sel_fix(0); snprintf(msg, sizeof msg, "game started"); project_desk(); } return rc; }
    if (!strcmp(SK[sk].act, "random")) { char sd[24]; snprintf(sd, sizeof sd, "%ld", now_ms() % 100000000L); char *av[] = { opb, "-", "setup", draft, "random", sd, NULL }; run_out(av, o, sizeof o); }
    else { char *av[] = { opb, "-", "setup", draft, SK[sk].act, SK[sk].arg[0] ? SK[sk].arg : NULL, NULL }; int rc = run_out(av, o, sizeof o); if (rc) { snprintf(msg, sizeof msg, "%s", o); msg[strcspn(msg, "\r\n")] = 0; return rc; } }
    snprintf(msg, sizeof msg, "setup: %s", SK[sk].act); project_desk(); return 0; }
static void make_feed(const char *feed) {
    char fb[16000], so[4000], lo[600]; size_t n = 0; int started = ledger_started(); char civ[48] = "", nick[120] = "", st1[200] = "", st2[200] = "", nextl[200] = "";
    if (started) { sel_fix(0); char *av[] = { opb, led, "ui", "civ1", sel[0] ? sel : (char *)"", NULL }; if (!sel[0]) av[4] = NULL; run_out(av, so, sizeof so); kv(so, "civ", civ, sizeof civ); char tn[16]; kv(so, "turn", tn, sizeof tn); snprintf(nick, sizeof nick, "%.20s T%s", civ, tn); kv(so, "stat_line", st1, sizeof st1); kv(so, "next", nextl, sizeof nextl); }
    else { char *av[] = { opb, "-", "setup", draft, "show", NULL }; run_out(av, so, sizeof so); kv(so, "nick_line", nick, sizeof nick); kv(so, "stat_line", st1, sizeof st1); }
    { char *av[] = { opb, led, "lesson", tut, "civ1", NULL }; run_out(av, lo, sizeof lo); }
    char goal[220], label[80], done[8]; kv(lo, "goal", goal, sizeof goal); kv(lo, "label", label, sizeof label); kv(lo, "done", done, sizeof done);
    const char *sline = msg; if (!strcmp(msg, HINT) || !strcmp(msg, "waiting for keys")) sline = goal[0] ? goal : (started ? nextl : st1);
    (void)done; snprintf(st2, sizeof st2, "%s", started ? (label[0] ? label : st1) : st1);
    n += (size_t)snprintf(fb + n, sizeof fb - n, "gm_on=1\ngm_pid=%ld\ngm_title=civ-snes\ngmt_0_text=%s\ngmt_1_text=%s\ngmt_2_text=%s\ngm_nt=3\ngm_nb=0\ngm_ne=0\ngm_nl=0\n", (long)getpid(), nick, st1, goal);
    n += (size_t)snprintf(fb + n, sizeof fb - n, "side_pal=\nside_left=1\nnick_line=%s\nstat_line=%s\nstat_line2=%s\n", nick, sline, st2);
    { const char *s = so; while (s && *s) { if (!strncmp(s, "party_", 6) || !strncmp(s, "n_party=", 8)) n += (size_t)snprintf(fb + n, sizeof fb - n, "%.*s\n", (int)strcspn(s, "\n"), s); const char *nl = strchr(s, '\n'); s = nl ? nl + 1 : NULL; } }
    n += (size_t)snprintf(fb + n, sizeof fb - n, "coins=0\ngm_pop=0\ngmbar_on=\ngmbar_n=0\n");
    put_if_changed(feed, fb, n); }
static long file_size(const char *p) { struct stat st; return stat(p, &st) ? -1 : (long)st.st_size; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: civ_driver <house> <relay_file> [--keys F] [--keybinds F] [--feed F] [--editor-pkg DIR] [--lock F] [--owner-pid N] [--from-start] [--once]\n"); return 2; }
    const char *relay = argv[2], *keys = NULL, *kbf = NULL, *feed = NULL, *pkg = NULL, *lockf = NULL; int from_start = 0, once = 0; long owner = 0; snprintf(house, sizeof house, "%s", argv[1]);
    for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i]; else if (!strcmp(argv[i], "--keybinds") && i + 1 < argc) kbf = argv[++i]; else if (!strcmp(argv[i], "--feed") && i + 1 < argc) feed = argv[++i];
        else if (!strcmp(argv[i], "--from-start")) from_start = 1; else if (!strcmp(argv[i], "--once")) once = 1; else if (!strcmp(argv[i], "--owner-pid") && i + 1 < argc) owner = atol(argv[++i]);
        else if (!strcmp(argv[i], "--editor-pkg") && i + 1 < argc) pkg = argv[++i]; else if (!strcmp(argv[i], "--lock") && i + 1 < argc) lockf = argv[++i]; else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; } }
    snprintf(book, sizeof book, "%s/@.apps/civ-snes", house); snprintf(opb, sizeof opb, "%s/ops/+x/civ_op.+x", book); snprintf(verbb, sizeof verbb, "%s/ops/+x/civ_verb.+x", book); snprintf(play_dir, sizeof play_dir, "%s/play", book);
    snprintf(draft, sizeof draft, "%s/play/setup.txt", book); snprintf(selp, sizeof selp, "%s/play/sel.txt", book); snprintf(tut, sizeof tut, "%s/tutorial.pdl", book);
    const char *e = getenv("CIV_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/civ-snes/game/ledger.txt", house);
    e = getenv("CIV_DESK"); if (e && *e) snprintf(desk, sizeof desk, "%s", e); else snprintf(desk, sizeof desk, "%s/civ-snes-pchq/pieces/system/maps/civ-snes/civ-0001", book);
    if (!owner) { const char *g = getenv("GAME_OWNER_PID"); if (g && *g) owner = atol(g); }
    if (lockf) { int lfd = open(lockf, O_CREAT | O_RDWR | O_CLOEXEC, 0644); if (lfd < 0 || flock(lfd, LOCK_EX | LOCK_NB)) return 0; }
    char dk[P], dkb[P]; if (!keys) { snprintf(dk, sizeof dk, "%s/keys.pdl", book); keys = dk; } if (!kbf) { snprintf(dkb, sizeof dkb, "%s/civ-snes-pchq/pieces/system/keybinds.pdl", book); kbf = dkb; }
    if (load_keys(kbf, keys)) { fprintf(stderr, "civ_driver: bad keybinds or keys file\n"); return 2; }
    signal(SIGTERM, on_term); signal(SIGINT, on_term); mkdir(play_dir, 0755);
    { char dir[P]; snprintf(dir, sizeof dir, "%s", led); char *s = strrchr(dir, '/'); if (s) { *s = 0; char cmd[P + 16]; snprintf(cmd, sizeof cmd, "%s", dir); for (char *c = cmd + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(cmd, 0755); *c = '/'; } mkdir(cmd, 0755); } }
    sel_load();
    if (!ledger_started() && file_size(draft) < 0) { char sd[24], o[3000]; snprintf(sd, sizeof sd, "%ld", now_ms() % 100000000L); char *av[] = { opb, "-", "setup", draft, "random", sd, NULL }; run_out(av, o, sizeof o); }   /* a fresh draft: a random seed, the rest the RULE defaults */
    sel_fix(0); project_desk(); bump_view(pkg);
    long cur = 0; if (!from_start) { struct stat st; if (!stat(relay, &st)) cur = (long)st.st_size; }
    long last_feed = -100000, last_key = 0, seen_led = -2, seen_dr = -2; int first_refusal = 0; char shown[500] = "";
    long shown_at = 0;
    for (;;) {
        FILE *f = fopen(relay, "r"); char ln[200];
        if (f) { struct stat st; if (!fstat(fileno(f), &st) && st.st_size < cur) cur = 0; fseek(f, cur, SEEK_SET);
            while (fgets(ln, sizeof ln, f)) { if (!strchr(ln, '\n')) break; cur += (long)strlen(ln); int code = 0; long ms = 0; if (sscanf(ln, "%d %ld", &code, &ms) < 1) continue; (void)ms;
                const char *gk = NULL; for (int i = 0; i < ngk; i++) if (GK[i].code == code) gk = GK[i].name; if (!gk) continue;
                int rc = ledger_started() ? game_key(gk) : setup_key(gk, pkg); if (rc && !first_refusal) first_refusal = rc;
                { char kt[520]; snprintf(kt, sizeof kt, "key %.20s: %.150s", gk, msg); snprintf(msg, sizeof msg, "%s", kt); }
                last_key = now_ms(); last_feed = 0; bump_view(pkg); }
            fclose(f); }
        if (strcmp(msg, shown)) { snprintf(shown, sizeof shown, "%s", msg); shown_at = now_ms(); } else if (now_ms() - shown_at > 8000 && strcmp(msg, HINT) && strcmp(msg, "waiting for keys")) { snprintf(msg, sizeof msg, "%s", HINT); last_feed = 0; }
        long ls = file_size(led), ds = file_size(draft); int ch = ls != seen_led || ds != seen_dr; seen_led = ls; seen_dr = ds;
        if (feed && (last_feed == 0 || now_ms() - last_feed >= 300) && (last_key > last_feed || ch || now_ms() - last_feed >= 30000)) { last_feed = now_ms(); make_feed(feed); }
        if (once) return first_refusal;
        if (stop_ || (owner > 0 && owner_gone(owner))) return 0;
        { int sl = poll_ms; if (idle_poll_ms > poll_ms && now_ms() - last_key > 3000) sl = idle_poll_ms; usleep((useconds_t)sl * 1000); }
    } }
