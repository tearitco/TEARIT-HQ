/* bv_session_sync - after a build, bring every live board-viewer SESSION's private binary copies up to date (owner 2026-11-05, audit decision D5 in
 * HQ-IQ-BOOK 02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md).
 *   bv_session_sync <house> [--dry-run]
 * Why: a session copies the files of ops/+x into <house>/&.widgits/board-viewer/pieces/sessions/<id>/ops/+x/ when it starts and runs ITS copy, so a rebuilt bv_menu_input / bv_render_* / bv_dispatch changes nothing for an open board
 * until it restarts (swapped by hand three times on 2026-11-05). This replaces a session's copy of a binary ONLY when (1) the session already has that file and (2) the bytes differ from the freshly built
 * <house>/&.widgits/board-viewer/ops/+x/<name>. The write is temp + rename (a running process keeps its old inode until it restarts; a short-lived op picks the new file at its next start). Never adds a file
 * a session does not have, never touches another house or system/ files. One line per file: updated / same / skipped; --dry-run prints "would update" and changes nothing. Exit 0, 2 usage.
 * Plain C, no shell. Cost: reads each pair of files once when run by hand; nothing at idle. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
static int same_bytes(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb"); int same = 1; if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    unsigned char x[8192], y[8192]; size_t ka, kb;
    do { ka = fread(x, 1, sizeof x, fa); kb = fread(y, 1, sizeof y, fb); if (ka != kb || memcmp(x, y, ka)) { same = 0; break; } } while (ka > 0);
    fclose(fa); fclose(fb); return same;
}
static int copy_atomic(const char *src, const char *dst, mode_t mode) {
    char tmp[2 * PATH_MAX + 700]; snprintf(tmp, sizeof tmp, "%s.sync_tmp", dst); FILE *a = fopen(src, "rb"), *b = a ? fopen(tmp, "wb") : NULL; unsigned char buf[8192]; size_t k;
    if (!a || !b) { if (a) fclose(a); return -1; }
    while ((k = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, k, b) != k) { fclose(a); fclose(b); remove(tmp); return -1; }
    fclose(a); if (fclose(b) != 0) { remove(tmp); return -1; } chmod(tmp, mode); return rename(tmp, dst);
}
int main(int argc, char **argv) {
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--dry-run"))) { fprintf(stderr, "usage: bv_session_sync <house> [--dry-run]\n"); return 2; }
    int dry = argc == 3; char built[PATH_MAX], sess[PATH_MAX]; snprintf(built, sizeof built, "%s/&.widgits/board-viewer/ops/+x", argv[1]); snprintf(sess, sizeof sess, "%s/&.widgits/board-viewer/pieces/sessions", argv[1]);
    DIR *sd = opendir(sess); if (!sd) { printf("no sessions folder: nothing to do\n"); return 0; }
    int up = 0, same = 0, skipped = 0, nsess = 0; struct dirent *s;
    while ((s = readdir(sd))) { if (s->d_name[0] == '.') continue; char dir[PATH_MAX + 300]; snprintf(dir, sizeof dir, "%s/%s/ops/+x", sess, s->d_name); DIR *od = opendir(dir); if (!od) continue; nsess++; struct dirent *f;
        while ((f = readdir(od))) { if (f->d_name[0] == '.' || strstr(f->d_name, ".sync_tmp")) continue; char sp[2 * PATH_MAX + 600], bp[PATH_MAX + 300]; snprintf(sp, sizeof sp, "%s/%s", dir, f->d_name); snprintf(bp, sizeof bp, "%s/%s", built, f->d_name);
            struct stat st; if (stat(bp, &st) != 0 || !S_ISREG(st.st_mode)) { printf("skipped  %s/%s (not built here)\n", s->d_name, f->d_name); skipped++; continue; }
            if (same_bytes(sp, bp)) { same++; continue; }
            if (dry) { printf("would update  %s/%s\n", s->d_name, f->d_name); up++; continue; }
            if (copy_atomic(bp, sp, st.st_mode & 0777) == 0) { printf("updated  %s/%s\n", s->d_name, f->d_name); up++; } else { printf("FAILED   %s/%s\n", s->d_name, f->d_name); skipped++; } }
        closedir(od); }
    closedir(sd); printf("%d session(s): %d %s, %d already current, %d skipped%s\n", nsess, up, dry ? "would be updated" : "updated", same, skipped, up && !dry ? " (a running engine keeps its old copy until it restarts)" : ""); return 0;
}
