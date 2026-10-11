/* desk_copy_op - copy a desk ("page") and its entities to a new desk with renamed, independent entities, so work can continue on the copy.
 *
 * Why: a desk file (`sessions/<id>/desks/<name>.pdl`) is only a list of references to pals under `livedesk/pals/`; a second desk that lists the same
 * pals shares their state. To get a real working copy (e.g. dsr -> dsr-test) the pals themselves must be copied under new names.
 *
 * Usage: desk_copy_op <livedesk_dir> <src_desk> <dst_desk> <from_prefix> <to_prefix> [--apply] [--report FILE] [--desks-dir DIR]
 *   <livedesk_dir>  the folder that holds pals/ and sessions/ (xyzfs/users/<uuid>/home/livedesk)
 *   <src_desk>/<dst_desk>  desk file names inside the desks dir (default <livedesk_dir>/sessions/s1/desks), e.g. dsr.pdl dsr-test.pdl
 *   from_prefix/to_prefix  a DESK row whose pal name starts with from_prefix is COPIED to a pal named to_prefix + the rest (dsr_store_a1 -> dsrtest_store_a1);
 *                          rows that do not match (cursword, door_civ ...) are NOT copied and stay shared by reference
 *   default is a DRY RUN: nothing is written, the report says what would happen. --apply writes.
 *   The source desk and source pals are only ever read. Nothing is overwritten: an existing destination desk or pal is a refusal (exit 2, nothing written).
 * Per copied pal: the folder is copied without runtime files (module_parent.pid, interact_relay.txt, last_signal.txt, kh_focus_debug.log, cli_io_state.txt,
 *   .hq_manager/) and WITHOUT identity (entity_uid.txt, inventory/zz.phone/): identity is minted by the spawn hook / phone_ensure_op, never duplicated. The
 *   old name is replaced by the new one in pal.pdl, meta.pdl and menu.chtpm, the folder pieces/registry/phymoji_assets/<old> is renamed, instance_id.txt gets a
 *   fresh unique DT<n> code. The `PAL | hash` line is DROPPED from the copy's pal.pdl: khtpm_phone.c freezes that hash as the entity_uid, so a copied hash would give the copy the original's identity and phone number; without it the copy gets a fresh identity. Each pal is built under a temp name and renamed into place.
 * Source desk rows: the LAST row per name wins (a desk file is an append-only history of saves). Exit: 0 ok, 1 errors, 2 usage/refused.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PB 4096
#define MAXROWS 512

static int g_apply = 0;
static FILE *g_rep = NULL;

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void say(const char *fmt, ...) {
    char buf[PB * 2]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    fputs(buf, stdout); if (g_rep) fputs(buf, g_rep);
}

static int exists(const char *p) { struct stat st; return lstat(p, &st) == 0; }
static int isdir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

static int skip_name(const char *rel, const char *name) {
    static const char *skip[] = { "module_parent.pid", "interact_relay.txt", "last_signal.txt", "kh_focus_debug.log", "cli_io_state.txt", ".hq_manager", "entity_uid.txt", NULL };
    for (int i = 0; skip[i]; i++) if (!strcmp(name, skip[i])) return 1;
    if (!strcmp(rel, "inventory") && !strcmp(name, "zz.phone")) return 1;
    return 0;
}

static int copy_file(const char *src, const char *dst, mode_t mode) {
    FILE *in = fopen(src, "rb"); if (!in) return -1;
    FILE *out = fopen(dst, "wb"); if (!out) { fclose(in); return -1; }
    char buf[65536]; size_t n; int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    fclose(in); if (fclose(out) != 0) rc = -1;
    chmod(dst, mode & 07777);
    return rc;
}

/* recursive copy; rel is the path inside the pal (for the skip rules) */
static int copy_tree(const char *src, const char *dst, const char *rel, long *files) {
    DIR *d = opendir(src); if (!d) return -1;
    if (mkdir(dst, 0775) != 0 && errno != EEXIST) { closedir(d); return -1; }
    struct dirent *e; int rc = 0;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (skip_name(rel, e->d_name)) continue;
        char s[PB], t[PB], r[PB]; struct stat st;
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name); snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        if (lstat(s, &st) != 0) { rc = -1; continue; }
        if (S_ISLNK(st.st_mode)) { char l[PB]; ssize_t k = readlink(s, l, sizeof(l) - 1); if (k > 0) { l[k] = 0; if (symlink(l, t) != 0) rc = -1; } else rc = -1; }
        else if (S_ISDIR(st.st_mode)) { if (copy_tree(s, t, r, files) != 0) rc = -1; }
        else if (S_ISREG(st.st_mode)) { if (copy_file(s, t, st.st_mode) != 0) rc = -1; else (*files)++; }
    }
    closedir(d); return rc;
}

/* drop every line that starts with `prefix` (atomic rewrite); missing file is fine */
static int drop_lines(const char *path, const char *prefix) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char tmp[PB]; snprintf(tmp, sizeof(tmp), "%s.tmp_dl", path);
    FILE *w = fopen(tmp, "w"); if (!w) { fclose(f); return -1; }
    char line[PB * 2]; size_t pl = strlen(prefix); int rc = 0;
    while (fgets(line, sizeof(line), f)) if (strncmp(line, prefix, pl)) { if (fputs(line, w) == EOF) rc = -1; }
    fclose(f); if (fclose(w) != 0) rc = -1;
    if (rc == 0) rc = rename(tmp, path); else remove(tmp);
    return rc;
}

/* replace every occurrence of `from` by `to` in one file (whole-file read, atomic rewrite); missing file is fine */
static int replace_in_file(const char *path, const char *from, const char *to) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return -1; }
    fclose(f); buf[n] = 0;
    size_t fl = strlen(from), tl = strlen(to), cap = (size_t)n * (tl > fl ? tl / fl + 1 : 1) + 16;
    char *out = malloc(cap); if (!out) { free(buf); return -1; }
    size_t o = 0; const char *p = buf, *q;
    while ((q = strstr(p, from))) { memcpy(out + o, p, (size_t)(q - p)); o += (size_t)(q - p); memcpy(out + o, to, tl); o += tl; p = q + fl; }
    size_t rest = strlen(p); memcpy(out + o, p, rest); o += rest;
    char tmp[PB]; snprintf(tmp, sizeof(tmp), "%s.tmp_dc", path);
    FILE *w = fopen(tmp, "wb"); int rc = -1;
    if (w) { rc = (fwrite(out, 1, o, w) == o) ? 0 : -1; if (fclose(w) != 0) rc = -1; if (rc == 0) rc = rename(tmp, path); else remove(tmp); }
    free(buf); free(out); return rc;
}

typedef struct { char name[256]; char path[PB]; char rest[PB]; int copy; char newname[256]; } Row;
static Row rows[MAXROWS]; static int nrows = 0;

static void trim(char *s) { size_t n = strlen(s); while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ')) s[--n] = 0; }

/* DESK | name | path | rest...  (rest = x|y|z|... kept verbatim) */
static int read_desk(const char *file, const char *from, const char *to) {
    FILE *f = fopen(file, "r"); if (!f) return -1;
    char line[PB * 2];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (strncmp(line, "DESK | ", 7)) continue;
        char *a = line + 7, *b = strstr(a, " | "); if (!b) continue; *b = 0; b += 3;
        char *c = strstr(b, " | "); if (!c) continue; *c = 0; c += 3;
        int idx = -1; for (int i = 0; i < nrows; i++) if (!strcmp(rows[i].name, a)) { idx = i; break; }
        if (idx < 0) { if (nrows >= MAXROWS) { fclose(f); return -1; } idx = nrows++; }
        snprintf(rows[idx].name, sizeof(rows[idx].name), "%s", a);
        snprintf(rows[idx].path, sizeof(rows[idx].path), "%s", b);
        snprintf(rows[idx].rest, sizeof(rows[idx].rest), "%s", c);
        size_t fl = strlen(from);
        rows[idx].copy = (fl > 0 && !strncmp(a, from, fl));
        if (rows[idx].copy) snprintf(rows[idx].newname, sizeof(rows[idx].newname), "%s%s", to, a + fl);
        else rows[idx].newname[0] = 0;
        if (rows[idx].copy) {
            for (const char *q2 = rows[idx].name; *q2; q2++) {
                int ok = (*q2 >= 'a' && *q2 <= 'z') || (*q2 >= 'A' && *q2 <= 'Z') || (*q2 >= '0' && *q2 <= '9') || *q2 == '_' || *q2 == '-' || *q2 == '.';
                if (!ok) { fprintf(stderr, "unsafe pal name: %s\n", rows[idx].name); fclose(f); return -1; }
            }
        }
    }
    fclose(f); return 0;
}

static void new_instance_id(const char *pals, char *out, size_t sz) {
    for (int n = 1; n < 100000; n++) {
        char id[32]; snprintf(id, sizeof(id), "DT%d", n); int used = 0;
        DIR *d = opendir(pals); struct dirent *e;
        while (d && (e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[PB], l[64] = ""; snprintf(p, sizeof(p), "%s/%s/instance_id.txt", pals, e->d_name);
            FILE *f = fopen(p, "r"); if (f) { if (fgets(l, sizeof(l), f)) trim(l); fclose(f); }
            if (!strcmp(l, id)) { used = 1; break; }
        }
        if (d) closedir(d);
        if (!used) { snprintf(out, sz, "%s", id); return; }
    }
    snprintf(out, sz, "DTX");
}

static int copy_pal(const char *pals, const Row *r, const char *newpath_root) {
    char src[PB], dst[PB], tmp[PB], f[PB], g[PB];
    snprintf(src, sizeof(src), "%s/%s", pals, r->name); snprintf(dst, sizeof(dst), "%s/%s", pals, r->newname);
    snprintf(tmp, sizeof(tmp), "%s/.%s.tmp_dc", pals, r->newname);
    (void)newpath_root;
    char cmd[PB * 2]; snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmp); if (system(cmd) != 0) return -1;
    long files = 0;
    if (copy_tree(src, tmp, "", &files) != 0) { say("ERROR copying %s\n", r->name); return -1; }
    const char *txt[] = { "pal.pdl", "meta.pdl", "menu.chtpm" };
    for (int i = 0; i < 3; i++) { snprintf(f, sizeof(f), "%s/%s", tmp, txt[i]); if (replace_in_file(f, r->name, r->newname) != 0) return -1; }
    snprintf(f, sizeof(f), "%s/pal.pdl", tmp); if (drop_lines(f, "PAL | hash |") != 0) return -1;
    snprintf(f, sizeof(f), "%s/pieces/registry/phymoji_assets/%s", tmp, r->name); snprintf(g, sizeof(g), "%s/pieces/registry/phymoji_assets/%s", tmp, r->newname);
    if (exists(f) && rename(f, g) != 0) return -1;
    char iid[32]; new_instance_id(pals, iid, sizeof(iid));
    snprintf(f, sizeof(f), "%s/instance_id.txt", tmp);
    FILE *w = fopen(f, "w"); if (!w) return -1; fprintf(w, "%s\n", iid); fclose(w);
    if (rename(tmp, dst) != 0) { say("ERROR rename %s\n", dst); return -1; }
    say("COPIED %s -> %s (%ld files, instance %s)\n", r->name, r->newname, files, iid);
    return 0;
}

int main(int argc, char **argv) {
    const char *pos[5]; int np = 0; const char *report = NULL, *desks = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--apply")) g_apply = 1;
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
        else if (!strcmp(argv[i], "--desks-dir") && i + 1 < argc) desks = argv[++i];
        else if (argv[i][0] != '-' && np < 5) pos[np++] = argv[i];
        else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
    }
    if (np != 5) { fprintf(stderr, "usage: desk_copy_op <livedesk_dir> <src_desk> <dst_desk> <from_prefix> <to_prefix> [--apply] [--report FILE] [--desks-dir DIR]\n"); return 2; }
    if (report) g_rep = fopen(report, "a");
    char pals[PB], dd[PB], srcf[PB], dstf[PB];
    snprintf(pals, sizeof(pals), "%s/pals", pos[0]);
    if (desks) snprintf(dd, sizeof(dd), "%s", desks); else snprintf(dd, sizeof(dd), "%s/sessions/s1/desks", pos[0]);
    snprintf(srcf, sizeof(srcf), "%s/%s", dd, pos[1]); snprintf(dstf, sizeof(dstf), "%s/%s", dd, pos[2]);
    if (!isdir(pals) || !exists(srcf)) { fprintf(stderr, "missing pals dir or source desk: %s / %s\n", pals, srcf); return 2; }
    if (strchr(pos[2], '/') || strchr(pos[1], '/')) { fprintf(stderr, "desk names must not contain /\n"); return 2; }
    if (exists(dstf)) { say("REFUSED: destination desk already exists: %s (nothing written)\n", dstf); return 2; }
    if (read_desk(srcf, pos[3], pos[4]) != 0 || nrows == 0) { fprintf(stderr, "cannot read DESK rows from %s\n", srcf); return 1; }
    int ncopy = 0, bad = 0;
    for (int i = 0; i < nrows; i++) {
        if (rows[i].copy) {
            char s[PB], d[PB]; snprintf(s, sizeof(s), "%s/%s", pals, rows[i].name); snprintf(d, sizeof(d), "%s/%s", pals, rows[i].newname);
            if (!isdir(s)) { say("MISSING source pal %s\n", s); bad++; }
            else if (exists(d)) { say("REFUSED: destination pal already exists: %s (nothing written)\n", d); return 2; }
            else ncopy++;
        }
    }
    if (bad) { say("%d source pal(s) missing: nothing written\n", bad); return 1; }
    say("%s: %s -> %s: %d row(s), %d pal(s) to copy, %d shared by reference\n", g_apply ? "APPLY" : "DRY RUN", pos[1], pos[2], nrows, ncopy, nrows - ncopy);
    for (int i = 0; i < nrows; i++) say("  %s %s%s%s\n", rows[i].copy ? "COPY  " : "SHARE ", rows[i].name, rows[i].copy ? " -> " : "", rows[i].copy ? rows[i].newname : "");
    if (!g_apply) { say("dry run: nothing written (use --apply)\n"); return 0; }
    int done = 0;
    for (int i = 0; i < nrows; i++) if (rows[i].copy) { if (copy_pal(pals, &rows[i], NULL) != 0) { say("FAILED at %s after %d copied pal(s); desk file NOT written (copied pals remain, remove them to retry)\n", rows[i].name, done); return 1; } done++; }
    char tmpd[PB]; snprintf(tmpd, sizeof(tmpd), "%s.tmp_dc", dstf);
    FILE *w = fopen(tmpd, "w"); if (!w) return 1;
    fprintf(w, "# %s - copy of %s made by desk_copy_op (pals %s* -> %s*; identity and phones are minted on first spawn)\n", pos[2], pos[1], pos[3], pos[4]);
    for (int i = 0; i < nrows; i++) {
        char path[PB]; snprintf(path, sizeof(path), "%s", rows[i].path);
        if (rows[i].copy) {
            char *at = strstr(path, rows[i].name);
            if (at) { char np2[PB]; snprintf(np2, sizeof(np2), "%.*s%s%s", (int)(at - path), path, rows[i].newname, at + strlen(rows[i].name)); snprintf(path, sizeof(path), "%s", np2); }
        }
        fprintf(w, "DESK | %s | %s | %s\n", rows[i].copy ? rows[i].newname : rows[i].name, path, rows[i].rest);
    }
    if (fclose(w) != 0 || rename(tmpd, dstf) != 0) { say("ERROR writing %s\n", dstf); return 1; }
    say("WROTE desk %s (%d rows). Source desk and source pals untouched.\n", dstf, nrows);
    if (g_rep) fclose(g_rep);
    return 0;
}
