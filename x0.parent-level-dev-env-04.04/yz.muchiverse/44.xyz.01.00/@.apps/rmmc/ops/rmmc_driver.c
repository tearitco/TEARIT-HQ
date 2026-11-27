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
#include <fcntl.h>
#include <sys/file.h>
#include "rmmc_play.inc"
static volatile sig_atomic_t stop_; static void on_term(int s) { (void)s; stop_ = 1; }
static int split(char *line, char **f, int max) {
    int n = 0; char *s = line;
    while (n < max) { while (*s == ' ') s++; f[n++] = s; char *b = strchr(s, '|'); if (!b) break; *b = 0; s = b + 1; }
    for (int i = 0; i < n; i++) { char *e = f[i] + strlen(f[i]); while (e > f[i] && isspace((unsigned char)e[-1])) *--e = 0; }
    return n;
}
static struct { char name[24]; int code; } GK[32]; static int ngk; static struct { char name[24], verb[24], arg[16]; } KV[32]; static int nkv; static int idle_poll_ms = 0, g_pick_dirty = 0; static int poll_ms = 100, idle_ms = 400, step_ms = 120, reset_confirm_ms = 4000;
static int load_keys(const char *kb, const char *kv) { char l[300], *x[6]; FILE *f = fopen(kb, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 3 && !strcmp(x[0], "GAMEKEY") && ngk < 32) { snprintf(GK[ngk].name, 24, "%s", x[1]); GK[ngk++].code = atoi(x[2]); } } fclose(f);
    f = fopen(kv, "r"); if (!f) return 2;
    while (fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 4 && !strcmp(x[0], "KEYVERB") && nkv < 32) { snprintf(KV[nkv].name, 24, "%s", x[1]); snprintf(KV[nkv].verb, 24, "%s", x[2]); snprintf(KV[nkv].arg, 16, "%s", !strcmp(x[3], "-") ? "" : x[3]); nkv++; }
        else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "poll_ms")) poll_ms = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "idle_after_ms")) idle_ms = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "reset_confirm_ms")) reset_confirm_ms = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "idle_poll_ms")) idle_poll_ms = atoi(x[2]); else if (n >= 3 && !strcmp(x[0], "RULE") && !strcmp(x[1], "step_ms")) step_ms = atoi(x[2]); }
    fclose(f); return ngk && nkv ? 0 : 2; }
static int owner_gone(long pid) {   /* gone = no such process, or a zombie (dead but not yet reaped by its parent) */
    if (kill((pid_t)pid, 0) != 0) return 1;
    char p[64], b[512]; snprintf(p, sizeof p, "/proc/%ld/stat", pid); FILE *f = fopen(p, "r"); if (!f) return 1; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    char *r = strrchr(b, ')'); return r && r[1] == ' ' && r[2] == 'Z'; }
/* key=value from a state file (xelector_01/state.txt, world_01/state.txt); out is empty when the key or the file is missing */
static void kv_get(const char *path, const char *key, char *out, size_t osz) { out[0] = 0; FILE *f = fopen(path, "r"); if (!f) return; char l[300]; size_t kl = strlen(key);
    while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out, osz, "%s", l + kl + 1); out[strcspn(out, "\r\n")] = 0; break; }
    fclose(f); }
/* desks.pdl (data): DESK | <desk id> | <ledger file under state/games/rmmc/game/>. The editor's open page says which row applies; no row = the driver writes nothing for that page. Returns 1 when found. */
static int desk_row(const char *book, const char *id, char *ledger, size_t lsz) { char p[4400], l[300], *x[6]; snprintf(p, sizeof p, "%s/desks.pdl", book); FILE *f = fopen(p, "r"); if (!f) return 0; int hit = 0;
    while (!hit && fgets(l, sizeof l, f)) { int n = split(l, x, 6); if (n >= 3 && !strcmp(x[0], "DESK") && !strcmp(x[1], id)) { snprintf(ledger, lsz, "%s", x[2]); hit = 1; } }
    fclose(f); return hit; }
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
/* Tell the engine to redraw (owner 2026-11-26: Asa walked in the ledger and the picture stayed frozen for minutes, because only the engine's own key handler appended to its change marker). Same append the key handler does: one byte to <session>/pieces/display/bv_screen_changed.txt, the session read from the editor board's ui.txt (bv_session=). Nothing is decided by it. */
static void bump_view(const char *pkg) {
    if (!pkg || !pkg[0]) return;
    char p[PATH_MAX + 160], ln[PATH_MAX + 40], ses[PATH_MAX] = "";
    snprintf(p, sizeof p, "%.4000s/../board/state/ui.txt", pkg);
    FILE *f = fopen(p, "r");
    if (!f) return;
    while (fgets(ln, sizeof ln, f)) {
        if (!strncmp(ln, "bv_session=", 11)) { snprintf(ses, sizeof ses, "%.4000s", ln + 11); ses[strcspn(ses, "\r\n")] = 0; }
    }
    fclose(f);
    if (!ses[0]) return;
    snprintf(p, sizeof p, "%.4000s/pieces/display/bv_screen_changed.txt", ses);
    FILE *m = fopen(p, "a");
    if (m) { fputc('.', m); fclose(m); }
}
/* STOPGAP (owner 2026-11-26, removal: when the engine's token follow, D118, also carries the token's LEVEL): a followed token moves the cursor's cell but not its level, so the third-person camera hung about 14 blocks above Asa and showed only grass. The driver tells the board its actor's level: floor_z of the board manifest + (ledger height - heightmap shift), written to the xelector's pos_z ONLY while that actor is the possessed one, then the redraw nudge. */
/* held.txt: the first-person arm's input (bv_render_3d.c bv_draw_hand). A PROJECTION of the game, rewritten after every key verb: item = the picked item (play/pick.txt), swing = CLOCK_MONOTONIC ms of the last
 * mine or place press (the engine runs the 300 ms swing from it), kind = which. Deleting it hides the arm. Same clock as the engine's, so the two processes agree. */
static long g_swing_ms = 0; static char g_swing_kind[8] = "", g_held_item[64] = "";      /* g_held_item: the item held.txt last said, so a hotbar CLICK (rmmc_pick_op writes only play/pick.txt, no key event) is noticed by held_poll */
static void held_write(const char *pkg, const char *book, const char *actor, const char *verb, int rc) {
    (void)rc; if (!strcmp(verb, "mine") || !strcmp(verb, "place")) {      /* the arm swings on every press, hit or miss, like Minecraft (the hotbar count and the ledger say whether it worked) */ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); g_swing_ms = t.tv_sec * 1000L + t.tv_nsec / 1000000L; snprintf(g_swing_kind, sizeof g_swing_kind, "%s", verb); }
    char pp[PATH_MAX + 64], item[64] = ""; snprintf(pp, sizeof pp, "%.4000s/play/pick.txt", book); FILE *f = fopen(pp, "r"); if (f) { if (fgets(item, sizeof item, f)) item[strcspn(item, "\r\n")] = 0; fclose(f); }
    char skin[24] = "249,193,157", slv[24] = "64,72,67"; { char rp[PATH_MAX + 80], l[200]; snprintf(rp, sizeof rp, "%.4000s/rigs/rig_%.40s.pdl", book, actor); FILE *rf = fopen(rp, "r"); while (rf && fgets(l, sizeof l, rf)) { int r, g, b; char k[16]; if (sscanf(l, " RIG | %15[^| ] | %d %d %d", k, &r, &g, &b) != 4) continue; if (!strcmp(k, "head")) snprintf(skin, sizeof skin, "%d,%d,%d", r, g, b); else if (!strcmp(k, "arm")) snprintf(slv, sizeof slv, "%d,%d,%d", r, g, b); } if (rf) fclose(rf); }      /* the rig file names the real colours: head = skin, arm = sleeve */
    snprintf(g_held_item, sizeof g_held_item, "%s", item);
    char hp[PATH_MAX + 64], want[300]; snprintf(hp, sizeof hp, "%.4000s/pieces/system/held.txt", pkg); snprintf(want, sizeof want, "item=%s\nswing=%ld\nkind=%s\nskin=%s\nsleeve=%s\n", item, g_swing_ms, g_swing_kind, skin, slv);
    FILE *w = fopen(hp, "w"); if (w) { fputs(want, w); fclose(w); } }
/* what a swing did, in words, for the left bar (QA 2026-11-26: the bar said only "key mine: mine"; the engine's answer never reached the player). Ledger only: the last MINE row (broke a block) or DAMAGE row (hit it, total so far) of the actor in the file's tail. */
static void mine_say(const char *led, const char *actor, char *msg, size_t sz) {
    FILE *f = fopen(led, "r"); if (!f) return; fseek(f, 0, SEEK_END); long n = ftell(f); long from = n > 3000 ? n - 3000 : 0; fseek(f, from, SEEK_SET);
    char buf[3100]; size_t got = fread(buf, 1, sizeof buf - 1, f); buf[got] = 0; fclose(f);
    char *last = NULL, *drop = NULL; char *p = buf; while (p && *p) { char *nl = strchr(p, '\n'); if (nl) *nl = 0; if ((!strncmp(p, "MINE |", 6) || !strncmp(p, "DAMAGE |", 8) || !strncmp(p, "PLACE |", 7)) && strstr(p, actor)) { last = p; drop = NULL; } else if (last && !strncmp(p, "DROP |", 6) && strstr(p, actor)) drop = p; p = nl ? nl + 1 : NULL; }
    if (!last) return;
    char cp[300]; snprintf(cp, sizeof cp, "%.299s", last); char *x[10]; int k = 0; char *q = cp; while (k < 10) { x[k++] = q; char *bar = strchr(q, '|'); if (!bar) break; *bar = 0; q = bar + 1; }
    for (int i = 0; i < k; i++) { while (*x[i] == ' ') x[i]++; char *e = x[i] + strlen(x[i]); while (e > x[i] && e[-1] == ' ') *--e = 0; }
    char dk[48] = "", dn[16] = "1"; if (drop) { char dc[200]; snprintf(dc, sizeof dc, "%.199s", drop); char *y[6]; int m = 0; char *r = dc; while (m < 6) { y[m++] = r; char *bar = strchr(r, '|'); if (!bar) break; *bar = 0; r = bar + 1; } for (int i = 0; i < m; i++) { while (*y[i] == ' ') y[i]++; char *e = y[i] + strlen(y[i]); while (e > y[i] && e[-1] == ' ') *--e = 0; } if (m >= 4) { snprintf(dk, sizeof dk, "%.47s", y[2]); snprintf(dn, sizeof dn, "%.15s", y[3]); } }
    if (!strcmp(x[0], "MINE") && k >= 6) { if (dk[0]) snprintf(msg, sz, "broke it: +%s %s", dn, dk); else snprintf(msg, sz, "broke it, no drop"); }      /* the BAG gain is the DROP row after the MINE (a grass block drops dirt: the old "broke grass_top (+1 in the bag)" named the wrong item); each result fits the 2-line, ~20-char bar after "key mine: " (stranger QA 2026-11-27: longer ones were clipped) */
    else if (!strcmp(x[0], "PLACE") && k >= 6) snprintf(msg, sz, "placed %s", x[2]);
    else if (!strcmp(x[0], "DAMAGE") && k >= 8) snprintf(msg, sz, "hit, swing again"); }
/* the hand follows the picked item even when the pick did not come from a key: a mouse click on a hotbar slot only writes play/pick.txt (rmmc_pick_op), so each tick one tiny read compares it with what held.txt last said and rewrites held.txt (and nudges the view: the engine only redraws on a change) when it differs */
static void held_poll(const char *pkg, const char *book, const char *actor) {
    char pp[PATH_MAX + 64], item[64] = ""; snprintf(pp, sizeof pp, "%.4000s/play/pick.txt", book); FILE *f = fopen(pp, "r"); if (f) { if (fgets(item, sizeof item, f)) item[strcspn(item, "\r\n")] = 0; fclose(f); }
    if (strcmp(item, g_held_item)) { held_write(pkg, book, actor, "", 0); bump_view(pkg); g_pick_dirty = 1; } }
/* IDLE COST (owner 2026-11-26: "its costing on idle"): the 2 s bar feed forked four helpers (vox_op where, seat, bag and rmmc_tutorial_op status, each replaying the whole ledger) even when nothing had changed: about 7% of a core at rest, measured. Everything the feed shows derives from the LEDGER and two small files, so it now runs only when one of them changed (the ledger path or size, the tutorial preference size, a pick change) or a key arrived, never more often than every 300 ms (a held pad would run it every step), plus a 30 s safety refresh. Size is the marker, never mtime. */
static int feed_inputs_changed(const char *led, const char *book) {
    static char seen_led[PATH_MAX]; static long seen_sz = -2, seen_tut = -2; struct stat st; long sz = stat(led, &st) ? -1 : (long)st.st_size; char tp[PATH_MAX + 64]; snprintf(tp, sizeof tp, "%.4000s/play/tutorial.txt", book); long tz = stat(tp, &st) ? -1 : (long)st.st_size;
    int ch = strcmp(seen_led, led) || sz != seen_sz || tz != seen_tut; snprintf(seen_led, sizeof seen_led, "%.4000s", led); seen_sz = sz; seen_tut = tz; return ch; }
static void sync_view(const char *pkg, const char *vop, const char *data, const char *led, const char *actor) {
    if (!pkg || !pkg[0]) return;
    char p[PATH_MAX + 160], ln[300], o[200]; int floor_z = -1, shift = 0, ay = 0, ax, az;
    snprintf(p, sizeof p, "%.4000s/pieces/system/board_manifest.txt", pkg);
    FILE *f = fopen(p, "r");
    while (f && fgets(ln, sizeof ln, f)) if (!strncmp(ln, "floor_z=", 8)) floor_z = atoi(ln + 8);
    if (f) fclose(f);
    snprintf(p, sizeof p, "%.4000s/terrain.heightmap", data);
    f = fopen(p, "r");
    while (f && fgets(ln, sizeof ln, f)) if (!strncmp(ln, "META | shift |", 14)) shift = atoi(ln + 14);
    if (f) fclose(f);
    char *av[] = { (char *)vop, "where", (char *)data, (char *)led, (char *)actor, NULL };
    if (floor_z >= 0 && run_out(av, o, sizeof o) == 0 && sscanf(o, "at=%d,%d,%d", &ax, &ay, &az) == 3) {
        char xp[PATH_MAX + 160], want[96]; snprintf(xp, sizeof xp, "%.4000s/pieces/xelector_01/state.txt", pkg); snprintf(want, sizeof want, "possessed_id=rig:%s", actor);
        char buf[4096]; size_t n = 0; int poss = 0, have = 0, same = 0; int z = floor_z + ay - shift; if (z < 1) z = 1;
        f = fopen(xp, "r");
        if (f) {
            char out[4096]; size_t on = 0;
            while (fgets(ln, sizeof ln, f)) {
                if (!strncmp(ln, want, strlen(want)) && (ln[strlen(want)] == '\n' || !ln[strlen(want)])) poss = 1;
                if (!strncmp(ln, "pos_z=", 6)) { have = 1; if (atoi(ln + 6) == z) same = 1; on += (size_t)snprintf(out + on, sizeof out - on, "pos_z=%d\n", z); }
                else on += (size_t)snprintf(out + on, sizeof out - on, "%s", ln);
                if (on > sizeof out - 400) break;
            }
            fclose(f); memcpy(buf, out, on); n = on;
        }
        if (poss && have && !same) { FILE *w = fopen(xp, "w"); if (w) { fwrite(buf, 1, n, w); fclose(w); } }
        if (poss) {   /* the see-through cutaway (view_cut_on) aims at the cell this file names, and it wins over the selector: the game says where the controlled character is (bv_render_3d.c, cut_target.txt). A projection of the ledger position, rewritten only when it changes */
            char cp[PATH_MAX + 160], want2[96], cur[96] = ""; snprintf(cp, sizeof cp, "%.4000s/pieces/system/cut_target.txt", pkg); snprintf(want2, sizeof want2, "x=%d\ny=%d\nroom=rmmc\n", ax, az);
            FILE *cf = fopen(cp, "r"); if (cf) { size_t cn = fread(cur, 1, sizeof cur - 1, cf); cur[cn] = 0; fclose(cf); }
            if (strcmp(cur, want2)) { cf = fopen(cp, "w"); if (cf) { fputs(want2, cf); fclose(cf); } } }
    }
    bump_view(pkg);
}
/* AIM (D147): the engine writes <pkg>/pieces/system/aim.txt in first person when the board says aim=1 (cell=<col>,<row>,<level> face=<dc>,<dr>,<dl>, or cell=none; no file = no aim). The engine's level is
 * relative to the floor (0 = the floor block), so vox y = level + heightmap shift; its column is vox x and its row vox z. Sets RMMC_AIM for rmmc_verb ("x,y,z,fx,fy,fz" in vox cells, or "none"), or
 * clears it so the verb falls back to the column in front. Returns 1 when an aim was set. */
static int aim_env(const char *pkg, const char *data) {
    unsetenv("RMMC_AIM"); if (!pkg || !pkg[0]) return 0;
    char p[PATH_MAX + 160], ln[200]; int cx, cr, cl, fc = 0, fr = 0, fl = 0, have = 0, none = 0, floor_z = -1, shift = 0;
    snprintf(p, sizeof p, "%.4000s/pieces/system/aim.txt", pkg); FILE *f = fopen(p, "r"); if (!f) return 0;
    while (fgets(ln, sizeof ln, f)) { if (sscanf(ln, "cell=%d,%d,%d", &cx, &cr, &cl) == 3) have = 1; else if (!strncmp(ln, "cell=none", 9)) none = 1; else sscanf(ln, "face=%d,%d,%d", &fc, &fr, &fl); }
    fclose(f); if (none) { setenv("RMMC_AIM", "none", 1); return 1; } if (!have) return 0;
    snprintf(p, sizeof p, "%.4000s/pieces/system/board_manifest.txt", pkg); f = fopen(p, "r"); while (f && fgets(ln, sizeof ln, f)) if (!strncmp(ln, "floor_z=", 8)) floor_z = atoi(ln + 8); if (f) fclose(f);
    snprintf(p, sizeof p, "%.4000s/terrain.heightmap", data); f = fopen(p, "r"); while (f && fgets(ln, sizeof ln, f)) if (!strncmp(ln, "META | shift |", 14)) shift = atoi(ln + 14); if (f) fclose(f);
    if (floor_z < 0) return 0;      /* no board manifest: not an editor board, no aim */
    (void)floor_z; char v[96]; snprintf(v, sizeof v, "%d,%d,%d,%d,%d,%d", cx, cl + shift, cr, fc, fl, fr); setenv("RMMC_AIM", v, 1); return 1; }      /* the engine writes the level RELATIVE to the floor (0 = the floor block, as its xelector ring: feet on block level ay-1-shift), so vox y = level + shift */
static long g_last_step = 0;      /* when the last walk step finished (RULE step_ms) */
static const char HINT[] = "ready: the Help menu lists the keys";      /* the idle message line; while a lesson runs the tutorial sentence shows in its place (chrome_feed) */
static const char *g_tut_led = NULL;      /* the ledger the tutorial op reads (set by main; the lesson progress is derived from it, never stored) */
static int chrome_feed(char *fb, size_t cap, const char *book, const char *actor, const char *where, const char *seatn, const char *msg) {
    size_t n = 0; char p[PATH_MAX + 64], line[256], pick[64] = ""; char wh[64]; snprintf(wh, sizeof wh, "%.60s", where); { char *sp = strchr(wh, ' '); if (sp) *sp = 0; }      /* the bar line is only the position: the reason a key did nothing needs the room below it */ char nick[96]; if (!strcmp(seatn, "human")) { snprintf(nick, sizeof nick, "YOU CONTROL %s", actor); for (char *c = nick + 12; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; } else snprintf(nick, sizeof nick, "%s (not controlled)", actor);
    char tshort[128] = "", tgoal[220] = "", tline[220] = "";      /* the in-game tutorial (tutorial.pdl, ops/rmmc_tutorial_op.c): the current step's short text goes to the left bar, its goal sentence to the game feed; off = all empty */
    if (g_tut_led) { char tp[PATH_MAX + 64], to[500] = ""; snprintf(tp, sizeof tp, "%.4000s/ops/+x/rmmc_tutorial_op.+x", book); char *av[] = { tp, (char *)"status", (char *)book, (char *)g_tut_led, (char *)actor, NULL }; run_out(av, to, sizeof to);
        if (!strncmp(to, "step=", 5)) { int si = 0, sn = 0; sscanf(to, "step=%d/%d", &si, &sn); char *sh = strstr(to, " short="), *hv = strstr(to, " have="), *go = strstr(to, " goal="); if (sh && hv && go && sh < hv && hv < go) { int have = 0, need = 1; sscanf(hv, " have=%d need=%d", &have, &need); *hv = 0; char lbl[64]; snprintf(lbl, sizeof lbl, "%.40s", sh + 7); if (need > 1) snprintf(tshort, sizeof tshort, "%d/%d %s %d of %d", si, sn, lbl, have, need); else snprintf(tshort, sizeof tshort, "%d/%d %s", si, sn, lbl);      /* ONE bar line (about 20 chars; rmmc_tutorial_op check holds every step to it): "2/7 arrows 0 of 5" */ snprintf(tgoal, sizeof tgoal, "TUTORIAL: %.200s", go + 6); tgoal[strcspn(tgoal, "\r\n")] = 0; snprintf(tline, sizeof tline, "%.200s", go + 6); tline[strcspn(tline, "\r\n")] = 0; } }
        else if (!strncmp(to, "done=", 5)) { snprintf(tshort, sizeof tshort, "lesson done!"); snprintf(tgoal, sizeof tgoal, "TUTORIAL: lesson complete. Well done."); snprintf(tline, sizeof tline, "Lesson complete. Well done."); } }
    /* STRANGER QA 2026-11-27 (case rmmc_stranger, D146): the goal sentence went only to gmt_2_text, which the editor draws in the centred popup, and this book turns the popup off (gm_pop=0, owner: "the centred popup is ugly"): a player never saw
     * a tutorial sentence. The LEFT BAR's message line now carries it whenever no key result is fresh (a key result shows for 8 s, then the line falls back to the sentence instead of the idle hint). Display only: nothing is decided by it. */
    const char *sline = msg; if (tline[0] && (!strcmp(msg, HINT) || !strcmp(msg, "waiting for keys"))) sline = tline;
    snprintf(p, sizeof p, "%.4000s/play/pick.txt", book); { FILE *f = fopen(p, "r"); if (f) { if (fgets(pick, sizeof pick, f)) pick[strcspn(pick, "\r\n")] = 0; fclose(f); } }
    n += (size_t)snprintf(fb + n, cap - n, "gm_on=1\ngm_pid=%ld\ngm_title=rmmc\ngmt_0_text=%s: %s\ngmt_1_text=seat %s | %s\ngmt_2_text=%s\ngm_nt=%d\ngm_nb=0\ngm_ne=0\ngm_nl=0\n", (long)getpid(), actor, where, seatn, msg, tgoal, tgoal[0] ? 3 : 2);      /* gm_pid: the editor watchdog (petslike_core.c GAME DRIVER WATCHDOG) restarts a DEAD driver only when the feed names its pid; without it a crashed rmmc driver never came back (found 2026-11-27, every other book driver writes it) */
    n += (size_t)snprintf(fb + n, cap - n, "side_pal=\nside_left=1\nparty_0_label=> %s\nparty_0_id=rmmc_%s\nparty_0_cls=active\nn_party=1\ncoins=0\nnick_line=%s\nstat_line=%s\nstat_line2=%s\n", actor, actor, nick, sline, tshort[0] ? tshort : wh);
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
    if (argc < 3) { fprintf(stderr, "usage: rmmc_driver <house> <relay_file> [--keys F] [--keybinds F] [--feed F] [--from-start] [--once] [--owner-pid N] [--editor-pkg DIR] [--lock FILE]\n"); return 2; }
    const char *house = argv[1], *relay = argv[2], *keys = NULL, *kbf = NULL, *feed = NULL, *pkg = NULL, *lockf = NULL; int from_start = 0, once = 0; long owner = 0;
    for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i]; else if (!strcmp(argv[i], "--keybinds") && i + 1 < argc) kbf = argv[++i]; else if (!strcmp(argv[i], "--feed") && i + 1 < argc) feed = argv[++i];
        else if (!strcmp(argv[i], "--from-start")) from_start = 1; else if (!strcmp(argv[i], "--once")) once = 1; else if (!strcmp(argv[i], "--owner-pid") && i + 1 < argc) owner = atol(argv[++i]);
        else if (!strcmp(argv[i], "--editor-pkg") && i + 1 < argc) pkg = argv[++i]; else if (!strcmp(argv[i], "--lock") && i + 1 < argc) lockf = argv[++i]; else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; } }
    static char book[4300], desk[4400], seed[4500], led[4400], vop[4400], data[4400], pop[4400], pose[4400], hm[4400], dkeys[4400], dkb[4400]; const char *e;
    snprintf(book, sizeof book, "%s/@.apps/rmmc", house); snprintf(vop, sizeof vop, "%s/@.apps/vox/ops/+x/vox_op.+x", house); snprintf(data, sizeof data, "%s/vox", book); snprintf(pop, sizeof pop, "%s/ops/+x/rmmc_project_op.+x", book);
    snprintf(pose, sizeof pose, "%s/pose.pdl", book); snprintf(hm, sizeof hm, "%s/vox/terrain.heightmap", book);
    e = getenv("RMMC_DESK"); if (e && *e) snprintf(desk, sizeof desk, "%s", e); else snprintf(desk, sizeof desk, "%s/rmmc-pchq/pieces/system/maps/rmmc/rmmc-0001", book);
    e = getenv("RMMC_SEED"); if (e && *e) snprintf(seed, sizeof seed, "%s", e); else snprintf(seed, sizeof seed, "%s/seed", desk);
    e = getenv("RMMC_LEDGER"); if (e && *e) snprintf(led, sizeof led, "%s", e); else snprintf(led, sizeof led, "%s/@.apps/rpg-edit-hq/state/games/rmmc/game/ledger.txt", house);
    static char actor_b[48]; const char *actor = actor_b; { const char *ea = getenv("RMMC_ACTOR"); snprintf(actor_b, sizeof actor_b, "%s", ea && *ea ? ea : "asa"); }
    int env_fixed = (getenv("RMMC_DESK") && *getenv("RMMC_DESK")) || (getenv("RMMC_LEDGER") && *getenv("RMMC_LEDGER"));   /* an environment that names the desk or the ledger wins over the open page (the harness and the director set it) */
    if (!owner) { const char *g = getenv("GAME_OWNER_PID"); if (g && *g) owner = atol(g); }   /* the Play launcher hands the game to the editor window this way */
    if (lockf) { int lfd = open(lockf, O_CREAT | O_RDWR | O_CLOEXEC, 0644); if (lfd < 0 || flock(lfd, LOCK_EX | LOCK_NB)) return 0; }   /* one instance: a second Play press finds the lock held and ends quietly */
    if (!keys) { snprintf(dkeys, sizeof dkeys, "%s/keys.pdl", book); keys = dkeys; } if (!kbf) { snprintf(dkb, sizeof dkb, "%s/rmmc-pchq/pieces/system/keybinds.pdl", book); kbf = dkb; }
    if (load_keys(kbf, keys)) { fprintf(stderr, "rmmc_driver: bad keybinds or keys file\n"); return 2; }
    signal(SIGTERM, on_term); signal(SIGINT, on_term);
    long cur = 0; if (!from_start) { struct stat st; if (!stat(relay, &st)) cur = (long)st.st_size; }
    char shown[500] = ""; long shown_at = 0;      /* a key result fades to HINT after 8 s (QA 2026-11-26: a refusal stayed on the bar forever); a "page ..." warning stays while you are on that page */
    char msg[500] = "waiting for keys", seatn[96] = "harness", where[320] = ""; long last_key = 0, last_feed = -100000; int idle_pending = 0, first_refusal = 0;
    char cur_did[96] = "", cur_poss[96] = "", poss_actor[48] = ""; int desk_ok = 1, first_follow = 1;
    for (;;) {
        if (pkg) {   /* FOLLOW THE EDITOR (2026-11-26): the open page picks desk and ledger (desks.pdl), the possessed rig: token picks the actor, and the seat follows the possession, a ledger row each way */
            char pp[4500], did[96], pid_[96]; snprintf(pp, sizeof pp, "%s/pieces/world_01/state.txt", pkg); kv_get(pp, "desk_id", did, sizeof did); snprintf(pp, sizeof pp, "%s/pieces/xelector_01/state.txt", pkg); kv_get(pp, "possessed_id", pid_, sizeof pid_);
            if (strcmp(did, cur_did)) { snprintf(cur_did, sizeof cur_did, "%s", did); char lf[300];
                if (env_fixed) desk_ok = 1;
                else if (did[0] && !strchr(did, '/') && desk_row(book, did, lf, sizeof lf)) { snprintf(desk, sizeof desk, "%.4000s/rmmc-pchq/pieces/system/maps/rmmc/%s", book, did); snprintf(seed, sizeof seed, "%s/seed", desk); snprintf(led, sizeof led, "%.4000s/@.apps/rpg-edit-hq/state/games/rmmc/game/%.200s", house, lf); desk_ok = 1; }
                else { desk_ok = 0; snprintf(msg, sizeof msg, "page %s has no row in desks.pdl: keys are ignored there", did); }
                if (desk_ok && !strncmp(msg, "page ", 5)) snprintf(msg, sizeof msg, "%s", HINT);      /* back on a page that has a desk: the old page warning no longer applies */
                last_feed = 0; }
            setenv("RMMC_DESK", desk, 1); setenv("RMMC_SEED", seed, 1); setenv("RMMC_LEDGER", led, 1);
            if (first_follow && desk_ok) {      /* a fresh driver does not remember who was possessed before it started: every actor of the page that is not possessed NOW gets a human seat back to the harness (a SEAT row only when the seat really is human), so a stuck seat never blocks the harness or a tutorial presenter */
                char ep[4600], el[300]; snprintf(ep, sizeof ep, "%.4000s/events.txt", seed); FILE *ef = fopen(ep, "r");
                while (ef && fgets(el, sizeof el, ef)) { char nm[48]; int ex, ey, er, eg, eb; if (sscanf(el, "%d %d %d %d %d %47s", &ex, &ey, &er, &eg, &eb, nm) != 6 || strncmp(nm, "rig:", 4) || !nm[4] || !strcmp(nm, pid_)) continue;
                    int ok = 1; for (const char *c = nm + 4; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) ok = 0; if (!ok) continue;
                    char so[100]; char *as[] = { vop, "seat", data, led, nm + 4, NULL }; run_out(as, so, sizeof so);
                    if (!strcmp(so, "seat=human")) { setenv("RMMC_ACTOR", nm + 4, 1); char m2[200]; rmmc_play_verb(house, book, "seat", "harness", "human", m2, sizeof m2); } }
                if (ef) fclose(ef); }
            first_follow = 0;
            if (strcmp(pid_, cur_poss)) {
                if (desk_ok && poss_actor[0]) { setenv("RMMC_ACTOR", poss_actor, 1); char m2[200]; rmmc_play_verb(house, book, "seat", "harness", "human", m2, sizeof m2); }      /* released, or switched to another token: the old actor goes back to the harness */
                poss_actor[0] = 0; snprintf(cur_poss, sizeof cur_poss, "%s", pid_);
                if (!strncmp(pid_, "rig:", 4) && pid_[4] && strlen(pid_ + 4) < sizeof poss_actor) { int ok = 1; for (const char *c = pid_ + 4; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) ok = 0; if (ok) snprintf(poss_actor, sizeof poss_actor, "%s", pid_ + 4); }
                if (poss_actor[0]) { snprintf(actor_b, sizeof actor_b, "%s", poss_actor); if (desk_ok) { setenv("RMMC_ACTOR", poss_actor, 1); char so[100]; char *as[] = { vop, "seat", data, led, poss_actor, NULL }; run_out(as, so, sizeof so);
                    if (strcmp(so, "seat=human")) { char m2[200]; rmmc_play_verb(house, book, "seat", "human", "human", m2, sizeof m2); } } }      /* a SEAT row only when the seat really changes (2026-11-27: every driver restart appended a SEAT human the ledger already said) */
                if (poss_actor[0] && desk_ok) sync_view(pkg, vop, data, led, poss_actor);      /* a fresh possession frames the actor at once */
                last_feed = 0; }
            setenv("RMMC_ACTOR", actor_b, 1); }
        FILE *f = fopen(relay, "r"); char ln[200];
        if (f && pkg && !desk_ok) { fseek(f, 0, SEEK_END); cur = ftell(f); fclose(f); f = NULL; }      /* a page with no desks.pdl row: the keys are read and dropped, never replayed later */
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
                if (!strcmp(KV[kv].verb, "walk") && step_ms > 0) {   /* a held key repeats 50-60 times a second and used to queue one step per repeat (the character kept walking after the key was let go). RULE step_ms (keys.pdl, 0 = off): a walk press inside the window of the last step is DROPPED, never queued (owner 2026-11-26) */
                    long tn = now_ms(); if (g_last_step && tn - g_last_step < step_ms) continue; }      /* the window starts when the PREVIOUS step has FINISHED (a step takes ~200 ms, longer than the window): everything that queued behind it is dropped */
                if (!strcmp(KV[kv].verb, "reset")) {   /* Home (owner 2026-11-26: a stuck player needs a way out): ONE press only asks, a second within 2 s runs the verb, so a stray tap never teleports the character */
                    static long reset_asked = 0; long tn = now_ms();
                    if (!reset_asked || tn - reset_asked > reset_confirm_ms) { reset_asked = tn; snprintf(msg, sizeof msg, "key %.20s: reset? press it again", gk); last_key = tn; last_feed = 0; continue; }
                    reset_asked = 0; }
                if (!strcmp(KV[kv].verb, "mine") || !strcmp(KV[kv].verb, "place")) aim_env(pkg, data); else unsetenv("RMMC_AIM");      /* AIM (D147): what the crosshair is on, when the board aims */
                int rc = rmmc_play_verb(house, book, KV[kv].verb, KV[kv].arg, "human", msg, sizeof msg); held_write(pkg, book, actor, KV[kv].verb, rc); if (!rc && (!strcmp(KV[kv].verb, "mine") || !strcmp(KV[kv].verb, "place"))) mine_say(led, actor, msg, sizeof msg); if (!strcmp(KV[kv].verb, "walk")) g_last_step = now_ms(); if (rc && !first_refusal) first_refusal = rc; { char kt[200]; const char *mm = msg; if (!strncmp(mm, "refused: refused: ", 18)) mm += 9; snprintf(kt, sizeof kt, "key %.20s: %.150s", gk, mm); { char *cut = strstr(kt, " (direct"); if (cut) { int paused = strstr(cut, "events are paused in Blueprintz") != NULL; *cut = 0; if (paused) snprintf(cut, sizeof kt - (size_t)(cut - kt), " (events paused)"); } cut = strstr(kt, " ["); if (cut) *cut = 0; }   /* the long "(direct: the page did not run)" tail is noise on the bar, but a PAUSE is the owner's switch and must stay visible (case rmmc_seat, 2026-11-27) */ snprintf(msg, sizeof msg, "%s", kt); }   /* the last key and what came of it, on screen (owner 2026-11-26: a silent key is a bug; the browser-style debug console replaces this later, plan section 15) */ last_key = now_ms(); idle_pending = 1; sync_view(pkg, vop, data, led, actor); }
            fclose(f); }
        if (strcmp(msg, shown)) { snprintf(shown, sizeof shown, "%s", msg); shown_at = now_ms(); } else if (now_ms() - shown_at > 8000 && strncmp(msg, "page ", 5) && strcmp(msg, HINT) && strcmp(msg, "waiting for keys")) { snprintf(msg, sizeof msg, "%s", HINT); last_feed = 0; }
        if ((last_feed == 0 || now_ms() - last_feed >= 300) && (last_key > last_feed || g_pick_dirty || feed_inputs_changed(led, book) || now_ms() - last_feed >= 30000)) { last_feed = now_ms(); g_pick_dirty = 0; char o2[300]; char *av[] = { vop, "where", data, led, (char *)actor, NULL }; run_out(av, o2, sizeof o2); snprintf(where, sizeof where, "%s", o2[0] ? o2 : "not spawned"); char o3[100]; char *a3[] = { vop, "seat", data, led, (char *)actor, NULL }; run_out(a3, o3, sizeof o3); snprintf(seatn, sizeof seatn, "%s", strncmp(o3, "seat=", 5) ? "harness" : o3 + 5); { char ob[400]; char *ab[] = { vop, "bag", data, led, (char *)actor, NULL }; run_out(ab, ob, sizeof ob); snprintf(g_bag, sizeof g_bag, "%s", strncmp(ob, "bag=", 4) ? "" : ob + 4); }
        if (feed) { char fb[16384]; g_tut_led = led[0] ? led : NULL; int n = chrome_feed(fb, sizeof fb, book, actor, where, seatn, msg); put_if_changed(feed, fb, (size_t)n); } }
        if (once) return first_refusal;
        if (idle_pending && now_ms() - last_key >= idle_ms) { char o2[200]; char *av[] = { pop, "project", seed, desk, led, pose, hm, "--idle", "--no-marker", NULL }; run_out(av, o2, sizeof o2); idle_pending = 0; }
        if (stop_ || (owner > 0 && owner_gone(owner))) return 0;
        held_poll(pkg, book, actor);
        if (g_swing_ms && now_ms() - g_swing_ms <= 450) bump_view(pkg);      /* the engine only redraws on a change: nudge it every tick of a swing and once after, so the arm swings and comes back (about 8 nudges per press) */
        { int sl = poll_ms; if (idle_poll_ms > poll_ms && now_ms() - last_key > 3000 && !(g_swing_ms && now_ms() - g_swing_ms <= 600)) sl = idle_poll_ms; usleep((useconds_t)sl * 1000); }      /* at rest the loop wakes every idle_poll_ms (keys.pdl RULE, 0 = poll_ms); 3 s after the last key it is back to poll_ms */
    }
}
