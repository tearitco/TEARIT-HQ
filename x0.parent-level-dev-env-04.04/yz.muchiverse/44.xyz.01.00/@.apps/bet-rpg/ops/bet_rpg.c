/* Casino Island, first window. Cast from party.pdl. Clock id bet.
 * Walks the pets. No chips, no tables, no hotel charge. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define NP 8
#define MAXR 16
#define MAXD 24
#define MAXS 16
#define FW 512
#define FH 320
static char HOUSE[PATH_MAX], APP[PATH_MAX], ST[PATH_MAX], SELF[PATH_MAX];
static char LC[PATH_MAX], CROOT[PATH_MAX], CFILE[PATH_MAX];
typedef struct { char id[16], name[32]; int fc, fr, over, cols, rows, tw; } Room;
typedef struct { int room, tx, ty, dest, ax, ay, autow; } Door;
typedef struct { int room, c, r, w, h, x, y; char sheet[24]; } Scen;
typedef struct { char name[24], id[16], sheet[24], room[16]; int idx, ri, x, y; } Pet;
static Pet party[NP]; static int nparty, active;
static void chat_line(const char *s);
static Room rooms[MAXR]; static int nroom;
static Door doors[MAXD]; static int ndoor;
static Scen scen[MAXS]; static int nscen;
static int flag_int(void) { char p[PATH_MAX], b[8]; snprintf(p, sizeof p, "%s/flag_int.txt", ST); FILE *f = fopen(p, "r"); if (!f) return 0; if (!fgets(b, sizeof b, f)) b[0] = 0; fclose(f); return b[0] == '1'; }
static void set_int(int on) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/flag_int.txt", ST); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", on ? 1 : 0); fclose(f); } }
static int room_idx(const char *id) { for (int i = 0; i < nroom; i++) if (!strcmp(rooms[i].id, id)) return i; return -1; }
static void load_rooms(void) {
    char p[PATH_MAX], line[240]; snprintf(p, sizeof p, "%s/rooms.pdl", APP); FILE *f = fopen(p, "r"); nroom = ndoor = nscen = 0; if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '-') continue;
        char *bar = strchr(line, '|'); if (!bar) continue;
        char kind[16]; snprintf(kind, sizeof kind, "%s", line); char *sp = strchr(kind, '|'); if (sp) *sp = 0; for (char *c = kind + strlen(kind) - 1; c >= kind && (*c == ' ' || *c == '\t'); c--) *c = 0;
        char *f0 = bar + 1;
        if (!strcmp(kind, "ROOM") && nroom < MAXR) {
            Room *r = &rooms[nroom]; memset(r, 0, sizeof *r); r->cols = 13; r->rows = 8; r->tw = 32;
            char over[16] = "", biome[24] = ""; int cols = 13, rows = 8, tw = 32;
            int n = sscanf(f0, " %15s | %31[^|] | %d | %d | %15s | %d | %d | %d | %23s", r->id, r->name, &r->fc, &r->fr, over, &cols, &rows, &tw, biome);
            if (n < 4) continue;
            for (char *c = r->id; *c; c++) if (*c == ' ') *c = 0;
            { char *e = r->name + strlen(r->name); while (e > r->name && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0; }
            if (n >= 5 && !strcmp(over, "over")) { r->over = 1; r->cols = cols; r->rows = rows; r->tw = tw > 0 ? tw : 24; }
            nroom++;
        } else if (!strcmp(kind, "DOOR") && ndoor < MAXD) {
            char rm[16], dest[16]; int tx, ax, ay, aut = 1, ty = 1;
            if (sscanf(f0, " %15s | %d | %15s | %d | %d | %d | %d", rm, &tx, dest, &ax, &ay, &aut, &ty) < 5) continue;
            ndoor++;
        } else if (!strcmp(kind, "SCENERY") && nscen < MAXS) {
            char rm[16], sh[24]; int c, r, w, h, x, y;
            if (sscanf(f0, " %15s | %23s | %d | %d | %d | %d | %d | %d", rm, sh, &c, &r, &w, &h, &x, &y) < 8) continue;
            Scen *s = &scen[nscen]; memset(s, 0, sizeof *s); snprintf(s->sheet, sizeof s->sheet, "%s", sh); s->c = c; s->r = r; s->w = w; s->h = h; s->x = x; s->y = y; s->room = -1; nscen++;
        }
    }
    fclose(f);
    /* second pass: the names were parsed into the same file again */
    f = fopen(p, "r"); int di = 0, si = 0;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "DOOR", 4) == 0 && di < ndoor) {
            char rm[16], dest[16]; int tx, ax, ay, aut = 1, ty = 1;
            if (sscanf(line, "DOOR | %15s | %d | %15s | %d | %d | %d | %d", rm, &tx, dest, &ax, &ay, &aut, &ty) >= 5) {
                doors[di].room = room_idx(rm); doors[di].dest = room_idx(dest); doors[di].tx = tx; doors[di].ax = ax; doors[di].ay = ay; doors[di].autow = aut; doors[di].ty = ty; di++;
            }
        } else if (strncmp(line, "SCENERY", 7) == 0 && si < nscen) {
            char rm[16], sh[24]; int c, r0, w, h, x, y;
            if (sscanf(line, "SCENERY | %15s | %23s | %d | %d | %d | %d | %d | %d", rm, sh, &c, &r0, &w, &h, &x, &y) >= 8) { scen[si].room = room_idx(rm); si++; }
        }
    }
    fclose(f);
}
static int door_at(int ri, int x, int y) { for (int i = 0; i < ndoor; i++) if (doors[i].room == ri && doors[i].tx == x && y == doors[i].ty + 1) return i; return -1; }
static int blocked(int ri, int x, int y) {
    if (ri < 0 || ri >= nroom) return 1;
    Room *r = &rooms[ri];
    if (x < 0 || y < 0 || x >= r->cols || y >= r->rows) return 1;
    if (r->over) { if (x < 2 || x > r->cols - 3 || y < 1 || y > r->rows - 2) return 1; if ((x <= 3 || x >= r->cols - 4) && (y <= 2 || y >= r->rows - 3)) return 1; }
    for (int i = 0; i < nscen; i++) if (scen[i].room == ri && x >= scen[i].x && x < scen[i].x + scen[i].w && y >= scen[i].y && y < scen[i].y + scen[i].h) return 1;
    return 0;
}
static void arrive(Pet *a, int di) {
    Door *d = &doors[di]; if (d->dest < 0 || d->dest >= nroom) return;
    a->ri = d->dest; snprintf(a->room, sizeof a->room, "%s", rooms[d->dest].id);
    int x = d->ax, y = d->ay;
    if (door_at(a->ri, x, y) >= 0 || blocked(a->ri, x, y)) { if (!blocked(a->ri, x, y + 1) && door_at(a->ri, x, y + 1) < 0) y++; else if (!blocked(a->ri, x, y - 1)) y--; }
    a->x = x; a->y = y;
    char ln[120]; snprintf(ln, sizeof ln, "%s walks into the %s", a->name, rooms[a->ri].name); chat_line(ln);
}
static void load_party(void) {
    load_rooms();
    char p[PATH_MAX], line[200]; snprintf(p, sizeof p, "%s/party.pdl", APP); FILE *f = fopen(p, "r"); nparty = 0; if (!f) return;
    while (fgets(line, sizeof line, f) && nparty < NP) {
        char n[24], sh[24]; int ix = 0;
        if (sscanf(line, "PET | %23s | %23s | %d", n, sh, &ix) != 3) continue;
        Pet *a = &party[nparty]; memset(a, 0, sizeof *a); snprintf(a->name, sizeof a->name, "%s", n); snprintf(a->id, sizeof a->id, "%s", n); snprintf(a->sheet, sizeof a->sheet, "%s", sh); a->idx = ix;
        for (char *c = a->id; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
        snprintf(a->room, sizeof a->room, "island"); a->x = 6 + (nparty % 6); a->y = 6; a->ri = room_idx("island"); if (a->ri < 0) a->ri = 0;
        char pp[PATH_MAX], b[80]; snprintf(pp, sizeof pp, "%s/pet_%s.txt", ST, a->id);
        FILE *pf = fopen(pp, "r"); if (pf) { if (fgets(b, sizeof b, pf)) { char rm[16]; int x, y; if (sscanf(b, "%15s %d %d", rm, &x, &y) == 3 && room_idx(rm) >= 0) { snprintf(a->room, sizeof a->room, "%s", rm); a->ri = room_idx(rm); a->x = x; a->y = y; } } fclose(pf); }
        nparty++;
    }
    fclose(f);
    char ap[PATH_MAX], ab[40]; snprintf(ap, sizeof ap, "%s/active.txt", ST); FILE *af = fopen(ap, "r");
    if (af) { if (fgets(ab, sizeof ab, af)) { char *e = strchr(ab, '\n'); if (e) *e = 0; for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, ab)) active = i; } fclose(af); }
}
static void save_active(void) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/active.txt", ST); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%s\n", nparty ? party[active].id : ""); fclose(f); } }
static void chat_line(const char *s) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/chat.txt", ST); FILE *f = fopen(p, "a"); if (f) { fprintf(f, "%s\n", s); fclose(f); }
    FILE *r = fopen(p, "r"); if (!r) return; char lines[40][160]; int n = 0;
    while (n < 40 && fgets(lines[n], sizeof lines[n], r)) n++;
    fclose(r); if (n <= 24) return;
    f = fopen(p, "w"); if (!f) return; for (int i = n - 24; i < n; i++) fputs(lines[i], f); fclose(f);
}
static void save_pets(void) {
    for (int i = 0; i < nparty; i++) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/pet_%s.txt", ST, party[i].id); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%s %d %d\n", party[i].room, party[i].x, party[i].y); fclose(f); } }
}
static void step_one(Pet *a) {
    Room *r = &rooms[a->ri];
    int toward = -1;
    if ((rand() % 4) == 0) { int best = 999, pick = -1; for (int i = 0; i < ndoor; i++) if (doors[i].room == a->ri && doors[i].autow) { int dx = doors[i].tx - a->x, dy = (doors[i].ty + 1) - a->y, m = abs(dx) + abs(dy); if (m < best) { best = m; pick = i; } } toward = pick; }
    int dx = 0, dy = 0;
    if (toward >= 0) { dx = doors[toward].tx > a->x ? 1 : doors[toward].tx < a->x ? -1 : 0; dy = (doors[toward].ty + 1) > a->y ? 1 : (doors[toward].ty + 1) < a->y ? -1 : 0; if (dx && dy) { if (rand() % 2) dx = 0; else dy = 0; } }
    else { dx = (rand() % 3) - 1; dy = (rand() % 3) - 1; }
    int nx = a->x + dx, ny = a->y + dy;
    int di = door_at(a->ri, nx, ny);
    if (di >= 0) { arrive(a, di); return; }
    if (blocked(a->ri, nx, ny)) return;
    if (!r->over && (ny < 2 || ny >= r->rows)) return;
    a->x = nx; a->y = ny;
}
static void step_pets(void) {
    int hold = flag_int();
    for (int i = 0; i < nparty; i++) { if (hold && i == active) continue; if (party[i].ri < 0) continue; step_one(&party[i]); }
    save_pets();
}
static int read_file(const char *p, char *b, int n) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return 0; } int k = (int)fread(b, 1, n - 1, f); b[k] = 0; fclose(f); return k; }
static int lc_run(const char *a, const char *b, const char *c, const char *d) {
    if (access(LC, X_OK) != 0) return -1;
    pid_t pid = fork(); if (pid < 0) return -1;
    if (pid == 0) { int fd = open("/dev/null", O_RDWR); if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); } setenv("LC_CLOCK_NO_POPUP", "1", 1); execl(LC, LC, CROOT, a, b, c, d, (char *)NULL); _exit(127); }
    int st = 0; for (int i = 0; i < 30; i++) { int w = waitpid(pid, &st, WNOHANG); if (w == pid) return 0; usleep(100000); }
    kill(pid, SIGKILL); waitpid(pid, &st, 0); return -1;
}
static void clock_paths(void) {
    snprintf(LC, sizeof LC, "%s/&.widgits/livedesk-clock/ops/+x/lc_clock.+x", HOUSE);
    snprintf(CROOT, sizeof CROOT, "%s/clock_root", ST);
    snprintf(CFILE, sizeof CFILE, "%s/#.desktop/clocks/bet.pdl", CROOT);
}
static void clock_ensure(void) {
    clock_paths(); char d[PATH_MAX]; snprintf(d, sizeof d, "%s/#.desktop/clocks", CROOT); mkdir(CROOT, 0755); mkdir(d, 0755);
    if (access(CFILE, F_OK) != 0) {
        lc_run("new", "bet", "user", "Casino Island");
        lc_run("rate", "bet", "min", NULL);
        lc_run("cmd", "bet", "settime", "21600000");
        lc_run("cmd", "bet", "pause", "--source");
    }
}
static void clock_ui(void) {
    char b[2048], out[160], p[PATH_MAX]; read_file(CFILE, b, sizeof b); long long ms = -1;
    for (char *l = strtok(b, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, "game_time_epoch_ms=", 19)) ms = atoll(l + 19);
    snprintf(p, sizeof p, "%s/clock_ui.txt", ST);
    if (ms < 0) { snprintf(out, sizeof out, "time_label=--:--\n"); FILE *f = fopen(p, "w"); if (f) { fputs(out, f); fclose(f); } return; }
    long long d = ms / 86400000, s = (ms - d * 86400000) / 1000; int hh = (int)(s / 3600); int mm = (int)(s % 3600 / 60); mm -= mm % 10;
    snprintf(out, sizeof out, "time_label=D%lld %02d:%02d\n", d + 1, hh, mm);
    FILE *f = fopen(p, "w"); if (f) { fputs(out, f); fclose(f); }
    const char *ph = (hh >= 19 || hh < 6) ? "night" : hh < 8 ? "dawn" : hh >= 17 ? "dusk" : "day";
    snprintf(p, sizeof p, "%s/daylight.txt", ST); f = fopen(p, "w"); if (f) { fprintf(f, "phase=%s\n", ph); fclose(f); }
}
static int flag_on(const char *name, int def) {
    char p[PATH_MAX], b[8]; snprintf(p, sizeof p, "%s/flag_%s.txt", ST, name); FILE *f = fopen(p, "r"); if (!f) return def; if (!fgets(b, sizeof b, f)) b[0] = 0; fclose(f); return b[0] != '0';
}
static void set_flag(const char *name, int on) { char p[PATH_MAX]; snprintf(p, sizeof p, "%s/flag_%s.txt", ST, name); FILE *f = fopen(p, "w"); if (f) { fprintf(f, "%d\n", on ? 1 : 0); fclose(f); } }
static const char *phase_now(void) {
    char p[PATH_MAX], b[40]; snprintf(p, sizeof p, "%s/daylight.txt", ST); FILE *f = fopen(p, "r"); if (!f) return "day"; if (!fgets(b, sizeof b, f)) b[0] = 0; fclose(f);
    if (strstr(b, "night")) return "night"; if (strstr(b, "dusk")) return "dusk"; if (strstr(b, "dawn")) return "dawn"; return "day";
}
static unsigned char *fr;
static void blit_px(unsigned char *px, int sw, int sh, int sx, int sy, int sws, int shs, int dx, int dy, int dw, int dh) {
    if (!px || dw < 1 || dh < 1) return;
    for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
        int u = sx + x * sws / dw, v = sy + y * shs / dh;
        if (u < 0 || v < 0 || u >= sw || v >= sh) continue;
        int X = dx + x, Y = dy + y; if (X < 0 || Y < 0 || X >= FW || Y >= FH) continue;
        unsigned char *a = px + ((size_t)v * sw + u) * 4; if (a[3] < 16) continue;
        memcpy(fr + ((size_t)Y * FW + X) * 4, a, 4);
    }
}
static int find_assets(char *out, int n) {
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s", APP);
    for (int i = 0; i < 10; i++) {
        char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.NNEST_ASSETS/rmmv-www-img", d);
        struct stat st; if (!stat(p, &st) && S_ISDIR(st.st_mode)) { snprintf(out, n, "%s", p); return 1; }
    }
    return 0;
}
static void paint(void) {
    if (!fr) fr = calloc((size_t)FW * FH, 4);
    if (!fr) return;
    memset(fr, 0, (size_t)FW * FH * 4);
    char assets[PATH_MAX], path[PATH_MAX];
    if (!find_assets(assets, sizeof assets) || nroom < 1) return;
    int v = party[active].ri; if (v < 0 || v >= nroom) v = 0;
    Room *rm = &rooms[v]; int tw = rm->tw > 0 ? rm->tw : 32;
    int ox = (FW - rm->cols * tw) / 2; if (ox < 0) ox = 0;
    int oy = (FH - rm->rows * tw) / 2; if (oy < 0) oy = 0;
    int w, h, n;
    snprintf(path, sizeof path, "%s/tilesets/World_A2.png", assets);
    unsigned char *grass = stbi_load(path, &w, &h, &n, 4);
    snprintf(path, sizeof path, "%s/tilesets/World_B.png", assets);
    int bw, bh, bn; unsigned char *worldb = stbi_load(path, &bw, &bh, &bn, 4);
    snprintf(path, sizeof path, "%s/tilesets/Inside_A5.png", assets);
    int iw, ih, in; unsigned char *inside = stbi_load(path, &iw, &ih, &in, 4);
    snprintf(path, sizeof path, "%s/characters/!Door1.png", assets);
    int dw, dh, dn; unsigned char *door = stbi_load(path, &dw, &dh, &dn, 4);
    snprintf(path, sizeof path, "%s/characters/People4.png", assets);
    int pw, ph, pn; unsigned char *peo = stbi_load(path, &pw, &ph, &pn, 4);
    for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) { unsigned char *q = fr + ((size_t)y * FW + x) * 4; q[0] = 18; q[1] = 48; q[2] = 92; q[3] = 255; }
    if (rm->over) {
        for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
            int sea = x < 2 || x > rm->cols - 3 || y < 1 || y > rm->rows - 2 || ((x <= 3 || x >= rm->cols - 4) && (y <= 2 || y >= rm->rows - 3));
            if (sea) continue;
            blit_px(grass, w, h, 48, 48 * 6, 48, 48, ox + x * tw, oy + y * tw, tw, tw);
        }
        for (int i = 0; i < nscen; i++) if (scen[i].room == v && !strcmp(scen[i].sheet, "World_B"))
            blit_px(worldb, bw, bh, scen[i].c * 48, scen[i].r * 48, scen[i].w * 48, scen[i].h * 48, ox + scen[i].x * tw, oy + scen[i].y * tw, scen[i].w * tw, scen[i].h * tw);
    } else {
        for (int y = 0; y < rm->rows; y++) for (int x = 0; x < rm->cols; x++) {
            int wall = y <= 1;
            int sx = wall ? 0 : rm->fc * 48, sy = wall ? 0 : rm->fr * 48;
            blit_px(inside, iw, ih, sx, sy, 48, 48, ox + x * tw, oy + y * tw, tw, tw);
        }
        for (int i = 0; i < ndoor; i++) if (doors[i].room == v)
            blit_px(door, dw, dh, 48, 0, 48, 48, ox + doors[i].tx * tw, oy + doors[i].ty * tw, tw, tw);
    }
    for (int i = 0; i < nparty; i++) if (party[i].ri == v) {
        int idx = party[i].idx;
        int sx = (idx % 4) * 3 * 48 + 48, sy = (idx / 4) * 4 * 48 + 3 * 48;
        blit_px(peo, pw, ph, sx, sy, 48, 48, ox + party[i].x * tw, oy + party[i].y * tw - tw / 6, tw, tw);
    }
    if (grass) stbi_image_free(grass);
    if (worldb) stbi_image_free(worldb);
    if (inside) stbi_image_free(inside);
    if (door) stbi_image_free(door);
    if (peo) stbi_image_free(peo);
    { const char *ph = phase_now(); int nr = 256, ng = 256, nb = 256, indoors = !rm->over;
      if (!strcmp(ph, "night")) { nr = 90; ng = 100; nb = 150; }
      else if (!strcmp(ph, "dusk")) { nr = 220; ng = 140; nb = 110; }
      else if (!strcmp(ph, "dawn")) { nr = 180; ng = 170; nb = 210; }
      if (indoors) { nr = 256 - (256 - nr) / 3; ng = 256 - (256 - ng) / 3; nb = 256 - (256 - nb) / 3; }
      if (nr != 256 || ng != 256 || nb != 256) for (int i = 0; i < FW * FH; i++) { unsigned char *q = fr + i * 4; q[0] = (unsigned char)(q[0] * nr / 256); q[1] = (unsigned char)(q[1] * ng / 256); q[2] = (unsigned char)(q[2] * nb / 256); }
    }
    char raw[PATH_MAX], rec[PATH_MAX]; snprintf(raw, sizeof raw, "%s/scene.raw", ST); snprintf(rec, sizeof rec, "%s/scene.receipt.txt", ST);
    FILE *f = fopen(raw, "wb"); if (f) { fwrite(fr, 1, (size_t)FW * FH * 4, f); fclose(f); }
    f = fopen(rec, "w"); if (f) { fprintf(f, "frame_w=%d\nframe_h=%d\n", FW, FH); fclose(f); }
}
static void write_ui(const char *note) {
    char p[PATH_MAX]; snprintf(p, sizeof p, "%s/ui.txt", ST); FILE *f = fopen(p, "w"); if (!f) return;
    int on = flag_int();
    char raw[PATH_MAX]; snprintf(raw, sizeof raw, "%s/scene.raw", ST);
    fprintf(f, "title=Casino Island\ncanvas_raw=%s\nnote=%s\ninteract_label=%s\ninteract_class=%s\n", raw, note, on ? "on" : "off", on ? "interact-active" : "");
    fprintf(f, "who_line=%s %s\nhelp_line=INT holds one pet. No bets yet.\n", nparty ? party[active].name : "-", on ? "held" : "walking");
    fprintf(f, "active_name=%s\nphase_line=%s\n", nparty ? party[active].name : "-", phase_now());
    fprintf(f, "left_visible=%s\nchat_visible=%s\nhb_visible=%s\n", flag_on("left", 1) ? "1" : "", flag_on("chat", 1) ? "1" : "", flag_on("hb", 1) ? "1" : "");
    fprintf(f, "n_party=%d\n", nparty);
    int v = nparty ? party[active].ri : 0; if (v < 0 || v >= nroom) v = 0;
    fprintf(f, "room_line=%s\n", nroom ? rooms[v].name : "-");
    for (int i = 0; i < nparty; i++) {
        fprintf(f, "pet_%d_label=%s\npet_%d_id=%s\nwalk_%d_where=%s in %s\n", i, party[i].name, i, party[i].id, i, party[i].name, party[i].room);
        fprintf(f, "party_%d_label=%s\nparty_%d_id=%s\nparty_%d_cls=%s\n", i, party[i].name, i, party[i].id, i, i == active ? "active" : "");
    }
    { char cp[PATH_MAX], cb[2400]; snprintf(cp, sizeof cp, "%s/chat.txt", ST); read_file(cp, cb, sizeof cb); char *ln[8] = {0}; int k = 0; for (char *l = strtok(cb, "\n"); l; l = strtok(NULL, "\n")) { ln[k % 8] = l; k++; } for (int i = 0; i < 8; i++) { int j = k - 8 + i; fprintf(f, "chat_%d=%s\n", i, j >= 0 ? ln[j % 8] : ""); } }
    char tp[PATH_MAX], line[200]; snprintf(tp, sizeof tp, "%s/town.pdl", APP); FILE *tf = fopen(tp, "r"); int nt = 0;
    if (tf) { while (fgets(line, sizeof line, tf)) if (!strncmp(line, "ROOM", 4)) nt++; fclose(tf); }
    fprintf(f, "n_town=%d\n", nt); tf = fopen(tp, "r"); int k = 0;
    if (tf) { while (fgets(line, sizeof line, tf) && k < 8) { char id[24], kind[24], lab[48]; int mb = 0; if (sscanf(line, "ROOM | %23s | %23s | %d | %47[^\n]", id, kind, &mb, lab) == 4) fprintf(f, "town_%d_label=%s %s min %d\n", k++, lab, kind, mb); } fclose(tf); }
    fclose(f);
}
static void verb(int argc, char **argv) {
    load_party(); clock_ensure(); clock_ui();
    const char *v = argc > 0 ? argv[0] : "status"; char note[120] = "the island is open";
    if (!strcmp(v, "int")) { int on = !flag_int(); set_int(on); snprintf(note, sizeof note, on ? "INT on: %s" : "INT off: the walk has them", nparty ? party[active].name : "-"); }
    else if (!strcmp(v, "select") && argc > 1) { for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, argv[1])) { active = i; save_active(); snprintf(note, sizeof note, "selected %s", party[i].name); } }
    else if (!strcmp(v, "toggle") && argc > 1) { int on = !flag_on(argv[1], 1); set_flag(argv[1], on); snprintf(note, sizeof note, "%s %s", argv[1], on ? "on" : "off"); }
    else if (!strcmp(v, "say") && argc > 1) { char b[200]; snprintf(b, sizeof b, "%s: %s", nparty ? party[active].name : "?", argv[1]); chat_line(b); snprintf(note, sizeof note, "said"); }
    else if (!strcmp(v, "help")) snprintf(note, sizeof note, "see help.txt");
    else if (!strcmp(v, "step")) { step_pets(); snprintf(note, sizeof note, "one step"); }
    else if (!strcmp(v, "door") && argc > 1) {
        for (int i = 0; i < nparty; i++) if (!strcmp(party[i].id, argv[1])) {
            for (int d = 0; d < ndoor; d++) if (doors[d].room == party[i].ri && doors[d].autow) { party[i].x = doors[d].tx; party[i].y = doors[d].ty + 1; arrive(&party[i], d); snprintf(note, sizeof note, "%s -> %s %d,%d", argv[1], party[i].room, party[i].x, party[i].y); break; }
        }
    }
    save_pets(); paint(); write_ui(note); printf("%s\n", note);
}
static void resident(void) {
    load_party(); clock_ensure(); mkdir(ST, 0755); int n = 0;
    for (;;) {
        if ((n++ % 5) == 0) { lc_run("step", "5", NULL, NULL); clock_ui(); }
        step_pets(); paint(); write_ui("pets walking the island");
        sleep(1);
    }
}
int main(int argc, char **argv) {
    char exe[PATH_MAX]; if (!realpath(argv[0], exe)) snprintf(exe, sizeof exe, "%s", argv[0]); snprintf(SELF, sizeof SELF, "%s", exe);
    if (argc == 3 && argv[1][0] == '/') { snprintf(HOUSE, sizeof HOUSE, "%s", argv[1]); snprintf(APP, sizeof APP, "%s", argv[2]); snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755); resident(); return 0; }
    for (int up = 0; up < 3; up++) { char *s = strrchr(exe, '/'); if (s) *s = 0; }
    snprintf(APP, sizeof APP, "%s", exe); snprintf(HOUSE, sizeof HOUSE, "%s", exe); for (int up = 0; up < 2; up++) { char *s = strrchr(HOUSE, '/'); if (s) *s = 0; }
    snprintf(ST, sizeof ST, "%s/state", APP); mkdir(ST, 0755); srand((unsigned)time(NULL));
    char *v[8]; int n = 0; for (int i = 1; i < argc && n < 8; i++) v[n++] = argv[i]; if (!n) v[n++] = "status";
    verb(n, v); return 0;
}
