/* rpg_pet.c - rpg-pet: the RPG Maker pet house. One C op, two shapes (design + how-to: @.apps/rpg-pet/HOW-TO-RPG-PET.md).
 *
 *   rpg_pet.+x <verb> [args]            one verb, then re-render state/ and exit   (window buttons, harness, scripts)
 *   rpg_pet.+x <pkg> <house> "<text>"   the cli_io action form: the typed text is the verb line, anything that is not a verb is chat
 *   rpg_pet.+x <house> <pkg>            resident mode, started by the window's <module>: polls INT keys, walks the pet on its own, uses doors
 *
 * Verbs: status | buy KEY [X Y] | use KEY | place ID X Y | remove ID | step DX DY | walk [X Y] | toggle chat|hb | view | int | reset
 * Every picture is an RPG Maker MV tile or character frame (sheets under #.NNEST_ASSETS/rmmv-www-img), nothing else is drawn.
 * State (state/, gitignored): house.txt = append-only ledger replayed on every call
 *     BUY|ts|item|price   PLACE|id|ts|item|x|y|room   MOVE|id|ts|x|y   REMOVE|id|ts
 *   pet.txt (room/x/y/dir/steps), flag_*.txt (chat hb int view), chat.txt, events.txt (append-only STEP/DOOR rows = the "event steps walking" ledger).
 * Outputs: state/scene.raw (RGBA 660x430) + scene.receipt.txt, state/ui.txt (key=value vars for rpg-pet.xhtpm). Both written only when their content changed
 * (the renderer repaints a canvas when its file changes - an unconditional rewrite costs CPU).
 * Keys in INT: arrows 200 up / 201 down / 202 left / 203 right (the renderer's reserved band), 27 Esc = INT off, 0 view, 1-4 pov (stored only: 3D is sprint item 7).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"

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

typedef struct { char key[32], label[40], sheet[24], effect[12]; int price, c, r, w, h, wall; } Item;
typedef struct { char id[16], name[32]; int fc, fr; } Room;
typedef struct { int room, tx, dest, ax, ay, autow; } Door;
typedef struct { int id, item, x, y, room; } Placed;
typedef struct { char name[40]; unsigned char *px; int w, h; } Sheet;

static char APP[PATH_MAX], ST[PATH_MAX], ASSETS[PATH_MAX];
static Item cat[MAXI]; static int ncat;
static Room rooms[8]; static int nroom;
static Door doors[24]; static int ndoor;
static Placed pl[MAXP]; static int npl, nid = 1, owned[MAXI], coins;
static Sheet sheets[8]; static int nsheet;
static unsigned char fr[FW * FH * 4];
static struct { int room, x, y, dir, steps; } pet;
static volatile sig_atomic_t quit_flag;

static void spath(char *o, const char *n) { snprintf(o, PATH_MAX, "%s/%s", ST, n); }
static int fexists(const char *p) { struct stat s; return stat(p, &s) == 0; }

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

/* ---------- data: catalog.pdl and rooms.pdl ---------- */
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0; return s; }
static int split_bar(char *line, char **f, int max) { int n = 0; char *p = line; while (n < max) { f[n++] = p; char *b = strchr(p, '|'); if (!b) break; *b = 0; p = b + 1; } for (int i = 0; i < n; i++) f[i] = trim(f[i]); return n; }
static int room_idx(const char *id) { for (int i = 0; i < nroom; i++) if (!strcmp(rooms[i].id, id)) return i; return -1; }
static int item_idx(const char *k) { for (int i = 0; i < ncat; i++) if (!strcmp(cat[i].key, k)) return i; return -1; }

static void load_data(void) {
    char p[PATH_MAX], line[400]; char *f[12]; FILE *fp;
    snprintf(p, sizeof p, "%s/catalog.pdl", APP); ncat = 0;
    if ((fp = fopen(p, "r"))) { while (fgets(line, sizeof line, fp) && ncat < MAXI) { if (split_bar(line, f, 12) == 11 && !strcmp(f[0], "ITEM")) { Item *it = &cat[ncat++]; snprintf(it->key, 32, "%s", f[1]); snprintf(it->label, 40, "%s", f[2]); it->price = atoi(f[3]); snprintf(it->sheet, 24, "%s", f[4]); it->c = atoi(f[5]); it->r = atoi(f[6]); it->w = atoi(f[7]); it->h = atoi(f[8]); it->wall = !strcmp(f[9], "wall"); snprintf(it->effect, 12, "%s", f[10]); } } fclose(fp); }
    snprintf(p, sizeof p, "%s/rooms.pdl", APP); nroom = ndoor = 0;
    if ((fp = fopen(p, "r"))) {
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 5 && !strcmp(f[0], "ROOM") && nroom < 8) { Room *r = &rooms[nroom++]; snprintf(r->id, 16, "%s", f[1]); snprintf(r->name, 32, "%s", f[2]); r->fc = atoi(f[3]); r->fr = atoi(f[4]); } }
        rewind(fp);
        while (fgets(line, sizeof line, fp)) { int n = split_bar(line, f, 12); if (n >= 7 && !strcmp(f[0], "DOOR") && ndoor < 24) { int a = room_idx(f[1]), b = room_idx(f[3]); if (a < 0 || b < 0) continue; Door *d = &doors[ndoor++]; d->room = a; d->tx = atoi(f[2]); d->dest = b; d->ax = atoi(f[4]); d->ay = atoi(f[5]); d->autow = atoi(f[6]); } }
        fclose(fp);
    }
}

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

/* ---------- pet state ---------- */
static void pet_load(void) {
    char p[PATH_MAX], b[300]; spath(p, "pet.txt"); read_file(p, b, sizeof b); pet.room = 0; pet.x = 6; pet.y = 5; pet.dir = 0; pet.steps = 0;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "room=", 5)) { int r = room_idx(l + 5); if (r >= 0) pet.room = r; } else if (!strncmp(l, "x=", 2)) pet.x = atoi(l + 2); else if (!strncmp(l, "y=", 2)) pet.y = atoi(l + 2); else if (!strncmp(l, "dir=", 4)) pet.dir = atoi(l + 4); else if (!strncmp(l, "steps=", 6)) pet.steps = atoi(l + 6); }
}
static void pet_save(void) { char p[PATH_MAX], b[200]; spath(p, "pet.txt"); snprintf(b, sizeof b, "room=%s\nx=%d\ny=%d\ndir=%d\nsteps=%d\n", rooms[pet.room].id, pet.x, pet.y, pet.dir, pet.steps); write_if_changed(p, b, strlen(b)); }

/* ---------- placement rules ---------- */
static int floor_blocked(int room, int x, int y, int skip_id) { for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || cat[q->item].wall) continue; if (x >= q->x && x < q->x + cat[q->item].w && y >= q->y && y < q->y + cat[q->item].h) return q->id; } return 0; }
static int door_here(int room, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx == x && y == TRIG_ROW) return i; return -1; }
static const char *check_place(int room, int item, int x, int y, int skip_id) {
    static char m[120]; Item *d = &cat[item];
    if (d->wall) {
        int wy = WALL_ROW + 1 - d->h;
        if (y != wy || x < 0 || x + d->w > COLS) { snprintf(m, sizeof m, "wall items go on the wall: x 0-%d, y %d", COLS - d->w, wy); return m; }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == room && doors[i].tx >= x && doors[i].tx < x + d->w) return "that wall spot is a door";
        for (int i = 0; i < npl; i++) { Placed *q = &pl[i]; if (q->room != room || q->id == skip_id || !cat[q->item].wall) continue; if (x < q->x + cat[q->item].w && q->x < x + d->w) return "wall spot taken"; }
        return "";
    }
    if (x < 0 || y < TRIG_ROW || x + d->w > COLS || y + d->h > ROWS) { snprintf(m, sizeof m, "floor items go on the floor: x 0-%d, y %d-%d", COLS - d->w, TRIG_ROW, ROWS - d->h); return m; }
    for (int i = 0; i < d->w; i++) for (int j = 0; j < d->h; j++) {
        int cx = x + i, cy = y + j; int o;
        if (room == pet.room && cx == pet.x && cy == pet.y) return "the pet is standing there";
        if (door_here(room, cx, cy) >= 0) return "that cell is in front of a door";
        if ((o = floor_blocked(room, cx, cy, skip_id))) { snprintf(m, sizeof m, "spot taken by #%d", o); return m; }
    }
    return "";
}
static int first_free(int room, int item, int *ox, int *oy) { Item *d = &cat[item]; if (d->wall) { int wy = WALL_ROW + 1 - d->h; for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, wy, 0)[0]) { *ox = x; *oy = wy; return 1; } return 0; } for (int y = TRIG_ROW; y < ROWS; y++) for (int x = 0; x < COLS; x++) if (!check_place(room, item, x, y, 0)[0]) { *ox = x; *oy = y; return 1; } return 0; }

/* ---------- drawing ---------- */
static Sheet *sheet(const char *dir, const char *name) {
    for (int i = 0; i < nsheet; i++) if (!strcmp(sheets[i].name, name)) return &sheets[i];
    if (nsheet >= 8) return NULL;
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
    Room *rm = &rooms[pet.room];
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) { if (y == 0) tile("Inside_A5", 0, 0, 1, 1, x, y); else if (y == WALL_ROW) tile("Inside_A5", 1, 3, 1, 1, x, y); else tile("Inside_A5", rm->fc, rm->fr, 1, 1, x, y); }
    Sheet *dr = sheet("characters", "!Door1");                                  /* the wood panel door: closed frame of the 4th door object */
    for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room) { blit(dr, 0, 192, T, T, OX + doors[i].tx * T, OY + WALL_ROW * T); tile("Inside_A5", 0, 6, 1, 1, doors[i].tx, TRIG_ROW); }
    int idx[MAXP], n = 0; for (int i = 0; i < npl; i++) if (pl[i].room == pet.room) idx[n++] = i;
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) { Placed *p = &pl[idx[a]], *q = &pl[idx[b]]; int ka = (!cat[p->item].wall) * 1000 + p->y + cat[p->item].h, kb = (!cat[q->item].wall) * 1000 + q->y + cat[q->item].h; if (kb < ka || (kb == ka && q->x < p->x)) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; } }
    for (int a = 0; a < n; a++) { Placed *p = &pl[idx[a]]; Item *d = &cat[p->item]; tile(d->sheet, d->c, d->r, d->w, d->h, p->x, p->y); }
    static const int cyc[4] = {1, 0, 1, 2}; int col = pet.steps ? cyc[pet.steps % 4] : 1; /* walking frames 1,0,1,2; standing = middle frame */
    blit(sheet("characters", "Actor1"), col * T, pet.dir * T, T, T, OX + pet.x * T, OY + pet.y * T - 8);
}

/* ---------- chat ---------- */
static void say(const char *who, const char *text) { char b[300]; snprintf(b, sizeof b, "%s: %s", who, text); append_line("chat.txt", b); }
static const char *pet_reply(const char *text) {
    static char b[80]; char s[200]; snprintf(s, sizeof s, "%s", text); for (char *c = s; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    int n = npl;
    #define HAS(e) ({ int h = 0; for (int i = 0; i < npl; i++) if (!strcmp(cat[pl[i].item].effect, e)) h = 1; h; })
    if (strstr(s, "bed") || strstr(s, "sleep")) return HAS("sleep") ? "zzz... a bed!" : "I have no bed yet.";
    if (strstr(s, "eat") || strstr(s, "food") || strstr(s, "table")) return HAS("eat") ? "yum, a table to eat at" : "I have nowhere to eat.";
    if (strstr(s, "book") || strstr(s, "read")) return HAS("read") ? "I like reading." : "no books here.";
    if (strstr(s, "piano") || strstr(s, "play")) return HAS("play") ? "la la la" : "no piano...";
    snprintf(b, sizeof b, "(%d things in my house)", n); return b;
}

/* ---------- outputs ---------- */
static char status_msg[200];
static void out_all(const char *msg) {
    render();
    char p[PATH_MAX]; spath(p, "scene.raw"); write_if_changed(p, fr, sizeof fr);
    spath(p, "scene.receipt.txt"); { char b[64]; snprintf(b, sizeof b, "frame_w=%d\nframe_h=%d\n", FW, FH); write_if_changed(p, b, strlen(b)); }
    static char u[16384]; int o = 0;
#define W(...) o += snprintf(u + o, sizeof u - o, __VA_ARGS__)
    char raw[PATH_MAX]; spath(raw, "scene.raw"); char h1[PATH_MAX], h2[PATH_MAX]; spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    int chat = flag_on("chat", 1), hb = flag_on("hb", 1), in = flag_on("int", 0); char view[16]; flag_str("view", "2d", view, sizeof view); for (char *c = view; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    W("title=rpg-pet  -  %s  -  coins %d\nscene_raw=%s\ncanvas_raw=%s\ncoins=%d\nmsg=%s\nroom_label=%s\n", rooms[pet.room].name, coins, raw, raw, coins, msg && msg[0] ? msg : "click an item to buy it", rooms[pet.room].name);
    W("interact_class=%s\ninteract_label=%s\nrp_h1=%s\nrp_h2=%s\n", in ? "interact-active" : "", in ? "on" : "off", h1, h2);
    W("chat_visible=%s\nhb_visible=%s\nview_label=view %s\nchat_toggle_label=window %s\n", chat ? "1" : "", hb ? "1" : "", view, chat ? "on" : "off");
    { char cp[PATH_MAX], cb[4000]; spath(cp, "chat.txt"); read_file(cp, cb, sizeof cb); char *ln[6] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 6] = l; k++; } for (int i = 0; i < 6; i++) { int j = k - 6 + i; W("chat_%d=%s\n", i, j >= 0 ? ln[j % 6] : ""); } }
    W("n_shop=%d\n", ncat); for (int i = 0; i < ncat; i++) { char own[16] = ""; if (owned[i] > 0) snprintf(own, sizeof own, " x%d", owned[i]); W("shop_%d_label=%s %dc%s\nshop_%d_key=%s\n", i, cat[i].label, cat[i].price, own, i, cat[i].key); }
    W("n_room=%d\n", nroom); for (int i = 0; i < nroom; i++) { int c = 0; for (int j = 0; j < npl; j++) if (pl[j].room == i) c++; W("room_%d_label=%s%s (%d things)\nroom_%d_id=%s\n", i, rooms[i].name, i == pet.room ? "  <- here" : "", c, i, rooms[i].id); }
    int nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) nb++; W("hb_n_slots=%d\nhb_title=bag: click a slot to place it\n", nb); nb = 0; for (int i = 0; i < ncat && nb < 5; i++) if (owned[i] > 0) { W("hb_%d_text=%s x%d\nhb_%d_key=%s\n", nb, cat[i].label, owned[i], nb, cat[i].key); nb++; }
    int np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == pet.room) np++; W("n_placed=%d\n", np); np = 0; for (int i = 0; i < npl; i++) if (pl[i].room == pet.room) { W("placed_%d_label=#%d %s (%d,%d) %s\nplaced_%d_id=%d\n", np, pl[i].id, cat[pl[i].item].label, pl[i].x, pl[i].y, cat[pl[i].item].effect, np, pl[i].id); np++; }
    spath(p, "ui.txt"); write_if_changed(p, u, o);
}

/* ---------- walking: event steps ---------- */
static int passable(int x, int y) { return x >= 0 && x < COLS && y >= TRIG_ROW && y < ROWS && !floor_blocked(pet.room, x, y, 0); }
static void teleport(int di) {
    Door *d = &doors[di]; char b[160]; snprintf(b, sizeof b, "DOOR|%ld|%s|%s", (long)time(NULL), rooms[d->room].id, rooms[d->dest].id); append_line("events.txt", b);
    pet.room = d->dest; pet.x = d->ax; pet.y = d->ay; pet.dir = 0; snprintf(b, sizeof b, "went through the door to the %s", rooms[pet.room].name); say("pet", b);
}
/* one tile step; returns 1 if the pet moved. A step onto a door's trigger cell teleports (the door contract). */
static int do_step(int dx, int dy, char *msg, size_t cap) {
    pet.dir = dy > 0 ? 0 : dx < 0 ? 1 : dx > 0 ? 2 : 3; int nx = pet.x + dx, ny = pet.y + dy;
    if (!passable(nx, ny)) { snprintf(msg, cap, "bump"); pet_save(); return 0; }
    pet.x = nx; pet.y = ny; pet.steps++; char b[100]; snprintf(b, sizeof b, "STEP|%ld|%s|%d|%d", (long)time(NULL), rooms[pet.room].id, nx, ny); append_line("events.txt", b);
    int di = door_here(pet.room, nx, ny); if (di >= 0) { teleport(di); snprintf(msg, cap, "door: now in the %s", rooms[pet.room].name); } else snprintf(msg, cap, "pet at %d,%d", nx, ny);
    pet_save(); return 1;
}
/* breadth-first path over free cells; fills the first move toward (tx,ty) */
static int bfs_first(int tx, int ty, int *dx, int *dy) {
    int dist[ROWS][COLS], qx[ROWS * COLS], qy[ROWS * COLS], h = 0, t = 0; for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) dist[y][x] = -1;
    int fx[ROWS][COLS], fy[ROWS][COLS]; dist[ty][tx] = 0; qx[t] = tx; qy[t++] = ty; static const int mx[4] = {0, -1, 1, 0}, my[4] = {1, 0, 0, -1};
    while (h < t) { int x = qx[h], y = qy[h++]; for (int k = 0; k < 4; k++) { int nx = x + mx[k], ny = y + my[k]; if (nx < 0 || nx >= COLS || ny < TRIG_ROW || ny >= ROWS || dist[ny][nx] >= 0) continue; if (floor_blocked(pet.room, nx, ny, 0)) continue; dist[ny][nx] = dist[y][x] + 1; fx[ny][nx] = x; fy[ny][nx] = y; qx[t] = nx; qy[t++] = ny; } }
    if (dist[pet.y][pet.x] <= 0) return 0;      /* already there or unreachable */
    int nx = fx[pet.y][pet.x], ny = fy[pet.y][pet.x]; *dx = nx - pet.x; *dy = ny - pet.y; return 1;
}
static int walk_step(int tx, int ty, char *msg, size_t cap) { int dx, dy; if (!bfs_first(tx, ty, &dx, &dy)) return 0; return do_step(dx, dy, msg, cap); }
/* choose where to wander: sometimes an auto door of this room (then the walk ends in a teleport), otherwise a random free cell */
static void pick_target(int *tx, int *ty) {
    int cand[8], nc = 0; for (int i = 0; i < ndoor; i++) if (doors[i].room == pet.room && doors[i].autow) cand[nc++] = i;
    if (nc && rand() % 100 < 22) { Door *d = &doors[cand[rand() % nc]]; *tx = d->tx; *ty = TRIG_ROW; return; }
    for (int k = 0; k < 40; k++) { int x = rand() % COLS, y = TRIG_ROW + rand() % (ROWS - TRIG_ROW); if (passable(x, y) && door_here(pet.room, x, y) < 0) { *tx = x; *ty = y; return; } }
    *tx = pet.x; *ty = pet.y;
}

/* ---------- verbs ---------- */
static void verb(int argc, char **argv, char *msg, size_t cap) {
    const char *v = argc ? argv[0] : "status"; long now = (long)time(NULL); char b[200]; msg[0] = 0;
    if (!strcmp(v, "status")) return;
    if (!strcmp(v, "reset")) { char p[PATH_MAX], q[PATH_MAX]; spath(p, "house.txt"); snprintf(q, sizeof q, "%s/house.txt.%ld.bak", ST, now); if (fexists(p)) rename(p, q); snprintf(msg, cap, "house reset (the old ledger is kept as house.txt.%ld.bak)", now); return; }
    if (!strcmp(v, "buy") && argc >= 2) {
        int i = item_idx(argv[1]); int x, y; if (i < 0) { snprintf(msg, cap, "unknown item %s", argv[1]); return; }
        if (cat[i].price > coins) { snprintf(msg, cap, "not enough coins: %s costs %d, you have %d", cat[i].label, cat[i].price, coins); return; }
        if (argc >= 4) { x = atoi(argv[2]); y = atoi(argv[3]); const char *bad = check_place(pet.room, i, x, y, 0); if (bad[0]) { snprintf(msg, cap, "not bought: %s", bad); return; } }
        else if (!first_free(pet.room, i, &x, &y)) { snprintf(msg, cap, "not bought: no free spot"); return; }
        snprintf(b, sizeof b, "BUY|%ld|%s|%d", now, cat[i].key, cat[i].price); append_line("house.txt", b); snprintf(b, sizeof b, "PLACE|%d|%ld|%s|%d|%d|%s", nid, now, cat[i].key, x, y, rooms[pet.room].id); append_line("house.txt", b);
        snprintf(msg, cap, "bought and placed %s as #%d at %d,%d (%d coins left)", cat[i].label, nid, x, y, coins - cat[i].price); return;
    }
    if (!strcmp(v, "use") && argc >= 2) { int i = item_idx(argv[1]), x, y; if (i < 0 || owned[i] <= 0) { snprintf(msg, cap, "none of that in the bag"); return; } if (!first_free(pet.room, i, &x, &y)) { snprintf(msg, cap, "no free spot"); return; }
        snprintf(b, sizeof b, "PLACE|%d|%ld|%s|%d|%d|%s", nid, now, cat[i].key, x, y, rooms[pet.room].id); append_line("house.txt", b); snprintf(msg, cap, "placed %s as #%d at %d,%d", cat[i].label, nid, x, y); return; }
    if (!strcmp(v, "place") && argc >= 4) { int id = atoi(argv[1]), x = atoi(argv[2]), y = atoi(argv[3]), k = -1; for (int i = 0; i < npl; i++) if (pl[i].id == id) k = i; if (k < 0) { snprintf(msg, cap, "no placed item #%d", id); return; }
        const char *bad = check_place(pl[k].room, pl[k].item, x, y, id); if (bad[0]) { snprintf(msg, cap, "not moved: %s", bad); return; } snprintf(b, sizeof b, "MOVE|%d|%ld|%d|%d", id, now, x, y); append_line("house.txt", b); snprintf(msg, cap, "moved #%d to %d,%d", id, x, y); return; }
    if (!strcmp(v, "remove") && argc >= 2) { int id = atoi(argv[1]), k = -1; for (int i = 0; i < npl; i++) if (pl[i].id == id) k = i; if (k < 0) { snprintf(msg, cap, "no placed item #%d", id); return; }
        snprintf(b, sizeof b, "REMOVE|%d|%ld", id, now); append_line("house.txt", b); snprintf(msg, cap, "put #%d (%s) back in the bag", id, cat[pl[k].item].label); return; }
    if (!strcmp(v, "goto") && argc >= 2) {        /* the Rooms dropdown: put the pet in that room, on the first free cell near the middle */
        int r = room_idx(argv[1]); if (r < 0) { snprintf(msg, cap, "no room %s", argv[1]); return; } pet.room = r; pet.dir = 0;
        for (int k = 0; k < COLS * ROWS; k++) { int x = (6 + k) % COLS, y = TRIG_ROW + 3 + ((6 + k) / COLS) % (ROWS - TRIG_ROW - 3 > 0 ? ROWS - TRIG_ROW - 3 : 1); if (passable(x, y) && door_here(r, x, y) < 0) { pet.x = x; pet.y = y; break; } }
        snprintf(b, sizeof b, "DOOR|%ld|menu|%s", now, rooms[r].id); append_line("events.txt", b); pet_save(); snprintf(msg, cap, "the pet is in the %s", rooms[r].name); return; }
    if (!strcmp(v, "step") && argc >= 3) { do_step(atoi(argv[1]), atoi(argv[2]), msg, cap); return; }
    if (!strcmp(v, "walk")) { int tx, ty; if (argc >= 3) { tx = atoi(argv[1]); ty = atoi(argv[2]); } else pick_target(&tx, &ty); if (!walk_step(tx, ty, msg, cap)) snprintf(msg, cap, "pet is at the target"); return; }
    if (!strcmp(v, "toggle") && argc >= 2 && (!strcmp(argv[1], "chat") || !strcmp(argv[1], "hb"))) { set_flag(argv[1], flag_on(argv[1], 1) ? "0" : "1"); snprintf(msg, cap, "%s toggled", argv[1]); return; }
    if (!strcmp(v, "view")) { char c[16]; flag_str("view", "2d", c, sizeof c); const char *n = !strcmp(c, "2d") ? "3d" : !strcmp(c, "3d") ? "glyph" : "2d"; set_flag("view", n); snprintf(msg, cap, "view %s (the picture is still 2D: 3D and the Chinese-glyph view are sprint items 7-8)", n); return; }
    if (!strcmp(v, "int")) { int on = !flag_on("int", 0); set_flag("int", on ? "1" : "0"); char d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); char p[PATH_MAX]; spath(p, "interact_relay.txt"); FILE *f = fopen(p, "a"); if (f) fclose(f); snprintf(p, sizeof p, "%s/keyboard/history.txt", ST); f = fopen(p, "a"); if (f) fclose(f);
        snprintf(msg, cap, on ? "INT on: arrow keys walk the pet, Esc leaves" : "INT off: the pet walks by itself"); return; }
    char text[300] = ""; for (int i = 0; i < argc; i++) { if (i) strncat(text, " ", sizeof text - strlen(text) - 1); strncat(text, argv[i], sizeof text - strlen(text) - 1); }
    if (text[0]) { say("you", text); say("pet", pet_reply(text)); }
}

/* ---------- resident mode ---------- */
static void bye(int s) { (void)s; quit_flag = 1; }
static long fsize(const char *p) { struct stat s; return stat(p, &s) == 0 ? (long)s.st_size : 0; }
/* new "<code> <ms>" lines since the stored offset; returns the codes */
static int poll_keys(const char *p, long *off, int *codes, int max) {
    long sz = fsize(p); if (sz < *off) *off = 0; if (sz == *off) return 0; FILE *f = fopen(p, "r"); if (!f) return 0; fseek(f, *off, SEEK_SET); char l[80]; int n = 0;
    while (n < max && fgets(l, sizeof l, f)) { if (l[0] == '#' || l[0] < '0' || l[0] > '9') continue; codes[n++] = atoi(l); }       /* "<decimal code> <ms>" */
    *off = ftell(f); fclose(f); return n;
}
static int daemon_main(void) {
    signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye); srand((unsigned)(time(NULL) ^ getpid()));
    char h1[PATH_MAX], h2[PATH_MAX], d[PATH_MAX]; snprintf(d, sizeof d, "%s/keyboard", ST); mkdir(d, 0755); spath(h1, "interact_relay.txt"); snprintf(h2, sizeof h2, "%s/keyboard/history.txt", ST);
    long o1 = fsize(h1), o2 = fsize(h2), last_walk = 0; int tx = -1, ty = -1; struct timespec ts; char msg[200];
    load_data(); replay(); pet_load(); out_all("");
    while (!quit_flag) {
        usleep(100000); int dirty = 0, codes[64], n; msg[0] = 0; pet_load(); int in = flag_on("int", 0);
        n = poll_keys(h1, &o1, codes, 32); n += poll_keys(h2, &o2, codes + n, 32);
        if (n || in) { /* keys are read always (offsets advance) but only act while INT is armed */ }
        if (in && n) { load_data(); replay(); }
        if (in) for (int i = 0; i < n; i++) { int c = codes[i], dx = 0, dy = 0;
            if (c == 200) dy = -1; else if (c == 201) dy = 1; else if (c == 202) dx = -1; else if (c == 203) dx = 1;
            else if (c == 27) { set_flag("int", "0"); snprintf(msg, sizeof msg, "INT off"); dirty = 1; continue; }
            else if (c == '0') { char cv[16]; flag_str("view", "2d", cv, sizeof cv); set_flag("view", !strcmp(cv, "2d") ? "3d" : "2d"); snprintf(msg, sizeof msg, "view toggled (3D draws with sprint item 7)"); dirty = 1; continue; }
            else if (c >= '1' && c <= '4') { char pv[8]; snprintf(pv, sizeof pv, "%c", c); set_flag("pov", pv); snprintf(msg, sizeof msg, "pov %c stored (3D draws with sprint item 7)", c); dirty = 1; continue; }
            else continue;
            do_step(dx, dy, msg, sizeof msg); dirty = 1; tx = -1; }
        clock_gettime(CLOCK_MONOTONIC, &ts); long ms = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
        if (!in && ms - last_walk > 900) {                    /* the event step: one tile per step, with pauses between walks */
            last_walk = ms; load_data(); replay();
            if (tx >= 0 && pet.x == tx && pet.y == ty) tx = -1;
            if (tx < 0 && rand() % 100 < 40) pick_target(&tx, &ty);
            if (tx >= 0) { int before = pet.room; if (walk_step(tx, ty, msg, sizeof msg)) dirty = 1; else tx = -1; if (pet.room != before) tx = -1; } }
        if (dirty) { load_data(); replay(); out_all(msg); }    /* nothing changed -> nothing rendered, nothing written, nothing repainted */
    }
    return 0;
}

/* ---------- main ---------- */
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
int main(int argc, char **argv) {
    int daemon = 0; char *vargv[32]; int vargc = 0; char textbuf[400];
    if (argc == 3 && is_dir(argv[1]) && is_dir(argv[2])) { daemon = 1; snprintf(APP, sizeof APP, "%s", argv[2]); }     /* module form: house, pkg */
    else {
        char exe[PATH_MAX]; if (!realpath(argv[0], exe)) snprintf(exe, sizeof exe, "%s", argv[0]);
        for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (s) *s = 0; } snprintf(APP, sizeof APP, "%s", exe);          /* .../rpg-pet/ops/+x/rpg_pet.+x -> .../rpg-pet */
        if (argc >= 4 && is_dir(argv[1])) { snprintf(textbuf, sizeof textbuf, "%s", argv[3]); for (char *t = strtok(textbuf, " "); t && vargc < 31; t = strtok(NULL, " ")) vargv[vargc++] = t; }
        else for (int i = 1; i < argc && vargc < 31; i++) vargv[vargc++] = argv[i];
    }
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755);
    { char d[PATH_MAX]; snprintf(d, sizeof d, "%s", APP); for (int up = 0; up < 8; up++) { char *s = strrchr(d, '/'); if (!s) break; *s = 0; snprintf(ASSETS, sizeof ASSETS, "%s/#.NNEST_ASSETS/rmmv-www-img", d); if (is_dir(ASSETS)) break; ASSETS[0] = 0; } if (!ASSETS[0]) { fprintf(stderr, "rpg_pet: no #.NNEST_ASSETS above %s\n", APP); return 1; } }
    if (daemon) return daemon_main();
    load_data(); replay(); pet_load();
    char msg[240]; verb(vargc, vargv, msg, sizeof msg);
    replay(); out_all(msg); printf("%s\n", msg[0] ? msg : "ok"); return 0;
}
