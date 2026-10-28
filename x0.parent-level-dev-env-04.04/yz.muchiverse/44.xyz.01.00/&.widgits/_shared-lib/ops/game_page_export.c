/* game_page_export - write a BOOK:PAGE page file for a petslike game from the game's OWN live state files (S2 of the games-as-books pivot, BOOK 16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md section 13;
 * the format is 18.pc-hq/PAGE-FILE.md). It only READS the game and WRITES the session folder you name; it never touches the game.
 * Usage:  game_page_export <app_dir> [<session_dir>|-] [--house <house_root>] [--if-changed]
 *   <session_dir>  omitted or `-` = <app_dir>/state/page (the game's own runtime folder; nothing else of the game is touched)
 *   --if-changed   write a file only when its content differs from what is on disk (a reader that polls the page by file size / marker sees no churn); prints `unchanged <page>` when nothing was written
 *   <app_dir>      the game, e.g. @.apps/rpg-space (state/trainer.txt, <app>/<name>-pchq/)
 *   <session_dir>  where to write: <session_dir>/session.pdl (STATE | name | <game>, STATE | active_desk | <room>) and <session_dir>/desks/<room>.pdl
 * The page is the room the controlled character is in (state/trainer.txt room=). Rows: hero_01 at the trainer's cell; xelector_01 at the SAME cell with its possessed id in the glyph field (the standard: a possessing xelector
 * stands on what it controls; the private xelector file may say otherwise, and that is reported); camera_01 with the fresh-start camera packed m=<mode>,y=<yaw>,p=<pitch>,z=0,h=0 (mode from arrow_config.txt default_camera_mode, yaw 180,
 * pitch 6 for modes 1/2 and -90 otherwise: the engine's own fresh defaults). Pixels are cells x 80. The `n` column is written as 0 (its meaning is not documented in PAGE-FILE.md).
 * NOT rows yet: the ship (it lives on the `space` page, not the room), pets and asteroids (bar rows with no pal folder).
 * Output (stdout): one line per fact: ROW ..., DISAGREE <what> <a> vs <b>, then `wrote <page> rows=3 disagreements=<n>`.  Exit 0 written, 1 a needed file is missing (named), 2 usage.
 * Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/game_page_export.+x game_page_export.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int kv(const char *file, const char *key, char *out, size_t cap) {
    FILE *f = fopen(file, "r"); char l[512]; size_t kl = strlen(key); out[0] = 0; if (!f) return -1;
    while (fgets(l, sizeof l, f)) if (!strncmp(l, key, kl) && l[kl] == '=') { snprintf(out, cap, "%s", l + kl + 1); out[strcspn(out, "\r\n")] = 0; fclose(f); return 1; }
    fclose(f); return 0;
}
static int whole(const char *s) { if (!*s) return 0; for (; *s; s++) if (*s < '0' || *s > '9') return 0; return 1; }
static int name_ok(const char *s) { size_t n = strlen(s); if (!n || n > 48) return 0; for (size_t i = 0; i < n; i++) if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_' || s[i] == '-')) return 0; return 1; }
static int write_file(const char *path, const char *content, int if_changed) {      /* 1 written, 0 unchanged, -1 error */
    if (if_changed) { FILE *o = fopen(path, "r"); if (o) { char b[4096]; size_t n = fread(b, 1, sizeof b - 1, o); b[n] = 0; fclose(o); if (!strcmp(b, content)) return 0; } }
    FILE *f = fopen(path, "w"); if (!f) return -1; fputs(content, f); fclose(f); return 1;
}
static int mkpath(const char *p) { char t[PATH_MAX + 8]; snprintf(t, sizeof t, "%s", p); for (char *q = t + 1; *q; q++) if (*q == '/') { *q = 0; if (mkdir(t, 0755) && errno != EEXIST) return -1; *q = '/'; } return mkdir(t, 0755) && errno != EEXIST ? -1 : 0; }
int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    char app[PATH_MAX], sess[PATH_MAX] = "", house[PATH_MAX] = ""; int if_changed = 0, have_sess = 0; snprintf(app, sizeof app, "%s", argv[1]);
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--house") && i + 1 < argc) snprintf(house, sizeof house, "%s", argv[++i]); else if (!strcmp(argv[i], "--if-changed")) if_changed = 1; else if (!have_sess && argv[i][0] != '-') { snprintf(sess, sizeof sess, "%s", argv[i]); have_sess = 1; } else if (!have_sess && !strcmp(argv[i], "-")) have_sess = 1; else goto usage; }
    char *ap = realpath(app, NULL); if (!ap) { fprintf(stderr, "game_page_export: no such game folder %s\n", app); return 1; }
    snprintf(app, sizeof app, "%s", ap); free(ap);
    if (!house[0]) { char d[PATH_MAX]; snprintf(d, sizeof d, "%s", app); for (int up = 0; up < 10 && !house[0]; up++) { char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0; char t[PATH_MAX + 16]; snprintf(t, sizeof t, "%s/&.widgits", d); if (is_dir(t)) snprintf(house, sizeof house, "%s", d); } }
    if (!house[0]) { fprintf(stderr, "game_page_export: no house root above %s; pass --house\n", app); return 2; }
    if (!sess[0]) snprintf(sess, sizeof sess, "%s/state/page", app);
    const char *game = strrchr(app, '/') + 1;
    char pchq[PATH_MAX + 80] = ""; { DIR *d = opendir(app); struct dirent *e; while (d && (e = readdir(d))) { size_t n = strlen(e->d_name); if (n > 5 && !strcmp(e->d_name + n - 5, "-pchq")) { snprintf(pchq, sizeof pchq, "%s/%s", app, e->d_name); break; } } if (d) closedir(d); }
    if (!pchq[0]) { fprintf(stderr, "game_page_export: %s has no <name>-pchq folder\n", app); return 1; }
    char tp[PATH_MAX + 80], room[64], hx[16], hy[16]; snprintf(tp, sizeof tp, "%s/state/trainer.txt", app);
    if (kv(tp, "room", room, sizeof room) < 1 || kv(tp, "x", hx, sizeof hx) < 1 || kv(tp, "y", hy, sizeof hy) < 1) { fprintf(stderr, "game_page_export: %s lacks room=, x= or y=\n", tp); return 1; }
    if (!name_ok(room) || !whole(hx) || !whole(hy)) { fprintf(stderr, "game_page_export: %s has an unusable room/x/y (%s %s %s)\n", tp, room, hx, hy); return 1; }
    int cx = atoi(hx), cy = atoi(hy), disagree = 0;
    char xp[PATH_MAX + 120], px[16] = "", py[16] = "", poss[64] = "", hp_[PATH_MAX + 120], hpx[16] = "", hpy[16] = "";
    snprintf(xp, sizeof xp, "%s/pieces/xelector_01/state.txt", pchq); kv(xp, "pos_x", px, sizeof px); kv(xp, "pos_y", py, sizeof py); kv(xp, "possessed_id", poss, sizeof poss);
    snprintf(hp_, sizeof hp_, "%s/pieces/hero_01/state.txt", pchq); kv(hp_, "pos_x", hpx, sizeof hpx); kv(hp_, "pos_y", hpy, sizeof hpy);
    printf("trainer.txt (the game's own, ticking): room=%s cell=%d,%d\n", room, cx, cy);
    if (px[0] && (atoi(px) != cx || atoi(py) != cy)) { printf("DISAGREE xelector_01/state.txt %s,%s vs trainer %d,%d\n", px, py, cx, cy); disagree++; }
    if (hpx[0] && (atoi(hpx) != cx || atoi(hpy) != cy)) { printf("DISAGREE hero_01/state.txt %s,%s vs trainer %d,%d\n", hpx, hpy, cx, cy); disagree++; }
    char cfg[PATH_MAX + 120], dm[16]; snprintf(cfg, sizeof cfg, "%s/pieces/system/arrow_config.txt", pchq); kv(cfg, "default_camera_mode", dm, sizeof dm);
    int mode = atoi(dm); if (mode < 1 || mode > 4) mode = 2; int pitch = (mode == 1 || mode == 2) ? 6 : -90;
    char rel[PATH_MAX]; { size_t hl = strlen(house); snprintf(rel, sizeof rel, "%s", strncmp(pchq, house, hl) == 0 && pchq[hl] == '/' ? pchq + hl + 1 : pchq); }
    char desks[PATH_MAX + 20]; snprintf(desks, sizeof desks, "%s/desks", sess);
    if (mkpath(sess)) { fprintf(stderr, "game_page_export: cannot create %s\n", sess); return 1; }
    if (mkpath(desks)) { fprintf(stderr, "game_page_export: cannot create %s\n", desks); return 1; }
    char sp[PATH_MAX + 40], pp[PATH_MAX + 120]; snprintf(sp, sizeof sp, "%s/session.pdl", sess); snprintf(pp, sizeof pp, "%s/%s.pdl", desks, room);
    char sc[256]; snprintf(sc, sizeof sc, "STATE | name | %s\nSTATE | active_desk | %s\n", game, room);
    char r1[PATH_MAX + 200], r2[PATH_MAX + 200], r3[PATH_MAX + 200], pc[3 * (PATH_MAX + 200) + PATH_MAX + 300];
    snprintf(r1, sizeof r1, "DESK | hero_01 | %s/pieces/hero_01 | %d | %d | %d | %d | . | 0", rel, cx * 80, cy * 80, cx, cy);
    snprintf(r2, sizeof r2, "DESK | xelector_01 | %s/pieces/xelector_01 | %d | %d | %d | %d | %s | 0", rel, cx * 80, cy * 80, cx, cy, poss[0] ? poss : "none");
    snprintf(r3, sizeof r3, "DESK | camera_01 | %s/pieces/display | 80 | 0 | 1 | 0 | m=%d,y=180,p=%d,z=0,h=0 | 0", rel, mode, pitch);
    snprintf(pc, sizeof pc, "# page `%s` of book `%s`, written by game_page_export from %s/state/trainer.txt (S2/S3a of the games-as-books pivot)\n%s\n%s\n%s\n", room, game, game, r1, r2, r3);
    int w1 = write_file(sp, sc, if_changed), w2 = write_file(pp, pc, if_changed);
    if (w1 < 0 || w2 < 0) { fprintf(stderr, "game_page_export: cannot write under %s\n", sess); return 1; }
    printf("ROW %s\nROW %s\nROW %s\n%s %s rows=3 disagreements=%d\n", r1, r2, r3, (w1 || w2) ? "wrote" : "unchanged", pp, disagree);
    return 0;
usage:
    fprintf(stderr, "Usage: game_page_export <app_dir> [<session_dir>|-] [--house <house_root>] [--if-changed]\n"); return 2;
}
