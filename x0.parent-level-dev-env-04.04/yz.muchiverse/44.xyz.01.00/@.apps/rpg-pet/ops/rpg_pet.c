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
 * Keys in INT (state/interact_relay.txt, "<decimal code> <ms>", same codes as pet-trainer keybinds.pdl): arrows 1000 left / 1001 right / 1002 up / 1003 down (the agent relay band 200-203 is also accepted), 9 Tab, 27 Esc = INT off, 0 view, 1-4 pov (stored only: 3D is sprint item 7).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
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
#define NP 6

typedef struct { char key[32], label[40], sheet[24], effect[12]; int price, c, r, w, h, wall; } Item;
typedef struct { char id[16], name[32]; int fc, fr, outdoor; } Room;
typedef struct { int room, c, r, w, h, x, y; char sheet[24]; } Scen;
typedef struct { int room, tx, dest, ax, ay, autow; } Door;
typedef struct { int id, item, x, y, room; } Placed;
typedef struct { char name[40]; unsigned char *px; int w, h; } Sheet;
typedef struct { int room, x, y, dir, steps, tx, ty; } Ent;          /* a walking thing: a pet, or the trainer. tx,ty = the walk target (-1 none) */

static char APP[PATH_MAX], ST[PATH_MAX], ASSETS[PATH_MAX], HOUSE[PATH_MAX], SELF[PATH_MAX];
static Item cat[MAXI]; static int ncat;
static Room rooms[8]; static int nroom;
static Door doors[24]; static int ndoor;
static Scen scen[24]; static int nscen;
static Placed pl[MAXP]; static int npl, nid = 1, owned[MAXI], coins;
static Sheet sheets[12]; static int nsheet;
static unsigned char fr[FW * FH * 4];
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
static long fsize(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : 0; }
static long now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L; }

/* ---------- small file helpers ---------- */
static int read_file(const char *p, char *buf, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { buf[0] = 0; return 0; } size_t n = fread(buf, 1, cap - 1, f); buf[n] = 0; fclose(f); return (int)n; }
/* replace a file atomically, only when the bytes differ; returns 1 when written */
static int write_if_changed(const char *p, const void *data, size_t n) {
    FILE *f = fopen(p, "rb");
    if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); if (sz == (long)n) { rewind(f); unsigned char *old = malloc(n ? n : 1); if (old && fread(old, 1, n, f) == n && !memcmp(old, data, n)) { free(old); fclose(f); return 0; } free(old); } fclose(f); }
    char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", p);
    f = fopen(tmp, "wb"); if (!f) return 0; fwrite(data, 1, n, f); fclose(f); rename(tmp, p); return 1;
}
static void append_line(const char *name, const char *line) { char p[PATH_MAX]; spath(p, name); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%s\n", line); fclose(f); } }
static void flag_str(const char *name, const char *def, char *out, size_t cap) { char p[PATH_MAX], n[64]; snprintf(n, sizeof n, "flag_%s.txt", name); spath(p, n); char b[64]; read_file(p, b, sizeof b); char *e = strchr(b, '\n'); if (e) *e = 0; snprintf(out, cap, "%s", b[0] ? b : def); }
static int flag_on(const char *name, int def) { char b[16]; flag_str(name, def ? "1" : "0", b, sizeof b); return b[0] == '1'; }
static void set_flag(const char *name, const char *v) { char p[PATH_MAX], n[64], b[80]; snprintf(n, sizeof n, "flag_%s.txt", name); spath(p, n); snprintf(b, sizeof b, "%s\n", v); write_if_changed(p, b, strlen(b)); }

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
    char p[PATH_MAX], line[300]; spath(p, "house.txt"); coins = START_COINS; npl = 0; nid = 1; memset(owned, 0, sizeof owned);
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
    if (!s) return;
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
    for (int k = 0; k < no; k++) { int i = order[k]; Ent *e = i < 0 ? &tr : &others[i]; int col = e->steps ? cyc[e->steps % 4] : 1, ci = i < 0 ? 0 : party[i].idx;
        blit(sheet("characters", i < 0 ? hero.sheet : party[i].sheet), (ci % 4) * 3 * T + col * T, (ci / 4) * 4 * T + e->dir * T, T, T, OX + e->x * T, OY + e->y * T - 8); }
    { int mr = 256, mg = 256, mb = 256, od = rm->outdoor;      /* phase tint (clock events write daylight.txt): strong outside, only a mild dimming indoors */
      if (!strcmp(phase, "night")) { if (od) { mr = 110; mg = 120; mb = 190; } else { mr = 200; mg = 205; mb = 235; } } else if (!strcmp(phase, "dusk")) { if (od) { mr = 256; mg = 200; mb = 160; } else { mr = 256; mg = 235; mb = 215; } } else if (!strcmp(phase, "dawn")) { if (od) { mr = 256; mg = 215; mb = 185; } else { mr = 256; mg = 240; mb = 225; } }
      if (mr != 256 || mg != 256 || mb != 256) for (int y = OY + (od ? 2 * T : 0); y < OY + ROWS * T; y++) for (int x = OX; x < OX + COLS * T; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = (unsigned char)(q[0] * mr / 256); q[1] = (unsigned char)(q[1] * mg / 256); q[2] = (unsigned char)(q[2] * mb / 256); } }
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
    static char keep[9000]; read_tail(p, keep, sizeof keep); char tmp[PATH_MAX + 8]; snprintf(tmp, sizeof tmp, "%s.tmp", p); FILE *f = fopen(tmp, "w"); if (!f) return; fputs(keep, f); fclose(f); rename(tmp, p);
}
static void say(const char *who, const char *text) { char b[300]; snprintf(b, sizeof b, "%s: %s", who, text); append_line("chat.txt", b); chat_trim(); }
static int g_rk = -1, g_ry;      /* what the last reply was about (0 sleep 1 eat 2 read 3 play, -1 default) and whether the house has such an item: the native-language reply is picked from these */
static const char *pet_reply(const char *text) {
    static char b[80]; g_rk = -1; g_ry = 0; char s[200]; snprintf(s, sizeof s, "%s", text); for (char *c = s; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    #define HAS(e) ({ int h = 0; for (int i = 0; i < npl; i++) if (!strcmp(cat[pl[i].item].effect, e)) h = 1; h; })
    if (strstr(s, "bed") || strstr(s, "sleep")) { g_rk = 0; g_ry = HAS("sleep"); return g_ry ? "zzz... a bed!" : "I have no bed yet."; }
    if (strstr(s, "eat") || strstr(s, "food") || strstr(s, "table")) { g_rk = 1; g_ry = HAS("eat"); return g_ry ? "yum, a table to eat at" : "I have nowhere to eat."; }
    if (strstr(s, "book") || strstr(s, "read")) { g_rk = 2; g_ry = HAS("read"); return g_ry ? "I like reading." : "no books here."; }
    if (strstr(s, "piano") || strstr(s, "play")) { g_rk = 3; g_ry = HAS("play"); return g_ry ? "la la la" : "no piano..."; }
    { static const char *dflt[4] = {"hmm, tell me more.", "I'm listening.", "interesting!", "okay, Harold."}; unsigned h = 0; for (const char *c = s; *c; c++) h = h * 31 + (unsigned char)*c;
        const char *w[] = {"hi", "hey", "hello", "yo", "hiya", "howdy", "sup", NULL}; for (int k = 0; w[k]; k++) { size_t n = strlen(w[k]); if (!strncmp(s, w[k], n) && (s[n] == 0 || s[n] == ' ' || s[n] == '!' || s[n] == ',')) { snprintf(b, sizeof b, "hello %s!", hero.name); return b; } }
        if (strchr(s, '?')) return "good question... let me think.";
        return dflt[h % 4]; }
}
/* ---------- voices: edge-tts per pet (voices.pdl), cached wav, one sound at a time, never blocks ---------- */
static int run_quiet(char *const av[], int wait) {
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } execvp(av[0], av[0] ? av : av); _exit(127); }
    if (!wait) return 0; int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
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
    pid_t pid = fork(); if (pid < 0) return; if (pid > 0) { waitpid(pid, NULL, 0); return; }          /* double fork: the grandchild does the slow work and is reparented */
    if (fork() != 0) _exit(0); setsid();
    if (!fexists(wav)) { char rateA[40], pitchA[40], home[PATH_MAX], edge[PATH_MAX]; snprintf(rateA, sizeof rateA, "--rate=%s", rate); snprintf(pitchA, sizeof pitchA, "--pitch=%s", pitch); const char *hm = getenv("HOME"); snprintf(home, sizeof home, "%s", hm ? hm : ""); snprintf(edge, sizeof edge, "%s/.local/bin/edge-tts", home);
        char *av[] = {"nice", "-n", "15", edge, "--voice", voice, rateA, pitchA, "--text", (char *)text, "--write-media", mp3, NULL}; if (run_quiet(av, 1) != 0 || !fexists(mp3)) _exit(0);
        char *cv[] = {"nice", "-n", "15", "ffmpeg", "-loglevel", "quiet", "-y", "-i", mp3, "-ar", "22050", "-ac", "1", wav, NULL}; run_quiet(cv, 1); remove(mp3); }
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
    snprintf(t, sizeof t, HI[0][v1], party[b].name); if (others[a].room == vroom && (a == active || b == active)) say(party[a].name, t);      /* chat only carries what happens in the room you are watching, or six pets drown your own line */ if (others[a].room == vroom) speak_greet(a, v1, party[b].name, 0);
    snprintf(t, sizeof t, RE[0][v2], party[a].name); if (others[b].room == vroom && (a == active || b == active)) say(party[b].name, t); if (others[b].room == vroom) speak_greet(b, v2, party[a].name, 1);
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
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); if (daemon) setenv("LC_CLOCK_EVENT_RUNNER", SELF, 1); setenv("RPG_PET_ROOT", APP, 1); execv(LC, av); _exit(127); }
    int st = 0; waitpid(pid, &st, 0); return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
static long long clock_kv_ll(const char *key) { char b[2048]; read_file(CFILE, b, sizeof b); size_t kl = strlen(key); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, key, kl) && l[kl] == '=') return atoll(l + kl + 1); return -1; }
static void clock_kv_s(const char *key, char *out, size_t cap) { char b[2048]; out[0] = 0; read_file(CFILE, b, sizeof b); size_t kl = strlen(key); for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out, cap, "%s", l + kl + 1); return; } }
static void write_phase(long long ms) { int h = (int)(ms / 3600000 % 24); const char *p = (h >= 19 || h < 6) ? "night" : h < 8 ? "dawn" : h >= 17 ? "dusk" : "day"; char b[40]; snprintf(b, sizeof b, "phase=%s\n", p); char f[PATH_MAX]; spath(f, "daylight.txt"); write_if_changed(f, b, strlen(b)); }
static void clock_ui(void) {          /* state/clock_ui.txt: its own tiny vars file, so a ticking label never re-renders the scene */
    char p[PATH_MAX], b[160], rate[16], run[8]; long long ms = clock_kv_ll("game_time_epoch_ms"); spath(p, "clock_ui.txt");
    if (ms < 0) { snprintf(b, sizeof b, "time_label=--:--\ntime_rate=-\ntime_running=0\n"); write_if_changed(p, b, strlen(b)); return; }
    char d0b[32]; char dp[PATH_MAX]; spath(dp, "clock_day0.txt"); read_file(dp, d0b, sizeof d0b); long long d0 = atoll(d0b), d = ms / 86400000, s = (ms - d * 86400000) / 1000;
    clock_kv_s("rate", rate, sizeof rate); clock_kv_s("running", run, sizeof run);
    snprintf(b, sizeof b, "time_label=D%lld %02d:%02d\ntime_rate=%s\ntime_running=%s\n", d - d0 + 1, (int)(s / 3600), (int)(s % 3600 / 60), rate, run[0] ? run : "0"); write_if_changed(p, b, strlen(b));
}
static void turn_str(char *out, size_t cap) { char p[PATH_MAX], line[300]; char *f[6]; snprintf(p, sizeof p, "%s/time.pdl", APP); snprintf(out, cap, "5m"); FILE *fp = fopen(p, "r"); if (!fp) return; while (fgets(line, sizeof line, fp)) if (split_bar(line, f, 6) >= 3 && !strcmp(f[0], "TURN")) snprintf(out, cap, "%s", f[2]); fclose(fp); }
static void clock_install(void) {
    clock_paths(); if (!clock_ok()) return; char d[PATH_MAX]; snprintf(d, sizeof d, "%s/#.desktop", CROOT); mkdir(CROOT, 0755); mkdir(d, 0755); snprintf(d, sizeof d, "%s/#.desktop/clocks", CROOT); mkdir(d, 0755); snprintf(d, sizeof d, "%s/common_events", CROOT); mkdir(d, 0755);
    if (!fexists(CFILE)) { lc_run(0, "new", "rpg", "user", "rpg-pet time", NULL); lc_run(0, "rate", "rpg", "min", NULL); lc_run(0, "cmd", "rpg", "settime", "21600000", "--source", "rpg-install", NULL);
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
static void clock_sync(void) { clock_paths(); clock_pause(!run_on() || flag_on("int", 0)); }
static void clock_start(void) { clock_paths(); if (!clock_ok()) return; clock_install(); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); clock_sync(); lc_run(1, "daemon-start", NULL); clock_ui(); }
static void clock_stop(void) { clock_paths(); if (!clock_ok() || !fexists(CFILE)) return; lc_run(0, "cmd", "rpg", "pause", "--source", "rpg-stop", NULL); usleep(1000000); lc_run(0, "daemon-stop", NULL); }
/* a roguelike turn's worth of game time: advance, then one deterministic pass so reminders fire now (no daemon wait) */
static void clock_turn(void) { clock_paths(); if (!clock_ok() || !fexists(CFILE)) return; char t[32]; turn_str(t, sizeof t); lc_run(0, "cmd", "rpg", "advance", t, "--source", "rpg-turn", NULL); lc_run(0, "step", "5", NULL); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); clock_ui(); }

/* ---------- outputs ---------- */
static void out_all(const char *msg) {
    calc_view(); render();
    char p[PATH_MAX]; spath(p, "scene.raw"); write_if_changed(p, fr, sizeof fr);
    spath(p, "scene.receipt.txt"); { char b[64]; snprintf(b, sizeof b, "frame_w=%d\nframe_h=%d\n", FW, FH); write_if_changed(p, b, strlen(b)); }
    static char u[16384]; int o = 0;
#define W(...) o += snprintf(u + o, sizeof u - o, __VA_ARGS__)
    char raw[PATH_MAX]; spath(raw, "scene.raw"); char h1[PATH_MAX], h2[PATH_MAX]; spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    int chat = flag_on("chat", 0), hb = flag_on("hb", 0), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=rpg-pet  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[vroom].name, coins, raw, raw, coins, msg && msg[0] ? msg : (in ? "INT on: turn based like a roguelike. Time moves when you act. Tab switches pet/player." : "real time: the pets live on their own. INT lets you control a pet or the player."), rooms[vroom].name);
    { char cc[16]; flag_str("ctl", "pet", cc, sizeof cc); char who[40]; snprintf(who, sizeof who, "on: %s", !strcmp(cc, "player") ? "you" : party[active].name); W("interact_class=%s\ninteract_label=%s\nrp_h1=%s\nrp_h2=%s\ncontrol_label=control: %s\n", in ? "interact-active" : "", in ? who : "off", h1, h2, !strcmp(cc, "player") ? "player" : party[active].name); }
    W("run_on=%d\nrun_label=%s\nrun_cls=%s\n", run_on(), run_on() ? "GO started" : "STOP stopped", run_on() ? "ph-green" : "ph-red");
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
    W("party_0_label=%s\nparty_0_id=%s\nparty_0_cls=%s\n", hero.name, hid, hs ? "active" : ""); for (int i = 0; i < nparty; i++) W("party_%d_label=%s\nparty_%d_id=%s\nparty_%d_cls=%s\n", i + 1, party[i].name, i + 1, party[i].id, i + 1, (!hs && i == active) ? "active" : "");
    W("n_room=%d\n", nroom); for (int i = 0; i < nroom; i++) { int c = 0, pc = 0; for (int j = 0; j < npl; j++) if (pl[j].room == i) c++; for (int j = 0; j < nparty; j++) if (others[j].room == i) pc++; W("room_%d_label=%s%s (%d things, %d pets)\nroom_%d_id=%s\n", i, rooms[i].name, i == vroom ? "  <- here" : "", c, pc, i, rooms[i].id); }
    int nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) nb++; W("hb_n_slots=%d\nhb_title=bag: click a slot to place it\n", nb ? nb : 1); if (!nb) W("hb_0_text=(bag empty - Shop)\nhb_0_key=none\n"); nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) { W("hb_%d_text=%s x%d\nhb_%d_key=%s\n", nb, cat[i].label, owned[i], nb, cat[i].key); nb++; }
    int np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == vroom) np++; W("n_placed=%d\n", np); np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == vroom) { W("placed_%d_label=#%d %s (%d,%d) %s\nplaced_%d_id=%d\n", np, pl[i].id, cat[pl[i].item].label, pl[i].x, pl[i].y, cat[pl[i].item].effect, np, pl[i].id); np++; }
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
static void end_turn(void) { if (!run_on()) return; clock_turn(); int in = flag_on("int", 0), c = ctl_idx(); for (int i = 0; i < nparty; i++) if (!(in && i == c)) ai_step(i); use_ent(active); }      /* the pet you control does not act on its own */


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
        snprintf(p, sizeof p, "%s/map.txt", d); { FILE *f = fopen(p, "w"); if (f) { for (int y = 0; y < ROWS; y++) { glyph[y][COLS] = 0; fprintf(f, "%s\n", glyph[y]); } fclose(f); } }
        snprintf(p, sizeof p, "%s/extrusion.pdl", d); { FILE *f = fopen(p, "w"); if (f) { fprintf(f, "SECTION      | KEY                | VALUE\n----------------------------------------------------------------------\nMETA         | map_id             | rpg-pet\nMETA         | desk_id            | %s\nMETA         | note               | %s (rpg-pet house, exported from the live house)\nEXTRUDE      | W                  | 3\nEXTRUDE      | D                  | 3\nEXTRUDE      | T                  | 2\nEXTRUDE      | f                  | 1\nEXTRUDE      | default            | 0\n", rooms[r].id, rooms[r].name); fclose(f); } }
        int cols = 16, nrows = (nslot + cols - 1) / cols; int aw = cols * XT, ah = nrows * XT; unsigned char *atlas = calloc((size_t)aw * ah * 4, 1);
        for (int i = 0; i < nslot; i++) for (int y = 0; y < XT; y++) memcpy(atlas + ((size_t)((i / cols) * XT + y) * aw + (i % cols) * XT) * 4, slotpx[i] + y * XT * 4, XT * 4);
        snprintf(p, sizeof p, "%s/cells.rgba", d); { FILE *f = fopen(p, "wb"); if (f) { fwrite(atlas, 1, (size_t)aw * ah * 4, f); fclose(f); } } snprintf(p, sizeof p, "%s/cells.png", d); stbi_write_png(p, aw, ah, 4, atlas, aw * 4); free(atlas);
        snprintf(p, sizeof p, "%s/cells.txt", d); { FILE *f = fopen(p, "w"); if (f) { fprintf(f, "width=%d\nheight=%d\ntile_px=%d\natlas_cols=16\natlas_tiles=%d\ncells\n", COLS, ROWS, XT, nslot); for (int y = 0; y < ROWS; y++) { for (int x = 0; x < COLS; x++) fprintf(f, "%s%d,%d", x ? " " : "", fs[y][x], ws[y][x]); fputc('\n', f); } fclose(f); } }
        snprintf(p, sizeof p, "%s/events.txt", d); { FILE *f = fopen(p, "w"); if (f) { int dc[4] = {2, 4, 6, 8};
            for (int i = 0; i < nparty; i++) if (others[i].room == r) { write_frame(party[i].sheet, party[i].idx, others[i].dir, 1); Sheet *s = sheet("characters", party[i].sheet); int ci = party[i].idx; unsigned char px[4] = {128, 128, 128, 255}; if (s) { int fx = (ci % 4) * 3 * T + T + T / 2, fy = (ci / 4) * 4 * T + others[i].dir * T + T / 2; if (fx < s->w && fy < s->h) memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4); } fprintf(f, "%d %d %d %d %d %s %d %d 1\n", others[i].x, others[i].y, px[0], px[1], px[2], party[i].sheet, ci, dc[others[i].dir & 3]); }
            if (tr.room == r) { write_frame(hero.sheet, 0, tr.dir, 1); Sheet *s = sheet("characters", hero.sheet); unsigned char px[4] = {200, 60, 60, 255}; if (s) { int fx = T + T / 2, fy = tr.dir * T + T / 2; memcpy(px, s->px + ((size_t)fy * s->w + fx) * 4, 4); } fprintf(f, "%d %d %d %d %d %s 0 %d 1\n", tr.x, tr.y, px[0], px[1], px[2], hero.sheet, dc[tr.dir & 3]); }
            fclose(f); } }
    }
    snprintf(p, sizeof p, "%s/game.pdl", root); { FILE *f = fopen(p, "w"); if (f) { fputs(gl, f); fclose(f); } }
    snprintf(msg, cap, "exported %d rooms as a pc-hq book: maps/rpg-pet (open it in pc-hq: Desk > rpg-pet, then 1-4 / 0 for 3D)", nroom);
}

/* ---------- verbs ---------- */
static void verb(int argc, char **argv, char *msg, size_t cap) {
    const char *v = argc ? argv[0] : "status"; long now = (long)time(NULL); char b[200]; msg[0] = 0; use_ent(active);
    if (!strcmp(v, "status")) return;
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
        if (!strcmp(argv[1], hid)) { set_flag("ctl", "player"); set_flag("follow", "player"); calc_view(); snprintf(msg, cap, "%s, the hero (owner of the monsters), in the %s - INT controls him", hero.name, rooms[tr.room].name); return; }
        int k = -1; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, argv[1])) k = i; if (k < 0) { snprintf(msg, cap, "no pet %s", argv[1]); return; }
        char p[PATH_MAX], bb[40]; spath(p, "active.txt"); snprintf(bb, sizeof bb, "%s\n", party[k].id); write_if_changed(p, bb, strlen(bb)); set_flag("follow", "pet"); set_flag("ctl", "pet"); active = k; calc_view(); snprintf(msg, cap, "watching %s (in the %s)", party[k].name, rooms[others[k].room].name); return; }
    if (!strcmp(v, "goto") && argc >= 2) {        /* the Rooms menu: with INT on, the controlled pet goes there (a turn); with INT off, the window just watches that room */
        int r = room_idx(argv[1]); if (r < 0) { snprintf(msg, cap, "no room %s", argv[1]); return; }
        if (flag_on("int", 0)) { int ci = ctl_idx(); use_ent(ci); pet.room = r; pet.dir = 0; pet.tx = pet.ty = -1; int placed = 0;
            for (int k = 0; k < COLS * ROWS && !placed; k++) { int x = (6 + k) % COLS, y = TRIG_ROW + 3 + ((6 + k) / COLS) % (ROWS - TRIG_ROW - 3 > 0 ? ROWS - TRIG_ROW - 3 : 1); if (passable(x, y) && door_here(r, x, y) < 0) { pet.x = x; pet.y = y; placed = 1; } }
            snprintf(b, sizeof b, "DOOR|%ld|%s|menu|%s", now, ci < 0 ? "player" : party[ci].id, rooms[r].id); append_line("events.txt", b); pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); turn_taken = 1; snprintf(msg, cap, "%s is now in the %s", ci < 0 ? "you" : party[ci].name, rooms[r].name); return; }
        snprintf(b, sizeof b, "room:%s", rooms[r].id); set_flag("follow", b); calc_view(); snprintf(msg, cap, "watching the %s", rooms[r].name); return; }
    if (!strcmp(v, "step") && argc >= 3) { if (!run_on()) { snprintf(msg, cap, "stopped: press Play (Player tab) first"); return; } if (!flag_on("int", 0)) { snprintf(msg, cap, "turn INT on first: INT lets you control %s or the player", party[active].name); return; } int ci = ctl_idx(); use_ent(ci); int moved = do_step(atoi(argv[1]), atoi(argv[2]), msg, cap); pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); if (moved) turn_taken = 1; return; }
    if (!strcmp(v, "control")) { char cur[16]; flag_str("ctl", "pet", cur, sizeof cur); const char *n = argc >= 2 ? argv[1] : (!strcmp(cur, "pet") ? "player" : "pet"); if (strcmp(n, "pet") && strcmp(n, "player")) { snprintf(msg, cap, "control pet or player"); return; } set_flag("ctl", n); set_flag("follow", !strcmp(n, "player") ? "player" : "pet"); calc_view(); snprintf(msg, cap, !strcmp(n, "player") ? "INT now controls you (the player)" : "INT now controls %s", party[active].name); return; }
    if (!strcmp(v, "walk")) { int moved = ai_step(active); use_ent(active); if (moved) snprintf(msg, cap, "%s took a step", party[active].name); else snprintf(msg, cap, "%s stays put", party[active].name); return; }
    if (!strcmp(v, "export3d")) { export3d(msg, cap); return; }
    if (!strcmp(v, "play")) { set_flag("running", "1"); clock_paths(); clock_install(); clock_sync(); lc_run(1, "daemon-start", NULL); snprintf(msg, cap, "STARTED: the pets live, the clock runs"); return; }
    if (!strcmp(v, "stop")) { set_flag("running", "0"); clock_sync(); snprintf(msg, cap, "STOPPED: everything is frozen (building and chat still work). Play resumes"); return; }
    if (!strcmp(v, "save") || !strcmp(v, "load")) {      /* slot copies of the state files (pets, trainer, ledger, relations, chat, flags, clock time) */
        int slot = argc >= 2 ? atoi(argv[1]) : 1; if (slot < 1 || slot > 9) slot = 1; int saving = !strcmp(v, "save"); char sd[PATH_MAX]; snprintf(sd, sizeof sd, "%s/slots", ST); mkdir(sd, 0755); snprintf(sd, sizeof sd, "%s/slots/%d", ST, slot); mkdir(sd, 0755);
        char names[40][48]; int nn = 0; snprintf(names[nn++], 48, "house.txt"); snprintf(names[nn++], 48, "trainer.txt"); snprintf(names[nn++], 48, "relations.txt"); snprintf(names[nn++], 48, "chat.txt"); snprintf(names[nn++], 48, "active.txt"); snprintf(names[nn++], 48, "daylight.txt"); snprintf(names[nn++], 48, "flag_ctl.txt"); snprintf(names[nn++], 48, "flag_follow.txt");
        for (int i = 0; i < nparty && nn < 38; i++) snprintf(names[nn++], 48, "pet_%s.txt", party[i].id);
        if (!saving && !fexists(sd)) { snprintf(msg, cap, "slot %d is empty", slot); return; } char mp[PATH_MAX]; snprintf(mp, sizeof mp, "%s/clock_ms.txt", sd);
        if (saving) { clock_paths(); long long ms = clock_kv_ll("game_time_epoch_ms"); char b[40]; snprintf(b, sizeof b, "%lld\n", ms); write_if_changed(mp, b, strlen(b)); }
        int ok = 0; for (int i = 0; i < nn; i++) { char a[PATH_MAX], bpath[PATH_MAX], buf[65536]; if (saving) { spath(a, names[i]); snprintf(bpath, sizeof bpath, "%s/%s", sd, names[i]); } else { snprintf(a, sizeof a, "%s/%s", sd, names[i]); spath(bpath, names[i]); }
            if (!fexists(a)) { if (!saving) remove(bpath); continue; } int n = read_file(a, buf, sizeof buf); write_if_changed(bpath, buf, n); ok++; }
        if (!saving) { char b[40]; read_file(mp, b, sizeof b); long long ms = atoll(b); if (ms > 0) { clock_paths(); clock_install(); char ma[32]; snprintf(ma, sizeof ma, "%lld", ms); lc_run(0, "cmd", "rpg", "settime", ma, "--source", "rpg-load", NULL); lc_run(0, "step", "5", NULL); } pet_load(); }
        snprintf(msg, cap, "%s slot %d (%d files)", saving ? "saved to" : "loaded from", slot, ok); return; }
    if (!strcmp(v, "clearchat")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "chat.txt"); snprintf(q, sizeof q, "%s/chat.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "chat cleared (kept as chat.txt.%ld.bak)", now); return; }
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb") || !strcmp(argv[1], "voice"))) { set_flag(argv[1], flag_on(argv[1], !strcmp(argv[1], "voice")) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
    if (!strcmp(v, "view")) { char c[16]; flag_str("view", "2d", c, sizeof c); const char *n = !strcmp(c, "2d") ? "3d" : !strcmp(c, "3d") ? "glyph" : "2d"; set_flag("view", n); snprintf(msg, cap, "view %s (the picture is still 2D: 3D and the Chinese-glyph view are sprint items 7-8)", n); return; }
    if (!strcmp(v, "int")) { int on = !flag_on("int", 0); set_flag("int", on ? "1" : "0"); char d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); char p[PATH_MAX]; spath(p, "interact_relay.txt"); FILE *f = fopen(p, "a"); if (f) fclose(f); snprintf(p, sizeof p, "%s/keyboard/history.txt", ST); f = fopen(p, "a"); if (f) fclose(f);
        clock_sync(); if (on) { set_flag("follow", "pet"); calc_view(); }
        { char cc[16]; flag_str("ctl", "pet", cc, sizeof cc); snprintf(msg, cap, on ? "INT on: you control %s - turn based like a roguelike, time moves when you act, the other pets act on their own, Tab switches pet/player, Esc leaves" : "INT off: real time, every pet lives on its own", !strcmp(cc, "player") ? "the player" : party[active].name); } return; }
    if (!strcmp(v, "clock") && argc >= 2) { clock_paths(); if (!clock_ok()) { snprintf(msg, cap, "no livedesk clock binary"); return; } clock_install();
        if (!strcmp(argv[1], "rate") && argc >= 3) { lc_run(0, "cmd", "rpg", "rate", argv[2], "--source", "rpg-menu", NULL); snprintf(msg, cap, "clock speed %s (real time mode)", argv[2]); }
        else if (!strcmp(argv[1], "advance") && argc >= 3) { lc_run(0, "cmd", "rpg", "advance", argv[2], "--source", "rpg-menu", NULL); lc_run(0, "step", "5", NULL); long long ms = clock_kv_ll("game_time_epoch_ms"); if (ms >= 0) write_phase(ms); snprintf(msg, cap, "skipped %s", argv[2]); }
        else if (!strcmp(argv[1], "start")) { clock_start(); snprintf(msg, cap, "clock started"); } else if (!strcmp(argv[1], "stop")) { clock_stop(); snprintf(msg, cap, "clock stopped"); }
        else if (!strcmp(argv[1], "reinstall")) { lc_run(0, "daemon-stop", NULL); char q[PATH_MAX]; snprintf(q, sizeof q, "%s/.schedule_installed", CROOT); remove(q); clock_install(); lc_run(1, "daemon-start", NULL); snprintf(msg, cap, "schedule reinstalled"); }
        clock_ui(); return; }
    char text[300] = ""; for (int i = 0; i < argc; i++) { if (i) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); }
    if (text[0]) { say(hero.name, text); const char *rp = pet_reply(text); say(party[active].name, rp); if (others[active].room == vroom) speak_reply(active); turn_taken = 1; }
}

/* ---------- event runner: the clock daemon calls  rpg_pet.+x <pkg> <root>  when a time.pdl event fires ---------- */
static int event_main(const char *pkg) {
    const char *ev = strrchr(pkg, '/'); ev = ev ? ev + 1 : pkg; if (!strncmp(ev, "common:", 7)) ev += 7; char b[80];
    if (!strncmp(ev, "phase_", 6)) { snprintf(b, sizeof b, "phase=%s\n", ev + 6); char p[PATH_MAX]; spath(p, "daylight.txt"); write_if_changed(p, b, strlen(b)); }
    else if (!strcmp(ev, "day_tick")) { snprintf(b, sizeof b, "DAY|%ld", (long)time(NULL)); append_line("events.txt", b); }
    return 0;
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
static int daemon_main(void) {
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
    char h1[PATH_MAX], h2[PATH_MAX], d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    long o1 = fsize(h1), o2 = fsize(h2), next_ms[NP], last_ui = 0; int was_run = run_on(); for (int i = 0; i < NP; i++) next_ms[i] = now_ms() + 600 + 450L * i; char msg[200], lastday[40] = "";
    load_data(); replay(); pet_load(); unstack(); clock_start(); out_all("");
    while (!quit_flag) {
        usleep(100000); int dirty = 0, codes[64], n; msg[0] = 0; load_data(); pet_load(); int in = flag_on("int", 0);
        n = poll_keys(h1, &o1, codes, 32); n += poll_keys(h2, &o2, codes + n, 32);     /* the offsets always advance; keys only act while INT is on */
        if (in && n) replay();
        if (in && n) { char kl[300], one[16]; kl[0] = 0; for (int i = 0; i < n && i < 20; i++) { snprintf(one, sizeof one, "%d ", codes[i]); strncat(kl, one, sizeof kl - strlen(kl) - 1); } char kp[PATH_MAX]; spath(kp, "keys_seen.txt"); write_if_changed(kp, kl, strlen(kl)); }      /* last batch of codes, for debugging */
        { int r = run_on(); if (r != was_run) { was_run = r; clock_sync(); dirty = 1; } }
        if (in) for (int i = 0; i < n; i++) { int c = codes[i], dx = 0, dy = 0;
            if (c == 1002 || c == 200) dy = -1; else if (c == 1003 || c == 201) dy = 1; else if (c == 1000 || c == 202) dx = -1; else if (c == 1001 || c == 203) dx = 1;
            else if (c == 27) { set_flag("int", "0"); clock_sync(); snprintf(msg, sizeof msg, "INT off: real time"); dirty = 1; break; }      /* Esc leaves INT (keyboard/history.txt: KEY_PRESSED: 27) */
            else if (!run_on()) continue;
            else if (c == 9) { char cc[16]; flag_str("ctl", "pet", cc, sizeof cc); int toplayer = !strcmp(cc, "pet"); set_flag("ctl", toplayer ? "player" : "pet"); set_flag("follow", toplayer ? "player" : "pet"); snprintf(msg, sizeof msg, toplayer ? "INT now controls you (the player)" : "INT now controls %s", party[active].name); dirty = 1; continue; }
            else if (c == '0') { char cv[16]; flag_str("view", "2d", cv, sizeof cv); set_flag("view", !strcmp(cv, "2d") ? "3d" : "2d"); snprintf(msg, sizeof msg, "view toggled (3D draws with sprint item 7)"); dirty = 1; continue; }
            else if (c >= '1' && c <= '4') { char pv[8]; snprintf(pv, sizeof pv, "%c", c); set_flag("pov", pv); snprintf(msg, sizeof msg, "pov %c stored (3D draws with sprint item 7)", c); dirty = 1; continue; }
            else continue;
            { int ci = ctl_idx(); use_ent(ci); if (do_step(dx, dy, msg, sizeof msg)) { pet_save(); set_flag("follow", ci < 0 ? "player" : "pet"); end_turn(); } else pet_save(); }      /* a bump costs no turn */
            dirty = 1; }
        long ms = now_ms();
        for (int i = 0; i < nparty; i++) if (ms >= next_ms[i]) { next_ms[i] = ms + 900 + rand() % 700; if (in || !run_on()) continue; replay(); if (ai_step(i)) dirty = 1; use_ent(active); }     /* real time (INT off): every pet walks on its own. INT on is turn based: no timers, only your acts advance time */
        if (ms - last_ui > 3000) { last_ui = ms; clock_paths(); clock_ui(); }
        { char dp[PATH_MAX], db[40]; spath(dp, "daylight.txt"); read_file(dp, db, sizeof db); if (strcmp(db, lastday)) { snprintf(lastday, sizeof lastday, "%s", db); dirty = 1; } }
        if (dirty) { replay(); out_all(msg); }    /* nothing changed -> nothing rendered, nothing written, nothing repainted */
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
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    { char d[PATH_MAX]; snprintf(d, sizeof d, "%s", APP); ASSETS[0] = HOUSE[0] = 0;
      for (int up = 0; up < 8; up++) { char *s = strrchr(d, '/'); if (!s) break; *s = 0; char p[PATH_MAX]; snprintf(p, sizeof p, "%s/&.widgits", d); if (!HOUSE[0] && is_dir(p)) snprintf(HOUSE, sizeof HOUSE, "%s", d); snprintf(ASSETS, sizeof ASSETS, "%s/#.NNEST_ASSETS/rmmv-www-img", d); if (is_dir(ASSETS)) break; ASSETS[0] = 0; }
      if (!ASSETS[0] || !HOUSE[0]) { fprintf(stderr, "rpg_pet: no #.NNEST_ASSETS / &.widgits above %s\n", APP); return 1; } }
    if (mode == 2) return event_main(argv[1]);
    if (mode == 1) return daemon_main();
    load_data(); replay(); pet_load(); clock_paths();
    char msg[240]; verb(vargc, vargv, msg, sizeof msg);
    if (turn_taken && flag_on("int", 0)) end_turn();       /* INT on: the owner acted, so it is a turn: the clock advances and every pet steps */
    replay(); out_all(msg); clock_ui(); printf("%s\n", msg[0] ? msg : "ok"); return 0;
}
