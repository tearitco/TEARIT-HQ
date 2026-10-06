/* khtpm_page_rows.c - read/write ONE named entity row of the livedesk page
 * file ("desk" file; pages and desks are the same thing now).
 *
 * Text-included canonical helper, pure file I/O (house convention for shared
 * multi-consumer code, same family as khtpm_grid_jump.c / khtpm_move_range.c).
 *
 * The page file is the shared source of truth for where entities are, for the
 * desk AND pc-hq (18.pc-hq/PAGE-FILE.md): a move in either window writes the
 * same row, so the other window shows it on its next draw. A row is
 *
 *   DESK | name | path | x_px | y_px | cell_x | cell_y | glyph | n
 *
 * Which file is "the page": pc-hq's open_book_page.txt first (source=desk +
 * pdl=<file> pins it; source=board means the board owns its map, no page),
 * else the livedesk's active session's active desk. This is the SAME
 * resolution bv_render_2d.c / bv_render_3d.c use (their private copies read it
 * the same way) - kept identical on purpose so a writer never targets a
 * different file than the drawers read.
 *
 * Functions are static + unused-tolerant. Prefix: pgr_. */
#ifndef KHTPM_PAGE_ROWS_C
#define KHTPM_PAGE_ROWS_C

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define PGR_UNUSED __attribute__((unused))
#else
#define PGR_UNUSED
#endif

#define PGR_PATH 4400

/* "KEY" row value from a `SECTION | KEY | VALUE` pdl (STATE rows). */
PGR_UNUSED static int pgr_pdl_value(const char *path, const char *key, char *out, size_t n) {
    char line[1024];
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *a = strchr(line, '|'), *b, *v;
        if (!a) continue;
        a++; while (*a == ' ') a++;
        if (!(b = strchr(a, '|'))) continue;
        if (strncmp(a, key, strlen(key)) != 0) continue;
        v = b + 1; while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        { size_t l = strlen(v); while (l && v[l - 1] == ' ') v[--l] = '\0'; }
        snprintf(out, n, "%s", v);
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

/* Resolve the page file. Returns 1 and fills out, or 0 when there is no page
 * (source=board, or no livedesk session) - the caller then uses its private
 * state file. */
PGR_UNUSED static int pgr_page_path(const char *house, char *out, size_t n) {
    char ob[PGR_PATH], line[PGR_PATH], source[32] = "", stored[PGR_PATH] = "";
    FILE *f;
    snprintf(ob, sizeof(ob), "%s/@.apps/piececraft-hq/pieces/display/open_book_page.txt", house);
    if ((f = fopen(ob, "r"))) {
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "source=", 7)) { snprintf(source, sizeof(source), "%s", line + 7); source[strcspn(source, "\r\n")] = 0; }
            else if (!strncmp(line, "pdl=", 4)) { snprintf(stored, sizeof(stored), "%s", line + 4); stored[strcspn(stored, "\r\n")] = 0; }
        }
        fclose(f);
        if (!strcmp(source, "board")) return 0;
        if (stored[0] && (f = fopen(stored, "r"))) { fclose(f); snprintf(out, n, "%s", stored); return 1; }
    }
    {
        char users[PGR_PATH];
        DIR *d;
        struct dirent *e;
        snprintf(users, sizeof(users), "%s/xyzfs/users", house);
        if (!(d = opendir(users))) return 0;
        while ((e = readdir(d))) {
            char sess[PGR_PATH], rootpdl[PGR_PATH], active[128], desk[128], sp[PGR_PATH];
            if (e->d_name[0] == '.') continue;
            snprintf(sess, sizeof(sess), "%s/%s/home/livedesk/sessions", users, e->d_name);
            snprintf(rootpdl, sizeof(rootpdl), "%s/session.pdl", sess);
            if (!pgr_pdl_value(rootpdl, "active_session", active, sizeof(active))) continue;
            snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sess, active);
            if (!pgr_pdl_value(sp, "active_desk", desk, sizeof(desk))) continue;
            snprintf(out, n, "%s/%s/desks/%s.pdl", sess, active, desk);
            closedir(d);
            return 1;
        }
        closedir(d);
    }
    return 0;
}

/* Split a DESK row into trimmed fields in-place (fld[0] = "DESK"). Returns the
 * field count. */
PGR_UNUSED static int pgr_split(char *line, char **fld, int max) {
    int nf = 0;
    char *p = line;
    while (nf < max) {
        char *bar = strchr(p, '|');
        char *e;
        while (*p == ' ') p++;
        fld[nf++] = p;
        if (!bar) { p += strcspn(p, "\r\n"); *p = '\0'; break; }
        *bar = '\0';
        e = bar;
        while (e > fld[nf - 1] && e[-1] == ' ') *--e = '\0';
        p = bar + 1;
    }
    return nf;
}

/* Cell of the named row. Returns 1 if found. path_out (optional) gets the row's
 * path field (the entity's directory, relative to the house). */
PGR_UNUSED static int pgr_get(const char *house, const char *name, int *cx, int *cy,
                              char *path_out, size_t pn) {
    char pdl[PGR_PATH], line[2048], *fld[10];
    FILE *f;
    int found = 0;
    if (!pgr_page_path(house, pdl, sizeof(pdl)) || !(f = fopen(pdl, "r"))) return 0;
    while (fgets(line, sizeof(line), f)) {
        int nf;
        if (strncmp(line, "DESK", 4) != 0) continue;
        nf = pgr_split(line, fld, 10);
        if (nf < 7 || strcmp(fld[1], name) != 0) continue;
        *cx = atoi(fld[5]); *cy = atoi(fld[6]);
        if (path_out && pn) snprintf(path_out, pn, "%s", fld[2]);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

/* Move the named row to (cx,cy): rewrites cell_x/cell_y and x_px/y_px (cells *
 * 80, the desk's convention), keeps every other field and every other line
 * byte-for-byte. Writes a temp file then renames, so a reader never sees a
 * half-written page. Returns 1 if the row existed and was written. */
PGR_UNUSED static int pgr_set_cell(const char *house, const char *name, int cx, int cy) {
    char pdl[PGR_PATH], tmp[PGR_PATH + 8], line[2048], copy[2048], *fld[10];
    FILE *in, *out;
    int hit = 0;
    if (!pgr_page_path(house, pdl, sizeof(pdl)) || !(in = fopen(pdl, "r"))) return 0;
    snprintf(tmp, sizeof(tmp), "%s.tmp", pdl);
    if (!(out = fopen(tmp, "w"))) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        int nf;
        if (!hit && !strncmp(line, "DESK", 4)) {
            snprintf(copy, sizeof(copy), "%s", line);
            nf = pgr_split(copy, fld, 10);
            if (nf >= 9 && !strcmp(fld[1], name)) {
                fprintf(out, "DESK | %s | %s | %d | %d | %d | %d | %s | %s\n",
                        fld[1], fld[2], cx * 80, cy * 80, cx, cy, fld[7], fld[8]);
                hit = 1;
                continue;
            }
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (!hit) { remove(tmp); return 0; }
    return rename(tmp, pdl) == 0;
}

/* Remove the named DESK row (the entity left the page: it was taken into an
 * inventory). The removed line, without its newline, is copied to row_out so
 * the caller can keep it and restore glyph/index on Place. Same tmp+rename
 * write as pgr_set_cell. Returns 1 if a row was removed. */
PGR_UNUSED static int pgr_remove_row(const char *house, const char *name, char *row_out, size_t rn) {
    char pdl[PGR_PATH], tmp[PGR_PATH + 8], line[2048], copy[2048], *fld[10];
    FILE *in, *out;
    int hit = 0;
    if (row_out && rn) row_out[0] = '\0';
    if (!pgr_page_path(house, pdl, sizeof(pdl)) || !(in = fopen(pdl, "r"))) return 0;
    snprintf(tmp, sizeof(tmp), "%s.tmp", pdl);
    if (!(out = fopen(tmp, "w"))) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        if (!hit && !strncmp(line, "DESK", 4)) {
            snprintf(copy, sizeof(copy), "%s", line);
            if (pgr_split(copy, fld, 10) >= 7 && !strcmp(fld[1], name)) {
                if (row_out && rn) { snprintf(row_out, rn, "%s", line); row_out[strcspn(row_out, "\r\n")] = '\0'; }
                hit = 1;
                continue;
            }
        }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (!hit) { remove(tmp); return 0; }
    return rename(tmp, pdl) == 0;
}

/* Append a DESK row for `name` at cell (cx,cy) (x_px/y_px = cells * 80). `path`
 * is the entity directory relative to the house, as every row stores it. The
 * index field n is the page's highest n plus one, so it never collides.
 * Returns 1 if appended, 2 if a row with that name is already there (nothing
 * written), 0 on error. */
PGR_UNUSED static int pgr_append_row(const char *house, const char *name, const char *path,
                                     int cx, int cy, const char *glyph) {
    char pdl[PGR_PATH], tmp[PGR_PATH + 8], line[2048], copy[2048], *fld[10];
    FILE *in, *out;
    int maxn = 0, exists = 0, last_nl = 1;
    if (!pgr_page_path(house, pdl, sizeof(pdl)) || !(in = fopen(pdl, "r"))) return 0;
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        snprintf(copy, sizeof(copy), "%s", line);
        if (pgr_split(copy, fld, 10) >= 9) {
            int n = atoi(fld[8]);
            if (n > maxn) maxn = n;
            if (!strcmp(fld[1], name)) exists = 1;
        }
    }
    if (exists) { fclose(in); return 2; }
    rewind(in);
    snprintf(tmp, sizeof(tmp), "%s.tmp", pdl);
    if (!(out = fopen(tmp, "w"))) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        fputs(line, out);
        last_nl = strchr(line, '\n') != NULL;
    }
    if (!last_nl) fputc('\n', out);
    fprintf(out, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
            name, path, cx * 80, cy * 80, cx, cy, (glyph && glyph[0]) ? glyph : "?", maxn + 1);
    fclose(in);
    fclose(out);
    return rename(tmp, pdl) == 0 ? 1 : 0;
}

#endif
