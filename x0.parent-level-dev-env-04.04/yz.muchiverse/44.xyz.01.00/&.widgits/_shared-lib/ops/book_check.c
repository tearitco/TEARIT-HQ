/* book_check - is this game a BOOK:PAGE citizen? A READ-ONLY conformance check of a pc-hq game folder against the house standard 18.pc-hq/PAGE-FILE.md ("book and session are the same thing, page and map are the same
 * thing; a page is one desk file whose rows are entities; pc-hq 2D and 3D read and write the same rows") plus the consistency of the game's own map book (maps/<game>/game.pdl vs its room folders).
 * Design: BOOK 16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md (sections 1b, 3, 4 step S1). It never writes anything.
 *
 * Usage:  book_check <pchq_root> [--house <house_root>] [--page <page.pdl>]
 *   <pchq_root>  the game's pc-hq folder, e.g. @.apps/rpg-space/rpg-space-pchq (it holds pieces/display/open_book_page.txt and pieces/system/maps/<game>/)
 *   --page       check THIS page file as if the game were bound to it (source=desk, pdl=<file>): what a game would score once moved onto the page file; A2 then passes with a note
 *   --house      the house root (the folder with &.widgits) that relative paths in page rows and in pdl= are resolved against; default = the nearest folder above <pchq_root> that has &.widgits
 * Output, one line per check:   PASS|FAIL|SKIP|INFO <code> <detail>      then   checks=<n> pass=<p> fail=<f> skip=<s> info=<i>   and   conformant=yes|no
 *   LAYER A (PAGE-FILE.md; A3..A10 are SKIPped when A2 fails, because there is no page file to read):
 *     A1 open_page_file   pieces/display/open_book_page.txt exists
 *     A2 source_desk      source=desk (or no source= but a pdl= pin, "older Synch pin"); source=board means the board owns its own page and keeps PRIVATE state files
 *     A3 page_file        the pdl= path exists (absolute, or relative to the house)
 *     A4 page_rows        every `DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n` row has 9 fields, a name, a path, and whole-number x_px y_px cell_x cell_y n
 *     A5 unique_names     no entity name twice
 *     A6 core_rows        hero_01, xelector_01 and camera_01 are each present once
 *     A7 camera_row       camera_01's glyph is the packed camera m=<int>,y=<int>,p=<int>,z=<int>,h=<int>
 *     A8 possession       xelector_01's glyph is `none` (or empty) or the name of an entity row
 *     A9 pal_paths        every row's path is an existing folder (relative to the house)
 *     A10 session         the page sits in <session>/desks/, <session>/session.pdl has STATE | name and STATE | active_desk, and that active page file exists
 *   LAYER B (the game's own book of maps, maps/<game>/):
 *     B1 game_pdl         exactly one maps/<game>/ with a game.pdl (GAME | n_desks, GAME | desk_<i>_id rows)
 *     B2 desk_ids         n_desks equals the number of desk ids; ids are valid names (letters, digits, - _; 1..48; the book_page_op rule) and unique
 *     B3 desk_folders     every desk id has a folder with a map.txt
 *     B4 no_orphans       no map folder that game.pdl does not list
 *   LAYER C (INFO only: a file's presence cannot prove it is still used): the private position files PAGE-FILE.md says must stop being authorities, listed when present.
 * Exit: 0 conformant (no FAIL) | 1 at least one FAIL | 2 usage or unreadable <pchq_root>.   Build: gcc -std=gnu11 -Wall -Wextra -O2 -o +x/book_check.+x book_check.c */
#define _GNU_SOURCE
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

static int nchk, npass, nfail, nskip, ninfo;
static void out(const char *kind, const char *code, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void out(const char *kind, const char *code, const char *fmt, ...) {
    char d[600]; va_list ap; va_start(ap, fmt); vsnprintf(d, sizeof d, fmt, ap); va_end(ap);
    printf("%s %s %s\n", kind, code, d);
    if (!strcmp(kind, "PASS")) { nchk++; npass++; } else if (!strcmp(kind, "FAIL")) { nchk++; nfail++; } else if (!strcmp(kind, "SKIP")) { nchk++; nskip++; } else ninfo++;
}
static char *trim(char *s) { while (*s == ' ' || *s == '\t') s++; char *e = s + strlen(s); while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0; return s; }
static int is_dir(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISDIR(s.st_mode); }
static int is_file(const char *p) { struct stat s; return stat(p, &s) == 0 && S_ISREG(s.st_mode); }
static int whole(const char *s) { if (!*s) return 0; for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0; return strlen(s) <= 9; }
static int name_ok(const char *s) { size_t n = strlen(s); if (!n || n > 48) return 0; for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-') return 0; return 1; }
static void resolve(const char *house, const char *p, char *o, size_t cap) { if (p[0] == '/') snprintf(o, cap, "%s", p); else snprintf(o, cap, "%s/%s", house, p); }
static int read_all(const char *p, char *b, size_t cap) { FILE *f = fopen(p, "r"); if (!f) { b[0] = 0; return 0; } size_t n = fread(b, 1, cap - 1, f); b[n] = 0; fclose(f); return 1; }

#define MAXE 512
typedef struct { char name[64], path[300], glyph[96]; } Ent;
static Ent E[MAXE]; static int nE;

int main(int argc, char **argv) {
    if (argc < 2) goto usage;
    char pchq[PATH_MAX], house[PATH_MAX] = "", forced[PATH_MAX] = ""; snprintf(pchq, sizeof pchq, "%s", argv[1]);
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--house") && i + 1 < argc) snprintf(house, sizeof house, "%s", argv[++i]); else if (!strcmp(argv[i], "--page") && i + 1 < argc) snprintf(forced, sizeof forced, "%s", argv[++i]); else goto usage; }
    { size_t n = strlen(pchq); while (n > 1 && pchq[n - 1] == '/') pchq[--n] = 0; }
    if (!is_dir(pchq)) { fprintf(stderr, "book_check: %s is not a folder\n", pchq); return 2; }
    if (!house[0]) { char d[PATH_MAX]; if (!realpath(pchq, d)) snprintf(d, sizeof d, "%s", pchq); for (int up = 0; up < 10 && !house[0]; up++) { char *s = strrchr(d, '/'); if (!s || s == d) break; *s = 0; char t[PATH_MAX + 16]; snprintf(t, sizeof t, "%s/&.widgits", d); if (is_dir(t)) snprintf(house, sizeof house, "%s", d); } }
    if (!house[0]) { fprintf(stderr, "book_check: no house root (a folder with &.widgits) above %s; pass --house\n", pchq); return 2; }
    char p[PATH_MAX + 64], buf[8192];
    /* ---- layer A ---- */
    char pdl[PATH_MAX] = "", src[16] = ""; int have_pin = 0;
    snprintf(p, sizeof p, "%s/pieces/display/open_book_page.txt", pchq);
    if (!is_file(p)) { out("FAIL", "A1", "open_page_file: %s does not exist (the game has no open book/page record)", "pieces/display/open_book_page.txt"); }
    else { out("PASS", "A1", "open_page_file: pieces/display/open_book_page.txt exists");
        read_all(p, buf, sizeof buf); for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) { if (!strncmp(l, "source=", 7)) snprintf(src, sizeof src, "%s", trim(l + 7)); else if (!strncmp(l, "pdl=", 4)) { snprintf(pdl, sizeof pdl, "%s", trim(l + 4)); have_pin = pdl[0] != 0; } } }
    if (forced[0]) { snprintf(src, sizeof src, "desk"); snprintf(pdl, sizeof pdl, "%s", forced); have_pin = 1; }
    int desk = !strcmp(src, "desk") || (!src[0] && have_pin);
    if (forced[0]) out("PASS", "A2", "source_desk: --page given: checking %s as if the game were bound to it", forced);
    else if (desk) out("PASS", "A2", "source_desk: the page is a desk file (source=%s)", src[0] ? src : "(older pin)");
    else out("FAIL", "A2", "source_desk: source=%s: the board owns its own page and keeps private state files (hero_01/xelector_01 state, camera keys in the board state, the skin's bar.txt) instead of rows in a page file", src[0] ? src : "(none)");
    char pagepath[PATH_MAX] = ""; int page_ok = 0;
    if (!desk) { out("SKIP", "A3", "page_file: needs A2"); }
    else if (!pdl[0]) out("FAIL", "A3", "page_file: source=desk but there is no pdl= line");
    else { resolve(house, pdl, pagepath, sizeof pagepath); if (is_file(pagepath)) { page_ok = 1; out("PASS", "A3", "page_file: %s exists", pdl); } else out("FAIL", "A3", "page_file: %s does not exist", pdl); }
    if (!page_ok) { const char *c[] = { "A4 page_rows", "A5 unique_names", "A6 core_rows", "A7 camera_row", "A8 possession", "A9 pal_paths", "A10 session" }; for (int i = 0; i < 7; i++) { char code[8], nm[32]; sscanf(c[i], "%7s %31s", code, nm); out("SKIP", code, "%s: needs a readable page file (A2, A3)", nm); } }
    else {
        static char pb[262144]; read_all(pagepath, pb, sizeof pb); int bad = 0, unknown = 0; char badname[64] = ""; nE = 0;
        for (char *l = strtok(pb, "\n"); l; l = strtok(NULL, "\n")) {
            char *t = trim(l); if (!*t || *t == '#') continue; if (strncmp(t, "DESK", 4) || (t[4] != ' ' && t[4] != '|')) { unknown++; continue; }
            char *f[12]; int nf = 0; char *q = t; while (nf < 12) { f[nf++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; } for (int i = 0; i < nf; i++) f[i] = trim(f[i]);
            int ok = nf == 9 && f[1][0] && f[2][0] && whole(f[3]) && whole(f[4]) && whole(f[5]) && whole(f[6]) && whole(f[8]);
            if (!ok) { if (!bad++) snprintf(badname, sizeof badname, "%s", nf > 1 ? f[1] : "?"); continue; }
            if (nE < MAXE) { snprintf(E[nE].name, sizeof E[nE].name, "%s", f[1]); snprintf(E[nE].path, sizeof E[nE].path, "%s", f[2]); snprintf(E[nE].glyph, sizeof E[nE].glyph, "%s", f[7]); nE++; } }
        if (bad) out("FAIL", "A4", "page_rows: %d malformed DESK row(s), the first is `%s` (need 9 fields, a name, a path, whole numbers in x_px y_px cell_x cell_y n)", bad, badname); else out("PASS", "A4", "page_rows: %d entity rows, all well formed%s", nE, unknown ? " (non-DESK rows ignored)" : "");
        int dup = 0; char dupn[64] = ""; for (int i = 0; i < nE && !dup; i++) for (int j = i + 1; j < nE; j++) if (!strcmp(E[i].name, E[j].name)) { dup = 1; snprintf(dupn, sizeof dupn, "%s", E[i].name); break; }
        if (dup) out("FAIL", "A5", "unique_names: `%s` appears more than once", dupn); else out("PASS", "A5", "unique_names: no entity name twice");
        const char *core[3] = { "hero_01", "xelector_01", "camera_01" }; int ci[3] = { -1, -1, -1 }, cn[3] = { 0, 0, 0 };
        for (int i = 0; i < nE; i++) for (int k = 0; k < 3; k++) if (!strcmp(E[i].name, core[k])) { if (ci[k] < 0) ci[k] = i; cn[k]++; }
        { char miss[120] = ""; for (int k = 0; k < 3; k++) if (cn[k] != 1) { size_t o = strlen(miss); snprintf(miss + o, sizeof miss - o, "%s%s(%d)", o ? ", " : "", core[k], cn[k]); }
          if (miss[0]) out("FAIL", "A6", "core_rows: need each of hero_01, xelector_01, camera_01 exactly once; found: %s", miss); else out("PASS", "A6", "core_rows: hero_01, xelector_01 and camera_01 are each a row"); }
        if (ci[2] < 0) out("SKIP", "A7", "camera_row: no camera_01 row");
        else { int m, y, pp, z, h; char tail; int ok = sscanf(E[ci[2]].glyph, "m=%d,y=%d,p=%d,z=%d,h=%d%c", &m, &y, &pp, &z, &h, &tail) == 5; if (ok) out("PASS", "A7", "camera_row: camera_01 carries mode=%d yaw=%d pitch=%d pan_z=%d height=%d", m, y, pp, z, h); else out("FAIL", "A7", "camera_row: camera_01's glyph `%s` is not m=<int>,y=<int>,p=<int>,z=<int>,h=<int>", E[ci[2]].glyph); }
        if (ci[1] < 0) out("SKIP", "A8", "possession: no xelector_01 row");
        else { const char *g = E[ci[1]].glyph; int ok = !g[0] || !strcmp(g, "none"); for (int i = 0; i < nE && !ok; i++) if (!strcmp(E[i].name, g)) ok = 1; if (ok) out("PASS", "A8", "possession: xelector_01 possesses `%s`", g[0] ? g : "none"); else out("FAIL", "A8", "possession: xelector_01 possesses `%s`, which is not an entity row", g); }
        int missing = 0; char mn[64] = ""; for (int i = 0; i < nE; i++) { char rp[PATH_MAX]; resolve(house, E[i].path, rp, sizeof rp); if (!is_dir(rp)) { if (!missing++) snprintf(mn, sizeof mn, "%s", E[i].name); } }
        if (missing) out("FAIL", "A9", "pal_paths: %d row path(s) are not folders, the first is `%s`", missing, mn); else out("PASS", "A9", "pal_paths: every row's path is a folder");
        { char dir[PATH_MAX], sess[PATH_MAX]; snprintf(dir, sizeof dir, "%s", pagepath); char *sl = strrchr(dir, '/'); if (sl) *sl = 0; size_t dl = strlen(dir);
          if (dl < 6 || strcmp(dir + dl - 6, "/desks")) out("SKIP", "A10", "session: the page file is not inside a <session>/desks/ folder");
          else { dir[dl - 6] = 0; snprintf(sess, sizeof sess, "%s/session.pdl", dir); if (!is_file(sess)) out("FAIL", "A10", "session: %s/session.pdl does not exist", "<session>");
            else { read_all(sess, buf, sizeof buf); char nm[96] = "", act[96] = ""; for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) { char *f[4]; int nf = 0; char *q = l; while (nf < 4) { f[nf++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; } if (nf >= 3) { for (int i = 0; i < nf; i++) f[i] = trim(f[i]); if (!strcmp(f[0], "STATE") && !strcmp(f[1], "name")) snprintf(nm, sizeof nm, "%s", f[2]); if (!strcmp(f[0], "STATE") && !strcmp(f[1], "active_desk")) snprintf(act, sizeof act, "%s", f[2]); } }
              char ap[PATH_MAX + 80]; snprintf(ap, sizeof ap, "%s/desks/%s.pdl", dir, act);
              if (!nm[0] || !act[0]) out("FAIL", "A10", "session: session.pdl lacks %s", !nm[0] ? "STATE | name" : "STATE | active_desk");
              else if (!is_file(ap)) out("FAIL", "A10", "session: active_desk `%s` has no page file", act);
              else out("PASS", "A10", "session: book `%s`, open page `%s` exists", nm, act); } } }
    }
    /* ---- layer B ---- */
    char mroot[PATH_MAX + 32]; snprintf(mroot, sizeof mroot, "%s/pieces/system/maps", pchq); char game[64] = ""; int ng = 0;
    { DIR *d = opendir(mroot); struct dirent *e; while (d && (e = readdir(d))) { if (e->d_name[0] == '.') continue; char t[PATH_MAX + 80]; snprintf(t, sizeof t, "%s/%s", mroot, e->d_name); if (is_dir(t) && ng++ == 0) snprintf(game, sizeof game, "%s", e->d_name); } if (d) closedir(d); }
    char gp[PATH_MAX + 160]; snprintf(gp, sizeof gp, "%s/%s/game.pdl", mroot, game);
    if (ng != 1 || !is_file(gp)) { out("FAIL", "B1", "game_pdl: need exactly one maps/<game>/ with a game.pdl (found %d game folder(s)%s)", ng, ng == 1 ? ", no game.pdl" : ""); out("SKIP", "B2", "desk_ids: needs B1"); out("SKIP", "B3", "desk_folders: needs B1"); out("SKIP", "B4", "no_orphans: needs B1"); }
    else {
        out("PASS", "B1", "game_pdl: maps/%s/game.pdl", game);
        read_all(gp, buf, sizeof buf); int nd = -1, nid = 0, bad = 0, dupid = 0; static char ids[128][64];
        for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) { char *f[4]; int nf = 0; char *q = l; while (nf < 4) { f[nf++] = q; char *b = strchr(q, '|'); if (!b) break; *b = 0; q = b + 1; } if (nf < 3) continue; for (int i = 0; i < nf; i++) f[i] = trim(f[i]); if (strcmp(f[0], "GAME")) continue;
            if (!strcmp(f[1], "n_desks")) nd = whole(f[2]) ? atoi(f[2]) : -2;
            else if (!strncmp(f[1], "desk_", 5) && strlen(f[1]) > 8 && !strcmp(f[1] + strlen(f[1]) - 3, "_id") && nid < 128) { if (!name_ok(f[2])) bad++; for (int i = 0; i < nid; i++) if (!strcmp(ids[i], f[2])) dupid++; snprintf(ids[nid++], 64, "%s", f[2]); } }
        if (nd < 0) out("FAIL", "B2", "desk_ids: game.pdl has no valid GAME | n_desks row");
        else if (nd != nid) out("FAIL", "B2", "desk_ids: n_desks says %d but game.pdl lists %d desk ids", nd, nid);
        else if (bad) out("FAIL", "B2", "desk_ids: %d desk id(s) are not valid names (letters, digits, - _, 1..48)", bad);
        else if (dupid) out("FAIL", "B2", "desk_ids: %d desk id(s) listed twice", dupid);
        else out("PASS", "B2", "desk_ids: n_desks=%d, all ids valid and unique", nd);
        int nomap = 0; char nmn[64] = ""; for (int i = 0; i < nid; i++) { char t[PATH_MAX + 200]; snprintf(t, sizeof t, "%s/%s/%s/map.txt", mroot, game, ids[i]); if (!is_file(t)) { if (!nomap++) snprintf(nmn, sizeof nmn, "%s", ids[i]); } }
        if (nomap) out("FAIL", "B3", "desk_folders: %d desk(s) have no folder with a map.txt, the first is `%s`", nomap, nmn); else out("PASS", "B3", "desk_folders: every desk has a folder with a map.txt");
        int orph = 0; char on[64] = ""; { char t[PATH_MAX + 100]; snprintf(t, sizeof t, "%s/%s", mroot, game); DIR *d = opendir(t); struct dirent *e; while (d && (e = readdir(d))) { if (e->d_name[0] == '.') continue; char fp[PATH_MAX + 200]; snprintf(fp, sizeof fp, "%s/%s", t, e->d_name); if (!is_dir(fp)) continue; int listed = 0; for (int i = 0; i < nid; i++) if (!strcmp(ids[i], e->d_name)) listed = 1; if (!listed) { if (!orph++) snprintf(on, sizeof on, "%s", e->d_name); } } if (d) closedir(d); }
        if (orph) out("FAIL", "B4", "no_orphans: %d map folder(s) are not in game.pdl, the first is `%s`", orph, on); else out("PASS", "B4", "no_orphans: every map folder is a listed desk");
    }
    /* ---- layer C ---- */
    { static const char *priv[] = { "pieces/hero_01/state.txt", "pieces/xelector_01/state.txt", "pieces/world_01/phymoji_entities.txt", "pieces/world_01/animals.txt", "pieces/display/synched_entities.txt" };
      for (size_t i = 0; i < sizeof priv / sizeof priv[0]; i++) { snprintf(p, sizeof p, "%s/%s", pchq, priv[i]); if (is_file(p)) out("INFO", "C1", "private position file present: %s (PAGE-FILE.md: must stop being an authority)", priv[i]); } }
    printf("checks=%d pass=%d fail=%d skip=%d info=%d\nconformant=%s\n", nchk, npass, nfail, nskip, ninfo, nfail ? "no" : "yes");
    return nfail ? 1 : 0;
usage:
    fprintf(stderr, "Usage: book_check <pchq_root> [--house <house_root>] [--page <page.pdl>]\n"); return 2;
}
