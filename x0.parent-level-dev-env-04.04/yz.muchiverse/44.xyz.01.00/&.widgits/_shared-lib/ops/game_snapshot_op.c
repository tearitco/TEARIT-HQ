/* game_snapshot_op - CONTENT-ADDRESSED SNAPSHOT STORE with a real RESTORE (CLOCK-AS-THE-PLAY-SPINE-DESIGN.md sections 7.2-7.5, build order step (a)+(b)).
 * Successor to game_slot_op's v1 (which only hashes and restores nothing). game_slot_op is left untouched; nothing calls this op yet.
 *
 * SAFETY: this op never touches live data on its own. It reads/writes ONLY the explicit paths given on the command line:
 *   <store_dir>  (written by save/restore --apply; objects, checkpoints, ledger)   <tree_dir> (READ only, by save and diff)
 *   <dest_dir>   (written only by restore --apply)   --extra FILE (read only)   --extra-dir DIR (written only by restore --apply)
 * No path is derived from the environment or from a default location.
 *
 * Usage:
 *   game_snapshot_op <store_dir> <tree_dir> save <id> [--extra FILE]... [--now-ms MS]
 *   game_snapshot_op <store_dir> <tree_dir> list                 (tree_dir is ignored, may be any existing path or "-")
 *   game_snapshot_op <store_dir> <tree_dir> verify <id>          (re-hash every stored object of <id> against its manifest)
 *   game_snapshot_op <store_dir> <tree_dir> restore <id> <dest_dir> [--apply] [--prune] [--extra-dir DIR] [--now-ms MS]
 *   game_snapshot_op <store_dir> <tree_dir> diff <id>            (<id> vs the live <tree_dir>: same / changed / missing / new)
 *
 * Store layout (all under <store_dir>):
 *   objects/<sha256[0:2]>/<sha256>      file contents, stored ONCE, shared by every checkpoint (mode 0444; temp file + rename)
 *   checkpoints/<id>/manifest.txt       sha256|size|mode|relpath, sorted by relpath (mode = 4 octal digits, permission bits 0777 only:
 *                                       setuid/setgid/sticky are never recorded or restored). Extra files live under the prefix __extra__/
 *   checkpoints/<id>/meta.pdl           SLOT | key | value : id, saved_ms, files, bytes, manifest_sha, parent
 *                                       (written in a temp dir then renamed into place, so a checkpoint is all-or-nothing)
 *   ledger.txt                          APPEND ONLY, never truncated:  <ms>|save|<id>|files=F|bytes=B|manifest=H|parent=P
 *                                       and, on restore --apply:       <ms>|rewind|from=<manifest sha of the previous head, or ->|to=<id>
 *   head.txt                            pointer: id of the current head (the next save's parent). Restore --apply moves it to the restored id,
 *                                       so a save after a rewind forks a new branch (parent = the restored id); old checkpoints stay readable.
 * save walks <tree_dir>: regular files only, symlinks are NOT followed (and not stored), directories named .git are not entered, and these are
 * the documented RUNTIME-SKIP list (never snapshotted, never reported, never pruned): *.pid, *.lock, *.tmp, cli_io_state.txt, interact_relay.txt.
 * --extra FILE adds an outside file (e.g. the clock state file, the schedule ledger) as __extra__/<basename>; basenames must be unique.
 * A file name containing '|' or a newline is refused (exit 1), never silently dropped. The store must not be inside <tree_dir>.
 * restore is a DRY RUN unless --apply. The whole plan is checked first (manifest sha vs meta, every object present and re-hashed, every path
 * relative with no '..', no symlink in any dest path component); any problem refuses with NOTHING written. Apply writes only under <dest_dir>
 * (created if absent): temp file, fchmod, atomic rename. Files in <dest_dir> that are not in the manifest are left alone and reported as
 * "extra" (they are deleted only with --prune; the runtime-skip list is exempt). __extra__ files are reported as "extra-file|..." and are
 * written only if --extra-dir DIR is given (under their basenames), so the caller decides where the clock state goes.
 * Marker rule: nothing here reads mtime. Time is only recorded (gettimeofday, or --now-ms for reproducible tests).
 * Exit: 0 ok | 1 failure (io, bad tree, missing checkpoint) | 2 usage or REFUSED (id exists, unsafe path, store inside tree) |
 *       3 corrupt or missing object / manifest does not match meta | 4 diff found differences. Output: line per action then one summary line. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ftw.h>
#ifndef FTW_ACTIONRETVAL
/* macOS has no nftw action return codes: a nonzero callback return STOPS the walk, so 'skip subtree' becomes 'continue' and .git files are filtered by path in visit(). */
#define FTW_ACTIONRETVAL 0
#define FTW_CONTINUE 0
#define FTW_SKIP_SUBTREE 0
#endif
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../khtpm_phone.c"   /* ph_sha_hex: the house's one SHA-256 (same text-include game_slot_op uses; no new header) */

#define GP 4096
#define XP "__extra__/"
typedef struct { char sha[65]; long long size; unsigned mode; char *path; } Ent;
static Ent *E; static int NE, CAP;
static char ROOT[GP], STORE[GP]; static size_t ROOTLEN; static int STORING, SCAN_ERR, HASHING;
static long long NEWOBJ, REUSED;

static void die(int rc, const char *msg, const char *a) { fprintf(stderr, "game_snapshot_op: %s%s%s\n", msg, a ? " " : "", a ? a : ""); exit(rc); }
static long long now_ms(void) { struct timeval tv; gettimeofday(&tv, NULL); return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000; }
static int cmp_ent(const void *a, const void *b) { return strcmp(((const Ent *)a)->path, ((const Ent *)b)->path); }
static void add_ent(const char *sha, long long size, unsigned mode, const char *path) {
    if (NE == CAP) { CAP = CAP ? CAP * 2 : 1024; E = realloc(E, (size_t)CAP * sizeof(Ent)); if (!E) die(1, "out of memory", NULL); }
    snprintf(E[NE].sha, sizeof(E[NE].sha), "%s", sha); E[NE].size = size; E[NE].mode = mode; E[NE].path = strdup(path); NE++;
}
static void free_ents(void) { for (int i = 0; i < NE; i++) free(E[i].path); free(E); E = NULL; NE = CAP = 0; }
static int skipname(const char *base) {
    size_t n = strlen(base);
    if (n >= 4 && (!strcmp(base + n - 4, ".pid") || !strcmp(base + n - 4, ".tmp"))) return 1;
    if (n >= 5 && !strcmp(base + n - 5, ".lock")) return 1;
    return !strcmp(base, "cli_io_state.txt") || !strcmp(base, "interact_relay.txt");
}
static char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    struct stat st; if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)) { fclose(f); return NULL; }
    char *b = malloc((size_t)st.st_size + 1); if (!b) { fclose(f); return NULL; }
    size_t n = fread(b, 1, (size_t)st.st_size, f); int bad = ferror(f); fclose(f);
    if (bad) { free(b); return NULL; }
    b[n] = 0; *len = n; return b;
}
static int put_atomic(const char *path, const char *data, size_t len, mode_t mode) {
    char tmp[GP + 32]; snprintf(tmp, sizeof(tmp), "%s.tmp%d", path, (int)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600); if (fd < 0) return -1;
    size_t off = 0; while (off < len) { ssize_t w = write(fd, data + off, len - off); if (w <= 0) { close(fd); unlink(tmp); return -1; } off += (size_t)w; }
    if (fchmod(fd, mode) != 0 || close(fd) != 0 || rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}
static void objpath(const char *sha, char *out, size_t n) { snprintf(out, n, "%s/objects/%.2s/%s", STORE, sha, sha); }
static int store_object(const char *sha, const char *data, size_t len) {
    char p[GP + 64], d[GP + 64]; struct stat st; objpath(sha, p, sizeof(p));
    if (stat(p, &st) == 0 && (size_t)st.st_size == len) { REUSED++; return 0; }       /* already stored (verify catches a tampered one) */
    snprintf(d, sizeof(d), "%s/objects", STORE); mkdir(d, 0755); snprintf(d, sizeof(d), "%s/objects/%.2s", STORE, sha); mkdir(d, 0755);
    if (put_atomic(p, data, len, 0444) != 0) return -1;
    NEWOBJ++; return 0;
}
static int visit(const char *fpath, const struct stat *sb, int type, struct FTW *ftw) {
    const char *base = fpath + ftw->base;
    if (type == FTW_D) return !strcmp(base, ".git") && ftw->level > 0 ? FTW_SKIP_SUBTREE : FTW_CONTINUE;
    if (type != FTW_F || !S_ISREG(sb->st_mode) || skipname(base)) return FTW_CONTINUE;      /* FTW_SL (symlink) is skipped: never followed */
    const char *rel = fpath + ROOTLEN; while (*rel == '/') rel++;
    if (!strncmp(rel, ".git/", 5) || strstr(rel, "/.git/")) return FTW_CONTINUE;   /* same result as FTW_SKIP_SUBTREE on glibc, and the only way on macOS */
    if (strchr(rel, '|') || strchr(rel, '\n')) { fprintf(stderr, "game_snapshot_op: refused file name with '|' or newline: %s\n", rel); SCAN_ERR = 1; return FTW_CONTINUE; }
    char sha[65] = "-"; unsigned mode = sb->st_mode & 0777;
    if (HASHING) {
        size_t n = 0; char *buf = read_all(fpath, &n);
        if (!buf) { fprintf(stderr, "game_snapshot_op: cannot read %s\n", fpath); SCAN_ERR = 1; return FTW_CONTINUE; }
        ph_sha_hex((unsigned char *)buf, n, sha);
        if (STORING && store_object(sha, buf, n) != 0) { fprintf(stderr, "game_snapshot_op: cannot store object for %s\n", fpath); SCAN_ERR = 1; }
        add_ent(sha, (long long)n, mode, rel); free(buf);
    } else add_ent(sha, (long long)sb->st_size, mode, rel);
    return FTW_CONTINUE;
}
static int scan(const char *root, int hashing) {
    snprintf(ROOT, sizeof(ROOT), "%s", root); ROOTLEN = strlen(ROOT); while (ROOTLEN > 1 && ROOT[ROOTLEN - 1] == '/') ROOT[--ROOTLEN] = 0;
    free_ents(); HASHING = hashing; SCAN_ERR = 0;
    if (nftw(ROOT, visit, 32, FTW_PHYS | FTW_ACTIONRETVAL) != 0 || SCAN_ERR) return -1;
    qsort(E, (size_t)NE, sizeof(Ent), cmp_ent); return 0;
}
static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static void ledger(const char *line) {
    char p[GP + 16], buf[GP]; snprintf(p, sizeof(p), "%s/ledger.txt", STORE);
    int n = snprintf(buf, sizeof(buf), "%s\n", line); int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0 || write(fd, buf, (size_t)n) != n) die(1, "cannot append ledger", p); close(fd);
}
static int valid_id(const char *id) {
    if (!*id || id[0] == '.' || strlen(id) > 120) return 0;
    for (const char *c = id; *c; c++) if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_' || *c == '-' || *c == '.')) return 0;
    return 1;
}
static void head_read(char *out, size_t n) {
    char p[GP + 16]; size_t len = 0; snprintf(p, sizeof(p), "%s/head.txt", STORE); char *b = read_all(p, &len);
    out[0] = 0; if (b) { snprintf(out, n, "%s", b); out[strcspn(out, "\r\n")] = 0; free(b); }
}
static void head_write(const char *id) { char p[GP + 16], l[256]; snprintf(p, sizeof(p), "%s/head.txt", STORE); int n = snprintf(l, sizeof(l), "%s\n", id); put_atomic(p, l, (size_t)n, 0644); }
static int meta_get(const char *id, const char *key, char *out, size_t n) {   /* value of "SLOT | key | value" in checkpoints/<id>/meta.pdl */
    char p[GP + 64], want[80], *b, *s; size_t len; snprintf(p, sizeof(p), "%s/checkpoints/%s/meta.pdl", STORE, id);
    if (!(b = read_all(p, &len))) return -1; snprintf(want, sizeof(want), "SLOT | %s | ", key);
    for (s = strtok(b, "\n"); s; s = strtok(NULL, "\n")) if (!strncmp(s, want, strlen(want))) { snprintf(out, n, "%s", s + strlen(want)); free(b); return 0; }
    free(b); return -1;
}
/* load + validate a checkpoint manifest into E; rc 0 ok, else the exit code to use (message printed). */
static int load_manifest(const char *id, char *msha) {
    char p[GP + 64], want[80]; size_t len = 0; snprintf(p, sizeof(p), "%s/checkpoints/%s/manifest.txt", STORE, id);
    char *m = read_all(p, &len); if (!m) { fprintf(stderr, "game_snapshot_op: no checkpoint %s\n", id); return 1; }
    ph_sha_hex((unsigned char *)m, len, msha);
    if (meta_get(id, "manifest_sha", want, sizeof(want)) != 0 || strcmp(want, msha)) { fprintf(stderr, "game_snapshot_op: REFUSED: manifest of %s does not match its meta.pdl manifest_sha (tampered or incomplete)\n", id); free(m); return 3; }
    free_ents();
    for (char *line = m, *nl; line && *line; line = nl ? nl + 1 : NULL) {
        nl = strchr(line, '\n'); if (nl) *nl = 0;
        char *a = strchr(line, '|'), *b = a ? strchr(a + 1, '|') : NULL, *c = b ? strchr(b + 1, '|') : NULL;
        if (!c || a - line != 64) { fprintf(stderr, "game_snapshot_op: REFUSED: malformed manifest row in %s\n", id); free(m); return 3; }
        *a = *b = *c = 0; add_ent(line, atoll(a + 1), (unsigned)strtoul(b + 1, NULL, 8) & 0777, c + 1);
    }
    free(m); return 0;
}
static int path_safe(const char *rel) {   /* relative, no empty / "." / ".." component */
    if (!*rel || rel[0] == '/') return 0;
    for (const char *s = rel; *s;) { const char *e = strchr(s, '/'); size_t n = e ? (size_t)(e - s) : strlen(s);
        if (n == 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) return 0; s = e ? e + 1 : s + n; }
    return 1;
}
/* check (and with make=1 create) every directory component of base/rel; refuse symlinks. rc 0 ok. */
static int walk_parents(const char *base, const char *rel, int make) {
    char cur[GP + 8]; struct stat st; snprintf(cur, sizeof(cur), "%s", base);
    if (lstat(cur, &st) == 0) { if (!is_dir(cur)) return -1; }
    else if (make) { if (mkdir(cur, 0755) != 0) return -1; }
    for (const char *s = rel, *e; (e = strchr(s, '/')); s = e + 1) {
        size_t l = strlen(cur); snprintf(cur + l, sizeof(cur) - l, "/%.*s", (int)(e - s), s);
        if (lstat(cur, &st) == 0) { if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) return -1; }
        else if (make) { if (mkdir(cur, 0755) != 0) return -1; } else return 0;   /* absent in a dry run: nothing deeper exists */
    }
    char leaf[GP + 8]; snprintf(leaf, sizeof(leaf), "%s/%s", base, rel);
    if (lstat(leaf, &st) == 0 && !S_ISREG(st.st_mode)) return -1;                 /* symlink or dir where a file goes */
    return 0;
}
static int object_ok(const Ent *e) {
    char p[GP + 64]; size_t n = 0; char sha[65]; objpath(e->sha, p, sizeof(p)); char *b = read_all(p, &n);
    if (!b) return 0; ph_sha_hex((unsigned char *)b, n, sha); int ok = !strcmp(sha, e->sha) && (long long)n == e->size; free(b); return ok;
}

static int cmd_save(const char *tree, const char *id, int nx, char **xf, long long ms) {
    char rs[PATH_MAX], rt[PATH_MAX], d[GP + 64], tmpd[GP + 64], p[GP + 128], parent[256], mh[65], led[GP], meta[2048];
    if (!valid_id(id)) die(2, "REFUSED: bad checkpoint id (use [A-Za-z0-9._-], no leading dot)", id);
    if (!is_dir(tree)) die(1, "no tree dir", tree);
    int made = mkdir(STORE, 0755) == 0; if (!realpath(STORE, rs) || !realpath(tree, rt)) die(1, "cannot resolve paths", NULL);
    { size_t n = strlen(rt); if (!strncmp(rs, rt, n) && (rs[n] == '/' || rs[n] == 0)) { if (made) rmdir(STORE); die(2, "REFUSED: store is inside the tree", STORE); } }
    snprintf(d, sizeof(d), "%s/checkpoints", STORE); mkdir(d, 0755);
    snprintf(d, sizeof(d), "%s/checkpoints/%s", STORE, id); if (access(d, F_OK) == 0) die(2, "REFUSED: checkpoint already exists (ids are never overwritten):", id);
    STORING = 1; NEWOBJ = REUSED = 0;
    if (scan(tree, 1) != 0) die(1, "cannot scan/store tree", tree);
    for (int i = 0; i < NE; i++) if (!strncmp(E[i].path, XP, strlen(XP))) die(2, "REFUSED: tree file under reserved prefix", E[i].path);
    for (int i = 0; i < nx; i++) {       /* extras: outside files, stored under __extra__/<basename> */
        const char *bn = strrchr(xf[i], '/'); bn = bn ? bn + 1 : xf[i]; char rel[GP]; size_t n = 0; struct stat st; char sha[65];
        snprintf(rel, sizeof(rel), XP "%s", bn); if (!*bn || strchr(bn, '|') || strchr(bn, '\n')) die(2, "REFUSED: bad --extra name", xf[i]);
        for (int j = 0; j < NE; j++) if (!strcmp(E[j].path, rel)) die(2, "REFUSED: duplicate --extra basename", bn);
        if (lstat(xf[i], &st) != 0 || !S_ISREG(st.st_mode)) die(1, "--extra is not a regular file:", xf[i]);
        char *buf = read_all(xf[i], &n); if (!buf) die(1, "cannot read --extra", xf[i]);
        ph_sha_hex((unsigned char *)buf, n, sha); if (store_object(sha, buf, n) != 0) die(1, "cannot store --extra", xf[i]);
        add_ent(sha, (long long)n, st.st_mode & 0777, rel); free(buf);
    }
    qsort(E, (size_t)NE, sizeof(Ent), cmp_ent);
    size_t cap = 256, len = 0; char *m = malloc(cap); long long bytes = 0; if (!m) die(1, "out of memory", NULL);
    for (int i = 0; i < NE; i++) { size_t need = strlen(E[i].path) + 120; if (len + need > cap) { cap = (cap + need) * 2; m = realloc(m, cap); if (!m) die(1, "out of memory", NULL); }
        len += (size_t)snprintf(m + len, cap - len, "%s|%lld|%04o|%s\n", E[i].sha, E[i].size, E[i].mode, E[i].path); bytes += E[i].size; }
    ph_sha_hex((unsigned char *)m, len, mh); head_read(parent, sizeof(parent)); if (!parent[0]) strcpy(parent, "-");
    snprintf(tmpd, sizeof(tmpd), "%s/checkpoints/.%s.tmp%d", STORE, id, (int)getpid()); if (mkdir(tmpd, 0755) != 0) die(1, "cannot create temp checkpoint dir", tmpd);
    snprintf(p, sizeof(p), "%s/manifest.txt", tmpd); if (put_atomic(p, m, len, 0644) != 0) die(1, "cannot write", p);
    snprintf(meta, sizeof(meta), "# checkpoint %s - see game_snapshot_op.c\nSLOT | id | %s\nSLOT | saved_ms | %lld\nSLOT | files | %d\nSLOT | bytes | %lld\nSLOT | manifest_sha | %s\nSLOT | parent | %s\n", id, id, ms, NE, bytes, mh, parent);
    snprintf(p, sizeof(p), "%s/meta.pdl", tmpd); if (put_atomic(p, meta, strlen(meta), 0644) != 0) die(1, "cannot write", p);
    if (rename(tmpd, d) != 0) die(1, "cannot publish checkpoint", d);
    snprintf(led, sizeof(led), "%lld|save|%s|files=%d|bytes=%lld|manifest=%s|parent=%s", ms, id, NE, bytes, mh, parent); ledger(led); head_write(id);
    printf("saved %s: files=%d bytes=%lld new_objects=%lld reused=%lld manifest=%.12s parent=%s\n", id, NE, bytes, NEWOBJ, REUSED, mh, parent);
    free(m); free_ents(); return 0;
}
static int cmd_list(void) {
    char d[GP + 16]; snprintf(d, sizeof(d), "%s/checkpoints", STORE); DIR *dp = opendir(d); if (!dp) { printf("0 checkpoints\n"); return 0; }
    struct { char id[128]; long long ms; } L[4096]; int n = 0; struct dirent *de;
    while ((de = readdir(dp)) && n < 4096) { if (de->d_name[0] == '.' || strlen(de->d_name) > 120) continue; char v[64]; snprintf(L[n].id, sizeof(L[n].id), "%s", de->d_name);
        L[n].ms = meta_get(de->d_name, "saved_ms", v, sizeof(v)) == 0 ? atoll(v) : 0; n++; }
    closedir(dp);
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && (L[j].ms < L[j - 1].ms || (L[j].ms == L[j - 1].ms && strcmp(L[j].id, L[j - 1].id) < 0)); j--) { __typeof__(L[0]) t = L[j]; L[j] = L[j - 1]; L[j - 1] = t; }
    for (int i = 0; i < n; i++) { char f[32] = "?", b[32] = "?", mh[80] = "?", par[160] = "?"; meta_get(L[i].id, "files", f, 32); meta_get(L[i].id, "bytes", b, 32); meta_get(L[i].id, "manifest_sha", mh, 80); meta_get(L[i].id, "parent", par, 160);
        printf("%s|%lld|files=%s|bytes=%s|manifest=%.12s|parent=%s\n", L[i].id, L[i].ms, f, b, mh, par); }
    printf("%d checkpoints\n", n); return 0;
}
static int cmd_verify(const char *id) {
    char msha[65]; int rc = load_manifest(id, msha), bad = 0; if (rc) return rc;
    for (int i = 0; i < NE; i++) if (!object_ok(&E[i])) { printf("BAD|%s|%s\n", E[i].sha, E[i].path); bad++; }
    if (bad) { printf("verify %s: FAILED objects=%d bad=%d\n", id, NE, bad); return 3; }
    printf("verify %s: OK objects=%d\n", id, NE); return 0;
}
static int cmd_diff(const char *tree, const char *id) {
    char msha[65]; int rc = load_manifest(id, msha); if (rc) return rc; if (!is_dir(tree)) die(1, "no tree dir", tree);
    Ent *M = E; int NM = NE; E = NULL; NE = CAP = 0;
    if (scan(tree, 1) != 0) die(1, "cannot scan tree", tree);
    char *seen = calloc((size_t)NE + 1, 1); int same = 0, changed = 0, missing = 0, fresh = 0; if (!seen) die(1, "out of memory", NULL);
    for (int i = 0; i < NM; i++) {
        if (!strncmp(M[i].path, XP, strlen(XP))) continue;                                  /* extras are not in the tree */
        Ent *hit = bsearch(&M[i], E, (size_t)NE, sizeof(Ent), cmp_ent);
        if (!hit) { printf("missing|%s\n", M[i].path); missing++; continue; }
        seen[hit - E] = 1;
        if (!strcmp(hit->sha, M[i].sha) && hit->mode == M[i].mode) same++; else { printf("changed|%s\n", M[i].path); changed++; }
    }
    for (int i = 0; i < NE; i++) if (!seen[i]) { printf("new|%s\n", E[i].path); fresh++; }
    printf("diff %s: same=%d changed=%d missing=%d new=%d\n", id, same, changed, missing, fresh);
    for (int i = 0; i < NM; i++) free(M[i].path); free(M); free(seen);
    return changed || missing || fresh ? 4 : 0;
}
static int place(const char *base, const char *rel, const Ent *e) {   /* write one object to base/rel: temp file, fchmod, atomic rename */
    char op[GP + 64], dst[GP + 8]; size_t n = 0; objpath(e->sha, op, sizeof(op)); char *b = read_all(op, &n); if (!b) return -1;
    snprintf(dst, sizeof(dst), "%s/%s", base, rel);
    if (walk_parents(base, rel, 1) != 0) { free(b); return -1; }
    int rc = put_atomic(dst, b, n, e->mode); free(b); return rc;
}
static int cmd_restore(const char *id, const char *dest, int apply, int prune, const char *xdir, long long ms) {
    char msha[65]; int rc = load_manifest(id, msha); if (rc) return rc;
    Ent *M = E; int NM = NE; E = NULL; NE = CAP = 0;
    int nc = 0, no = 0, ns = 0, nx = 0, ne = 0;
    char *kind = calloc((size_t)NM + 1, 1); if (!kind) die(1, "out of memory", NULL);       /* 'c' create 'o' overwrite 's' same 'x' extra-file */
    /* pass 1: validate everything, write nothing */
    for (int i = 0; i < NM; i++) {
        int isx = !strncmp(M[i].path, XP, strlen(XP)); const char *rel = isx ? M[i].path + strlen(XP) : M[i].path;
        if (!path_safe(rel) || (isx && strchr(rel, '/'))) { fprintf(stderr, "game_snapshot_op: REFUSED: unsafe path in manifest: %s (nothing written)\n", M[i].path); return 2; }
        if (!object_ok(&M[i])) { fprintf(stderr, "game_snapshot_op: REFUSED: object missing or corrupt for %s (%s); nothing written\n", M[i].path, M[i].sha); return 3; }
        if (isx) { kind[i] = 'x'; if (xdir && walk_parents(xdir, rel, 0) != 0) { fprintf(stderr, "game_snapshot_op: REFUSED: unsafe extra-dir path %s\n", rel); return 2; } continue; }
        if (walk_parents(dest, rel, 0) != 0) { fprintf(stderr, "game_snapshot_op: REFUSED: symlink or non-file in destination path for %s (nothing written)\n", rel); return 2; }
        char dst[GP + 8]; struct stat st; snprintf(dst, sizeof(dst), "%s/%s", dest, rel);
        if (lstat(dst, &st) != 0) kind[i] = 'c';
        else { size_t n = 0; char *b = read_all(dst, &n), sha[65] = ""; if (b) { ph_sha_hex((unsigned char *)b, n, sha); free(b); }
               kind[i] = (!strcmp(sha, M[i].sha) && (st.st_mode & 0777) == M[i].mode) ? 's' : 'o'; }
    }
    for (int i = 0; i < NM; i++) { if (kind[i] == 'c') nc++; else if (kind[i] == 'o') no++; else if (kind[i] == 's') ns++; else nx++; }
    /* extras present in dest but not in the manifest */
    char *xl = NULL; size_t xlen = 0; int *xi = NULL;
    if (is_dir(dest)) {
        Ent *keep = M; int nk = NM; M = NULL; NM = 0;
        if (scan(dest, 0) != 0) die(1, "cannot scan dest", dest);
        for (int i = 0; i < NE; i++) { Ent *hit = bsearch(&E[i], keep, (size_t)nk, sizeof(Ent), cmp_ent); if (!hit) { ne++; xlen += strlen(E[i].path) + 1; } }
        xl = malloc(xlen + 1); xi = malloc(sizeof(int) * (size_t)(ne + 1)); if (!xl || !xi) die(1, "out of memory", NULL);
        { int k = 0; for (int i = 0; i < NE; i++) { Ent *hit = bsearch(&E[i], keep, (size_t)nk, sizeof(Ent), cmp_ent); if (!hit) xi[k++] = i; } }
        M = keep; NM = nk;
    }
    for (int i = 0; i < NM; i++) {
        if (kind[i] == 'c') printf("%s|%s\n", apply ? "create" : "would-create", M[i].path);
        else if (kind[i] == 'o') printf("%s|%s\n", apply ? "overwrite" : "would-overwrite", M[i].path);
        else if (kind[i] == 'x') printf("extra-file|%s|%s|%04o\n", M[i].path, M[i].sha, M[i].mode);
    }
    for (int k = 0; k < ne; k++) printf("%s|%s\n", prune ? (apply ? "remove" : "would-remove") : "extra", E[xi[k]].path);
    if (!apply) { printf("DRY RUN restore %s -> %s: create=%d overwrite=%d same=%d extra=%d extra_files=%d (nothing written; pass --apply)\n", id, dest, nc, no, ns, ne, nx); return 0; }
    /* pass 2: apply (dest only) */
    for (int i = 0; i < NM; i++) {
        if (kind[i] == 'c' || kind[i] == 'o') { if (place(dest, M[i].path, &M[i]) != 0) { fprintf(stderr, "game_snapshot_op: FAILED writing %s (restore incomplete)\n", M[i].path); return 1; } }
        else if (kind[i] == 'x' && xdir) { if (place(xdir, M[i].path + strlen(XP), &M[i]) != 0) { fprintf(stderr, "game_snapshot_op: FAILED writing extra %s\n", M[i].path); return 1; } }
    }
    if (prune) for (int k = 0; k < ne; k++) { char dst[GP + 8]; snprintf(dst, sizeof(dst), "%s/%s", dest, E[xi[k]].path); unlink(dst); }
    { char from[96], hb[256], led[GP], cur[96] = "-"; head_read(hb, sizeof(hb)); if (hb[0] && meta_get(hb, "manifest_sha", from, sizeof(from)) == 0) snprintf(cur, sizeof(cur), "%s", from);
      snprintf(led, sizeof(led), "%lld|rewind|from=%s|to=%s", ms, cur, id); ledger(led); head_write(id); }
    printf("RESTORED %s -> %s: created=%d overwritten=%d same=%d extra=%d%s extra_files=%d%s\n", id, dest, nc, no, ns, ne, prune ? " (removed)" : " (left in place)", nx, xdir ? " (written to --extra-dir)" : " (reported only)");
    free(kind); free(xl); free(xi); return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: game_snapshot_op <store_dir> <tree_dir> save <id> [--extra FILE]... | list | verify <id> | restore <id> <dest_dir> [--apply] [--prune] [--extra-dir DIR] | diff <id>   [--now-ms MS]\n"); return 2; }
    const char *verb = argv[3], *tree = argv[2]; long long ms = 0; int apply = 0, prune = 0, nx = 0; const char *xdir = NULL; char *xf[64]; const char *pos[4]; int np = 0;
    snprintf(STORE, sizeof(STORE), "%s", argv[1]); { size_t l = strlen(STORE); while (l > 1 && STORE[l - 1] == '/') STORE[--l] = 0; }
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--apply")) apply = 1; else if (!strcmp(argv[i], "--prune")) prune = 1;
        else if (!strcmp(argv[i], "--extra") && i + 1 < argc && nx < 64) xf[nx++] = argv[++i];
        else if (!strcmp(argv[i], "--extra-dir") && i + 1 < argc) xdir = argv[++i];
        else if (!strcmp(argv[i], "--now-ms") && i + 1 < argc) ms = atoll(argv[++i]);
        else if (np < 4) pos[np++] = argv[i]; else die(2, "too many arguments", argv[i]);
    }
    if (!ms) ms = now_ms();
    if (!strcmp(verb, "save") && np == 1) return cmd_save(tree, pos[0], nx, xf, ms);
    if (!strcmp(verb, "list") && np == 0) return cmd_list();
    if (!strcmp(verb, "verify") && np == 1) return cmd_verify(pos[0]);
    if (!strcmp(verb, "diff") && np == 1) return cmd_diff(tree, pos[0]);
    if (!strcmp(verb, "restore") && np == 2) { if (!valid_id(pos[0])) die(2, "REFUSED: bad id", pos[0]); return cmd_restore(pos[0], pos[1], apply, prune, xdir, ms); }
    fprintf(stderr, "game_snapshot_op: bad verb or arguments\n"); return 2;
}
