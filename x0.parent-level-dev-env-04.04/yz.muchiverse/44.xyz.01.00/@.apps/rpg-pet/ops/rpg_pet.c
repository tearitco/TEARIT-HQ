/* rpg_pet.c - rpg-pet: the RPG Maker pet house. One C op, three shapes (design + how-to: @.apps/rpg-pet/HOW-TO-RPG-PET.md).
 *
 *   rpg_pet.+x <verb> [args]            one verb, then re-render state/ and exit   (window buttons, harness, scripts)
 *   rpg_pet.+x <pkg> <house> "<text>"   the cli_io action form: the typed text is the verb line, anything that is not a verb is chat
 *   rpg_pet.+x <house> <pkg>            resident mode, started by the window's <module>: INT keys, the pets' own walking, the clock
 *   rpg_pet.+x <clock-pkg> <root>       event-runner mode: the clock daemon runs it when a time.pdl event fires (pkg ends in common:<event>)
 *
 * WHO MOVES: by default every pet walks, meets the others and uses doors BY ITSELF (one tile per event step). INT (interact mode) lets the owner TAKE CONTROL of the
 * selected pet (like possession in pc-hq): its arrow keys walk that pet; the other five keep acting on their own. INT off hands the pet back to itself.
 * TIME, like a roguelike: INT ON = turn based. The clock is paused; each move or act of the controlled pet (a step, buy, place, remove, chat) is ONE TURN: the clock
 *   advances by the TURN row of time.pdl and every OTHER pet takes one step. INT OFF = real time: the clock runs at its rate and all pets walk on a timer.
 * INT can also control the PLAYER (the person, RPG Maker Actor1, the trainer): flag ctl = pet | player; Tab (key 9), the verb `control` or the Menu row switches. The village is where the
 * player normally lives and is not built yet, so for now the player may walk the house too.
 * The clock is the house livedesk clock (&.widgits/livedesk-clock lc_clock.+x), own mini root state/clock_root, clock id "rpg" - same mechanism as pet-trainer's pet_clock.sh.
 *
 * PLAY MODE (same mechanics as pet-trainer's Player tab): the house is STARTED (flag running = 1, green "GO started") or STOPPED (red "STOP stopped"). Stopped freezes the
 * SIMULATION: no pet steps, the clock is paused, INT cannot move anyone. Building (shop, place, remove) and chat still work while stopped, like a maker editor.
 * Player tab: Play, Stop, Save (slot 1), Load (slot 1) = copies of the state files under state/slots/<n>/.
 * Verbs: play | stop | save [n] | load [n] | status | buy KEY [X Y] | use KEY | place ID X Y | remove ID | step DX DY (trainer) | walk (one pet step, test) | select ID | goto ROOM (moves the trainer + the view)
 *        toggle chat|hb | view | int | clock rate|advance|start|stop|reinstall [arg] | reset
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
 * State (state/, gitignored): house.txt = append-only ledger replayed on every call  BUY|ts|item|price  PLACE|id|ts|item|x|y|room  MOVE|id|ts|x|y  REMOVE|id|ts
 *   pet_<id>.txt + trainer.txt (room x y dir steps tx ty), flag_*.txt, chat.txt, events.txt (STEP/DOOR/DAY rows = the "event steps" ledger),
 *   relations.txt (append-only MEET rows: who met whom; the data a later "share a room" rule reads), daylight.txt, clock_ui.txt, clock_root/.
 * Outputs: state/scene.raw (RGBA 660x430) + scene.receipt.txt, state/ui.txt + state/clock_ui.txt. Written only when their content changed (the renderer repaints a canvas
 * when its file changes - an unconditional rewrite costs CPU).
 * Keys in INT (state/interact_relay.txt, "<decimal code> <ms>", same codes as pet-trainer keybinds.pdl): arrows 1000 left / 1001 right / 1002 up / 1003 down (the agent relay band 200-203 is also accepted), 9 Tab, 27 Esc = INT off, 0 view, 1-4 pov (forwarded to this board session; the viewer sets the camera).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/file.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"

#define T 48
#define FW 660
#define FH 430
#define COLS 13
#define ROWS 8
#define OX ((FW - COLS * T) / 2)
#define OY ((FH - ROWS * T) / 2)
#define WALL_ROW 1
#define TRIG_ROW 2
#define START_COINS 500
#define MAXI 40
#define MAXP 200
#define NP 8      /* the party: up to 6 DB monsters + the pals made with "+ new pal" (pals.pdl); 8 = the 8 Monster.png pictures */

typedef struct { char key[32], label[40], sheet[24], effect[12]; int price, c, r, w, h, wall; } Item;
typedef struct { char id[16], name[32]; int fc, fr, outdoor; } Room;
typedef struct { int room, c, r, w, h, x, y; char sheet[24]; } Scen;
typedef struct { int room, tx, dest, ax, ay, autow; } Door;
typedef struct { int id, item, x, y, room; } Placed;
typedef struct { char name[40]; unsigned char *px; int w, h; } Sheet;
typedef struct { int room, x, y, dir, steps, tx, ty; } Ent;          /* a walking thing: a pet, or the trainer. tx,ty = the walk target (-1 none) */

static char APP[PATH_MAX], ST[PATH_MAX], ASSETS[PATH_MAX], HOUSE[PATH_MAX], SELF[PATH_MAX];
#define SIBLING_APP "/@.apps/" "dsr-rpg" "/"      /* the carbon-copy game that shares the one pc-hq engine + world: its board window is NOT a viewer of this game (see viewer_kinds); the copy sets this to the other game */
static Item cat[MAXI]; static int ncat;
static Room rooms[8]; static int nroom;
static Door doors[24]; static int ndoor;
static Scen scen[24]; static int nscen;
static Placed pl[MAXP]; static int npl, nid = 1, owned[MAXI], coins;
static Sheet sheets[12]; static int nsheet;
static unsigned char fr[FW * FH * 4];
static int g_bg_only = 0;      /* render() without pets, Harold and the day/night tint: the static background the pc-hq 2D view blits as map.png (its sprites come from events.txt) */
/* the standard RPG Maker screen tone written by events-hq's tint_screen command: state/world/screen_state.pdl has "mode=tint" and "value=r,g,b,gray" (1 = a tone is set). Read at most every 400 ms. */
static int read_file(const char *p, char *buf, size_t cap);
static int g_tone_stale = 0;      /* set by the daemon when screen_state.pdl changed: the next read_tone() re-reads at once */
static int read_tone(int *t) {
    static long last_ms = 0; static int have = 0, tv[4]; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); long ms = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
    if (!last_ms || g_tone_stale || ms - last_ms > 400) { last_ms = ms; g_tone_stale = 0; have = 0; char p[PATH_MAX], b[300]; snprintf(p, sizeof p, "%s/world/screen_state.pdl", ST); if (read_file(p, b, sizeof b) > 0 && strstr(b, "mode=tint")) { char *v = strstr(b, "value="); if (v && sscanf(v + 6, "%d,%d,%d,%d", &tv[0], &tv[1], &tv[2], &tv[3]) == 4) { for (int q = 0; q < 4; q++) tv[q] = tv[q] < -255 ? -255 : tv[q] > 255 ? 255 : tv[q]; have = 1; } } }
    if (have) memcpy(t, tv, sizeof tv); return have;
}
static struct { char id[16], name[24], sheet[24], nick[24], profile[96]; int idx, mhp, mmp, atk, def, mat, mdf, agi, luk; } party[NP]; static int nparty, active;
static struct { char name[24], nick[24], profile[96], sheet[24]; int idx, mhp, mmp, atk, def, mat, mdf, agi, luk; } hero;      /* the main hero: DB SYSTEM Party member1 (Harold), the player; he owns the monsters */
static Ent others[NP], tr;      /* every pet's state, and the trainer's */
static Ent pet;                 /* the mover being operated on right now: a copy of others[me] (or tr when me == -1) */
static int me, vroom;           /* me: -1 trainer, 0.. a pet.  vroom: the room the window shows */
static volatile sig_atomic_t quit_flag;
static int turn_taken;          /* a verb that counts as a roguelike turn sets this */

static void spath(char *o, const char *n) { snprintf(o, PATH_MAX, "%s/%s", ST, n); }
static int fexists(const char *p) { struct stat s; return stat(p, &s) == 0; }
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
/* Play one of this app's events-hq pages (<app>/events/<name>/, trigger <trig>) through events-hq's play_event.sh. MUCHI_TARGET_ENT points every
 * event at ONE shared world dir (<app>/state/world) so state commands land in one place: map_state.pdl (transfer_player) and screen_state.pdl (tint_screen).
 * Returns 1 when the page ran. */
/* HARDENING (hq-crash-hardening.md): every child this process waits for is waited on with a deadline; one that outlives it is killed and reaped, so a hung helper (lc_clock, bash, an event page) can never freeze a verb or the daemon loop */
static int g_reap_to;      /* set by reap_timed when it had to kill the child */
static int reap_timed(pid_t pid, int secs) { struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0); int st = 0;
    for (;;) { pid_t r = waitpid(pid, &st, WNOHANG); if (r == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; if (r < 0 && errno != EINTR) return -1;
        clock_gettime(CLOCK_MONOTONIC, &t1); if (t1.tv_sec - t0.tv_sec >= secs) { g_reap_to = 1; break; } usleep(5000); }
    kill(-pid, SIGKILL); kill(pid, SIGKILL); for (int i = 0; i < 200; i++) { if (waitpid(pid, &st, WNOHANG) == pid) break; usleep(5000); } return -1; }
static int sh_timed(const char *cmd, int secs) { pid_t pid = fork(); if (pid < 0) return -1; if (pid == 0) { setpgid(0, 0); signal(SIGPIPE, SIG_DFL); execl("/bin/sh", "sh", "-c", cmd, (char *)NULL); _exit(127); } return reap_timed(pid, secs); }
static int run_event_x(const char *name, const char *trig, const char *envs);
static int run_event(const char *name, const char *trig) { return run_event_x(name, trig, ""); }
/* same, with extra environment words for the event's commands (e.g. "RPG_OTHER='Blip'", read by the registered pet_says command) */
static int run_event_x(const char *name, const char *trig, const char *envs) {
    char pd[PATH_MAX], pl[PATH_MAX], wd[PATH_MAX], cmd[4 * PATH_MAX];
    snprintf(pd, sizeof pd, "%s/events/%s", APP, name); snprintf(pl, sizeof pl, "%s/&.widgits/events-hq/ops/play_event.sh", HOUSE); snprintf(wd, sizeof wd, "%s/world", ST);
    if (!fexists(pl) || !is_dir(pd)) return 0; mkdir(wd, 0755);
    snprintf(cmd, sizeof cmd, "%s MUCHI_TARGET_ENT='%s' bash '%s' '%s' '%s' '%s' >/dev/null 2>&1", envs, wd, pl, pd, HOUSE, trig);
    return sh_timed(cmd, 20) == 0;
}
static long fsize(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : 0; }
static long now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L; }

/* ---------- small file helpers ---------- */
static int read_file(const char *p, char *buf, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { buf[0] = 0; return 0; } size_t n = fread(buf, 1, cap - 1, f); buf[n] = 0; fclose(f); return (int)n; }
/* replace a file atomically, only when the bytes differ; returns 1 when written */
static int g_scene_ver = 0;      /* bumped whenever the painted scene frame (state/scene.raw) changed: the board engine only redraws when woken, follow_pchq nudges it */
static int write_if_changed(const char *p, const void *data, size_t n) {
    FILE *f = fopen(p, "rb");
    if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); if (sz == (long)n) { rewind(f); unsigned char *old = malloc(n ? n : 1); if (old && fread(old, 1, n, f) == n && !memcmp(old, data, n)) { free(old); fclose(f); return 0; } free(old); } fclose(f); }
    char tmp[PATH_MAX + 24]; snprintf(tmp, sizeof tmp, "%s.tmp.%d", p, (int)getpid());      /* per process: the daemon and a one-shot verb can write the same file at the same moment */
    f = fopen(tmp, "wb"); if (!f) return 0; size_t wn = fwrite(data, 1, n, f); int ce = fclose(f); if (wn != n || ce != 0 || rename(tmp, p) != 0) { remove(tmp); return 0; } return 1;      /* ENOSPC / EIO: the good file stays, the half-written tmp goes (hq-crash-hardening.md) */
}
/* HARDENING: files another process reads (the 3D export for the board renderer) are written to <file>.tmp.<pid> and renamed on a clean close, never in place */
static FILE *fopen_tmp(const char *p) { char t[PATH_MAX + 24]; snprintf(t, sizeof t, "%s.tmp.%d", p, (int)getpid()); return fopen(t, "wb"); }
static int fclose_tmp(FILE *f, const char *p) { char t[PATH_MAX + 24]; snprintf(t, sizeof t, "%s.tmp.%d", p, (int)getpid()); int bad = ferror(f); if (fclose(f) != 0) bad = 1; if (bad || rename(t, p) != 0) { remove(t); return 0; } return 1; }
static int png_atomic(const char *p, int w, int h, const unsigned char *px, int stride) { char t[PATH_MAX + 24]; snprintf(t, sizeof t, "%s.tmp.%d", p, (int)getpid()); if (stbi_write_png(t, w, h, 4, px, stride) && rename(t, p) == 0) return 1; remove(t); return 0; }
/* HARDENING: one write() of the whole line on an O_APPEND fd (rows from two processes never interleave or tear); a log file past 4 MB drops further rows (the house.txt ledger never does) so a runaway loop cannot fill the disk */
static void append_line(const char *name, const char *line) { char p[PATH_MAX], b[1100]; spath(p, name); int fd = open(p, O_WRONLY | O_APPEND | O_CREAT, 0644); if (fd < 0) return; struct stat sb; if (strcmp(name, "house.txt") && fstat(fd, &sb) == 0 && sb.st_size > 4L * 1024 * 1024) { close(fd); return; }
    int n = snprintf(b, sizeof b, "%s\n", line); if (n >= (int)sizeof b) { n = (int)sizeof b - 1; b[n - 1] = '\n'; } ssize_t w = write(fd, b, (size_t)n); (void)w; close(fd); }
/* SWITCHES: "running" (Play / Stop) and "int" (INT on / off) live as switches in <app>/state/world/switches.txt ("name=1"), the file events-hq's control_switch command writes (the events/play,
 * stop, int_on, int_off pages). flag_str() reads the switch first; set_flag() mirrors into it, so code that still sets the flag directly (Esc leaves INT) and the event pages stay one state. */
static int sw_get(const char *name, char *out, size_t cap) {
    char p[PATH_MAX], b[1024]; snprintf(p, sizeof p, "%s/world/switches.txt", ST); if (read_file(p, b, sizeof b) <= 0) return 0; size_t nl = strlen(name);
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, name, nl) && l[nl] == '=') { snprintf(out, cap, "%s", l + nl + 1); return 1; } return 0;
}
static void sw_set(const char *name, const char *v) {
    char p[PATH_MAX], d[PATH_MAX], b[1024], o[1100]; snprintf(d, sizeof d, "%s/world", ST); mkdir(d, 0755); snprintf(p, sizeof p, "%s/switches.txt", d); read_file(p, b, sizeof b); size_t nl = strlen(name); int n = 0, hit = 0;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, name, nl) && l[nl] == '=') { n += snprintf(o + n, sizeof o - n, "%s=%s\n", name, v); hit = 1; } else if (l[0]) n += snprintf(o + n, sizeof o - n, "%s\n", l); }
    if (!hit) n += snprintf(o + n, sizeof o - n, "%s=%s\n", name, v); write_if_changed(p, o, (size_t)n);
}
static int is_switch_flag(const char *n) { return !strcmp(n, "running") || !strcmp(n, "int"); }
/* tuning.pdl: "TUNE | name | value | text | default" rows. tune(name, default) is one small table lookup; the file is re-read when its size or mtime moves (load_tune is called by load_data). */
static struct { char k[32]; int v; } g_tn[24]; static int g_tnn = 0; static long g_tn_sig = -1;
static void load_tune(void) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/tuning.pdl", APP); struct stat st; long sig = stat(p, &st) == 0 ? (long)st.st_size * 1000003L + (long)st.st_mtime : -2; if (sig == g_tn_sig) return; g_tn_sig = sig; g_tnn = 0;
    FILE *f = fopen(p, "r"); if (!f) return; char l[300];
    while (fgets(l, sizeof l, f)) { if (strncmp(l, "TUNE", 4)) continue; char *fl[5]; int n = 0; for (char *t = strtok(l, "|"); t && n < 5; t = strtok(NULL, "|")) fl[n++] = t; if (n < 3 || g_tnn >= 24) continue;
        char *k = fl[1]; while (*k == ' ') k++; char *e = k + strlen(k); while (e > k && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; snprintf(g_tn[g_tnn].k, sizeof g_tn[g_tnn].k, "%s", k); g_tn[g_tnn].v = atoi(fl[2]); g_tnn++; }
    fclose(f);
}
static int tune(const char *name, int def) { for (int i = 0; i < g_tnn; i++) if (!strcmp(g_tn[i].k, name) && g_tn[i].v > 0) return g_tn[i].v; return def; }
/* CPU safety (house hq-cpu-safety.md 3b, in-memory CLOCK_MONOTONIC gate for a long-lived process): the daemon asks for the same few flags a dozen times per 100 ms tick and every ask re-read the
 * switches file and the flag file. Inside the daemon a value is reused for 150 ms; set_flag() drops the cache so the daemon sees its own writes at once, and the one-shot verb processes never cache. */
static void flag_str_raw(const char *name, const char *def, char *out, size_t cap);
static int g_fcache_on = 0, g_fcn = 0;
static struct { char k[48]; char v[64]; long t; } g_fc[32];
static void flag_str(const char *name, const char *def, char *out, size_t cap) {
    char ck[48]; long cnow = 0; if (g_fcache_on) { snprintf(ck, sizeof ck, "%s|%s", name, def); cnow = now_ms(); for (int i = 0; i < g_fcn; i++) if (cnow - g_fc[i].t < tune("flag_cache_ms", 150) && !strcmp(g_fc[i].k, ck)) { snprintf(out, cap, "%s", g_fc[i].v); return; } }
    flag_str_raw(name, def, out, cap);
    if (g_fcache_on) { int s = -1; for (int i = 0; i < g_fcn; i++) if (!strcmp(g_fc[i].k, ck)) s = i; if (s < 0 && g_fcn < 32) s = g_fcn++; if (s >= 0) { snprintf(g_fc[s].k, sizeof g_fc[s].k, "%s", ck); snprintf(g_fc[s].v, sizeof g_fc[s].v, "%s", out); g_fc[s].t = cnow; } }
}
static void flag_str_raw(const char *name, const char *def, char *out, size_t cap) { if (is_switch_flag(name) && sw_get(name, out, cap) && out[0]) return; char p[PATH_MAX], n[64]; snprintf(n, sizeof n, "flag_%s.txt", name); spath(p, n); char b[64]; read_file(p, b, sizeof b); char *e = strchr(b, '\n'); if (e) *e = 0; snprintf(out, cap, "%s", b[0] ? b : def); }
static int flag_on(const char *name, int def) { char b[16]; flag_str(name, def ? "1" : "0", b, sizeof b); return b[0] == '1'; }
static const char *g_ctl_why = "?";      /* who is changing ctl (pet/player): the owner reports it flips by itself; ctl_trace.txt names the cause of each flip */
static void set_flag(const char *name, const char *v) { g_fcn = 0; if (!strcmp(name, "ctl")) { char tp[PATH_MAX]; snprintf(tp, sizeof tp, "%s/ctl_trace.txt", ST); FILE *tf = fsize(tp) > 1000000 ? NULL : fopen(tp, "a"); if (tf) { fprintf(tf, "%ld ctl=%s why=%s pid=%d\n", (long)time(NULL), v, g_ctl_why, (int)getpid()); fclose(tf); } } if (is_switch_flag(name)) sw_set(name, v); char p[PATH_MAX], n[64], b[80]; snprintf(n, sizeof n, "flag_%s.txt", name); spath(p, n); snprintf(b, sizeof b, "%s\n", v); write_if_changed(p, b, strlen(b)); }

/* ---------- data: catalog.pdl, rooms.pdl, the RPG Maker DB party ---------- */
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split_bar(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int room_idx(const char *id) { for (int i = 0; i < nroom; i++) if (!strcmp(rooms[i].id, id)) return i; return -1; }
static int item_idx(const char *k) { for (int i = 0; i < ncat; i++) if (!strcmp(cat[i].key, k)) return i; return -1; }

/* the pets are RPG Maker DB entries: SYSTEM PetParty member1..6 names, then each ACTOR block by name (db-hq data). party.pdl only picks the picture. */
static void load_party(void) {
    char db[PATH_MAX], p[PATH_MAX], line[400]; char *f[6]; FILE *fp; const char *env = getenv("PET_DB");
    if (env && env[0]) snprintf(db, sizeof db, "%s", env); else snprintf(db, sizeof db, "%s/&.widgits/db-hq/data", HOUSE);
    nparty = 0; char names[NP][24]; int nn = 0, inparty = 0, inhero = 0; memset(&hero, 0, sizeof hero); snprintf(hero.name, 24, "Harold"); snprintf(hero.sheet, 24, "Actor1");
    snprintf(p, sizeof p, "%s/system.pdl", db);
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "SYSTEM")) continue; if (!strcmp(f[1], "name")) { inparty = !strcmp(f[2], getenv("PET_PARTY") && getenv("PET_PARTY")[0] ? getenv("PET_PARTY") : "RpgPetParty"); inhero = !strcmp(f[2], "Party"); } else if (inhero && !strcmp(f[1], "member1")) snprintf(hero.name, 24, "%s", f[2]); else if (inparty && !strncmp(f[1], "member", 6) && nn < NP) snprintf(names[nn++], 24, "%s", f[2]); } fclose(fp); }
    for (int i = 0; i < nn; i++) { memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].name, 24, "%s", names[i]); snprintf(party[nparty].id, 16, "%s", names[i]); for (char *c = party[nparty].id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32; party[nparty].idx = i; snprintf(party[nparty].sheet, 24, "Monster"); nparty++; }
    snprintf(p, sizeof p, "%s/pals.pdl", APP);      /* pals: PAL | id | name | monster picture index. Made by the Pals menu (newpal), app-local: the house DB is not touched */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { char *g[6]; if (split_bar(line, g, 6) < 4 || strcmp(g[0], "PAL") || nparty >= NP) continue;
        memset(&party[nparty], 0, sizeof party[nparty]); snprintf(party[nparty].id, 16, "%s", g[1]); snprintf(party[nparty].name, 24, "%s", g[2]); snprintf(party[nparty].nick, 24, "pal"); snprintf(party[nparty].profile, 96, "A pal made in the Pals menu.");
        party[nparty].idx = atoi(g[3]) & 7; snprintf(party[nparty].sheet, 24, "Monster"); party[nparty].mhp = 40; party[nparty].mmp = 20; party[nparty].atk = party[nparty].def = party[nparty].agi = party[nparty].luk = 10; party[nparty].mat = party[nparty].mdf = 8; nparty++; } fclose(fp); }
    snprintf(p, sizeof p, "%s/actors.pdl", db); int cur = -1, curh = 0;
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { if (split_bar(line, f, 3) < 3 || strcmp(f[0], "ACTOR")) continue;
        if (!strcmp(f[1], "name")) { cur = -1; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].name, f[2])) cur = i; curh = !strcmp(f[2], hero.name); continue; }
        if (curh) { if (!strcmp(f[1], "nickname")) snprintf(hero.nick, 24, "%s", f[2]); else if (!strcmp(f[1], "profile")) snprintf(hero.profile, 96, "%s", f[2]); else if (!strcmp(f[1], "mhp")) hero.mhp = atoi(f[2]); else if (!strcmp(f[1], "mmp")) hero.mmp = atoi(f[2]); else if (!strcmp(f[1], "atk")) hero.atk = atoi(f[2]); else if (!strcmp(f[1], "def")) hero.def = atoi(f[2]);
            else if (!strcmp(f[1], "mat")) hero.mat = atoi(f[2]); else if (!strcmp(f[1], "mdf")) hero.mdf = atoi(f[2]); else if (!strcmp(f[1], "agi")) hero.agi = atoi(f[2]); else if (!strcmp(f[1], "luk")) hero.luk = atoi(f[2]); else if (!strcmp(f[1], "character") && f[2][0]) snprintf(hero.sheet, 24, "%s", f[2]); }
        if (cur < 0) continue;
        if (!strcmp(f[1], "nickname")) snprintf(party[cur].nick, 24, "%s", f[2]); else if (!strcmp(f[1], "profile")) snprintf(party[cur].profile, 96, "%s", f[2]);
        else if (!strcmp(f[1], "mhp")) party[cur].mhp = atoi(f[2]); else if (!strcmp(f[1], "mmp")) party[cur].mmp = atoi(f[2]); else if (!strcmp(f[1], "atk")) party[cur].atk = atoi(f[2]); else if (!strcmp(f[1], "def")) party[cur].def = atoi(f[2]);
        else if (!strcmp(f[1], "mat")) party[cur].mat = atoi(f[2]); else if (!strcmp(f[1], "mdf")) party[cur].mdf = atoi(f[2]); else if (!strcmp(f[1], "agi")) party[cur].agi = atoi(f[2]); else if (!strcmp(f[1], "luk")) party[cur].luk = atoi(f[2]);
        else if (!strcmp(f[1], "character") && f[2][0]) { char *c = strchr(f[2], ':'); if (c) { *c = 0; snprintf(party[cur].sheet, 24, "%s", f[2]); party[cur].idx = atoi(c + 1); } else snprintf(party[cur].sheet, 24, "%s", f[2]); } } fclose(fp); }
    snprintf(p, sizeof p, "%s/party.pdl", APP);       /* the picture choice, unless the DB character field already named one (a "Sheet:index" value) */
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp)) { char *g[6]; if (split_bar(line, g, 6) >= 4 && !strcmp(g[0], "PET")) for (int i = 0; i < nparty; i++) if (!strcmp(party[i].name, g[1])) { snprintf(party[i].sheet, 24, "%s", g[2]); party[i].idx = atoi(g[3]); } } fclose(fp); }
    for (int i = 0; i < nparty; i++) if (party[i].idx < 0 || party[i].idx > 7) party[i].idx = 0;      /* HARDENING: the sheet cell index is used in pixel arithmetic */
}
/* DOORS ARE EVENTS: <app>/events/door_<room>_<tx>/event_pkg/pages/page_1/event.ir.pdl holds "META | door | <room> <tx> <auto>" and a real "transfer_player"
 * node (map=<dest room>|xy=<arrive x>,<arrive y>), the same page Events (hq) lists and play_event.sh can play (mr_world transfer). Stepping on a door is a
 * hot path (every pet, every step), so this reads the pages and runs the one command in-process instead of starting bash + prisc per step. If any door
 * page exists they REPLACE the DOOR rows of rooms.pdl (kept as the fallback). A cheap signature (dir count + sizes) skips the re-parse while nothing changed. */
static void load_door_events(void) {
    static Door cache[24]; static int ncache = 0; static long sig = -1; char dir[PATH_MAX]; snprintf(dir, sizeof dir, "%s/events", APP);
    DIR *dp = opendir(dir); if (!dp) return; struct dirent *de; long s = 0; int names = 0; char ir[PATH_MAX]; struct stat st;
    while ((de = readdir(dp))) { if (strncmp(de->d_name, "door_", 5)) continue; snprintf(ir, sizeof ir, "%s/%s/event_pkg/pages/page_1/event.ir.pdl", dir, de->d_name); if (!stat(ir, &st)) { s = s * 31 + (long)st.st_size + de->d_name[strlen(de->d_name) - 1]; names++; } }
    closedir(dp); if (!names) return;
    if (s != sig) { sig = s; ncache = 0; dp = opendir(dir); if (!dp) return;
        while ((de = readdir(dp)) && ncache < 24) { if (strncmp(de->d_name, "door_", 5)) continue; snprintf(ir, sizeof ir, "%s/%s/event_pkg/pages/page_1/event.ir.pdl", dir, de->d_name); FILE *fp = fopen(ir, "r"); if (!fp) continue;
            char line[400], room[16] = "", dest[16] = ""; int tx = -1, ax = 0, ay = 0, au = 1, got = 0;
            while (fgets(line, sizeof line, fp)) { char *f[6]; char cp[400]; snprintf(cp, sizeof cp, "%s", line);
                if (!strncmp(line, "META", 4) && strstr(line, "| door ")) { char *q = strstr(line, "| door"); q = strchr(q + 6, '|'); if (q) sscanf(q + 1, " %15s %d %d", room, &tx, &au); }
                else if (strstr(line, "type=transfer_player")) { char *m = strstr(line, "map="), *x = strstr(line, "xy="); if (m && x) { sscanf(m + 4, "%15[^|]", dest); sscanf(x + 3, "%d,%d", &ax, &ay); got = 1; } }
                (void)f; }
            fclose(fp); int a = room_idx(room), b = room_idx(dest); if (!got || tx < 0 || a < 0 || b < 0) continue;
            Door *d = &cache[ncache++]; d->room = a; d->tx = tx; d->dest = b; d->ax = ax; d->ay = ay; d->autow = au; }
        closedir(dp); }
    if (ncache > 0) { ndoor = 0; for (int i = 0; i < ncache; i++) doors[ndoor++] = cache[i]; }
}
static void load_data(void) {
    char p[PATH_MAX], line[400]; char *f[12]; FILE *fp;
    snprintf(p, sizeof p, "%s/catalog.pdl", APP); ncat = 0;
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp) && ncat < MAXI) { if (split_bar(line, f, 12) == 11 && !strcmp(f[0], "ITEM")) { Item *it = &cat[ncat++]; snprintf(it->key, 32, "%s", f[1]); snprintf(it->label, 40, "%s", f[2]); it->price = atoi(f[3]); snprintf(it->sheet, 24, "%s", f[4]); it->c = atoi(f[5]); it->r = atoi(f[6]); it->w = atoi(f[7]); it->h = atoi(f[8]); it->wall = !strcmp(f[9], "wall"); snprintf(it->effect, 12, "%s", f[10]); } } fclose(fp); }
    load_party();
    snprintf(p, sizeof p, "%s/rooms.pdl", APP); nroom = ndoor = 0;
    if ((fp = fopen(p, "r"))) {
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 5 && !strcmp(f[0], "ROOM") && nroom < 8) { Room *r = &rooms[nroom++]; snprintf(r->id, 16, "%s", f[1]); snprintf(r->name, 32, "%s", f[2]); r->fc = atoi(f[3]); r->fr = atoi(f[4]); r->outdoor = n >= 6 && !strcmp(f[5], "outdoor"); } }
        rewind(fp);
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 7 && !strcmp(f[0], "DOOR") && ndoor < 24) { int a = room_idx(f[1]), b = room_idx(f[3]); if (a < 0 || b < 0) continue; Door *d = &doors[ndoor++]; d->room = a; d->tx = atoi(f[2]); d->dest = b; d->ax = atoi(f[4]); d->ay = atoi(f[5]); d->autow = atoi(f[6]); } }
        rewind(fp); nscen = 0;
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 9 && !strcmp(f[0], "SCENERY") && nscen < 24) { int a = room_idx(f[1]); if (a < 0) continue; Scen *s = &scen[nscen++]; s->room = a; snprintf(s->sheet, 24, "%s", f[2]); s->c = atoi(f[3]); s->r = atoi(f[4]); s->w = atoi(f[5]); s->h = atoi(f[6]); s->x = atoi(f[7]); s->y = atoi(f[8]); } }
        fclose(fp);
    }
    load_door_events();
}

/* each pet is its OWN entity: a stable entity_uid (32 hex, made once, state/uid_<id>.txt) -> wallet id "e" + the first 24 hex (the house rule, see CHAIN-ECONOMY-DESIGN.md). No chain wallet is created yet. */
static void ent_wallet(const char *id, char *out, size_t cap) {
    char p[PATH_MAX], n[64], b[64]; snprintf(n, sizeof n, "uid_%s.txt", id); spath(p, n); read_file(p, b, sizeof b);
    if (strlen(b) < 32) { unsigned char r[16]; FILE *u = fopen("/dev/urandom", "rb"); if (u) { if (fread(r, 1, 16, u) != 16) memset(r, 7, 16); fclose(u); } else memset(r, 7, 16); for (int k = 0; k < 16; k++) snprintf(b + k * 2, 3, "%02x", r[k]); strncat(b, "\n", 2); write_if_changed(p, b, strlen(b)); }
    snprintf(out, cap, "e%.24s", b);
}
static void pet_wallet(int i, char *out, size_t cap) { ent_wallet(party[i].id, out, cap); }

/* ---------- ledger replay ---------- */
static void replay(void) {
    char p[PATH_MAX], line[300]; spath(p, "house.txt");
    { static long last_sz = -2; long sz = fsize(p); if (sz == last_sz) return; last_sz = sz; }      /* the ledger is append-only and only buy / use / build verbs write it: re-parsing it for every pet step was ~3% of a core */
    coins = START_COINS; npl = 0; nid = 1; memset(owned, 0, sizeof owned);
    FILE *fp = fopen(p, "r"); if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[8]; int n = split_bar(line, f, 8);
        if (!strcmp(f[0], "BUY") && n >= 4) { int i = item_idx(f[2]); coins -= atoi(f[3]); if (i >= 0) owned[i]++; }
        else if (!strcmp(f[0], "PLACE") && n >= 6 && npl < MAXP) { int i = item_idx(f[3]); if (i < 0) continue; Placed *q = &pl[npl++]; q->id = atoi(f[1]); q->item = i; q->x = atoi(f[4]); q->y = atoi(f[5]); q->room = n >= 7 ? room_idx(f[6]) : 0; if (q->room < 0) q->room = 0; owned[i]--; if (q->id >= nid) nid = q->id + 1; }
        else if (!strcmp(f[0], "MOVE") && n >= 5) { int id = atoi(f[1]); for (int i = 0; i < npl; i++) if (pl[i].id == id) { pl[i].x = atoi(f[3]); pl[i].y = atoi(f[4]); } }
        else if (!strcmp(f[0], "REMOVE") && n >= 3) { int id = atoi(f[1]); for (int i = 0; i < npl; i++) if (pl[i].id == id) { owned[pl[i].item]++; pl[i] = pl[--npl]; break; } }
    }
    fclose(fp);
}

/* ---------- entities: the six pets and the trainer ---------- */
static void ent_file(int i, char *p) { char n[64]; if (i < 0) snprintf(n, sizeof n, "trainer.txt"); else snprintf(n, sizeof n, "pet_%s.txt", party[i].id); spath(p, n); }
static void load_ent(int i, Ent *e) {
    char p[PATH_MAX], b[300]; ent_file(i, p); read_file(p, b, sizeof b);
    e->room = 0; e->x = i < 0 ? 2 : 4 + i; e->y = i < 0 ? 6 : 5; e->dir = 0; e->steps = 0; e->tx = e->ty = -1;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "room=", 5)) { int r = room_idx(l + 5); if (r >= 0) e->room = r; } else if (!strncmp(l, "x=", 2)) e->x = atoi(l + 2); else if (!strncmp(l, "y=", 2)) e->y = atoi(l + 2); else if (!strncmp(l, "dir=", 4)) e->dir = atoi(l + 4); else if (!strncmp(l, "steps=", 6)) e->steps = atoi(l + 6); else if (!strncmp(l, "tx=", 3)) e->tx = atoi(l + 3); else if (!strncmp(l, "ty=", 3)) e->ty = atoi(l + 3); }
    /* HARDENING: a pet file is untrusted (hand edit, truncation, garbage): coordinates, facing and the walk target are forced inside the room so no array is ever indexed from file data */
    { int cw = 13, ch = 8;
      if (e->x < 0 || e->x >= cw) e->x = cw / 2; if (e->y < 0 || e->y >= ch) e->y = ch / 2; e->dir = ((e->dir % 4) + 4) % 4; if (e->steps < 0 || e->steps > 1000000000) e->steps = 0; if (e->tx < 0 || e->tx >= cw || e->ty < 0 || e->ty >= ch) e->tx = e->ty = -1; }
}
static void calc_view(void) { char f[40]; flag_str("follow", "pet", f, sizeof f); vroom = !strcmp(f, "player") ? tr.room : others[active].room; if (!strncmp(f, "room:", 5)) { int r = room_idx(f + 5); if (r >= 0) vroom = r; } if (vroom < 0 || vroom >= nroom) vroom = 0; }   /* the window shows the watched pet's room, or a room picked from the Rooms menu */
static void pet_load(void) {           /* everything: active pet, all pets, the trainer, the view room */
    char p[PATH_MAX], b[40]; spath(p, "active.txt"); read_file(p, b, sizeof b); char *e = strchr(b, '\n'); if (e) *e = 0; active = 0; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, b)) active = i;
    for (int i = 0; i < nparty; i++) load_ent(i, &others[i]);
    load_ent(-1, &tr); calc_view(); me = active; pet = others[active];
}
/* what INT controls: the selected pet, or the player (the person) */
static int ctl_idx(void) { char b[16]; flag_str("ctl", "pet", b, sizeof b); return !strcmp(b, "player") ? -1 : active; }
static void use_ent(int i) { me = i; pet = i < 0 ? tr : others[i]; }
static void pet_save(void) {            /* commit the mover back and write its file */
    char p[PATH_MAX], b[240]; ent_file(me, p); snprintf(b, sizeof b, "room=%s\nx=%d\ny=%d\ndir=%d\nsteps=%d\ntx=%d\nty=%d\n", rooms[pet.room].id, pet.x, pet.y, pet.dir, pet.steps, pet.tx, pet.ty);
    write_if_changed(p, b, strlen(b)); if (me < 0) tr = pet; else others[me] = pet; calc_view();
}
/* who stands in a cell, besides the mover: 1.. = a pet, 100 = the trainer */
static int other_at(int room, int x, int y) {
    for (int i = 0; i < nparty; i++) if (i != me && others[i].room == room && others[i].x == x && others[i].y == y) return i + 1;
    if (me >= 0 && tr.room == room && tr.x == x && tr.y == y) return 100;
    return 0;
}

/* ---------- placement rules ---------- */
static int floor_blocked(int room, int x, int y, int skip_id) { for (int i = 0; i < nscen; i++) if (scen[i].room == room && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 9999; for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == TRIG_ROW) return i; return -1; }
static const char *check_place(int room, int item, int x, int y, int skip_id) {
    static char m[120]; Item *d = &cat[item];
    if (d->wall) {
        int wy = WALL_ROW + 1 - d->h; if (rooms[room].outdoor) return "wall items only go inside";
        if (y != wy || x < 0 || x + d->w > COLS) { snprintf(m, sizeof m, "wall items go on the wall: x 0-%d, y %d", COLS - d->w, wy); return m; }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx >= x && doors[i].tx < x + d->w) return "that wall spot is a door";
        for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || !cat[q->item].wall) continue; if (x < q->x + cat[q->item].w && q->x < x + d->w) return "wall spot taken"; }
        return "";
    }
    if (x < 0 || y < TRIG_ROW || x + d->w > COLS || y + d->h > ROWS) { snprintf(m, sizeof m, "floor items go on the floor: x 0-%d, y %d-%d", COLS - d->w, TRIG_ROW, ROWS - d->h); return m; }
    int sv = me; me = -2;                  /* -2: nobody is excluded, so every pet and the trainer count */
    for (int i = 0; i < d->w; i++) for (int j = 0; j < d->h; j++) {
        int cx = x + i, cy = y + j, o;
        if (other_at(room, cx, cy) || (tr.room == room && tr.x == cx && tr.y == cy)) { me = sv; return "someone is standing there"; }
        if (door_here(room, cx, cy) >= 0) { me = sv; return "that cell is in front of a door"; }
        if ((o = floor_blocked(room, cx, cy, skip_id))) { me = sv; if (o == 9999) return "there is scenery there"; snprintf(m, sizeof m, "spot taken by #%d", o); return m; }
    }
    me = sv; return "";
}
static int first_free(int room, int item, int *ox, int *oy) { Item *d = &cat[item]; if (d->wall) { int wy = WALL_ROW + 1 - d->h; for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, wy, 0)[0]) { *ox = x; *oy = wy; return 1; } return 0; } for (int y = TRIG_ROW; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, y, 0)[0]) { *ox = x; *oy = y; return 1; } return 0; }

/* ---------- drawing ---------- */
static Sheet *sheet(const char *dir, const char *name) {
    for (int i = 0; i < nsheet; i++) if (!strcmp(sheets[i].name, name)) return &sheets[i];
    if (nsheet >= 12) return NULL;
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/%s/%s.png", ASSETS, dir, name); int n; Sheet *s = &sheets[nsheet];
    s->px = stbi_load(p, &s->w, &s->h, &n, 4); if (!s->px) return NULL; snprintf(s->name, 40, "%s", name); nsheet++; return s;
}
static void blit(Sheet *s, int sx, int sy, int w, int h, int dx, int dy) {
    if (!s || w < 0 || h < 0 || w > 4 * FW || h > 4 * FH) return;
    for (int y = 0; y < h; y++) { int Y = dy + y; if (Y < 0 || Y >= FH || sy + y >= s->h) continue;
        for (int x = 0; x < w; x++) { int X = dx + x; if (X < 0 || X >= FW || sx + x >= s->w) continue;
            unsigned char *a = s->px + ((size_t)(sy + y) * s->w + sx + x) * 4, *b = fr + ((size_t)Y * FW + X) * 4; int al = a[3]; if (!al) continue;
            for (int c = 0; c < 3; c++) b[c] = (unsigned char)((a[c] * al + b[c] * (255 - al)) / 255); b[3] = 255; } }
}
static void tile(const char *sh, int c, int r, int w, int h, int tx, int ty) { blit(sheet("tilesets", sh), c * T, r * T, w * T, h * T, OX + tx * T, OY + ty * T); }

static void render(void) {
    for (int i = 0; i < FW * FH; i++) { fr[i * 4] = 14; fr[i * 4 + 1] = 14; fr[i * 4 + 2] = 18; fr[i * 4 + 3] = 255; }
    Room *rm = &rooms[vroom]; char phase[40] = "day"; { char dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(phase, sizeof phase, "night"); else if (strstr(db, "dusk")) snprintf(phase, sizeof phase, "dusk"); else if (strstr(db, "dawn")) snprintf(phase, sizeof phase, "dawn"); }
    if (rm->outdoor) {      /* sky = an RPG Maker parallax picked by the game clock; ground, path and trees are Outside tiles */
        Sheet *sk = sheet("parallaxes", !strcmp(phase, "night") ? "StarlitSky" : (!strcmp(phase, "dusk") || !strcmp(phase, "dawn")) ? "Sunset" : "CloudySky1"); if (sk) blit(sk, 0, !strcmp(phase, "day") ? 330 : 120, COLS * T, 2 * T, OX, OY);
        for (int x = 0; x < COLS; x++) { tile("Outside_A5", 2, 0, 1, 1, x, WALL_ROW); for (int y = TRIG_ROW; y < ROWS; y++) tile("Outside_A5", 0, 2, 1, 1, x, y); }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom) for (int y = TRIG_ROW; y < ROWS - 1; y++) tile("Outside_A5", 1, 2, 1, 1, doors[i].tx, y);
    } else
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { if (y == 0) tile("Inside_A5", 0, 0, 1, 1, x, y); else if (y == WALL_ROW) tile("Inside_A5", 1, 3, 1, 1, x, y); else tile("Inside_A5", rm->fc, rm->fr, 1, 1, x, y); }
    Sheet *dr = sheet("characters", "!Door1");                                  /* the wood panel door: closed frame of the 4th door object */
    for (int i = 0; i < ndoor; i++) if (doors[i].room == vroom) { blit(dr, 0, 192, T, T, OX + doors[i].tx * T, OY + WALL_ROW * T); if (!rm->outdoor) tile("Inside_A5", 0, 6, 1, 1, doors[i].tx, TRIG_ROW); }
    for (int i = 0; i < nscen; i++) if (scen[i].room == vroom) tile(scen[i].sheet, scen[i].c, scen[i].r, scen[i].w, scen[i].h, scen[i].x, scen[i].y);
    int idx[MAXP], n = 0; for (int i = 0; i < npl; i++) if (pl[i].room == vroom) idx[n++] = i;
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) { Placed *p = &pl[idx[a]], *q = &pl[idx[b]]; int ka = (!cat[p->item].wall) * 1000 + p->y + cat[p->item].h, kb = (!cat[q->item].wall) * 1000 + q->y + cat[q->item].h; if (kb < ka || (kb == ka && q->x < p->x)) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; } }
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
    static const int cyc[4] = {1, 0, 1, 2}; int order[NP + 1], no = 0;           /* walking frames 1,0,1,2; standing = middle frame. -1 = the trainer */
    for (int i = 0; i < nparty; i++) if (others[i].room == vroom) order[no++] = i;
    #define EY(k) ((k) < 0 ? tr.y : others[k].y)
    for (int a2 = 0; a2 < no; a2++) for (int b2 = a2 + 1; b2 < no; b2++) if (EY(order[b2]) < EY(order[a2])) { int tmp = order[a2]; order[a2] = order[b2]; order[b2] = tmp; }
    if (tr.room == vroom) order[no++] = -1;
    for (int a2 = 0; a2 < no; a2++) for (int b2 = a2 + 1; b2 < no; b2++) if (EY(order[b2]) < EY(order[a2])) { int tmp = order[a2]; order[a2] = order[b2]; order[b2] = tmp; }
    for (int k = 0; k < (g_bg_only ? 0 : no); k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * T + col * T, (ci / 4) * 4 * T + e->dir * T, T, T, OX + e->x * T, OY + e->y * T - 8); }
    if (!g_bg_only) { int tone[4]; if (read_tone(tone)) {      /* RPG Maker Tint Screen (events-hq tint_screen -> state/world/screen_state.pdl "value=r,g,b,gray"): add per channel, gray = pull toward the luminance; indoors it is only 40% */
        int od = rm->outdoor, pc = od ? 100 : 40, tr_ = tone[0] * pc / 100, tg_ = tone[1] * pc / 100, tb_ = tone[2] * pc / 100, gy = tone[3] * pc / 100;
        if (tr_ || tg_ || tb_ || gy) for (int y = OY + (od ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; int r = q[0], g = q[1], b = q[2], l = (r * 30 + g * 59 + b * 11) / 100;
            if (gy) { r += (l - r) * gy / 255; g += (l - g) * gy / 255; b += (l - b) * gy / 255; } r += tr_; g += tg_; b += tb_;
            q[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r); q[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g); q[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b); } }
      else { int mr = 256, mg = 256, mb = 256, od = rm->outdoor;      /* phase tint (clock events write daylight.txt): strong outside, only a mild dimming indoors */
      if (!strcmp(phase, "night")) { if (od) { mr = 110; mg = 120; mb = 190; } else { mr = 200; mg = 205; mb = 235; } } else if (!strcmp(phase, "dusk")) { if (od) { mr = 256; mg = 200; mb = 160; } else { mr = 256; mg = 235; mb = 215; } } else if (!strcmp(phase, "dawn")) { if (od) { mr = 256; mg = 215; mb = 185; } else { mr = 256; mg = 240; mb = 225; } }
      if (mr != 256 || mg != 256 || mb != 256) for (int y = OY + (od ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = (unsigned char)(q[0] * mr / 256); q[1] = (unsigned char)(q[1] * mg / 256); q[2] = (unsigned char)(q[2] * mb / 256); } } }
}

/* ---------- chat, relations ---------- */
/* the LAST `cap-1` bytes of a file, cut to start at a line boundary (the chat overlay shows the newest lines; reading the head froze it on the first 4000 bytes of a 66 KB log) */
static int read_tail(const char *p, char *buf, size_t cap) {
    FILE *f = fopen(p, "r"); if (!f) { buf[0] = 0; return 0; } fseek(f, 0, SEEK_END); long sz = ftell(f); long off = sz > (long)cap - 1 ? sz - ((long)cap - 1) : 0; fseek(f, off, SEEK_SET);
    size_t n = fread(buf, 1, cap - 1, f); fclose(f); buf[n] = 0; if (off > 0) { char *nl = strchr(buf, '\n'); if (nl) memmove(buf, nl + 1, strlen(nl + 1) + 1); } return (int)strlen(buf);
}
/* chat.txt is a rolling log: past ~24 KB keep only the newest ~8 KB */
static void chat_trim(void) {
    char p[PATH_MAX]; spath(p, "chat.txt"); struct stat st; if (stat(p, &st) || st.st_size < 24000) return;
    static char keep[9000]; read_tail(p, keep, sizeof keep); write_if_changed(p, keep, strlen(keep));      /* pid-named tmp + checked rename: a kill mid-trim or a full disk never leaves a half chat.txt */
}
static void say(const char *who, const char *text) { char b[300]; snprintf(b, sizeof b, "%s: %s", who, text); append_line("chat.txt", b); chat_trim(); }
static int g_rk = -1, g_ry;      /* what the last reply was about (0 sleep 1 eat 2 read 3 play, -1 default) and whether the house has such an item: the native-language reply is picked from these */
static const char *g_speaker = NULL;      /* who typed the line (the reply addresses them) */
static const char *pet_reply(const char *text) {
    static char b[80]; g_rk = -1; g_ry = 0; char s[200]; snprintf(s, sizeof s, "%s", text); for (char *c = s; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    #define HAS(e) ({ int h = 0; for (int i = 0; i < npl; i++) if (!strcmp(cat[pl[i].item].effect, e)) h = 1; h; })
    if (strstr(s, "bed") || strstr(s, "sleep")) { g_rk = 0; g_ry = HAS("sleep"); return g_ry ? "zzz... a bed!" : "I have no bed yet."; }
    if (strstr(s, "eat") || strstr(s, "food") || strstr(s, "table")) { g_rk = 1; g_ry = HAS("eat"); return g_ry ? "yum, a table to eat at" : "I have nowhere to eat."; }
    if (strstr(s, "book") || strstr(s, "read")) { g_rk = 2; g_ry = HAS("read"); return g_ry ? "I like reading." : "no books here."; }
    if (strstr(s, "piano") || strstr(s, "play")) { g_rk = 3; g_ry = HAS("play"); return g_ry ? "la la la" : "no piano..."; }
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Harold."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
        const char *w[] = {"hi", "hey", "hello", "yo", "hiya", "howdy", "sup", NULL}; for (int k = 0; w[k]; k++) { size_t n = strlen(w[k]); if (!strncmp(s, w[k], n) && (s[n] == 0 || s[n] == ' ' || s[n] == '!' || s[n] == ',')) { snprintf(b, sizeof b, "hello %s!", g_speaker ? g_speaker : hero.name); return b; } }
        if (strchr(s, '?')) return "good question... let me think.";
        return dflt[h % 4]; }
}
/* ---------- voices: offline Kokoro per pet (voices.pdl VOICE+KMAP rows; ops/pet_tts_local.py), cached wav, one sound at a time, never blocks ---------- */
static int run_quiet(char *const av[], int wait) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } signal(SIGPIPE, SIG_DFL); execvp(av[0], av[0] ? av : av); _exit(127); }
    if (!wait) return 0; return reap_timed(pid, 20);
}
/* each pet speaks its own language (voices.pdl 6th column: en | zh | ja | ko); the chat window keeps showing English for the owner to read */
static int pet_lang(int i) {
    char p[PATH_MAX], line[300], *f[7]; int l = 0; snprintf(p, sizeof p, "%s/voices.pdl", APP); FILE *fp = fopen(p, "r"); if (!fp) return 0;
    while (fgets(line, sizeof line, fp)) if (split_bar(line, f, 7) >= 6 && !strcmp(f[0], "VOICE") && !strcmp(f[1], party[i].id)) l = !strcmp(f[5], "zh") ? 1 : !strcmp(f[5], "ja") ? 2 : !strcmp(f[5], "ko") ? 3 : 0; fclose(fp); return l;
}
static const char *HI[4][4] = {{"hi %s!", "%s! over here", "oh, %s", "hello %s, nice day"}, {"你好，%s！", "%s，来这边", "哦，是%s", "你好%s，今天天气真好"}, {"こんにちは、%sさん", "%s、こっちだよ", "あ、%sだ", "やあ、%s、いい天気だね"}, {"안녕, %s!", "%s, 이쪽이야", "아, %s", "안녕 %s, 날씨 좋다"}};
static const char *RE[4][4] = {{"hi %s", "hey %s!", "hello", "%s, you too"}, {"你好，%s", "嘿，%s！", "你好", "%s，你也是"}, {"やあ、%s", "%sさん、こんにちは", "こんにちは", "%s、君もね"}, {"안녕 %s", "어, %s!", "안녕", "%s, 너도"}};
static const char *RP[4][5][2] = {{{"I have no bed yet.", "zzz... a bed!"}, {"I have nowhere to eat.", "yum, a table to eat at"}, {"no books here.", "I like reading."}, {"no piano...", "la la la"}, {"(%d things in the house)", "(%d things in the house)"}},
    {{"我还没有床。", "呼呼……有床了！"}, {"我没地方吃饭。", "好吃，有桌子可以吃饭"}, {"这里没有书。", "我喜欢看书。"}, {"没有钢琴……", "啦啦啦"}, {"家里有%d件东西", "家里有%d件东西"}},
    {{"まだベッドがないよ。", "ぐーぐー…ベッドだ！"}, {"ご飯を食べる場所がないよ。", "おいしい！テーブルがある"}, {"ここには本がないよ。", "本を読むのが好き。"}, {"ピアノがないなあ…", "らんらんらん"}, {"家に%d個の物があるよ", "家に%d個の物があるよ"}},
    {{"아직 침대가 없어.", "쿨쿨… 침대다!"}, {"밥 먹을 곳이 없어.", "맛있다, 식탁이 있네"}, {"여기엔 책이 없어.", "나는 책 읽는 게 좋아."}, {"피아노가 없네…", "랄랄라"}, {"집에 물건이 %d개 있어", "집에 물건이 %d개 있어"}}};
static void speak(int i, const char *text);
static void speak_greet(int i, int variant, const char *other, int reply) { char t[160]; const char *fmt = (reply ? RE : HI)[pet_lang(i)][variant & 3]; snprintf(t, sizeof t, fmt, other); speak(i, t); }
static void speak_reply(int i) { char t[160]; int k = g_rk < 0 ? 4 : g_rk; snprintf(t, sizeof t, RP[pet_lang(i)][k][g_rk < 0 ? 0 : g_ry ? 1 : 0], npl); speak(i, t); }
static void speak(int i, const char *text) {
    if (!flag_on("voice", 1) || i < 0 || !text || !text[0]) return; static long last = 0; long now = now_ms(); if (now - last < 2500) return; last = now;     /* a pet speaks aloud only while on screen */
    char p[PATH_MAX], line[300], *f[6], voice[48] = "en-US-AnaNeural", rate[16] = "+0%", pitch[16] = "+0Hz"; snprintf(p, sizeof p, "%s/voices.pdl", APP); FILE *fp = fopen(p, "r");
    if (fp) { while (fgets(line, sizeof line, fp)) if (split_bar(line, f, 6) >= 5 && !strcmp(f[0], "VOICE") && !strcmp(f[1], party[i].id)) { snprintf(voice, sizeof voice, "%s", f[2]); snprintf(rate, sizeof rate, "%s", f[3]); snprintf(pitch, sizeof pitch, "%s", f[4]); } fclose(fp); }
    unsigned long h = 5381; for (const char *c = text; *c; c++) h = h * 33 + (unsigned char)*c; char dir[PATH_MAX], wav[PATH_MAX], mp3[PATH_MAX]; snprintf(dir, sizeof dir, "%s/audio", ST); mkdir(dir, 0755);
    snprintf(wav, sizeof wav, "%s/say_%s_%08lx.wav", dir, party[i].id, h & 0xffffffffUL); snprintf(mp3, sizeof mp3, "%s/say_%s_%08lx.mp3", dir, party[i].id, h & 0xffffffffUL);
    /* one synthesis at a time, decided BEFORE the fork: each greeting is a fresh verb process, so a static timer cannot throttle; this lock is held by the grandchild until its wav exists, a speech that finds it taken is dropped, not queued (20 queued Kokoro processes at 200 MB each pegged the machine, 2026-10-16) */
    int sfd = -1; if (!fexists(wav)) { char sp[PATH_MAX]; snprintf(sp, sizeof sp, "%s/synth.busy", dir); sfd = open(sp, O_CREAT | O_RDWR, 0644); if (sfd < 0 || flock(sfd, LOCK_EX | LOCK_NB) != 0) { if (sfd >= 0) close(sfd); return; } }
    pid_t pid = fork(); if (pid < 0) { if (sfd >= 0) close(sfd); return; } if (pid > 0) { waitpid(pid, NULL, 0); if (sfd >= 0) close(sfd); return; }          /* double fork: the grandchild does the slow work and is reparented */
    if (fork() != 0) _exit(0); setsid();
    if (!fexists(wav)) { char rateA[40], pitchA[40], home[PATH_MAX], edge[PATH_MAX]; snprintf(rateA, sizeof rateA, "--rate=%s", rate); snprintf(pitchA, sizeof pitchA, "--pitch=%s", pitch); const char *hm = getenv("HOME"); snprintf(home, sizeof home, "%s", hm ? hm : ""); snprintf(edge, sizeof edge, "%s/../pet-trainer/ops/pet_tts_local.py", APP); (void)home;   /* offline Kokoro, same arguments as edge-tts (edge: HTTP 403 since 2026-10-12, rate-limit risk); writes a WAV into the "mp3" path, ffmpeg below re-encodes it */
        char *av[] = {"nice", "-n", "15", edge, "--voice", voice, rateA, pitchA, "--text", (char *)text, "--write-media", mp3, NULL}; if (run_quiet(av, 1) != 0 || !fexists(mp3)) _exit(0);
        char *cv[] = {"nice", "-n", "15", "ffmpeg", "-loglevel", "quiet", "-y", "-i", mp3, "-ar", "22050", "-ac", "1", wav, NULL}; run_quiet(cv, 1); remove(mp3); }
    if (sfd >= 0) close(sfd);      /* synthesis over: the next greeting may synthesize while this one plays */
    if (!fexists(wav)) _exit(0);
    { char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/play.lock", dir); int fd = open(lp, O_CREAT | O_RDWR, 0644); if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) _exit(0); char *pv[] = {"nice", "-n", "10", "paplay", "--volume=32000", wav, NULL}; run_quiet(pv, 1); }
    _exit(0);
}

/* association: two pets side by side greet each other (at most once per 25 real seconds per pair). Every meeting is an append-only MEET row: the data a later
 * "share a room because of a relationship" rule will read (see HOW-TO-RPG-PET.md, plan: a room per pet). */
static long last_meet(int a, int b) {
    char p[PATH_MAX], line[200]; spath(p, "relations.txt"); FILE *f = fopen(p, "r"); long best = 0; if (!f) return 0; char *g[6];
    while (fgets(line, sizeof line, f)) { if (split_bar(line, g, 6) >= 4 && !strcmp(g[0], "MEET")) { int x = atoi(g[2]), y = atoi(g[3]); if ((x == a && y == b) || (x == b && y == a)) best = atol(g[1]); } } fclose(f); return best;
}
static void meet(int a, int b) {
    long now = (long)time(NULL); if (now - last_meet(a, b) < 25) return;
    char l[200]; snprintf(l, sizeof l, "MEET|%ld|%d|%d|%s", now, a, b, rooms[others[a].room].id); append_line("relations.txt", l);
    int v1 = rand() % 4, v2 = rand() % 4; char t[120];       /* English in the chat, the pet's own language aloud */
    /* The greeting lines are DATA in event pages: events/greet_<pet>/ has a page per line (trigger hi0..hi3 = greets, re0..re3 = answers) whose pet_says command carries the chat line and the spoken line
     * ({other} = the other pet's name, passed as RPG_OTHER). The C keeps what must not move into a page: who may be heard (RPG_SAY / RPG_SPEAK), the 2.5 s voice throttle, and the old tables below as the fallback. */
    static long last_sp = 0; long nm = now_ms(); int sp_a = others[a].room == vroom, sp_b = others[b].room == vroom, sy_a = sp_a && (a == active || b == active), sy_b = sp_b && (a == active || b == active);
    if ((sp_a || sp_b) && nm - last_sp >= 2500) last_sp = nm; else sp_a = sp_b = 0;
    static long last_page = 0; int page_ok = now_ms() - last_page >= tune("greet_page_ms", 6000); if ((sy_a || sp_a || sy_b || sp_b) && page_ok) last_page = now_ms();      /* CPU safety: a page run is a sh + bash + prisc launch; at most one greeting page per 6 s (measured ~0.15 s of CPU each: play_event.sh fans out), the answer and every other greeting use the C text, which is identical */
    { char ev[64], tg[16], en[200]; snprintf(ev, sizeof ev, "greet_%s", party[a].id); snprintf(tg, sizeof tg, "hi%d", v1); snprintf(en, sizeof en, "RPG_OTHER='%s' RPG_SAY=%d RPG_SPEAK=%d", party[b].name, sy_a, sp_a);
      if ((sy_a || sp_a) && !(page_ok && run_event_x(ev, tg, en))) { snprintf(t, sizeof t, HI[0][v1], party[b].name); if (sy_a) say(party[a].name, t); if (sp_a) speak_greet(a, v1, party[b].name, 0); } }      /* a page run is a bash + prisc launch (~0.1 s): nothing is said when nobody is watching that room, so none is run */
    { char ev[64], tg[16], en[200]; snprintf(ev, sizeof ev, "greet_%s", party[b].id); snprintf(tg, sizeof tg, "re%d", v2); snprintf(en, sizeof en, "RPG_OTHER='%s' RPG_SAY=%d RPG_SPEAK=%d", party[a].name, sy_b, sp_b);
      if (sy_b || sp_b) { snprintf(t, sizeof t, RE[0][v2], party[a].name); if (sy_b) say(party[b].name, t); if (sp_b) speak_greet(b, v2, party[a].name, 1); } }
}
static void friend_line(int i, char *out, size_t cap) {
    char p[PATH_MAX], line[200]; int cnt[NP] = {0}; spath(p, "relations.txt"); FILE *f = fopen(p, "r"); char *g[6];
    if (f) { while (fgets(line, sizeof line, f)) { if (split_bar(line, g, 6) >= 4 && !strcmp(g[0], "MEET")) { int x = atoi(g[2]), y = atoi(g[3]); if (x == i && y >= 0 && y < NP) cnt[y]++; else if (y == i && x >= 0 && x < NP) cnt[x]++; } } fclose(f); }
    int b1 = -1, b2 = -1; for (int k = 0; k < nparty; k++) if (cnt[k] > 0) { if (b1 < 0 || cnt[k] > cnt[b1]) { b2 = b1; b1 = k; } else if (b2 < 0 || cnt[k] > cnt[b2]) b2 = k; }
    if (b1 < 0) snprintf(out, cap, "no friends met yet"); else if (b2 < 0) snprintf(out, cap, "friends: %s x%d", party[b1].name, cnt[b1]); else snprintf(out, cap, "friends: %s x%d, %s x%d", party[b1].name, cnt[b1], party[b2].name, cnt[b2]);
}

/* ---------- the clock (house livedesk clock, own root) ---------- */
static char LC[PATH_MAX], CROOT[PATH_MAX], CFILE[PATH_MAX];
static void clock_paths(void) { snprintf(LC, sizeof LC, "%s/&.widgits/livedesk-clock/ops/+x/lc_clock.+x", HOUSE); snprintf(CROOT, sizeof CROOT, "%s/clock_root", ST); snprintf(CFILE, sizeof CFILE, "%s/#.desktop/clocks/rpg.pdl", CROOT); }
static int clock_ok(void) { return fexists(LC); }
/* run lc_clock.+x <root> args... (NULL ended); daemon=1 gives it the event runner (this binary) so reminders call back into event-runner mode */
static int lc_run(int daemon, ...) {
    char *av[24]; int n = 0; va_list ap; va_start(ap, daemon); av[n++] = LC; av[n++] = CROOT; const char *a; while (n < 22 && (a = va_arg(ap, const char *))) av[n++] = (char *)a; va_end(ap); av[n] = NULL;
    /* HARDENING: a clock helper that hung once (killed after 3 s) is not called again for 20 s, so one stuck lc_clock costs one verb 3 s, never 3 s per call */
    static long dead_until; if (now_ms() < dead_until) return -1;
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("RPG_PET_ROOT", APP, 1); signal(SIGPIPE, SIG_DFL); execv(LC, av); _exit(127); }
    g_reap_to = 0; int rc_ = reap_timed(pid, 3); if (g_reap_to) dead_until = now_ms() + 20000; return rc_;
}
static long long clock_kv_ll(const char *key) { char b[2048]; read_file(CFILE, b, sizeof b); size_t kl = strlen(key); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, key, kl) && l[kl] == '=') return atoll(l + kl + 1); return -1; }
static void clock_kv_s(const char *key, char *out, size_t cap) { char b[2048]; out[0] = 0; read_file(CFILE, b, sizeof b); size_t kl = strlen(key); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out, cap, "%s", l + kl + 1); return; } }
static void write_phase(long long ms) { int h = (int)(ms / 3600000 % 24); const char *p = (h >= 19 || h < 6) ? "night" : h < 8 ? "dawn" : h >= 17 ? "dusk" : "day"; char b[40]; snprintf(b, sizeof b, "phase=%s\n", p); char f[PATH_MAX]; spath(f, "daylight.txt"); write_if_changed(f, b, strlen(b)); }
/* The game clock runs fast (a game day in seconds), so time_label changed ~0.6 times a second and EVERY change made the board window reparse + re-lay-out + redraw (about 67 ms each, measured with a redraw trace: it was
 * the biggest single source of the window's ~25% CPU). The label is written at most once per 5 s: a write that differs from the file ONLY in the time_label line (the first line) is skipped while the file is younger than 5 s. */
static void clock_ui_write(const char *p, const char *b) {
    char old[200]; int n = read_file(p, old, sizeof old);
    if (n > 0) { const char *o1 = strchr(old, '\n'), *b1 = strchr(b, '\n'); if (o1 && b1 && !strcmp(o1, b1)) { struct stat st; if (stat(p, &st) == 0 && time(NULL) - st.st_mtime < 5) return; } }
    write_if_changed(p, b, strlen(b));
}
static void clock_ui(void) {          /* state/clock_ui.txt: its own tiny vars file, so a ticking label never re-renders the scene */
    char p[PATH_MAX], b[160], rate[16], run[8]; long long ms = clock_kv_ll("game_time_epoch_ms"); spath(p, "clock_ui.txt");
    if (ms < 0) { snprintf(b, sizeof b, "time_label=--:--\ntime_rate=-\ntime_running=0\n"); write_if_changed(p, b, strlen(b)); return; }
    char d0b[32]; char dp[PATH_MAX]; spath(dp, "clock_day0.txt"); read_file(dp, d0b, sizeof d0b); long long d0 = atoll(d0b), d = ms / 86400000, s = (ms - d * 86400000) / 1000;
    clock_kv_s("rate", rate, sizeof rate); clock_kv_s("running", run, sizeof run);
    snprintf(b, sizeof b, "time_label=D%lld %02d:%02d\ntime_rate=%s\ntime_running=%s\n", d - d0 + 1, (int)(s / 3600), (int)(s % 3600 / 60), rate, run[0] ? run : "0"); clock_ui_write(p, b);
}
/* HARDENING: an amount handed to lc_clock (advance / rate / the TURN row of time.pdl) is 1-4 digits + a unit word, or a bare word; lc_clock 'advance 2147483648999m' spun for 10+ s of CPU */
static int lc_amount_ok(const char *s) { int d = 0, l = 0; while (s[d] >= '0' && s[d] <= '9') d++; if (d > 4) return 0; while (s[d + l] >= 'a' && s[d + l] <= 'z') l++; return s[d + l] == 0 && l >= 1 && l <= 8; }
static void turn_str(char *out, size_t cap) { char p[PATH_MAX], line[300]; char *f[6]; snprintf(p, sizeof p, "%s/time.pdl", APP); snprintf(out, cap, "5m"); FILE *fp = fopen(p, "r"); if (!fp) return; while (fgets(line, sizeof line, fp)) if (split_bar(line, f, 6) >= 3 && !strcmp(f[0], "TURN")) if (lc_amount_ok(f[2])) snprintf(out, cap, "%s", f[2]); fclose(fp); }
static void clock_install(void) {
    clock_paths(); if (!clock_ok()) return; char d[PATH_MAX]; snprintf(d, sizeof d, "%s/#.desktop", CROOT); mkdir(CROOT, 0755); mkdir(d, 0755); snprintf(d, sizeof d, "%s/#.desktop/clocks", CROOT); mkdir(d, 0755); snprintf(d, sizeof d, "%s/common_events", CROOT); mkdir(d, 0755);
    if (!fexists(CFILE)) { lc_run(0, "new", "rpg", "user", "rpg-pet time", NULL); lc_run(0, "rate", "rpg", "sec", NULL); lc_run(0, "cmd", "rpg", "settime", "21600000", "--source", "rpg-install", NULL);
        lc_run(0, "cmd", "rpg", "pause", "--source", "rpg-install", NULL); lc_run(0, "step", "5", NULL); char p[PATH_MAX]; spath(p, "clock_day0.txt"); write_if_changed(p, "0\n", 2); }
    char tp[PATH_MAX], tb[8192], sp[PATH_MAX], sb[40]; snprintf(tp, sizeof tp, "%s/time.pdl", APP); int tn = read_file(tp, tb, sizeof tb); unsigned long sig = 5381; for (int i = 0; i < tn; i++) sig = sig * 33 + (unsigned char)tb[i];
    snprintf(sp, sizeof sp, "%s/.schedule_installed", CROOT); read_file(sp, sb, sizeof sb);
    char want[40]; snprintf(want, sizeof want, "%lu", sig);
    if (strncmp(sb, want, strlen(want)) != 0 || !sb[0]) {
        char q[PATH_MAX]; snprintf(q, sizeof q, "%s/#.desktop/clocks/reminders.pdl", CROOT); remove(q); snprintf(q, sizeof q, "%s/#.desktop/clocks/schedule_ledger.txt", CROOT); remove(q);
        long long now = clock_kv_ll("game_time_epoch_ms"); if (now < 0) now = 0; char *f[6];
        for (char *l = strtok(tb, "\n"); l; l = strtok(NULL, "\n")) { char line[300]; snprintf(line, sizeof line, "%s", l); if (split_bar(line, f, 6) < 5 || strcmp(f[0], "AT")) continue;
            char when[40], ev[64], note[120], rep[40]; snprintf(ev, sizeof ev, "common:%s", f[1]); snprintf(note, sizeof note, "%s", f[4]); snprintf(rep, sizeof rep, "%s", f[3]);
            if (strchr(f[2], ':')) { int hh = atoi(f[2]), mm = atoi(strchr(f[2], ':') + 1); long long base = now - now % 86400000, at = base + hh * 3600000LL + mm * 60000LL; if (at <= now) at += 86400000; snprintf(when, sizeof when, "%lld", at); } else snprintf(when, sizeof when, "%s", f[2]);
            lc_run(0, "reminder-add", "rpg", when, ev, note, rep, NULL); }
        write_if_changed(sp, want, strlen(want)); }
}
static void clock_pause(int pause) { if (!clock_ok() || !fexists(CFILE)) return; lc_run(0, "cmd", "rpg", pause ? "pause" : "resume", "--source", "rpg-int", NULL); }
static int run_on(void) { return flag_on("running", 1); }
/* the clock runs only when the house is started AND INT is off (INT on = turn based: only your acts advance time) */
/* TIME MODE (one top-bar tab "[rogue] live"): rogue = turn time, while INT is on the clock moves only when you act and every other pet steps; live = the clock runs and pets walk on their timers even with INT on.
 * 2026-10-11: INT is the one switch: on = rogue (time moves only when you act, the other pets step with you), off = live (the clock runs, pets walk on their timers). The "mode" verb / tab now just toggles INT. */
static int rogue_mode(void) { return flag_on("int", 0); }      /* owner 2026-10-11: INT on = roguelike (turn based), INT off = auto / live; the old separate timemode flag is gone (with it set to live, INT on let every pet roam free) */
static int turn_mode(void) { return flag_on("int", 0) && rogue_mode(); }
static void clock_sync(void) { clock_paths(); clock_pause(!run_on() || turn_mode()); }
static void clock_start(void) { clock_paths(); if (!clock_ok()) return; clock_install(); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); clock_sync(); lc_run(1, "daemon-start", NULL); clock_ui(); }
static void clock_stop(void) { clock_paths(); if (!clock_ok() || !fexists(CFILE)) return; lc_run(0, "cmd", "rpg", "pause", "--source", "rpg-stop", NULL); usleep(1000000); lc_run(0, "daemon-stop", NULL); }
/* a roguelike turn's worth of game time: advance, then one deterministic pass so reminders fire now (no daemon wait) */
static void clock_turn(void) { clock_paths(); if (!clock_ok() || !fexists(CFILE)) return; char t[32]; turn_str(t, sizeof t); lc_run(0, "cmd", "rpg", "advance", t, "--source", "rpg-turn", NULL); lc_run(0, "step", "5", NULL); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); clock_ui(); }

/* ---------- outputs ---------- */
/* a pinned bar's text rows are one line high and never grow (the layout counts one row per text): long ui values are also written as <key>_0 .. <key>_1, split at spaces into pieces of at most 26 characters (2026-10-17) */
static int ui_wrap_keys(char *u, int o, size_t cap) {
    static const char *keys[] = { "nick_line", "friend_line", NULL };
    for (int k = 0; keys[k]; k++) {
        char pat[40]; snprintf(pat, sizeof pat, "%s=", keys[k]); char *s = strstr(u, pat); if (!s || (s != u && s[-1] != '\n')) continue;
        s += strlen(pat); char val[200]; size_t n = strcspn(s, "\n"); if (n >= sizeof val) n = sizeof val - 1; memcpy(val, s, n); val[n] = 0;
        char *p = val;
        for (int r = 0; r < 3; r++) { size_t len = strlen(p), cut = len; if (len > 26) { cut = 26; while (cut > 8 && p[cut] != ' ') cut--; if (p[cut] != ' ') cut = 26; }
            char piece[40]; snprintf(piece, sizeof piece, "%.*s", (int)cut, p); p += cut; while (*p == ' ') p++;
            if (o + 60 < (int)cap) o += snprintf(u + o, cap - o, "%s_%d=%s\n", keys[k], r, piece); }
    }
    return o;
}
static void out_all(const char *msg) {
    calc_view(); render();
    char p[PATH_MAX]; spath(p, "scene.raw"); if (write_if_changed(p, fr, sizeof fr)) g_scene_ver++;
    spath(p, "scene.receipt.txt"); { char b[64]; snprintf(b, sizeof b, "frame_w=%d\nframe_h=%d\n", FW, FH); write_if_changed(p, b, strlen(b)); }
    static char u[65536]; int o = 0;
#define W(...) do { if (o < (int)sizeof u - 1) { int w_ = snprintf(u + o, sizeof u - o, __VA_ARGS__); if (w_ > 0) o += w_; if (o > (int)sizeof u - 1) o = (int)sizeof u - 1; } } while (0)      /* HARDENING: snprintf returns the WOULD-BE length; unclamped it ran o past the buffer and wrapped size_t */
    char raw[PATH_MAX]; spath(raw, "scene.raw"); char h1[PATH_MAX], h2[PATH_MAX]; spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=rpg-pet  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
    { char cc[16]; flag_str("ctl", "pet", cc, sizeof cc); char who[40]; snprintf(who, sizeof who, "on: %s", !strcmp(cc, "player") ? "you" : party[active].name); W("interact_class=%s\ninteract_label=%s\nrp_h1=%s\nrp_h2=%s\ncontrol_label=control: %s\n", in ? "interact-active" : "", in ? who : "off", h1, h2, !strcmp(cc, "player") ? "player" : party[active].name); }
    W("run_on=%d\nrun_label=%s\nrun_cls=%s\n", run_on(), run_on() ? "GO started" : "STOP stopped", run_on() ? "ph-green" : "ph-red");
    W("mode_label=%s\nmode_cls=%s\n", rogue_mode() ? "[rogue] live" : "rogue [live]", rogue_mode() ? "ph-green" : "ph-red");
    W("left_visible=%s\n", flag_on("left", 1) ? "1" : "");      /* the pinned info bar (default on, Menu > Info show / hide, its _ button) */
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_tail(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
    W("n_shop=%d\n", ncat); for (int i = 0; i < ncat; i++) { char own[16] = ""; if (owned[i] > 0) snprintf(own, sizeof own, " x%d", owned[i]); W("shop_%d_label=%s %dc%s\nshop_%d_key=%s\n", i, cat[i].label, cat[i].price, own, i, cat[i].key); }
    char fl[160], cc[16], hid[24]; flag_str("ctl", "pet", cc, sizeof cc); int hs = !strcmp(cc, "player"); snprintf(hid, sizeof hid, "%s", hero.name); for (char *c = hid; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    if (hs) { int o = snprintf(fl, sizeof fl, "owns:"); for (int k = 0; k < nparty && o < (int)sizeof fl - 12; k++) o += snprintf(fl + o, sizeof fl - o, "%s %s", k ? "," : "", party[k].name); } else friend_line(active, fl, sizeof fl);
    const char *an = hs ? hero.name : party[active].name;
    W("n_party=%d\nactive_name=%s\nbook_label=book:%s\npage_label=page:%s\n", nparty + 1, an, an, rooms[vroom].name);
    W("stat_line=HP %d  MP %d  atk %d  def %d\nstat_line2=mat %d  mdf %d  agi %d  luk %d\nprofile_line=%s\nfriend_line=%s\n", hs ? hero.mhp : party[active].mhp, hs ? hero.mmp : party[active].mmp, hs ? hero.atk : party[active].atk, hs ? hero.def : party[active].def, hs ? hero.mat : party[active].mat, hs ? hero.mdf : party[active].mdf, hs ? hero.agi : party[active].agi, hs ? hero.luk : party[active].luk, hs ? hero.profile : party[active].profile, fl);
    if (hs) W("nick_line=%s (%s), owner of %d monsters, in the %s\n", hero.name, hero.nick, nparty, rooms[tr.room].name); else W("nick_line=%s (the %s), %s's monster, in the %s\n", party[active].name, party[active].nick, hero.name, rooms[others[active].room].name);
    { char wl[40]; if (hs) ent_wallet(hid, wl, sizeof wl); else pet_wallet(active, wl, sizeof wl); W("wallet_line=wallet %.14s...\n", wl); }
    W("party_0_label=%s\nparty_0_id=%s\nparty_0_cls=%s\n", hero.name, hid, hs ? "active nav-selected" : ""); for (int i = 0; i < nparty; i++) W("party_%d_label=%s\nparty_%d_id=%s\nparty_%d_cls=%s\n", i + 1, party[i].name, i + 1, party[i].id, i + 1, (!hs && i == active) ? "active nav-selected" : "");
    W("n_room=%d\n", nroom); for (int i = 0; i < nroom; i++) { int c = 0, pc = 0; for (int j = 0; j < npl; j++) if (pl[j].room == i) c++; for (int j = 0; j < nparty; j++) if (others[j].room == i) pc++; W("room_%d_label=%s%s (%d things, %d pets)\nroom_%d_id=%s\n", i, rooms[i].name, i == vroom ? "  <- here" : "", c, pc, i, rooms[i].id); }
    int nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) nb++; W("hb_n_slots=%d\nhb_title=bag: click a slot to place it\n", nb ? nb : 1); if (!nb) W("hb_0_text=(bag empty - Shop)\nhb_0_key=none\n"); nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) { W("hb_%d_text=%s x%d\nhb_%d_key=%s\n", nb, cat[i].label, owned[i], nb, cat[i].key); nb++; }
    int np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == vroom) np++; W("n_placed=%d\n", np); np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == vroom) { W("placed_%d_label=#%d %s (%d,%d) %s\nplaced_%d_id=%d\n", np, pl[i].id, cat[pl[i].item].label, pl[i].x, pl[i].y, cat[pl[i].item].effect, np, pl[i].id); np++; }
    o = ui_wrap_keys(u, o, sizeof u);
    { /* HARDENING: ui.txt is a complete key=value file whatever the state files held: only lines "key=value" (key = letters digits _) survive, control bytes in a value become spaces, a cut-off last line is dropped */
      int r_ = 0, w_ = 0; while (r_ < o) { int e_ = r_; while (e_ < o && u[e_] != '\n') e_++; if (e_ >= o) break; int k_ = r_; while (k_ < e_ && (isalnum((unsigned char)u[k_]) || u[k_] == '_')) k_++;
        if (k_ > r_ && k_ < e_ && u[k_] == '=') { for (int q_ = r_; q_ < e_; q_++) u[w_++] = ((unsigned char)u[q_] < 32) ? ' ' : u[q_]; u[w_++] = '\n'; } r_ = e_ + 1; } o = w_; }
    spath(p, "ui.txt"); write_if_changed(p, u, o);
}

/* ---------- walking: event steps ---------- */
static int passable(int x, int y) { return x >= 0 && x < COLS && y >= TRIG_ROW && y < ROWS && !floor_blocked(pet.room, x, y, 0) && !other_at(pet.room, x, y); }
static int free_near(int x, int y, int *ox, int *oy);
static void teleport(int di) {
    Door *d = &doors[di]; int di_from_room = d->room; char b[160]; snprintf(b, sizeof b, "DOOR|%ld|%s|%s|%s", (long)time(NULL), me < 0 ? "trainer" : party[me].id, rooms[d->room].id, rooms[d->dest].id); append_line("events.txt", b);
    pet.room = d->dest; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; pet.tx = pet.ty = -1; { int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; } }      /* land on the nearest FREE cell, not on whoever is already there */
    if (me >= 0 && me == active && (pet.room == vroom || di_from_room == vroom)) { snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say(party[me].name, b); }
}
/* one tile step of the mover; returns 1 if it moved. A step onto a door's trigger cell teleports (the door contract). The caller commits with pet_save(). */
/* the free cell nearest (x,y) in the mover's room (ring search): arrivals and unstacking use it so two pets never share a cell */
static int free_near(int x, int y, int *ox, int *oy) {
    for (int r = 0; r < COLS + ROWS; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) { if (abs(dx) + abs(dy) != r) continue; int cx = x + dx, cy = y + dy;
        if (cx >= 0 && cx < COLS && cy >= TRIG_ROW && cy < ROWS && !floor_blocked(pet.room, cx, cy, 0) && !other_at(pet.room, cx, cy) && door_here(pet.room, cx, cy) < 0) { *ox = cx; *oy = cy; return 1; } }
    return 0;
}
/* pets (and Harold) sharing a cell, from old saves or arrivals, are spread to free cells - a stacked pet used to be unable to path out */
static int unstack(void) {
    int moved = 0; for (int i = -1; i < nparty; i++) { use_ent(i); int shared = other_at(pet.room, pet.x, pet.y); if (!shared || (i < 0 && shared)) { if (i < 0 || !shared) continue; }
        if (i < 0) continue; int fx, fy; if (free_near(pet.x, pet.y, &fx, &fy)) { pet.x = fx; pet.y = fy; pet.tx = pet.ty = -1; pet_save(); moved++; } }
    use_ent(active); return moved; }
static int do_step(int dx, int dy, char *msg, size_t cap) {
    pet.dir = dy > 0 ? 0 : dx < 0 ? 1 : dx > 0 ? 2 : 3; int nx = pet.x + dx, ny = pet.y + dy;
    if (!passable(nx, ny)) { if (msg) snprintf(msg, cap, "bump"); return 0; }
    pet.x = nx; pet.y = ny; pet.steps++; char b[100]; snprintf(b, sizeof b, "STEP|%ld|%s|%s|%d|%d", (long)time(NULL), me < 0 ? "trainer" : party[me].id, rooms[pet.room].id, nx, ny); append_line("events.txt", b);
    int di = door_here(pet.room, nx, ny); if (di >= 0) { teleport(di); if (msg) snprintf(msg, cap, "door: now in the %s", rooms[pet.room].name); } else if (msg) snprintf(msg, cap, "at %d,%d", nx, ny);
    return 1;
}
/* breadth-first path over free cells; fills the first move toward (tx,ty) */
static int bfs_first(int tx, int ty, int *dx, int *dy) {
    int dist[ROWS][COLS], qx[ROWS * COLS], qy[ROWS * COLS], h = 0, t = 0; for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) dist[y][x] = -1;
    int fx[ROWS][COLS], fy[ROWS][COLS]; dist[ty][tx] = 0; qx[t] = tx; qy[t++] = ty; static const int mx[4] = {0, -1, 1, 0}, my[4] = {1, 0, 0, -1};
    while (h < t) { int x = qx[h], y = qy[h++]; for (int k = 0; k < 4; k++) { int nx = x + mx[k], ny = y + my[k]; if (nx < 0 || nx >= COLS || ny < TRIG_ROW || ny >= ROWS || dist[ny][nx] >= 0) continue; if (floor_blocked(pet.room, nx, ny, 0) || (other_at(pet.room, nx, ny) && !(nx == pet.x && ny == pet.y))) continue; dist[ny][nx] = dist[y][x] + 1; fx[ny][nx] = x; fy[ny][nx] = y; qx[t] = nx; qy[t++] = ny; } }
    if (dist[pet.y][pet.x] <= 0) return 0;      /* already there or unreachable */
    int nx = fx[pet.y][pet.x], ny = fy[pet.y][pet.x]; *dx = nx - pet.x; *dy = ny - pet.y; return 1;
}
/* where this pet wanders next: sometimes toward another pet (association), sometimes an auto door (then the walk ends in a teleport), otherwise a random free cell */
static void pick_target(void) {
    int r = rand() % 100;
    if (r < 32 && nparty > 1) { int j = rand() % nparty; if (j == me) j = (j + 1) % nparty;
        if (others[j].room == pet.room) { static const int nx[4] = {1, -1, 0, 0}, ny[4] = {0, 0, 1, -1}; for (int k = 0; k < 4; k++) { int cx = others[j].x + nx[k], cy = others[j].y + ny[k]; if (passable(cx, cy) && door_here(pet.room, cx, cy) < 0) { pet.tx = cx; pet.ty = cy; return; } } }
        else { int pick = -1; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && doors[i].dest == others[j].room) pick = i; if (pick < 0) for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow) pick = i; if (pick >= 0) { pet.tx = doors[pick].tx; pet.ty = TRIG_ROW; return; } } }
    if (r < 47) { int cand[8], nc = 0; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow && nc < 8) cand[nc++] = i; if (nc) { Door *d = &doors[cand[rand() % nc]]; pet.tx = d->tx; pet.ty = TRIG_ROW; return; } }
    for (int k = 0; k < 40; k++) { int x = rand() % COLS, y = TRIG_ROW + rand() % (ROWS - TRIG_ROW); if (passable(x, y) && door_here(pet.room, x, y) < 0) { pet.tx = x; pet.ty = y; return; } }
    pet.tx = pet.ty = -1;
}
/* ONE autonomous step of pet i: keep or pick a target, walk a tile, greet a neighbour. Pets are never owner controlled; this runs on a timer (INT off) or once per turn (INT on). */
static int ai_step(int i) {
    use_ent(i); int moved = 0;
    if (pet.tx >= 0 && pet.x == pet.tx && pet.y == pet.ty) pet.tx = pet.ty = -1;
    if (pet.tx < 0 && rand() % 100 < 45) pick_target();
    if (pet.tx >= 0) { int before = pet.room, dx, dy; if (bfs_first(pet.tx, pet.ty, &dx, &dy) && do_step(dx, dy, NULL, 0)) { moved = 1; if (pet.room != before) pet.tx = pet.ty = -1; } else pet.tx = pet.ty = -1; }
    pet_save();
    for (int j = 0; j < nparty; j++) if (j != i && others[j].room == others[i].room && abs(others[j].x - others[i].x) + abs(others[j].y - others[i].y) == 1) { meet(i < j ? i : j, i < j ? j : i); break; }
    return moved;
}
/* a roguelike turn: the clock moves on, and every pet takes its step */
static void end_turn(void) { if (!run_on() || !turn_mode()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */


/* ---------- 3D export: every room as a pc-hq map desk (same files as the TSOTS levels) ----------
 * @.apps/piececraft-hq/pieces/system/maps/rpg-pet/{game.pdl, <room>/{map.txt, extrusion.pdl, cells.txt, cells.rgba, cells.png, events.txt}}.  The board-viewer 3D renderer
 * (bv_render_3d) reads exactly these: map.txt = one glyph per cell (W wall 3 high, D door 3, T tree 2, f furniture 1, . floor), cells.txt = "floor_slot,wall_slot" per cell into an
 * atlas of 24 px tiles (slot 0 blank), events.txt = "x y r g b charset index dir pattern" (a coloured box per pet / Harold). All pictures are RPG Maker tiles downscaled 48 -> 24. */
#define XT 24
#define MAXSLOT 400
static unsigned char slotpx[MAXSLOT][XT * XT * 4]; static int nslot;
static void tile24(Sheet *s, int sx, int sy, unsigned char *out) {          /* 48x48 region -> 24x24 by 2x2 averaging; out is RGBA */
    for (int y = 0; y < XT; y++) for (int x = 0; x < XT; x++) { int acc[4] = {0, 0, 0, 0}, wsum = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) { int px = sx + x * 2 + dx, py = sy + y * 2 + dy; if (!s || px >= s->w || py >= s->h) continue; unsigned char *a = s->px + ((size_t)py * s->w + px) * 4; acc[0] += a[0] * a[3]; acc[1] += a[1] * a[3]; acc[2] += a[2] * a[3]; acc[3] += a[3]; wsum += a[3]; }
        unsigned char *o = out + (y * XT + x) * 4; if (wsum) { o[0] = acc[0] / wsum; o[1] = acc[1] / wsum; o[2] = acc[2] / wsum; o[3] = acc[3] / 4; } else o[0] = o[1] = o[2] = o[3] = 0; }
}
static void over24(unsigned char *dst, const unsigned char *src) { for (int i = 0; i < XT * XT; i++) { int al = src[i * 4 + 3]; if (!al) continue; for (int c = 0; c < 3; c++) dst[i * 4 + c] = (unsigned char)((src[i * 4 + c] * al + dst[i * 4 + c] * (255 - al)) / 255); dst[i * 4 + 3] = 255; } }
static int slot_of(const unsigned char *px) {
    int any = 0; for (int i = 0; i < XT * XT; i++) if (px[i * 4 + 3]) { any = 1; break; } if (!any) return 0;
    for (int i = 1; i < nslot; i++) if (!memcmp(slotpx[i], px, XT * XT * 4)) return i;
    if (nslot >= MAXSLOT) return 0; memcpy(slotpx[nslot], px, XT * XT * 4); return nslot++;
}
/* 3D sprite frames: the board-viewer 3D raymarcher extrudes a 16x24 RGBA frame per map event (8 voxels deep), looked up as <ASSETS>/../tsots-characters/frames/<charset>_<index>_<dir 2|4|6|8>_<pattern>.rgba
 * (found by walking up from the map folder; same files the 98 TSOTS desks use, tsots_events.py write_frame). No frame file = the flat colour box. A 3x4-frame cell of the 12x8 sheet, nearest-resized to 16x24. */
static void write_frame(const char *sh, int idx, int dir4, int pat) {
    Sheet *s = sheet("characters", sh); if (!s) return; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/../tsots-characters", ASSETS); mkdir(d, 0755); snprintf(d, sizeof d, "%s/../tsots-characters/frames", ASSETS); mkdir(d, 0755);
    static const int dc[4] = {2, 4, 6, 8}; snprintf(p, sizeof p, "%s/%s_%d_%d_%d.rgba", d, sh, idx, dc[dir4 & 3], pat); if (fexists(p)) return;
    int pw = s->w / 12, ph = s->h / 8; if (pw < 1 || ph < 1) return; int sx = ((idx % 4) * 3 + pat) * pw, sy = ((idx / 4) * 4 + dir4) * ph; unsigned char out[16 * 24 * 4];
    for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) { int px = sx + x * pw / 16, py = sy + y * ph / 24; memcpy(out + (y * 16 + x) * 4, s->px + ((size_t)py * s->w + px) * 4, 4); }
    FILE *f = fopen(p, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); }
}
/* the average-ish colour of a sprite frame (the 3D box fallback colour in events.txt): the centre pixel of the frame */
static void frame_rgb(Sheet *s, int idx, int dir4, unsigned char *px) {
    if (!s) return; int fx = (idx % 4) * 3 * T + T + T / 2, fy = (idx / 4) * 4 * T + dir4 * T + T / 2; if (fx < s->w && fy < s->h) memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4);
}
/* events.txt of one room for the 3D book: one row per monster / Harold in it, "x y r g b charset index dir pattern" (dir 2/4/6/8, pattern 0-2 = the walk frame, from the step count),
 * plus the 16x24 frame file for that exact facing and pattern. Rewritten only when the text changes (atomic tmp+rename in write_if_changed). */
/* map.png of one room for the pc-hq 2D view: the room painted by render() (floor, wall, doors, scenery, furniture), COLS*T x ROWS*T = 624 x 384 (tile step = width / map.txt columns = 48). Written only when the picture changed. */
static void write_room_png(int r) {
    static unsigned long last[16]; int saved = vroom; vroom = r; g_bg_only = 1; render(); g_bg_only = 0;
    static unsigned char crop[COLS * T * ROWS * T * 4]; unsigned long sum = 5381; for (int y = 0; y < ROWS * T; y++) memcpy(crop + (size_t)y * COLS * T * 4, fr + ((size_t)(OY + y) * FW + OX) * 4, COLS * T * 4);
    for (size_t i = 0; i < sizeof crop; i += 7) sum = sum * 33 + crop[i];
    if (r >= 0 && r < 16 && sum != last[r]) { last[r] = sum; char d[PATH_MAX], p[PATH_MAX]; snprintf(d, sizeof d, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/%s", HOUSE, rooms[r].id); mkdir(d, 0755); snprintf(p, sizeof p, "%s/map.png", d); png_atomic(p, COLS * T, ROWS * T, crop, COLS * T * 4); }
    vroom = saved; calc_view();
}
/* The 3D sky of an OUTDOOR room (the same picture the 2D painter uses, so 2D and 3D share one day / night cycle): maps/rpg-pet/<room>/parallax.rgba (raw RGBA8, the RPG Maker parallax image for the phase:
 * night StarlitSky, dusk and dawn Sunset, day CloudySky1) + parallax.pdl (show, width, height, loop_x, stamp). The screen tone events-hq's tint_screen wrote is applied to the pixels (outdoors 100%).
 * Rewritten only when the phase or the tone changes; the stamp row tells the 3D renderer to reload and re-upload. Indoor rooms: show 0 (the flat sky colour). */
static void write_room_sky(int r, const char *d) {
    static char lastkey[16][96]; if (r < 0 || r >= 16) return;
    char ph[40] = "day", dp[PATH_MAX], db[64]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strstr(db, "night")) snprintf(ph, sizeof ph, "night"); else if (strstr(db, "dusk")) snprintf(ph, sizeof ph, "dusk"); else if (strstr(db, "dawn")) snprintf(ph, sizeof ph, "dawn");
    int tone[4] = {0, 0, 0, 0}, ht = read_tone(tone); char key[96]; snprintf(key, sizeof key, "%d|%s|%d,%d,%d,%d,%d", rooms[r].outdoor, ph, ht, tone[0], tone[1], tone[2], tone[3]);
    if (!strcmp(key, lastkey[r])) return; snprintf(lastkey[r], sizeof lastkey[r], "%s", key);
    char pp[PATH_MAX], rp[PATH_MAX]; snprintf(pp, sizeof pp, "%s/parallax.pdl", d); snprintf(rp, sizeof rp, "%s/parallax.rgba", d);
    Sheet *sk = rooms[r].outdoor ? sheet("parallaxes", !strcmp(ph, "night") ? "StarlitSky" : (!strcmp(ph, "dusk") || !strcmp(ph, "dawn")) ? "Sunset" : "CloudySky1") : NULL;
    if (!sk || !sk->px || sk->w < 1 || sk->h < 1 || sk->w > 4096 || sk->h > 4096) { static const char off[] = "PARALLAX | show | 0\nPARALLAX | name | \n"; write_if_changed(pp, off, sizeof off - 1); return; }
    size_t n = (size_t)sk->w * sk->h * 4; unsigned char *b = (unsigned char *)malloc(n); if (!b) return; memcpy(b, sk->px, n);
    if (ht) for (size_t i = 0; i < n; i += 4) { int rr = b[i], gg = b[i + 1], bb = b[i + 2], l = (rr * 30 + gg * 59 + bb * 11) / 100, gy = tone[3];
        if (gy) { rr += (l - rr) * gy / 255; gg += (l - gg) * gy / 255; bb += (l - bb) * gy / 255; } rr += tone[0]; gg += tone[1]; bb += tone[2];
        b[i] = (unsigned char)(rr < 0 ? 0 : rr > 255 ? 255 : rr); b[i + 1] = (unsigned char)(gg < 0 ? 0 : gg > 255 ? 255 : gg); b[i + 2] = (unsigned char)(bb < 0 ? 0 : bb > 255 ? 255 : bb); }
    { char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", rp); FILE *fp = fopen(tmp, "wb"); if (fp) { size_t wn_ = fwrite(b, 1, n, fp); if (fclose(fp) == 0 && wn_ == n) rename(tmp, rp); else remove(tmp); } } free(b);
    char pd[240]; int po = snprintf(pd, sizeof pd, "PARALLAX | show | 1\nPARALLAX | name | %s\nPARALLAX | width | %d\nPARALLAX | height | %d\nPARALLAX | loop_x | 1\nPARALLAX | stamp | %ld\n", ph, sk->w, sk->h, (long)now_ms());
    { char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", pp); FILE *fp = fopen(tmp, "w"); if (fp) { size_t wn_ = fwrite(pd, 1, (size_t)po, fp); if (fclose(fp) == 0 && wn_ == (size_t)po) rename(tmp, pp); else remove(tmp); } }
}
static void write_room_events(int r) {
    char d[PATH_MAX], p[PATH_MAX], buf[4096]; int o = 0; static const int dc[4] = {2, 4, 6, 8};
    snprintf(d, sizeof d, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/%s", HOUSE, rooms[r].id); mkdir(d, 0755);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) { int pat = others[i].steps % 3; write_frame(party[i].sheet, party[i].idx, others[i].dir, pat); unsigned char px[4] = {128, 128, 128, 255}; frame_rgb(sheet("characters", party[i].sheet), party[i].idx, others[i].dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s %d %d %d %c\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, party[i].idx, dc[others[i].dir & 3], pat, party[i].name[0] >= 'A' && party[i].name[0] <= 'Z' ? party[i].name[0] + 32 : (party[i].name[0] ? party[i].name[0] : 'm')); }
    if (tr.room == r) { int pat = tr.steps % 3; write_frame(hero.sheet, 0, tr.dir, pat); unsigned char px[4] = {200, 60, 60, 255}; frame_rgb(sheet("characters", hero.sheet), 0, tr.dir & 3, px);
        o += snprintf(buf + o, sizeof buf - o, "%d %d %d %d %d %s 0 %d %d @\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3], pat); }
    snprintf(p, sizeof p, "%s/events.txt", d); write_if_changed(p, buf, o);
    write_room_sky(r, d);
    /* ascii_legend.txt: the ` (backtick) glyph view of this book. Rows are glyph|height|r|g|b|asset|name|char like terrain_legend.txt; events.txt carries a 10th column with each sprite's letter (@ = the trainer, the pet's lowercase initial) */
    static const char leg[] = "W|3|150|150|160|-|wall|#\nD|3|170|110|50|-|door|+\nT|2|50|150|60|-|tree|T\nf|1|190|150|90|-|furniture|&\n.|0|90|80|60|-|floor|.\n";
    snprintf(p, sizeof p, "%s/ascii_legend.txt", d); write_if_changed(p, leg, sizeof leg - 1);
    { char bp[PATH_MAX]; snprintf(bp, sizeof bp, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet/view.pdl", HOUSE); char vw[PATH_MAX + 160]; char sc[PATH_MAX]; spath(sc, "scene.raw"); int vn = snprintf(vw, sizeof vw, "VIEW | cell_px | 48\nVIEW | default_style | scene\nVIEW | scene_raw | %s\nVIEW | scene_w | %d\nVIEW | scene_h | %d\n", sc, FW, FH); write_if_changed(bp, vw, (size_t)vn); }      /* the engine draws this book's 2D at rpg-pet's own 48 px tile (house desk_grid.pdl says 80) */
    /* bar.txt: the bottom entities bar of the pc-hq board lists these (id<TAB>name<TAB>x<TAB>y<TAB>trigger), Harold first */
    char bb[2048]; int bo = 0, id = 1; if (tr.room == r) bo += snprintf(bb + bo, sizeof bb - bo, "%d\t%s\t%d\t%d\ton-click\n", id++, hero.name, tr.x, tr.y);
    for (int i = 0; i < nparty; i++) if (others[i].room == r) bo += snprintf(bb + bo, sizeof bb - bo, "%d\t%s\t%d\t%d\ton-click\n", id++, party[i].name, others[i].x, others[i].y);
    snprintf(p, sizeof p, "%s/bar.txt", d); write_if_changed(p, bb, bo);
}
/* ---------- the pc-hq 3D view follows the SELECTED entity ----------
 * pc-hq's camera (modes 1 and 2) anchors on the xelector's pos_x/pos_y, and for a painted map the renderer stands the xelector on the floor as the same box as a sprite. So following is state files:
 * @.apps/piececraft-hq/pieces/xelector_01/state.txt and hero_01/state.txt get the selected entity's cell, the pc-hq desk is switched (inbox CONFIRM_SET_DESK) when it changes room, and the
 * board-viewer session is nudged to redraw (append to its interact_relay.txt: code 1 is unbound, a pure "render now"). Only when the selected entity's room/cell/facing changed, so a
 * human driving the pc-hq xelector by hand between steps is not fought. Does nothing unless pc-hq has the rpg-pet book open. */
static int kv_set_int(const char *path, const char *key, int val) {
    char b[8192]; int n = read_file(path, b, sizeof b); if (n <= 0) return 0; char out[8300]; int o = 0, found = 0; size_t kl = strlen(key);
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, key, kl) && l[kl] == '=') { o += snprintf(out + o, sizeof out - o, "%s=%d\n", key, val); found = 1; } else o += snprintf(out + o, sizeof out - o, "%s\n", l); }
    if (!found) o += snprintf(out + o, sizeof out - o, "%s=%d\n", key, val); return write_if_changed(path, out, o);
}
/* the engine keeps its own cursor (selector_x / selector_y in the session's bv_state.txt) and an arrow key moves THAT by one and then rewrites the xelector from it: if it is not on the monster the first
 * arrow jumps the xelector somewhere stale. Put it on the monster whenever the xelector is put there. */
static void sync_selector(int x, int y) {
    char up[PATH_MAX], ub[1024], sess[PATH_MAX] = "", bp[PATH_MAX]; snprintf(up, sizeof up, "%s/@.apps/rpg-pet/board/state/ui.txt", HOUSE); if (!fexists(up)) return; read_file(up, ub, sizeof ub);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0]) return; snprintf(bp, sizeof bp, "%s/pieces/system/bv_state.txt", sess); if (!fexists(bp)) return; kv_set_int(bp, "selector_x", x); kv_set_int(bp, "selector_y", y);
}
/* CPU safety: follow_pchq() exports the rooms and wakes the board engine (a redraw key into its relay) every time the scene changes. With NO board window open that drove a headless engine at several
 * redraws a second (chtpm_rgb_render 7% + prisc 3-4% of a core, measured, for hours after the window was closed). Only follow when a window of a board layout (pchq-board.xhtpm: the rpg-pet board, pc-hq
 * or the layout-studio sandbox) is really running; the answer is cached for 2 s (one /proc scan). */
static int viewer_kinds(void) {
    static long last = 0; static int v = 3; long t = now_ms(); if (last && t - last < tune("viewer_scan_ms", 2000)) return v; last = t; v = 0;
    DIR *d = opendir("/proc"); if (!d) return 3; struct dirent *e;
    while ((e = readdir(d))) { if (e->d_name[0] < '1' || e->d_name[0] > '9') continue; char p[64], b[2048]; snprintf(p, sizeof p, "/proc/%s/cmdline", e->d_name); FILE *f = fopen(p, "r"); if (!f) continue; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
        if (!strstr(b, "khtpm_core_render")) continue; for (size_t i = 0; i < n; i++) if (!b[i]) b[i] = ' '; if (strstr(b, "pchq-board.xhtpm") && !strstr(b, SIBLING_APP)) v |= 1; if (strstr(b, "rpg-pet.xhtpm")) v |= 2; }
    closedir(d); return v;
}
static int board_window_open(void) { return viewer_kinds() & 1; }
static int g_pin_now = 0;      /* set when a followed step bumped: follow_pchq puts the xelector back on the monster at once */
static void board_interact(int want, int force);
/* The sun and moon of the 3D view (pieces/sun_01, moon_01) are a pure function of the pc-hq world clock: world_01/state.txt game_time_epoch_sec, turned into positions (and the sky light level) by the
 * already running pc_clock_daemon. That clock is parked at noon (autotick_enabled=0), so they never moved. While the rpg-pet book is open rpg-pet publishes ITS game time there ("D55 15:41" in
 * clock_ui.txt -> epoch = a midnight base + day * 86400 + h * 3600 + m * 60), so the sun and moon follow the same clock the phase events and the tint follow: sunrise at dawn, noon at 12:00, the moon
 * opposite. Written only when the minute changes. */
static void publish_sky(const char *wp) {
    char p[PATH_MAX], b[1024]; spath(p, "clock_ui.txt"); if (read_file(p, b, sizeof b) <= 0) return; char *t = strstr(b, "time_label=D"); if (!t) return; int d = 0, h = 0, m = 0; if (sscanf(t + 12, "%d %d:%d", &d, &h, &m) != 3) return;
    long long sec = 1767225600LL + (long long)d * 86400 + h * 3600 + m * 60; char v[40], v2[40]; snprintf(v, sizeof v, "%lld", sec); snprintf(v2, sizeof v2, "%lld", sec * 1000);
    { char o[4096], x[4096]; read_file(wp, x, sizeof x); int n = 0, f1 = 0, f2 = 0; for (char *l = strtok(x, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "game_time_epoch_sec=", 20)) { n += snprintf(o + n, sizeof o - n, "game_time_epoch_sec=%s\n", v); f1 = 1; } else if (!strncmp(l, "game_time_epoch_ms=", 19)) { n += snprintf(o + n, sizeof o - n, "game_time_epoch_ms=%s\n", v2); f2 = 1; } else n += snprintf(o + n, sizeof o - n, "%s\n", l); }
      if (!f1) n += snprintf(o + n, sizeof o - n, "game_time_epoch_sec=%s\n", v); if (!f2) n += snprintf(o + n, sizeof o - n, "game_time_epoch_ms=%s\n", v2); write_if_changed(wp, o, (size_t)n); }
}
static void follow_pchq(void) {
    char wp[PATH_MAX], xp[PATH_MAX], hp[PATH_MAX], m[64] = "", dk[64] = ""; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE);
    char wb[2048]; read_file(wp, wb, sizeof wb); for (char *l = strtok(wb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "map_id=", 7)) snprintf(m, sizeof m, "%s", l + 7); else if (!strncmp(l, "desk_id=", 8)) snprintf(dk, sizeof dk, "%s", l + 8); }
    if (strcmp(m, "rpg-pet")) return;
    publish_sky(wp);
    for (int r = 0; r < nroom; r++) write_room_events(r);
    { static unsigned long hs = 0; unsigned long s2 = 5381 + npl; for (int i = 0; i < npl; i++) s2 = s2 * 33 + pl[i].item * 131 + pl[i].x * 17 + pl[i].y * 7 + pl[i].room; if (s2 != hs) { hs = s2; for (int r = 0; r < nroom; r++) write_room_png(r); } }
    snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); snprintf(hp, sizeof hp, "%s/@.apps/piececraft-hq/pieces/hero_01/state.txt", HOUSE);
    int ci = ctl_idx(); Ent *e = ci < 0 ? &tr : &others[ci]; static int lr = -1, lx = -1, ly = -1, ld = -1, lw = -1; int changed = 0;
    int pin = g_pin_now; g_pin_now = 0;
    if (pin || e->room != lr || e->x != lx || e->y != ly) { lr = e->room; lx = e->x; ly = e->y; changed = 1;
        kv_set_int(xp, "pos_x", e->x); kv_set_int(xp, "pos_y", e->y); kv_set_int(hp, "pos_x", e->x); kv_set_int(hp, "pos_y", e->y); sync_selector(e->x, e->y); }
    if (strcmp(dk, rooms[e->room].id) && lw != e->room) { lw = e->room; char ib[PATH_MAX], c[96]; snprintf(ib, sizeof ib, "%s/@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt", HOUSE); snprintf(c, sizeof c, "CONFIRM_SET_DESK:%s", rooms[e->room].id); FILE *f = fopen(ib, "a"); if (f) { fprintf(f, "%s\n", c); fclose(f); } changed = 1; }
    if (!strcmp(dk, rooms[e->room].id)) lw = e->room;
    /* any pet/Harold stepping changes the sprites too: redraw when the events text changed (write_room_events reports via the checksum below) */
    static unsigned long ev_sum = 0; unsigned long s = 0; for (int i = 0; i < nparty; i++) s = s * 131 + (others[i].room * 97 + others[i].x * 31 + others[i].y * 7 + others[i].dir + others[i].steps % 3); s = s * 131 + tr.x * 31 + tr.y + tr.room; if (s != ev_sum) { ev_sum = s; changed = 1; } (void)ld;
    { static int last_scene = -1; if (last_scene != g_scene_ver) { last_scene = g_scene_ver; changed = 1; } }      /* the painted scene changed (a sprite stepped, the tone changed): the engine shows it in its "scene" 2D style, so wake it */
    if (changed) {      /* nudge the LIVE engine session so it redraws (an idle engine only renders on input): every board window has its own ui.txt with bv_session=; the one whose session dir exists and whose ui.txt is the newest wins (a stale file points at a dead session). The nudge key is 126 (tilde), bound to nothing, so the view and camera are never touched (the old "1" is the POV key). */
        const char *cand[3] = {"@.apps/rpg-pet/board/state/ui.txt", "@.apps/piececraft-hq/state/ui.txt", "@.apps/layout-studio/sandbox/state/ui.txt"}; char best[PATH_MAX] = ""; time_t bt = 0;
        for (int k = 0; k < 3; k++) { char up[PATH_MAX], ub[1024], sess[PATH_MAX] = "", rp[PATH_MAX]; struct stat us; snprintf(up, sizeof up, "%s/%s", HOUSE, cand[k]); if (stat(up, &us)) continue; read_file(up, ub, sizeof ub);
            for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
            snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess); if (sess[0] && fexists(rp) && (!best[0] || us.st_mtime > bt)) { snprintf(best, sizeof best, "%s", rp); bt = us.st_mtime; } }
        if (best[0]) { FILE *f = fopen(best, "a"); if (f) { fputs("126\n", f); fclose(f); } } }
}
static void export3d(char *msg, size_t cap) {
    char root[PATH_MAX], d[PATH_MAX], p[PATH_MAX]; snprintf(root, sizeof root, "%s/@.apps/piececraft-hq/pieces/system/maps/rpg-pet", HOUSE);
    { char a[PATH_MAX]; snprintf(a, sizeof a, "%s/@.apps/piececraft-hq/pieces/system/maps", HOUSE); mkdir(a, 0755); } mkdir(root, 0755);
    char gl[8192]; int go = 0; go += snprintf(gl + go, sizeof gl - go, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nGAME         | type               | board-game\nGAME         | icon               | \xF0\x9F\x8F\xA0\nGAME         | label              | RPG-Pet\nGAME         | n_chunks           | 1\nGAME         | chunk_0_x          | 0\nGAME         | chunk_0_y          | 0\nGAME         | n_desks            | %d\n", nroom);
    for (int r = 0; r < nroom; r++) {
        go += snprintf(gl + go, sizeof gl - go, "GAME         | desk_%d_id          | %s\nGAME         | desk_%d_label       | %s\n", r + 1, rooms[r].id, r + 1, rooms[r].name);
        snprintf(d, sizeof d, "%s/%s", root, rooms[r].id); mkdir(d, 0755);
        char glyph[ROWS][COLS + 1]; int fs[ROWS][COLS], ws[ROWS][COLS]; unsigned char a[XT * XT * 4], b[XT * XT * 4], fl[XT * XT * 4]; nslot = 1; memset(slotpx[0], 0, XT * XT * 4);
        Room *rm = &rooms[r]; Sheet *a5i = sheet("tilesets", "Inside_A5"), *a5o = sheet("tilesets", "Outside_A5"), *dr = sheet("characters", "!Door1");
        unsigned char floorpx[XT * XT * 4], wallpx[XT * XT * 4], ceilpx[XT * XT * 4], doorpx[XT * XT * 4], pathpx[XT * XT * 4], matpx[XT * XT * 4];
        if (rm->outdoor) { tile24(a5o, rm->fc * T, rm->fr * T, floorpx); tile24(a5o, 2 * T, 0, wallpx); memcpy(ceilpx, wallpx, sizeof ceilpx); tile24(a5o, 1 * T, 2 * T, pathpx); }
        else { tile24(a5i, rm->fc * T, rm->fr * T, floorpx); tile24(a5i, 1 * T, 3 * T, wallpx); tile24(a5i, 0, 0, ceilpx); memcpy(pathpx, floorpx, sizeof pathpx); }
        tile24(dr, 0, 192, doorpx); tile24(a5i, 0, 6 * T, matpx);
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { glyph[y][x] = '.'; fs[y][x] = ws[y][x] = 0; }
        for (int x = 0; x < COLS; x++) { glyph[0][x] = glyph[1][x] = 'W'; ws[0][x] = slot_of(ceilpx); ws[1][x] = slot_of(wallpx); }
        for (int y = TRIG_ROW; y < ROWS; y++) for (int x = 0; x < COLS; x++) fs[y][x] = slot_of(floorpx);
        for (int i = 0; i < ndoor; i++) if (doors[i].room == r) { int tx = doors[i].tx; memcpy(a, wallpx, sizeof a); over24(a, doorpx); glyph[1][tx] = 'D'; ws[1][tx] = slot_of(a);
            if (rm->outdoor) { for (int y = TRIG_ROW; y < ROWS - 1; y++) fs[y][tx] = slot_of(pathpx); } else fs[TRIG_ROW][tx] = slot_of(matpx); }
        for (int i = 0; i < nscen; i++) if (scen[i].room == r) for (int ci = 0; ci < scen[i].w; ci++) for (int cj = 0; cj < scen[i].h; cj++) { int x = scen[i].x + ci, y = scen[i].y + cj; if (x < 0 || x >= COLS || y < 0 || y >= ROWS) continue;
            Sheet *s = sheet("tilesets", scen[i].sheet); memcpy(b, floorpx, sizeof b); tile24(s, (scen[i].c + ci) * T, (scen[i].r + cj) * T, a); over24(b, a); glyph[y][x] = scen[i].h >= 2 ? 'T' : 'f'; ws[y][x] = slot_of(b); }
        for (int i = 0; i < npl; i++) if (pl[i].room == r) { Item *it = &cat[pl[i].item]; Sheet *s = sheet("tilesets", it->sheet);
            for (int ci = 0; ci < it->w; ci++) for (int cj = 0; cj < it->h; cj++) { int x = pl[i].x + ci, y = pl[i].y + cj; if (x < 0 || x >= COLS || y < 0 || y >= ROWS) continue;
                tile24(s, (it->c + ci) * T, (it->r + cj) * T, a);
                if (it->wall) { memcpy(b, wallpx, sizeof b); over24(b, a); ws[y][x] = slot_of(b); } else { memcpy(b, floorpx, sizeof b); over24(b, a); glyph[y][x] = 'f'; ws[y][x] = slot_of(b); } } }
        (void)fl;
        snprintf(p, sizeof p, "%s/map.txt", d); { FILE *f = fopen_tmp(p); if (f) { for (int y = 0; y < ROWS; y++) { glyph[y][COLS] = 0; fprintf(f, "%s\n", glyph[y]); } fclose_tmp(f, p); } }
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | rpg-pet\nMETA         | desk_id            | %s\nMETA         | note               | %s (rpg-pet house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose_tmp(f, p); } }
        int cols = 16, nrows = (nslot + cols - 1) / cols; int aw = cols * XT, ah = nrows * XT; unsigned char *atlas = calloc((size_t)aw * ah * 4, 1);
        for (int i = 0; i < nslot; i++) for (int y = 0; y < XT; y++) memcpy(atlas + ((size_t)((i / cols) * XT + y) * aw + (i % cols) * XT) * 4, slotpx[i] + y * XT * 4, XT * 4);
        snprintf(p, sizeof p, "%s/cells.rgba", d); { FILE *f = fopen_tmp(p); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose_tmp(f, p); } } snprintf(p, sizeof p, "%s/cells.png", d); png_atomic(p, aw, ah, atlas, aw * 4); free(atlas);
        snprintf(p, sizeof p, "%s/cells.txt", d); { FILE *f = fopen_tmp(p); if (f) { fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\ncells\n", COLS, ROWS, XT, nslot); for (int y = 0; y < ROWS; y++) { for (int x = 0; x < COLS; x++) fprintf(f, "%s%d,%d", x ? " " : "", fs[y][x], ws[y][x]); fputc('\n', f); } fclose_tmp(f, p); } }
        write_room_events(r); write_room_png(r);
    }
    snprintf(p, sizeof p, "%s/game.pdl", root); { FILE *f = fopen_tmp(p); if (f) { fputs(gl, f); fclose_tmp(f, p); } }
    snprintf(msg, cap, "exported %d rooms as a pc-hq book: maps/rpg-pet (open it in pc-hq: Desk > rpg-pet, then 1-4 / 0 for 3D)", nroom);
}

/* ---------- verbs ---------- */
static void clock_event(const char *ev);
static int copy_file(const char *from, const char *to) { FILE *in = fopen(from, "rb"); if (!in) return 0; char tmpc[PATH_MAX + 24]; snprintf(tmpc, sizeof tmpc, "%s.tmp.%d", to, (int)getpid()); FILE *out = fopen(tmpc, "wb"); if (!out) { fclose(in); return 0; } char buf[8192]; size_t n; int ok = 1; while ((n = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, n, out) != n) { ok = 0; break; } fclose(in); if (fclose(out) != 0) ok = 0; if (ok && rename(tmpc, to) != 0) ok = 0; if (!ok) remove(tmpc); return ok; }
static void verb(int argc, char **argv, char *msg, size_t cap) {
    const char *v = argc ? argv[0] : "status"; long now = (long)time(NULL); char b[200]; msg[0] = 0; use_ent(active);
    if (!strcmp(v, "status")) return;
    if (!strcmp(v, "pet_says") && argc >= 4) {      /* the registered pet_says command of the greet pages: pet_says <pet id> <chat line> <spoken line> [other]; {other} is replaced; RPG_SAY / RPG_SPEAK say whether it is heard (chat / voice) */
        int pi = -1; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, argv[1])) pi = i; if (pi < 0) { snprintf(msg, cap, "no such pet"); return; }
        const char *oth = argc >= 5 ? argv[4] : ""; char ch[200], vo[200]; for (int k = 0; k < 2; k++) { const char *s = argv[2 + k]; char *o = k ? vo : ch; size_t n = 0; while (*s && n < 190) { if (!strncmp(s, "{other}", 7)) { n += (size_t)snprintf(o + n, 200 - n, "%s", oth); s += 7; } else o[n++] = *s++; } o[n < 199 ? n : 199] = 0; }
        const char *es = getenv("RPG_SAY"), *ek = getenv("RPG_SPEAK"); if (!es || es[0] == '1') say(party[pi].name, ch); if (!ek || ek[0] == '1') speak(pi, vo); snprintf(msg, cap, "%s said it", party[pi].name); return; }
    if (!strcmp(v, "sync")) { { char sb[8]; const char *nm[2] = {"running", "int"}; for (int k = 0; k < 2; k++) if (sw_get(nm[k], sb, sizeof sb) && sb[0]) { char fp[PATH_MAX], fn[64], fv[16]; snprintf(fn, sizeof fn, "flag_%s.txt", nm[k]); spath(fp, fn); snprintf(fv, sizeof fv, "%c\n", sb[0]); write_if_changed(fp, fv, strlen(fv)); } }      /* keep the old flag files in step with the switches */
        clock_paths(); if (run_on()) clock_install(); clock_sync(); if (run_on()) lc_run(1, "daemon-start", NULL); snprintf(msg, cap, "synced"); return; }      /* the clock follows the running / int switches (the last node of the play and stop event pages) */
    if (!strcmp(v, "phase") && argc >= 2) { clock_event(argv[1]); snprintf(msg, cap, "phase %s", argv[1]); return; }
    if (!strcmp(v, "day_tick")) { clock_event("day_tick"); snprintf(msg, cap, "new day"); return; }
    if (!strcmp(v, "newgame")) {      /* a fresh game: the current one is saved to slot 0 first (Undo new game = Load slot 0), then every game file is removed so the next read starts from the defaults. The clock is not touched. */
        char m2[200] = ""; char *sv[] = { (char *)"save", (char *)"0", NULL }; verb(2, sv, m2, sizeof m2);
        char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%s/slots/0/events.txt", ST); if (!fexists(sd)) { snprintf(msg, cap, "new game NOT started: the backup to slot 0 failed (%s)", m2); return; }
        static const char *fixed[] = { "house.txt", "trainer.txt", "relations.txt", "chat.txt", "active.txt", "daylight.txt", "flag_ctl.txt", "flag_follow.txt", "events.txt" };
        for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++) { char p[PATH_MAX]; spath(p, fixed[i]); remove(p); }
        for (int i = 0; i < nparty; i++) { char p[PATH_MAX], nm[48]; snprintf(nm, sizeof nm, "pet_%s.txt", party[i].id); spath(p, nm); remove(p); }
        pet_load(); replay();
        snprintf(msg, cap, "new game started; the game you left is in slot 0 (Player > Undo new game)"); return; }
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
    if (!strcmp(v, "buy") && argc >= 2) {
        int i = item_idx(argv[1]); int x, y; if (i < 0) { snprintf(msg, cap, "unknown item %s", argv[1]); return; }
        if (cat[i].price > coins) { snprintf(msg, cap, "not enough coins: %s costs %d, you have %d", cat[i].label, cat[i].price, coins); return; }
        if (argc >= 4) { x = atoi(argv[2]); y = atoi(argv[3]); const char *bad = check_place(vroom, i, x, y, 0); if (bad[0]) { snprintf(msg, cap, "not bought: %s", bad); return; } }
        else if (!first_free(vroom, i, &x, &y)) { snprintf(msg, cap, "not bought: no free spot"); return; }
        snprintf(b, sizeof b, "BUY|%ld|%s|%d", now, cat[i].key, cat[i].price); append_line("house.txt", b); snprintf(b, sizeof b, "PLACE|%d|%ld|%s|%d|%d|%s", nid, now, cat[i].key, x, y, rooms[vroom].id); append_line("house.txt", b);
        snprintf(msg, cap, "bought and placed %s as #%d at %d,%d (%d coins left)", cat[i].label, nid, x, y, coins - cat[i].price); turn_taken = 1; return;
    }
    if (!strcmp(v, "use") && argc >= 2) { int i = item_idx(argv[1]), x, y; if (i < 0 || owned[i] <= 0) { snprintf(msg, cap, "none of that in the bag"); return; } if (!first_free(vroom, i, &x, &y)) { snprintf(msg, cap, "no free spot"); return; }
        snprintf(b, sizeof b, "PLACE|%d|%ld|%s|%d|%d|%s", nid, now, cat[i].key, x, y, rooms[vroom].id); append_line("house.txt", b); snprintf(msg, cap, "placed %s as #%d at %d,%d", cat[i].label, nid, x, y); turn_taken = 1; return; }
    if (!strcmp(v, "place") && argc >= 4) { int id = atoi(argv[1]), x = atoi(argv[2]), y = atoi(argv[3]), k = -1; for (int i = 0; i < npl; i++) if (pl[i].id == id) k = i; if (k < 0) { snprintf(msg, cap, "no placed item #%d", id); return; }
        const char *bad = check_place(pl[k].room, pl[k].item, x, y, id); if (bad[0]) { snprintf(msg, cap, "not moved: %s", bad); return; } snprintf(b, sizeof b, "MOVE|%d|%ld|%d|%d", id, now, x, y); append_line("house.txt", b); snprintf(msg, cap, "moved #%d to %d,%d", id, x, y); turn_taken = 1; return; }
    if (!strcmp(v, "remove") && argc >= 2) { int id = atoi(argv[1]), k = -1; for (int i = 0; i < npl; i++) if (pl[i].id == id) k = i; if (k < 0) { snprintf(msg, cap, "no placed item #%d", id); return; }
        snprintf(b, sizeof b, "REMOVE|%d|%ld", id, now); append_line("house.txt", b); snprintf(msg, cap, "put #%d (%s) back in the bag", id, cat[pl[k].item].label); turn_taken = 1; return; }
    if (!strcmp(v, "select") && argc >= 2) { char hid[24]; snprintf(hid, sizeof hid, "%s", hero.name); for (char *c = hid; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
        if (!strcmp(argv[1], hid)) { g_ctl_why = "select-hero-row"; set_flag("ctl", "player"); set_flag("follow", "player"); calc_view(); snprintf(msg, cap, "%s, the hero (owner of the monsters), in the %s - INT controls him", hero.name, rooms[tr.room].name); return; }
        int k = -1; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, argv[1])) k = i; if (k < 0) { snprintf(msg, cap, "no pet %s", argv[1]); return; }
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
    if (!strcmp(v, "newpal")) {       /* the Pals menu "+ new pal": a new pet row in pals.pdl (default stats, next monster picture), then select it */
        if (nparty >= NP) { snprintf(msg, cap, "no more pals (the house holds %d pets)", NP); return; }
        int n = 1; char id[24], pp[PATH_MAX];
        for (;;) { snprintf(id, sizeof id, "pal_%d", n); int dup = 0; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, id)) dup = 1; if (!dup) break; n++; }
        snprintf(pp, sizeof pp, "%s/pals.pdl", APP); FILE *pf = fopen(pp, "a"); if (!pf) { snprintf(msg, cap, "cannot write pals.pdl"); return; }
        fprintf(pf, "PAL | %s | Pal %d | %d\n", id, n, nparty & 7); fclose(pf);
        load_data(); char *sa[2] = { "select", id }; verb(2, sa, msg, cap); return; }
    if (!strcmp(v, "delpal")) {       /* the Pals menu "- remove last pal": drops the last PAL row (its saved state file stays, harmless) */
        char pp[PATH_MAX], keep[8192] = ""; snprintf(pp, sizeof pp, "%s/pals.pdl", APP); FILE *pf = fopen(pp, "r"); if (!pf) { snprintf(msg, cap, "no pals to remove"); return; }
        char ln[300], last[300] = ""; size_t o = 0; while (fgets(ln, sizeof ln, pf)) { if (!strncmp(ln, "PAL", 3)) { if (last[0] && o + strlen(last) < sizeof keep) { memcpy(keep + o, last, strlen(last)); o += strlen(last); } snprintf(last, sizeof last, "%s", ln); } else if (o + strlen(ln) < sizeof keep) { memcpy(keep + o, ln, strlen(ln)); o += strlen(ln); } } fclose(pf);
        if (!last[0]) { snprintf(msg, cap, "no pals to remove"); return; }
        keep[o] = 0; pf = fopen(pp, "w"); if (pf) { fputs(keep, pf); fclose(pf); } load_data(); if (active >= nparty) active = 0; snprintf(msg, cap, "removed the last pal"); return; }
    if (!strcmp(v, "newroom")) {      /* the Rooms menu "+ new room": an empty room row appended to rooms.pdl (floor Inside_A5 1,2), then the window goes there like goto */
        if (nroom >= 8) { snprintf(msg, cap, "no more rooms (the house holds 8)"); return; }
        int n = nroom + 1; char id[24]; do { snprintf(id, sizeof id, "room_%d", n++); } while (room_idx(id) >= 0);
        char rp[PATH_MAX], rl[120]; snprintf(rp, sizeof rp, "%s/rooms.pdl", APP); FILE *rf = fopen(rp, "a");
        if (!rf) { snprintf(msg, cap, "cannot write rooms.pdl"); return; }
        snprintf(rl, sizeof rl, "ROOM | %s | Room %d | 1 | 2\n", id, nroom + 1); fputs(rl, rf); fclose(rf);
        load_data(); char *ga[2] = { "goto", id }; verb(2, ga, msg, cap); return; }
    if (!strcmp(v, "goto") && argc >= 2) {        /* the Rooms menu: with INT on, the controlled pet goes there (a turn); with INT off, the window just watches that room */
        int r = room_idx(argv[1]); if (r < 0) { snprintf(msg, cap, "no room %s", argv[1]); return; }
        if (flag_on("int", 0)) { int ci = ctl_idx(); use_ent(ci); pet.room = r; pet.dir = 0; pet.tx = pet.ty = -1; int placed = 0;
            for (int k = 0; k < COLS * ROWS && !placed; k++) { int x = (6 + k) % COLS, y = TRIG_ROW + 3 + ((6 + k) / COLS) % (ROWS - TRIG_ROW - 3 > 0 ? ROWS - TRIG_ROW - 3 : 1); if (passable(x, y) && door_here(r, x, y) < 0) { pet.x = x; pet.y = y; placed = 1; } }
            snprintf(b, sizeof b, "DOOR|%ld|%s|menu|%s", now, ci < 0 ? "player" : party[ci].id, rooms[r].id); append_line("events.txt", b); pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); turn_taken = 1; snprintf(msg, cap, "%s is now in the %s", ci < 0 ? "you" : party[ci].name, rooms[r].name); return; }
        snprintf(b, sizeof b, "room:%s", rooms[r].id); set_flag("follow", b); calc_view(); snprintf(msg, cap, "watching the %s", rooms[r].name); return; }
    if (!strcmp(v, "step") && argc >= 3) { if (!run_on()) { snprintf(msg, cap, "stopped: press Play (Player tab) first"); return; } if (!flag_on("int", 0)) { snprintf(msg, cap, "turn INT on first: INT lets you control %s or the player", party[active].name); return; } int ci = ctl_idx(); use_ent(ci); int moved = do_step(atoi(argv[1]), atoi(argv[2]), msg, cap); pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); if (moved) turn_taken = 1; return; }
    if (!strcmp(v, "control")) { g_ctl_why = "control-menu-verb"; char cur[16]; flag_str("ctl", "pet", cur, sizeof cur); const char *n = argc >= 2 ? argv[1] : (!strcmp(cur, "pet") ? "player" : "pet"); if (strcmp(n, "pet") && strcmp(n, "player")) { snprintf(msg, cap, "control pet or player"); return; } set_flag("ctl", n); set_flag("follow", !strcmp(n, "player") ? "player" : "pet"); calc_view(); snprintf(msg, cap, !strcmp(n, "player") ? "INT now controls you (the player)" : "INT now controls %s", party[active].name); return; }
    if (!strcmp(v, "walk")) { int moved = ai_step(active); use_ent(active); if (moved) snprintf(msg, cap, "%s took a step", party[active].name); else snprintf(msg, cap, "%s stays put", party[active].name); return; }
    if (!strcmp(v, "export3d")) { export3d(msg, cap); return; }
    if (!strcmp(v, "mode")) v = "int";      /* the mode tab is the INT switch now */
    if (!strcmp(v, "mode")) { int to_rogue = argc >= 2 ? !strcmp(argv[1], "rogue") : !rogue_mode(); set_flag("timemode", to_rogue ? "rogue" : "live"); clock_sync();
        snprintf(msg, cap, to_rogue ? "rogue: with INT on, time moves only when you act" : "live: time keeps running, even with INT on"); return; }
    if (!strcmp(v, "play")) { if (!(run_event("play", "ui") && run_on())) { set_flag("running", "1"); clock_paths(); clock_install(); clock_sync(); lc_run(1, "daemon-start", NULL); } snprintf(msg, cap, "STARTED: the pets live, the clock runs"); return; }
    if (!strcmp(v, "stop")) { if (!(run_event("stop", "ui") && !run_on())) { set_flag("running", "0"); clock_sync(); } snprintf(msg, cap, "STOPPED: everything is frozen (building and chat still work). Play resumes"); return; }
    if (!strcmp(v, "save") || !strcmp(v, "load")) {      /* slot copies of the state files (pets, trainer, ledger, relations, chat, flags, clock time) */
        int slot = (argc >= 2 && argv[1][0] >= '0' && argv[1][0] <= '9' && !argv[1][1]) ? argv[1][0] - '0' : 1;      /* slots 1-9 are yours; slot 0 is the automatic copy New game makes of the game it replaces */ int saving = !strcmp(v, "save"); char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%s/slots", ST); mkdir(sd, 0755); snprintf(sd, sizeof sd, "%s/slots/%d", ST, slot); mkdir(sd, 0755);
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt"); snprintf(names[nn++], 48, "events.txt");      /* events.txt is bigger than the 64 KB read buffer below: streamed */
        for (int i = 0; i < nparty && nn < 38; i++) snprintf(names[nn++], 48, "pet_%s.txt", party[i].id);
        if (!saving && !fexists(sd)) { snprintf(msg, cap, "slot %d is empty", slot); return; } char mp[PATH_MAX]; snprintf(mp, sizeof mp, "%s/clock_ms.txt", sd);
        if (saving) { clock_paths(); long long ms = clock_kv_ll("game_time_epoch_ms"); char b[40]; snprintf(b, sizeof b, "%lld\n", ms); write_if_changed(mp, b, strlen(b)); }
        int ok = 0; for (int i = 0; i < nn; i++) { char a[PATH_MAX], bpath[PATH_MAX], buf[65536]; if (saving) { spath(a, names[i]); snprintf(bpath, sizeof bpath, "%s/%s", sd, names[i]); } else { snprintf(a, sizeof a, "%s/%s", sd, names[i]); spath(bpath, names[i]); }
            if (!fexists(a)) { if (!saving) remove(bpath); continue; }
            if (!strcmp(names[i], "events.txt")) { if (copy_file(a, bpath)) ok++; continue; }
            if (copy_file(a, bpath)) ok++; }      /* every file is streamed: read_file stops at 64 KB and silently cut relations.txt (65,535 bytes) and chat.txt in the slot */
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
    if (!strcmp(v, "clearchat")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "chat.txt"); snprintf(q, sizeof q, "%s/chat.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "chat cleared (kept as chat.txt.%ld.bak)", now); return; }
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice") || !strcmp(argv[1], "left"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice") || !strcmp(argv[1], "left")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
    if (!strcmp(v, "view")) { char c[16]; flag_str("view", "2d", c, sizeof c); const char *n = !strcmp(c, "2d") ? "3d" : !strcmp(c, "3d") ? "glyph" : "2d"; set_flag("view", n); snprintf(msg, cap, "view %s (the picture is still 2D: 3D and the Chinese-glyph view are sprint items 7-8)", n); return; }
    if (!strcmp(v, "int")) { int on = !flag_on("int", 0); if (!(run_event(on ? "int_on" : "int_off", "ui") && flag_on("int", 0) == on)) set_flag("int", on ? "1" : "0"); if (!on) board_interact(0, 1); char d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); char p[PATH_MAX]; spath(p, "interact_relay.txt"); FILE *f = fopen(p, "a"); if (f) fclose(f); snprintf(p, sizeof p, "%s/keyboard/history.txt", ST); f = fopen(p, "a"); if (f) fclose(f);
        clock_sync(); if (on) { set_flag("follow", "pet"); calc_view(); }
        { char cc[16]; flag_str("ctl", "pet", cc, sizeof cc); snprintf(msg, cap, on ? "INT on: you control %s - turn based like a roguelike, time moves when you act, the other pets act on their own, Tab switches pet/player, Esc leaves" : "INT off: real time, every pet lives on its own", !strcmp(cc, "player") ? "the player" : party[active].name); } return; }
    if (!strcmp(v, "clock") && argc >= 2) { clock_paths(); if (!clock_ok()) { snprintf(msg, cap, "no LyfeDesk clock binary"); return; } clock_install();
        if (!strcmp(argv[1], "rate") && argc >= 3 && lc_amount_ok(argv[2])) { lc_run(0, "cmd", "rpg", "rate", argv[2], "--source", "rpg-menu", NULL); snprintf(msg, cap, "clock speed %s (real time mode)", argv[2]); }
        else if (!strcmp(argv[1], "advance") && argc >= 3 && lc_amount_ok(argv[2])) { lc_run(0, "cmd", "rpg", "advance", argv[2], "--source", "rpg-menu", NULL); lc_run(0, "step", "5", NULL); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); snprintf(msg, cap, "skipped %s", argv[2]); }
        else if (!strcmp(argv[1], "start")) { clock_start(); snprintf(msg, cap, "clock started"); } else if (!strcmp(argv[1], "stop")) { clock_stop(); snprintf(msg, cap, "clock stopped"); }
        else if (!strcmp(argv[1], "reinstall")) { lc_run(0, "daemon-stop", NULL); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/.schedule_installed", CROOT); remove(q); clock_install(); lc_run(1, "daemon-start", NULL); snprintf(msg, cap, "schedule reinstalled"); }
        clock_ui(); return; }
    char text[300] = ""; for (int i = 0; i < argc; i++) { if (i) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); }
    if (text[0]) {
        int sp = ctl_idx(), rep = active;      /* you speak AS the selected entity: Harold (sp<0) or the selected monster; a monster is answered by the nearest OTHER pet in its room */
        if (sp >= 0) { int best = -1, bd = 1 << 30; for (int j = 0; j < nparty; j++) if (j != sp && others[j].room == others[sp].room) { int d = abs(others[j].x - others[sp].x) + abs(others[j].y - others[sp].y); if (d < bd) { bd = d; best = j; } } rep = best; }
        g_speaker = sp < 0 ? hero.name : party[sp].name; say(g_speaker, text);
        if (rep >= 0) { const char *rp = pet_reply(text); int on_screen = others[rep].room == vroom, done = 0;
            if (g_rk >= 0 && g_rk < 4) { char ev[64], tg[16], en[200]; snprintf(ev, sizeof ev, "reply_%s", party[rep].id); snprintf(tg, sizeof tg, "t%d%d", g_rk, g_ry ? 1 : 0); snprintf(en, sizeof en, "RPG_OTHER='%s' RPG_SAY=1 RPG_SPEAK=%d", g_speaker, on_screen); done = run_event_x(ev, tg, en); }      /* topic answers are event pages (events/reply_<pet>); free text and the house count stay C */
            if (!done) { say(party[rep].name, rp); if (on_screen) speak_reply(rep); } }
        turn_taken = 1; }
}

/* ---------- event runner: the clock daemon calls  rpg_pet.+x <pkg> <root>  when a time.pdl event fires ---------- */
/* what a clock event does (the verbs "phase <name>" and "day_tick" call it too: the event pages' rpg_pet_verb command runs them) */
static void clock_event(const char *ev) {
    char b[80];
    if (!strncmp(ev, "phase_", 6)) ev += 6;      /* "phase_night" or "night" */
    if (!strcmp(ev, "dawn") || !strcmp(ev, "day") || !strcmp(ev, "dusk") || !strcmp(ev, "night")) { snprintf(b, sizeof b, "phase=%s\n", ev); char p[PATH_MAX]; spath(p, "daylight.txt"); write_if_changed(p, b, strlen(b)); }
    else if (!strcmp(ev, "day_tick")) { snprintf(b, sizeof b, "DAY|%ld", (long)time(NULL)); append_line("events.txt", b); }
}
/* The clock fires "common:<event>". The event is a real events-hq page in <app>/events/<event>/ (trigger "clock"), played by events-hq's play_event.sh
 * (event.pal -> cmd_1.sh -> the registered rpg_pet_verb command -> this binary's verb). No page = the built-in behaviour above, so a missing events-hq never stops the clock. */
static int event_main(const char *pkg) {
    const char *ev = strrchr(pkg, '/'); ev = ev ? ev + 1 : pkg; if (!strncmp(ev, "common:", 7)) ev += 7;
    if (run_event(ev, "clock")) return 0;
    clock_event(ev); return 0;
}

/* ---------- resident mode ---------- */
static void bye(int s) { (void)s; quit_flag = 1; }
/* new "<code> <ms>" lines since the stored offset; returns the codes */
static int poll_keys(const char *p, long *off, int *codes, int max) {
    long sz = fsize(p); if (sz < *off) *off = 0; if (sz == *off) return 0; FILE *f = fopen(p, "r"); if (!f) return 0; fseek(f, *off, SEEK_SET); char l[80]; int n = 0;
    while (n < max && fgets(l, sizeof l, f)) { if (!strncmp(l, "KEY_PRESSED:", 12)) { codes[n++] = atoi(l + 12); continue; }       /* keyboard/history.txt: Esc arrives as "KEY_PRESSED: 27" */
        if (l[0] == '#' || l[0] < '0' || l[0] > '9') continue; codes[n++] = atoi(l); }       /* interact_relay.txt: "<decimal code> <ms>" */
    *off = ftell(f); fclose(f); return n;
}
/* the rpg-pet board window's published Interact state: 1 engaged, 0 not, -1 no board window / no session */
static int board_armed(void) {
    char up[PATH_MAX], ub[1024], sess[PATH_MAX] = ""; int armed = -1; snprintf(up, sizeof up, "%s/@.apps/rpg-pet/board/state/ui.txt", HOUSE); if (!fexists(up)) return -1; read_file(up, ub, sizeof ub);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11); else if (!strncmp(l, "interact_armed=", 15)) armed = atoi(l + 15); }
    if (!sess[0] || !fexists(sess)) return -1;
    { char tp[PATH_MAX], tb[16]; snprintf(tp, sizeof tp, "%s/pieces/display/active_gui_is_typing.txt", sess); if (fexists(tp) && read_file(tp, tb, sizeof tb) > 0) { int v = atoi(tb); return v != 0; } }      /* the engine's own flag (what pchq_board_action.sh trusts); ui.txt's interact_armed lags behind it */
    return armed;
}
/* Esc to the board engine: the only key that leaves its Interact mode (Enter / 13, what a second click on the In: tab sends, is an ordinary key inside it). */
static void board_esc(void) {
    char up[PATH_MAX], ub[1024], sess[PATH_MAX] = ""; snprintf(up, sizeof up, "%s/@.apps/rpg-pet/board/state/ui.txt", HOUSE); if (!fexists(up)) return; read_file(up, ub, sizeof ub);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0] || !fexists(sess)) return; char p[PATH_MAX]; FILE *f;
    snprintf(p, sizeof p, "%s/pieces/keyboard/history.txt", sess); if ((f = fopen(p, "a"))) { fputs("KEY_PRESSED: 27\n", f); fclose(f); }
    snprintf(p, sizeof p, "%s/pieces/apps/player_app/interact_relay.txt", sess); if ((f = fopen(p, "a"))) { fputs("126\n", f); fclose(f); }
}
/* The board window's own Interact mode (its In: tab) is what makes the engine take the arrow keys and move the xelector. rpg-pet's INT drives it: engaged while INT is on, released when INT goes
 * off (pchq_board_action.sh <session> interact = the engage / disengage key, a toggle, so it is only sent when the board's published state differs from what INT wants). */
static long g_own13 = 0;      /* when this daemon last sent the engage key (its own 13 in the keyboard history is not a click on the In: tab) */
static void board_interact(int want, int force) {
    const char *cand[3] = {"@.apps/rpg-pet/board/state/ui.txt", "@.apps/piececraft-hq/state/ui.txt", "@.apps/layout-studio/sandbox/state/ui.txt"}; static long last = 0; long now = now_ms(); if (!force && now - last < 4000) return;
    for (int k = 0; k < 3; k++) { char up[PATH_MAX], ub[1024], sess[PATH_MAX] = ""; snprintf(up, sizeof up, "%s/%s", HOUSE, cand[k]); if (!fexists(up)) continue; read_file(up, ub, sizeof ub); int armed = 0;
        for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11); else if (!strncmp(l, "interact_armed=", 15)) armed = atoi(l + 15); }
        if (!sess[0] || !fexists(sess) || board_armed() == want) continue; if (k != 0) continue;      /* only the rpg-pet board's own window */
        last = now; g_own13 = now; if (!want) { board_esc(); return; }      /* leaving Interact takes Esc: the engage key (Enter) is an ordinary key inside it */
        char cmd[PATH_MAX * 2 + 64]; snprintf(cmd, sizeof cmd, "sh '%s/@.apps/piececraft-hq/ops/pchq_board_action.sh' '%s' interact >/dev/null 2>&1", HOUSE, sess); sh_timed(cmd, 10);
        { char rp[PATH_MAX]; snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess); FILE *nf = fopen(rp, "a"); if (nf) { fputs("126\n", nf); fclose(nf); } }      /* an idle engine only reads its relay when woken: 126 is the unbound "render now" key */
        return; }
}
/* INT in the board window: the ENGINE moves the xelector on the arrow keys (pc-hq's own Interact mode: camera aware, left is left on screen, 2D and 3D alike); the chosen monster follows it one cell per
 * tick, and a bump (wall, furniture, another pet) snaps the xelector back onto the monster. Reads the xelector the engine writes (pieces/xelector_01/state.txt) and does nothing unless the pc-hq book
 * open is rpg-pet on the monster's own room, or when the selection just changed (follow_pchq puts the xelector on the new monster first). */
static int follow_xelector(char *msg, size_t cap) {
    static int lci = -2; int ci = ctl_idx(); if (ci != lci) { lci = ci; return 0; }
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE); read_file(wp, wb, sizeof wb);
    for (char *l = strtok(wb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "map_id=", 7)) snprintf(m, sizeof m, "%s", l + 7); else if (!strncmp(l, "desk_id=", 8)) snprintf(dk, sizeof dk, "%s", l + 8); }
    Ent *e = ci < 0 ? &tr : &others[ci]; if (strcmp(m, "rpg-pet") || strcmp(dk, rooms[e->room].id)) return 0;
    snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
    for (char *l = strtok(kb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "pos_x=", 6)) xx = atoi(l + 6); else if (!strncmp(l, "pos_y=", 6)) xy = atoi(l + 6); }
    if (xx < 0 || xy < 0 || (xx == e->x && xy == e->y)) return 0;
    int ddx = xx - e->x, ddy = xy - e->y, dx = 0, dy = 0; if (abs(ddx) >= abs(ddy)) dx = ddx > 0 ? 1 : -1; else dy = ddy > 0 ? 1 : -1;
    use_ent(ci); if (do_step(dx, dy, msg, cap)) { pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); end_turn(); } else { pet_save(); g_pin_now = 1; }      /* a bump costs no turn */
    return 1;
}
/* ENTER = possess (owner 2026-10-12): while INT is on and the xelector stands on a character, Enter chooses that character (same as clicking it in the toolbar): it becomes the one INT controls. Enter reaches the
 * engine, not this daemon, so the daemon reads the engine session's keyboard history for new "KEY_PRESSED: 13" lines (its own engage key, g_own13, is ignored). */
static int possess_at_xelector(char *msg, size_t cap) {
    char wp[PATH_MAX], wb[2048], m[64] = "", dk[64] = "", xp[PATH_MAX], kb[512]; snprintf(wp, sizeof wp, "%s/@.apps/piececraft-hq/pieces/world_01/state.txt", HOUSE); read_file(wp, wb, sizeof wb);
    for (char *l = strtok(wb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "map_id=", 7)) snprintf(m, sizeof m, "%s", l + 7); else if (!strncmp(l, "desk_id=", 8)) snprintf(dk, sizeof dk, "%s", l + 8); }
    if (strcmp(m, "rpg-pet")) return 0; snprintf(xp, sizeof xp, "%s/@.apps/piececraft-hq/pieces/xelector_01/state.txt", HOUSE); read_file(xp, kb, sizeof kb); int xx = -1, xy = -1;
    for (char *l = strtok(kb, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "pos_x=", 6)) xx = atoi(l + 6); else if (!strncmp(l, "pos_y=", 6)) xy = atoi(l + 6); }
    int pick = -2; for (int i = 0; i < nparty; i++) if (!strcmp(rooms[others[i].room].id, dk) && others[i].x == xx && others[i].y == xy) pick = i;
    if (pick == -2 && !strcmp(rooms[tr.room].id, dk) && tr.x == xx && tr.y == xy) pick = -1;
    if (pick == -2) return 0;
    if (pick == -1) { g_ctl_why = "enter-possess"; set_flag("ctl", "player"); set_flag("follow", "player"); snprintf(msg, cap, "%s chosen (Enter)", hero.name); }
    else { char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[pick].id); write_if_changed(p, bb, strlen(bb)); g_ctl_why = "enter-possess"; set_flag("ctl", "pet"); set_flag("follow", "pet"); active = pick; snprintf(msg, cap, "%s chosen (Enter)", party[pick].name); }
    calc_view(); return 1;
}
static int enter_pressed_in_engine(void) {
    static long off = -1; static char last_sess[PATH_MAX] = ""; char up[PATH_MAX], ub[1024], sess[PATH_MAX] = ""; snprintf(up, sizeof up, "%s/@.apps/rpg-pet/board/state/ui.txt", HOUSE); if (!fexists(up)) return 0; read_file(up, ub, sizeof ub);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0]) return 0; char hp[PATH_MAX]; snprintf(hp, sizeof hp, "%s/pieces/keyboard/history.txt", sess); long sz = fsize(hp); if (strcmp(sess, last_sess) || off < 0 || sz < off) { snprintf(last_sess, sizeof last_sess, "%s", sess); off = sz; return 0; }
    if (sz == off) return 0; int codes[32]; int n = poll_keys(hp, &off, codes, 32); int hit = 0; for (int i = 0; i < n; i++) if (codes[i] == 13) hit = 1;
    if (hit && now_ms() - g_own13 < 2500) hit = 0; return hit;
}
static void board_cam_key(int code) {
    /* INT ate 0 and 1-4 before the viewer saw them. Hand the same code to THIS board's session only. The viewer applies render_mode / camera_mode. Piececraft and other desks are not written. */
    char up[PATH_MAX], ub[1024], sess[PATH_MAX] = "", rp[PATH_MAX];
    snprintf(up, sizeof up, "%s/@.apps/rpg-pet/board/state/ui.txt", HOUSE);
    if (!fexists(up)) return;
    read_file(up, ub, sizeof ub);
    for (char *l = strtok(ub, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "bv_session=", 11)) snprintf(sess, sizeof sess, "%s", l + 11);
    if (!sess[0]) return;
    snprintf(rp, sizeof rp, "%s/pieces/apps/player_app/interact_relay.txt", sess);
    FILE *f = fopen(rp, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
}
static int daemon_main(void) {
    { char lp[PATH_MAX]; snprintf(lp, sizeof lp, "%s/daemon.lock", ST); static int lfd; lfd = open(lp, O_CREAT | O_RDWR, 0644);      /* ONE resident daemon per game folder (2026-10-17: a board launch and the standalone window each started one, both ticking the same state): flock is dropped by the kernel when this process dies, close-on-exec so a leftover child cannot hold it */
      if (lfd >= 0) { fcntl(lfd, F_SETFD, FD_CLOEXEC); if (flock(lfd, LOCK_EX | LOCK_NB) != 0) { fprintf(stderr, "daemon already running for %s: this one exits\n", ST); return 0; } } }
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
    char h1[PATH_MAX], h2[PATH_MAX], d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    long o1 = fsize(h1), o2 = fsize(h2), next_ms[NP], last_ui = 0; int was_run = run_on() + 2 * rogue_mode(); for (int i = 0; i < NP; i++) next_ms[i] = now_ms() + 600 + 450L * i; char msg[200], lastday[40] = "", lasttone[300] = "";
    g_fcache_on = 1; load_tune(); load_data(); replay(); pet_load(); unstack(); clock_start(); out_all("");
    long last_load = now_ms(), last_follow = 0;
    while (!quit_flag) {
        usleep(100000); while (waitpid(-1, NULL, WNOHANG) > 0) {}      /* reap fire-and-forget children (run_quiet wait=0): no zombies pile up */
        int dirty = 0, codes[64], n; msg[0] = 0; { long lt = now_ms(); if (lt - last_load >= tune("reload_ms", 2000)) { last_load = lt; load_tune(); load_data(); } }      /* catalog / party / rooms / door pages are static: re-parsing them every 100 ms was most of the daemon's own CPU */
        pet_load(); int in = flag_on("int", 0), tm = turn_mode(); static int prev_in = 0; static long int_on_at = 0; if (in && !prev_in) int_on_at = now_ms(); prev_in = in;
        if (in && enter_pressed_in_engine() && possess_at_xelector(msg, sizeof msg)) { dirty = 1; follow_pchq(); }
        n = poll_keys(h1, &o1, codes, 32); n += poll_keys(h2, &o2, codes + n, 32);     /* the offsets always advance; keys only act while INT is on */
        if (in && n) replay();
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
        { int r = run_on() + 2 * rogue_mode(); if (r != was_run) { was_run = r; clock_sync(); dirty = 1; } }
        if (in) for (int i = 0; i < n; i++) { int c = codes[i], dx = 0, dy = 0;
            if (c == 1002 || c == 200) dy = -1; else if (c == 1003 || c == 201) dy = 1; else if (c == 1000 || c == 202) dx = -1; else if (c == 1001 || c == 203) dx = 1;
            else if (c == 27) { set_flag("int", "0"); clock_sync(); snprintf(msg, sizeof msg, "INT off: real time"); dirty = 1; break; }      /* Esc leaves INT (keyboard/history.txt: KEY_PRESSED: 27) */
            else if (!run_on()) continue;
            else if (c == 9) continue;      /* owner 2026-10-12: no Tab pet/player toggle - INT controls whoever is chosen from the toolbar (every cast member is playable, Harold is just the default selection) */
            else if (c == '0') { char cv[16]; flag_str("view", "2d", cv, sizeof cv); set_flag("view", !strcmp(cv, "2d") ? "3d" : "2d"); board_cam_key(c); snprintf(msg, sizeof msg, "view toggled"); dirty = 1; continue; }
            else if (c >= '1' && c <= '4') { char pv[8]; snprintf(pv, sizeof pv, "%c", c); set_flag("pov", pv); board_cam_key(c); snprintf(msg, sizeof msg, "pov %c", c); dirty = 1; continue; }
            else continue;
            { int ci = ctl_idx(); use_ent(ci); if (do_step(dx, dy, msg, sizeof msg)) { pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); end_turn(); } else pet_save(); }      /* a bump costs no turn */
            dirty = 1; }
        {   /* the board's In: tab is rpg-pet's INT. Clicking it engages the ENGINE'S Interact mode (pchq_board_action.sh interact); a rising edge of that while INT is off turns INT on: auto stops (the
             * clock and the pet timers wait for you), the arrows move the chosen monster through the xelector (follow_xelector below). Rising edge only, so an INT that something else turned off while the engine
             * was still engaged does not switch itself back on; checked at most every 400 ms and only while a board window exists. */
            static int prev_ar = -1; static long chk = 0; long t = now_ms();
            if (in || !board_window_open()) prev_ar = -1;
            else if (t - chk >= tune("edge_poll_ms", 400)) { chk = t; int ar2 = board_armed(); if (prev_ar == 0 && ar2 == 1) {
                    if (!(run_event("int_on", "ui") && flag_on("int", 0))) set_flag("int", "1");
                    clock_sync(); snprintf(msg, sizeof msg, "INT on: you control %s - turn based", party[active].name); dirty = 1; in = 1; prev_in = 1; int_on_at = now_ms(); }
                prev_ar = ar2; }
        }
        {   /* Esc in the board window makes the ENGINE leave its Interact mode; rpg-pet's INT has to follow, or the game stays turn based and the real time ticker never restarts. A board whose In: was seen on and is now off = Esc. */
            static int was_armed = 0, tried = 0; static long sent_at = 0; int ar = in ? board_armed() : -1;      /* only while INT is on: it needs two file reads */
            static int was_in = 0; if (in && !was_in) g_pin_now = 1; was_in = in;
            if (!in) { was_armed = 0; tried = 0; }
            else if (ar == 1) { was_armed = 1; tried = 1; }
            else if (ar == 0 && was_armed) { was_armed = 0; if (!(run_event("int_off", "ui") && !flag_on("int", 0))) set_flag("int", "0"); clock_sync(); snprintf(msg, sizeof msg, "INT off: real time"); dirty = 1; in = 0; }
            else if (ar == 0 && run_on() && (!tried || now_ms() - sent_at > tune("engage_retry_ms", 20000))) { board_interact(1, 1); tried = 1; sent_at = now_ms(); }      /* ONE engage per INT on (the key is a toggle and the engine answers seconds late: a second one switches it back off, which looked like Esc and turned INT off by itself) */
        }
        {   /* a click on a pet or the trainer in the board's 2D scene selects them (the old window did the same on its painted scene). The canvas click is the house file #.desktop/pchq_canvas_click.txt "cx cy cw ch" (the
             * click and the canvas size); the scene frame is FW x FH with the room grid at OX,OY, T px per cell, so the cell is ((px-OX)/T, (py-OY)/T). Looked at only while a board window exists; the first
             * look just records the current click so an old one never selects anything. */
            static long csig = -1, cchk = 0; long t = now_ms();
            if (board_window_open() && t - cchk >= tune("click_poll_ms", 150)) { cchk = t; char cp[PATH_MAX]; snprintf(cp, sizeof cp, "%s/#.desktop/pchq_canvas_click.txt", HOUSE); struct stat cs;
                long sg = stat(cp, &cs) == 0 ? (long)cs.st_size * 1000003L + (long)cs.st_mtim.tv_sec * 1000L + cs.st_mtim.tv_nsec / 1000000L : -2;
                if (csig == -1) csig = sg;
                else if (sg != csig) { csig = sg; int cx = 0, cy = 0, cw = 0, ch = 0; FILE *cf = fopen(cp, "r"); if (cf) { if (fscanf(cf, "%d %d %d %d", &cx, &cy, &cw, &ch) != 4) cw = 0; fclose(cf); }
                    if (cw > 0 && ch > 0) { int col = (cx * FW / cw - OX) / T, row = (cy * FH / ch - OY) / T; char *sa[2]; char sid[24]; sa[0] = "select"; sa[1] = sid; int hit = 0;
                        for (int i = 0; i < nparty && !hit; i++) if (others[i].room == vroom && others[i].x == col && others[i].y == row) { snprintf(sid, sizeof sid, "%s", party[i].id); hit = 1; }
                        if (!hit && tr.room == vroom && tr.x == col && tr.y == row) { snprintf(sid, sizeof sid, "%s", hero.name); for (char *q = sid; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; hit = 1; }
                        if (hit) { verb(2, sa, msg, sizeof msg); dirty = 1; } } } }
        }
        if (in && run_on() && follow_xelector(msg, sizeof msg)) dirty = 1;
        long ms = now_ms();
        for (int i = 0; i < nparty; i++) if (ms >= next_ms[i]) { next_ms[i] = ms + tune("step_min_ms", 900) + rand() % tune("step_jitter_ms", 700); if (tm || !run_on()) continue; replay(); if (ai_step(i)) { static long last_far = 0; if (others[i].room == vroom || ms - last_far > tune("far_repaint_ms", 1500)) { dirty = 1; if (others[i].room != vroom) last_far = ms; } } use_ent(active); }     /* real time (INT off): every pet walks on its own. INT on is turn based: no timers, only your acts advance time */
        if (ms - last_ui > 3000) { last_ui = ms; clock_paths(); clock_ui(); }
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
        { char tp[PATH_MAX], tb[300]; snprintf(tp, sizeof tp, "%s/world/screen_state.pdl", ST); read_file(tp, tb, sizeof tb); if (strcmp(tb, lasttone)) { snprintf(lasttone, sizeof lasttone, "%s", tb); g_tone_stale = 1; dirty = 1; } }      /* an events-hq tint_screen wrote a new tone */
        { static int was_vo = 1; int vo = viewer_kinds() != 0; if (vo && !was_vo) dirty = 1; was_vo = vo; if (!vo) dirty = 0; }      /* nobody is looking (no rpg-pet / board window): the scene frame and the ui files have no reader, so they are not rendered; a window opening renders at once */
        if (dirty) { replay(); out_all(msg); }    /* nothing changed -> nothing rendered, nothing written, nothing repainted */
        if ((dirty || now_ms() - last_follow >= tune("follow_ms", 400)) && board_window_open()) { last_follow = now_ms(); follow_pchq(); }      /* the pc-hq 3D view (if open on the rpg-pet book) follows the selected entity; it re-compares the exported files of every room, so at most every 400 ms unless something changed */
    }
    clock_stop();
    return 0;
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    int mode = 0; char *vargv[32]; int vargc = 0; char textbuf[400];          /* 0 verb, 1 resident, 2 event runner */
    char exe[PATH_MAX]; if (!realpath(argv[0], exe)) snprintf(exe, sizeof exe, "%s", argv[0]); snprintf(SELF, sizeof SELF, "%s", exe);
    if (argc == 3 && is_dir(argv[1]) && is_dir(argv[2])) { mode = 1; snprintf(APP, sizeof APP, "%s", argv[2]); }     /* module form: house, pkg */
    else {
        for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (s) *s = 0; } snprintf(APP, sizeof APP, "%s", exe);          /* .../rpg-pet/ops/+x/rpg_pet.+x -> .../rpg-pet */
        if (argc == 3 && (strstr(argv[1], "common:") || strstr(argv[1], "common_events/")) && argv[2][0] == '/') mode = 2;      /* the clock daemon passes <root>/common_events/<event> (or a common:<event> pkg) - anything else is chat text */
        else if (argc >= 4 && is_dir(argv[1])) { snprintf(textbuf, sizeof textbuf, "%s", argv[3]); for (char *t = strtok(textbuf, " "); t && vargc < 31; t = strtok(NULL, " ")) vargv[vargc++] = t; }
        else for (int i = 1; i < argc && vargc < 31; i++) vargv[vargc++] = argv[i];
    }
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    signal(SIGPIPE, SIG_IGN);      /* a closed pipe / dead child must never kill a verb or the daemon (the children get SIG_DFL back before exec) */
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    { char d[PATH_MAX]; snprintf(d, sizeof d, "%s", APP); ASSETS[0] = HOUSE[0] = 0;
      for (int up = 0; up < 8; up++) { char *s = strrchr(d, '/'); if (!s) break; *s = 0; char p[PATH_MAX]; snprintf(p, sizeof p, "%s/&.widgits", d); if (!HOUSE[0] && is_dir(p)) snprintf(HOUSE, sizeof HOUSE, "%s", d); snprintf(ASSETS, sizeof ASSETS, "%s/#.NNEST_ASSETS/rmmv-www-img", d); if (is_dir(ASSETS)) break; ASSETS[0] = 0; }
      if (!ASSETS[0] || !HOUSE[0]) { fprintf(stderr, "rpg_pet: no #.NNEST_ASSETS / &.widgits above %s\n", APP); return 1; } }
    if (mode == 2) return event_main(argv[1]);
    if (mode == 1) return daemon_main();
    load_data(); replay(); pet_load(); clock_paths();
    char msg[240]; verb(vargc, vargv, msg, sizeof msg);
    if (turn_taken && turn_mode()) end_turn();       /* INT on: the owner acted, so it is a turn: the clock advances and every pet steps */
    replay(); if (viewer_kinds()) out_all(msg); clock_ui(); printf("%s\n", msg[0] ? msg : "ok"); return 0;      /* a verb (and every event page that calls one: pet_says, sync, the clock events) used to render the whole scene and ui files even with no window open; with no viewer there is no reader */
}
