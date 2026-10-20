#define _POSIX_C_SOURCE 200809L
/* One office room for game-dev-hq, painted from the house RPG Maker sheets.
 * Writes the pc-hq book maps/game-dev-hq/office (map.png, events.txt, frames, cells).
 * Called from game_dev_hq.c. See 13.agent-coms/2026-10-20/from-grok-game-dev-office.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../&.widgits/_shared-lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../&.widgits/_shared-lib/stb_image_write.h"

#define S48 48
#define COLS 13
#define ROWS 8
#define TW 48

static int join2(char *dst, size_t n, const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la + 1 + lb + 1 > n) { if (n) dst[0] = 0; return 0; }
    memcpy(dst, a, la); dst[la] = '/'; memcpy(dst + la + 1, b, lb + 1); return 1;
}

static void mkdir_p(char *path) {
    for (char *p = path + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(path, 0755); *p = '/'; }
    mkdir(path, 0755);
}

static int find_assets(const char *app, char *out, size_t n) {
    char d[PATH_MAX]; snprintf(d, sizeof d, "%s", app);
    for (int i = 0; i < 8; i++) {
        char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
        char p[PATH_MAX];
        if (!join2(p, sizeof p, d, "#.NNEST_ASSETS")) continue;
        char q[PATH_MAX];
        if (!join2(q, sizeof q, p, "rmmv-www-img")) continue;
        struct stat st; if (!stat(q, &st) && S_ISDIR(st.st_mode)) { snprintf(out, n, "%s", q); return 1; }
    }
    /* assets live one repo above the house (NNEST-12.00/#.NNEST_ASSETS) */
    snprintf(d, sizeof d, "%s", app);
    for (int i = 0; i < 10; i++) {
        char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0;
        char p[PATH_MAX]; snprintf(p, sizeof p, "%s/#.NNEST_ASSETS/rmmv-www-img", d);
        struct stat st; if (!stat(p, &st) && S_ISDIR(st.st_mode)) { snprintf(out, n, "%s", p); return 1; }
    }
    return 0;
}

typedef struct { unsigned char *px; int w, h; } Img;

static Img load_img(const char *assets, const char *dir, const char *name) {
    Img im = {0};
    char a[PATH_MAX], b[PATH_MAX], c[PATH_MAX];
    if (!join2(a, sizeof a, assets, dir)) return im;
    snprintf(b, sizeof b, "%s.png", name);
    if (!join2(c, sizeof c, a, b)) return im;
    int n; im.px = stbi_load(c, &im.w, &im.h, &n, 4); return im;
}

static void blit48(unsigned char *fr, int fw, Img *s, int sx, int sy, int dx, int dy) {
    if (!s->px) return;
    for (int y = 0; y < TW; y++) for (int x = 0; x < TW; x++) {
        int X = dx + x, Y = dy + y; if (X < 0 || Y < 0 || X >= fw || Y >= ROWS * TW) continue;
        int u = sx + x, v = sy + y; if (u < 0 || v < 0 || u >= s->w || v >= s->h) continue;
        unsigned char *sp = s->px + ((size_t)v * s->w + u) * 4;
        if (sp[3] < 16) continue;
        memcpy(fr + ((size_t)Y * fw + X) * 4, sp, 4);
    }
}

static void sample24(Img *s, int sx, int sy, unsigned char *out) {
    memset(out, 0, 24 * 24 * 4);
    if (!s->px) return;
    for (int y = 0; y < 24; y++) for (int x = 0; x < 24; x++) {
        int u = sx + x * 2, v = sy + y * 2; if (u >= s->w || v >= s->h) continue;
        memcpy(out + (y * 24 + x) * 4, s->px + ((size_t)v * s->w + u) * 4, 4);
    }
}

static void write_frame(const char *assets, Img *s, const char *sheet, int idx) {
    if (!s->px) return;
    char root[PATH_MAX], dir[PATH_MAX], name[64], path[PATH_MAX];
    snprintf(root, sizeof root, "%s", assets);
    char *sl = strrchr(root, '/'); if (!sl) return; *sl = 0;
    if (!join2(dir, sizeof dir, root, "tsots-characters")) return;
    char frames[PATH_MAX]; if (!join2(frames, sizeof frames, dir, "frames")) return;
    mkdir_p(frames);
    snprintf(name, sizeof name, "%s_%d_8_1.rgba", sheet, idx);
    if (!join2(path, sizeof path, frames, name)) return;
    int sx = (idx % 4) * 3 * S48 + S48;
    int sy = (idx / 4) * 4 * S48 + 3 * S48;
    unsigned char out[16 * 24 * 4];
    for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) {
        int u = sx + 8 + x * 2, v = sy + y * 2;
        unsigned char *d = out + (y * 16 + x) * 4;
        if (!s->px || u >= s->w || v >= s->h) { d[0] = d[1] = d[2] = 0; d[3] = 0; continue; }
        memcpy(d, s->px + ((size_t)v * s->w + u) * 4, 4);
    }
    FILE *f = fopen(path, "wb"); if (f) { fwrite(out, 1, sizeof out, f); fclose(f); }
}

static int sheet_for(const char *party, const char *name, char *sheet, int *idx) {
    FILE *f = fopen(party, "r"); if (!f) return 0;
    char line[200]; int ok = 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "PET", 3)) continue;
        char n[32], sh[32]; int ix = 0;
        if (sscanf(line, "PET | %31s | %31s | %d", n, sh, &ix) == 3 && !strcmp(n, name)) {
            snprintf(sheet, 24, "%s", sh); *idx = ix; ok = 1; break;
        }
    }
    fclose(f); return ok;
}

void gd_export_office(const char *app) {
    char assets[PATH_MAX]; if (!find_assets(app, assets, sizeof assets)) return;
    Img floor = load_img(assets, "tilesets", "Inside_A5");
    Img furn = load_img(assets, "tilesets", "Inside_B");
    int fw = COLS * TW, fh = ROWS * TW;
    unsigned char *fr = calloc((size_t)fw * fh, 4); if (!fr) { stbi_image_free(floor.px); stbi_image_free(furn.px); return; }
    for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLS; x++) {
        int tc = (y < 2) ? 1 : 1, tr = (y < 2) ? 3 : 4;
        blit48(fr, fw, &floor, tc * S48, tr * S48, x * TW, y * TW);
    }
    static const int desks[] = {2, 5, 8, 11};
    for (int i = 0; i < 4; i++) blit48(fr, fw, &furn, 1 * S48, 12 * S48, desks[i] * TW, 4 * TW);
    { Img door = load_img(assets, "characters", "!Door1"); blit48(fr, fw, &door, 0, 0, 6 * TW, 6 * TW); stbi_image_free(door.px); }

    char st[PATH_MAX], party[PATH_MAX], staffp[PATH_MAX];
    join2(st, sizeof st, app, "state");
    join2(party, sizeof party, app, "party.pdl");
    join2(staffp, sizeof staffp, st, "staff.pdl");

    char ev[1024]; int eo = 0;
    FILE *sf = fopen(staffp, "r");
    int si = 0;
    if (sf) {
        char line[300];
        while (fgets(line, sizeof line, sf) && si < 4) {
            if (strncmp(line, "STAFF | ", 8) || strstr(line, "| id |")) continue;
            char id[16], name[24], job[24];
            if (sscanf(line, "STAFF | %15s | %23s | %23s", id, name, job) != 3) continue;
            char sheet[24] = "People1"; int idx = 0;
            if (!sheet_for(party, name, sheet, &idx)) { snprintf(sheet, sizeof sheet, "People3"); idx = 0; }
            Img ch = load_img(assets, "characters", sheet);
            int sx = (idx % 4) * 3 * S48 + S48;
            int sy = (idx / 4) * 4 * S48 + 3 * S48;
            int x = desks[si], y = 5, skip = 0;
            { char posp[PATH_MAX]; FILE *pf;
              if (join2(posp, sizeof posp, st, "pos.txt") && (pf = fopen(posp, "r"))) {
                char pid[16]; int px, py;
                while (fscanf(pf, "%15s %d %d", pid, &px, &py) == 3) if (!strcmp(pid, id)) { x = px; y = py; if (py < 0) skip = 1; }
                fclose(pf);
              } }
            if (!skip) blit48(fr, fw, &ch, sx, sy, x * TW, y * TW - 8);
            write_frame(assets, &ch, sheet, idx);
            unsigned char rgb[4] = {180, 180, 180, 255};
            if (ch.px) for (int v = sy; v < sy + S48 && v < ch.h; v++) for (int u = sx; u < sx + S48 && u < ch.w; u++) {
                unsigned char *sp = ch.px + ((size_t)v * ch.w + u) * 4; if (sp[3] > 200) { memcpy(rgb, sp, 4); v = sy + S48; break; } }
            char letter = name[0] ? name[0] : 'a'; if (letter >= 'A' && letter <= 'Z') letter = (char)(letter + 32);
            if (!skip) eo += snprintf(ev + eo, sizeof ev - eo, "%d %d %d %d %d %s %d 8 1 %c\n", x, y, rgb[0], rgb[1], rgb[2], sheet, idx, letter);
            stbi_image_free(ch.px); si++;
        }
        fclose(sf);
    }

    char pkg[PATH_MAX], maps[PATH_MAX], room[PATH_MAX];
    join2(pkg, sizeof pkg, app, "game-dev-hq-pchq");
    join2(maps, sizeof maps, pkg, "pieces");
    char sys[PATH_MAX], book[PATH_MAX];
    join2(sys, sizeof sys, maps, "system");
    join2(book, sizeof book, sys, "maps");
    char root[PATH_MAX]; join2(root, sizeof root, book, "game-dev-hq");
    join2(room, sizeof room, root, "office");
    mkdir_p(room);

    char rawp[PATH_MAX], recp[PATH_MAX];
    join2(rawp, sizeof rawp, st, "scene.raw");
    join2(recp, sizeof recp, st, "scene.receipt.txt");
    { FILE *rf = fopen(rawp, "wb"); if (rf) { fwrite(fr, 1, (size_t)fw * fh * 4, rf); fclose(rf); } }
    { FILE *rcf = fopen(recp, "w"); if (rcf) { fprintf(rcf, "frame_w=%d\nframe_h=%d\n", fw, fh); fclose(rcf); } }
    char png[PATH_MAX]; join2(png, sizeof png, room, "map.png");
    char tmp[PATH_MAX]; snprintf(tmp, sizeof tmp, "%s.tmp", png);
    if (stbi_write_png(tmp, fw, fh, 4, fr, fw * 4)) rename(tmp, png); else remove(tmp);

    char ep[PATH_MAX]; join2(ep, sizeof ep, room, "events.txt");
    FILE *ef = fopen(ep, "w"); if (ef) { fwrite(ev, 1, (size_t)eo, ef); fclose(ef); }

    unsigned char floor24[24 * 24 * 4], wall24[24 * 24 * 4], desk24[24 * 24 * 4];
    sample24(&floor, 1 * S48, 4 * S48, floor24);
    sample24(&floor, 1 * S48, 3 * S48, wall24);
    sample24(&furn, 1 * S48, 12 * S48, desk24);
    int aw = 16 * 24, ah = 24;
    unsigned char *atlas = calloc((size_t)aw * ah, 4);
    if (atlas) {
        for (int y = 0; y < 24; y++) {
            memcpy(atlas + (y * aw + 0) * 4, floor24 + y * 24 * 4, 24 * 4);
            memcpy(atlas + (y * aw + 24) * 4, wall24 + y * 24 * 4, 24 * 4);
            memcpy(atlas + (y * aw + 48) * 4, desk24 + y * 24 * 4, 24 * 4);
        }
        char cr[PATH_MAX]; join2(cr, sizeof cr, room, "cells.rgba");
        FILE *cf = fopen(cr, "wb"); if (cf) { fwrite(atlas, 1, (size_t)aw * ah * 4, cf); fclose(cf); }
        char cp[PATH_MAX]; join2(cp, sizeof cp, room, "cells.png");
        snprintf(tmp, sizeof tmp, "%s.tmp", cp);
        if (stbi_write_png(tmp, aw, ah, 4, atlas, aw * 4)) rename(tmp, cp); else remove(tmp);
        free(atlas);
    }
    char ct[PATH_MAX]; join2(ct, sizeof ct, room, "cells.txt");
    FILE *tf = fopen(ct, "w");
    if (tf) {
        fprintf(tf, "width=%d\nheight=%d\ntile_px=24\natlas_cols=16\natlas_tiles=3\ncells\n", COLS, ROWS);
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                int fls = 0, wl = (y < 2) ? 1 : 0;
                for (int i = 0; i < 4; i++) if (x == desks[i] && y == 4) wl = 2;
                fprintf(tf, "%s%d,%d", x ? " " : "", fls, wl);
            }
            fputc('\n', tf);
        }
        fclose(tf);
    }
    char mp[PATH_MAX]; join2(mp, sizeof mp, room, "map.txt");
    FILE *mf = fopen(mp, "w");
    if (mf) {
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                char g = '.'; if (y < 2) g = 'W';
                for (int i = 0; i < 4; i++) if (x == desks[i] && y == 4) g = 'f';
                fputc(g, mf);
            }
            fputc('\n', mf);
        }
        fclose(mf);
    }
    char ex[PATH_MAX]; join2(ex, sizeof ex, room, "extrusion.pdl");
    FILE *xf = fopen(ex, "w");
    if (xf) {
        fputs("SECTION      | KEY                | VALUE\n"
              "----------------------------------------------------------------------\n"
              "META         | map_id             | game-dev-hq\n"
              "META         | desk_id            | office\n"
              "EXTRUDE      | W                  | 3\n"
              "EXTRUDE      | f                  | 1\n"
              "EXTRUDE      | default            | 0\n", xf);
        fclose(xf);
    }
    char gp[PATH_MAX]; join2(gp, sizeof gp, root, "game.pdl");
    FILE *gf = fopen(gp, "w");
    if (gf) {
        fputs("SECTION      | KEY                | VALUE\n"
              "----------------------------------------------------------------------\n"
              "GAME         | type               | board-game\n"
              "GAME         | label              | game-dev-hq\n"
              "GAME         | n_chunks           | 1\n"
              "GAME         | chunk_0_x          | 0\n"
              "GAME         | chunk_0_y          | 0\n"
              "GAME         | n_desks            | 1\n"
              "GAME         | desk_1_id          | office\n"
              "GAME         | desk_1_label       | Office\n", gf);
        fclose(gf);
    }
    char bar[PATH_MAX]; join2(bar, sizeof bar, room, "bar.txt");
    FILE *bf = fopen(bar, "w");
    if (bf) {
        /* names again, in the same order as the events rows */
        FILE *sf2 = fopen(staffp, "r"); int idn = 1, k = 0; char line[300];
        if (sf2) while (fgets(line, sizeof line, sf2) && k < 4) {
            if (strncmp(line, "STAFF | ", 8) || strstr(line, "| id |")) continue;
            char id[16], name[24], job[24];
            if (sscanf(line, "STAFF | %15s | %23s | %23s", id, name, job) != 3) continue;
            fprintf(bf, "%d\t%s\t%d\t5\ton-click\n", idn++, name, desks[k]); k++;
        }
        if (sf2) fclose(sf2);
        fclose(bf);
    }
    free(fr); stbi_image_free(floor.px); stbi_image_free(furn.px);
    char note[PATH_MAX]; join2(note, sizeof note, st, "office_book.txt");
    FILE *nf = fopen(note, "w"); if (nf) { fprintf(nf, "%s\n", room); fclose(nf); }
}
