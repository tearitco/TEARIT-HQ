/* rmmc_driver - the HUMAN controller of Asa (plan 7.4 and 9.1, gate G5, D137; shape of tearit_driver.c / tear_on_driver.c): tails the key relay of the possessed board and turns each key into the SAME verb pages the beat op plays.
 *   rmmc_driver <house> <relay_file> [--keys keys.pdl] [--keybinds keybinds.pdl] [--feed <game_ui.txt>] [--from-start] [--once] [--owner-pid N]
 * relay_file = <host>/state/interact_relay.txt: lines `<key> <ms>`; arrows 1000 left, 1001 right, 1002 up, 1003 down, space 32 (the engine appends them while a game-driven token is possessed, D97/D118). A byte cursor starts at
 * the END of the file (history is never replayed) unless --from-start; the cursor lives in memory only (the relay is append-only input, nothing here decides state). keys.pdl maps GAMEKEY names to verb + arg (data);
 * keybinds.pdl gives name -> code.
 * ONE controller (the seat is a LEDGER row, `vox_op seat`): while the seat is `harness` every key is refused (exit 24 with --once, a message in the feed otherwise) and NOTHING is written. Verbs are played as pages
 * (rmmc_play.inc: through play_event.sh, or directly with a visible message when the page is missing or the owner paused events in Blueprintz). The pose expiry is OURS: idle_after_ms after the last key the driver re-projects
 * with --idle --no-marker (pattern 0; events only, the engine re-reads events.txt every frame), never a ledger row.
 * The UI feed (--feed) is a PROJECTION written only when its bytes change (temp + rename): who Asa is, where, the seat and the last message. Environment as rmmc_verb: RMMC_LEDGER, RMMC_DESK, RMMC_SEED, RMMC_ACTOR.
 * Loop mode ends when --owner-pid is gone (the house rule: a process the house starts exits with its owner; register it with proc_reg_op / kh_spawn when started for real) or on SIGTERM. --once reads to the end of the file and exits:
 * 0 all keys accepted or ignored, 24 seat is harness, else the code of the first refused verb.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/rmmc_driver.+x rmmc_driver.c */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "rmmc_play.inc"
static volatile sig_atomic_t stop_; static void on_term(int s) { (void)s; stop_ = 1; }
static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static struct { char name[24]; int code; } GK[32]; static int ngk; static struct { char name[24], verb[24], arg[16]; } KV[32]; static int nkv; static int poll_ms = 100, idle_ms = 400;
static int load_keys(const char *kb, const char *kv) { char l[300], *x[6]; FILE *f = fopen(kb, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 3 && !strcmp(x[0], "GAMEKEY") && ngk < 32) { snprintf(GK[ngk].name, 24, "%s", x[1]); GK[ngk++].code = atoi(x[2]); } } fclose(f);
    f = fopen(kv, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 4 && !strcmp(x[0], "KEYVERB") && nkv < 32) { snprintf(KV[nkv].name, 24, "%s", x[1]); snprintf(KV[nkv].verb, 24, "%s", x[2]); snprintf(KV[nkv].arg, 16, "%s", !strcmp(x[3], "-") ? "" : x[3]); nkv++; }
        else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "poll_ms")) poll_ms = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "idle_after_ms")) idle_ms = atoi(x[2]); }
    fclose(f); return ngk && nkv ? 0 : 2; }
static int owner_gone(long pid) {   /* gone = no such process, or a zombie (dead but not yet reaped by its parent) */
    if (kill((pid_t)pid, 0) != 0) return 1;
    char p[64], b[512]; snprintf(p, sizeof p, "/proc/%ld/stat", pid); FILE *f = fopen(p, "r"); if (!f) return 1; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    char *r = strrchr(b, ')'); return r && r[1] == ' ' && r[2] == 'Z'; }
static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }
static int run_out(char *const av[], char *out, size_t osz) { int p[2]; if (pipe(p)) return 99; pid_t c = fork(); if (c < 0) return 99; if (c == 0) { dup2(p[1], 1); dup2(p[1], 2); close(p[0]); close(p[1]); execv(av[0], av); _exit(127); }
    close(p[1]); size_t n = 0; ssize_t k; while (n < osz - 1 && (k = read(p[0], out + n, osz - 1 - n)) > 0) n += (size_t)k; out[n] = 0; close(p[0]); int st = 0; waitpid(c, &st, 0); out[strcspn(out, "\r\n")] = 0; return WIFEXITED(st) ? WEXITSTATUS(st) : 99; }
static void put_if_changed(const char *path, const char *buf, size_t n) {
    char cur[4096]; FILE *f = fopen(path, "r"); if (f) { size_t got = fread(cur, 1, sizeof cur, f); int more = fgetc(f) != EOF; fclose(f); if (!more && got == n && !memcmp(cur, buf, n)) return; }
    char t[4400]; snprintf(t, sizeof t, "%s.tmp", path); f = fopen(t, "w"); if (!f) return; fwrite(buf, 1, n, f); fclose(f); rename(t, path); }
/* The editor chrome of this book (owner 2026-11-25: "we dont need palettes side bar, it can get custom sidebar of its own, can have pals; we still have a hotbar too, that can be used for MC").
 * Feed keys only, no new window code (BOOK 02-architecture/BOARD-SIDEBAR-HOTBAR-AND-SELECTION-UI-2026-11-20.md):
 *   LEFT BAR = the PALS bar, not the palettes: side_pal empty hides the tile palettes, side_left=1 shows the pals bar (party_<i>_*, n_party): Asa now, Terumon later (a data row, not code).
 *   HOTBAR  = the generic bottom strip of the editor (game-bar: gmbar_on, gmbar_title, gmbar_n, gmbar_<i>_label / _act): the Minecraft block slots of hotbar.pdl; a click runs
 *             rmmc_pick_op <item> (a PREFERENCE, play/pick.txt); the picked slot is marked "> ". gm_pop=0 keeps the centred popup away.
 *   gm_on=1 stays (the daemon's liveness and clock-hold read it). All of this is a PROJECTION of the view; the ledger is the only game state. */
static char g_bag[512];   /* "dirt:3,pick:1" from vox_op bag: the hotbar labels show how many of each item Asa holds */
static int bag_count(const char *item) { char t[512]; snprintf(t, sizeof t, "%s", g_bag); for (char *s = strtok(t, ","); s; s = strtok(NULL, ",")) { char *c = strchr(s, ':'); if (c && (size_t)(c - s) == strlen(item) && !strncmp(s, item, strlen(item))) return atoi(c + 1); } return 0; }
static int chrome_feed(char *fb, size_t cap, const char *book, const char *actor, const char *where, const char *seatn, const char *msg) {
    size_t n = 0; char p[PATH_MAX + 64], line[256], pick[64] = "";
    snprintf(p, sizeof p, "%.4000s/play/pick.txt", book); { FILE *f = fopen(p, "r"); if (f) { if (fgets(pick, sizeof pick, f)) pick[strcspn(pick, "\r\n")] = 0; fclose(f); } }
    n += (size_t)snprintf(fb + n, cap - n, "gm_on=1\ngm_title=rmmc\ngmt_0_text=%s: %s\ngmt_1_text=seat %s | %s\ngm_nt=2\ngm_nb=0\ngm_ne=0\ngm_nl=0\n", actor, where, seatn, msg);
    n += (size_t)snprintf(fb + n, cap - n, "side_pal=\nside_left=1\nparty_0_label=> %s\nparty_0_id=rmmc_%s\nparty_0_cls=active\nn_party=1\ncoins=0\nnick_line=%s the builder\nstat_line=%s\nstat_line2=seat %s\n", actor, actor, actor, where, seatn);
    int k = 0; char slots[12000] = ""; size_t sn = 0;
    int page = 0, ri = 0; { char pg[PATH_MAX + 32]; snprintf(pg, sizeof pg, "%.4000s/play/page.txt", book); FILE *pf = fopen(pg, "r"); if (pf) { if (fscanf(pf, "%d", &page) != 1) page = 0; fclose(pf); } if (page < 0) page = 0; }   /* the hotbar shows 9 slots a page and a flip slot: 14 slots are wider than the bar (a PREFERENCE, play/page.txt) */
    snprintf(p, sizeof p, "%.4000s/hotbar.pdl", book); FILE *f = fopen(p, "r");
    while (f && fgets(line, sizeof line, f)) {
        char a[16], item[64], label[64], spr[128] = ""; int nf = sscanf(line, " %15[^| ] | %63[^| ] | %63[^|\n] | %127[^|\n]", a, item, label, spr); if (nf < 3 || strcmp(a, "SLOT")) continue; if (ri++ / 9 != page) continue;
        for (char *e = spr + strlen(spr); e > spr && (e[-1] == ' ' || e[-1] == '\t'); ) *--e = 0;
        for (char *e = label + strlen(label); e > label && (e[-1] == ' ' || e[-1] == '\t'); ) *--e = 0;
        if (sn + 400 > sizeof slots) break;
        char cnt[24] = ""; { int bc = bag_count(item); if (bc > 0) snprintf(cnt, sizeof cnt, "  x%d", bc); }
        char shown[48]; snprintf(shown, sizeof shown, "%s%s", !strcmp(pick, item) ? "> " : "", cnt[0] ? cnt + 2 : "·");   /* count only, as a Minecraft hotbar: a label with the item name made 14 slots wider than the bar */
        sn += (size_t)snprintf(slots + sn, sizeof slots - sn, "gmbar_%d_label=%s%s%s\ngmbar_%d_sprite=%s%s%s\ngmbar_%d_act=sh -c \"'%s/ops/+x/rmmc_pick_op.+x' %s\"\n", k, "", shown, "", k, spr[0] ? book : "", spr[0] ? "/" : "", spr, k, book, item); k++;
    }
    if (f) fclose(f);
    if (ri > 9) sn += (size_t)snprintf(slots + sn, sizeof slots - sn, "gmbar_%d_label=»  more\ngmbar_%d_sprite=\ngmbar_%d_act=sh -c \"'%s/ops/+x/rmmc_pick_op.+x' __page\"\n", k, k, k, book), k++;
    n += (size_t)snprintf(fb + n, cap - n, "gm_pop=0\ngmbar_on=%s\ngmbar_title=Blocks  (picked: %s)\ngmbar_min_act=void\ngmbar_n=%d\n%s", k ? "1" : "", pick[0] ? pick : "none", k, slots);
    return (int)(n < cap ? n : cap - 1);
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: rmmc_driver <house> <relay_file> [--keys F] [--keybinds F] [--feed F] [--from-start] [--once] [--owner-pid N]\n"); return 2; }
    const char *house = argv[1], *relay = argv[2], *keys = NULL, *kbf = NULL, *feed = NULL; int from_start = 0, once = 0; long owner = 0;
    for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i]; else if (!strcmp(argv[i], "--keybinds") && i + 1 < argc) kbf = argv[++i]; else if (!strcmp(argv[i], "--feed") && i + 1 < argc) feed = argv[++i];
        else if (!strcmp(argv[i], "--from-start")) from_start = 1; else if (!strcmp(argv[i], "--once")) once = 1; else if (!strcmp(argv[i], "--owner-pid") && i + 1 < argc) owner = atol(argv[++i]); else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; } }
    static char book[4300], desk[4400], seed[4500], led[4400], vop[4400], data[4400], pop[4400], pose[4400], hm[4400], dkeys[4400], dkb[4400]; const char *e;
    snprintf(book, sizeof book, "%s/@.apps/rmmc", house); snprintf(vop, sizeof vop, "%s/@.apps/vox/ops/+x/vox_op.+x", house); snprintf(data, sizeof data, "%s/vox", book); snprintf(pop, sizeof pop, "%s/ops/+x/rmmc_project_op.+x", book);
    snprintf(pose, sizeof pose, "%s/pose.pdl", book); snprintf(hm, sizeof hm, "%s/vox/terrain.heightmap", book);
    e = getenv("RMMC_DESK"); if (e && *e) snprintf(desk, sizeof desk, "%s", e); else snprintf(desk, sizeof desk, "%s/rmmc-pchq/pieces/system/maps/rmmc/rmmc-0001", book);
    e = getenv("RMMC_SEED"); if (e && *e) snprintf(seed, sizeof seed, "%s", e); else snprintf(seed, sizeof seed, "%s/seed", desk);
    e = getenv("RMMC_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", house);
    const char *actor = getenv("RMMC_ACTOR"); if (!actor || !*actor) actor = "asa";
    if (!keys) { snprintf(dkeys, sizeof dkeys, "%s/keys.pdl", book); keys = dkeys; } if (!kbf) { snprintf(dkb, sizeof dkb, "%s/rmmc-pchq/pieces/system/keybinds.pdl", book); kbf = dkb; }
    if (load_keys(kbf, keys)) { fprintf(stderr, "rmmc_driver: bad keybinds or keys file\n"); return 2; }
    signal(SIGTERM, on_term); signal(SIGINT, on_term);
    long cur = 0; if (!from_start) { struct stat st; if (!stat(relay, &st)) cur = (long)st.st_size; }
    char msg[500] = "waiting for keys", seatn[96] = "harness", where[320] = ""; long last_key = 0, last_feed = -100000; int idle_pending = 0, first_refusal = 0;
    for (;;) {
        FILE *f = fopen(relay, "r"); char ln[200];
        if (f) { struct stat st; if (!fstat(fileno(f), &st) && st.st_size < cur) cur = 0;   /* the relay was trimmed: start over */
            fseek(f, cur, SEEK_SET); while (fgets(ln, sizeof ln, f)) { if (!strchr(ln, '\n')) break; cur += (long)strlen(ln); int code = 0; long ms = 0; if (sscanf(ln, "%d %ld", &code, &ms) < 1) continue; (void)ms;
                const char *gk = NULL; for (int i = 0; i < ngk; i++) if (GK[i].code == code) gk = GK[i].name; if (!gk) continue;
                int kv = -1; for (int i = 0; i < nkv; i++) if (!strcmp(KV[i].name, gk)) kv = i; if (kv < 0) continue;
                if (!strcmp(KV[kv].verb, "pick")) {   /* the hotbar number keys: a PREFERENCE (play/pick.txt, play/page.txt), never the ledger, so no seat check and no pose; 1..9 pick that slot of the current page, 0 flips the page */
                    char pk[PATH_MAX + 64], o4[200]; snprintf(pk, sizeof pk, "%.4000s/ops/+x/rmmc_pick_op.+x", book);
                    if (!strcmp(KV[kv].arg, "page")) { char *av[] = { pk, "__page", NULL }; run_out(av, o4, sizeof o4); } else { char *av[] = { pk, "__slot", KV[kv].arg, NULL }; run_out(av, o4, sizeof o4); }
                    last_key = now_ms(); last_feed = 0; continue; }
                { char o2[100]; char *av[] = { vop, "seat", data, led, (char *)actor, NULL }; run_out(av, o2, sizeof o2); snprintf(seatn, sizeof seatn, "%s", strncmp(o2, "seat=", 5) ? "harness" : o2 + 5);
                  if (strcmp(seatn, "human")) { snprintf(msg, sizeof msg, "refused: the seat is %s, keys are ignored (exit 24)", seatn); if (!first_refusal) first_refusal = 24; if (once) { fprintf(stderr, "%s\n", msg); fclose(f); return 24; } continue; } }
                int rc = rmmc_play_verb(house, book, KV[kv].verb, KV[kv].arg, "human", msg, sizeof msg); if (rc && !first_refusal) first_refusal = rc; last_key = now_ms(); idle_pending = 1; }
            fclose(f); }
        if (last_key > last_feed || now_ms() - last_feed >= 2000) { last_feed = now_ms(); char o2[300]; char *av[] = { vop, "where", data, led, (char *)actor, NULL }; run_out(av, o2, sizeof o2); snprintf(where, sizeof where, "%s", o2[0] ? o2 : "not spawned"); char o3[100]; char *a3[] = { vop, "seat", data, led, (char *)actor, NULL }; run_out(a3, o3, sizeof o3); snprintf(seatn, sizeof seatn, "%s", strncmp(o3, "seat=", 5) ? "harness" : o3 + 5); { char ob[400]; char *ab[] = { vop, "bag", data, led, (char *)actor, NULL }; run_out(ab, ob, sizeof ob); snprintf(g_bag, sizeof g_bag, "%s", strncmp(ob, "bag=", 4) ? "" : ob + 4); }
        if (feed) { char fb[16384]; int n = chrome_feed(fb, sizeof fb, book, actor, where, seatn, msg); put_if_changed(feed, fb, (size_t)n); } }
        if (once) return first_refusal;
        if (idle_pending && now_ms() - last_key >= idle_ms) { char o2[200]; char *av[] = { pop, "project", seed, desk, led, pose, hm, "--idle", "--no-marker", NULL }; run_out(av, o2, sizeof o2); idle_pending = 0; }
        if (stop_ || (owner > 0 && owner_gone(owner))) return 0;
        usleep((useconds_t)poll_ms * 1000);
    }
}
